#include "prometheus_sink.h"
#include <snappy.h>
#include <remote.pb.h>

using namespace std::chrono_literals;

template<class T>
std::optional<T> get_optional(const avro::GenericRecord &record, const std::string &name) {
  if (!record.hasField(name))
    throw std::invalid_argument("no such member: " + name);

  const avro::GenericDatum &field = record.field(name);

  if (field.type() == avro::AVRO_NULL)
    return std::nullopt;
  return kspp::avro_utils::convert<T>(field);
}

namespace kspp {
  prometheus_sink::prometheus_sink(std::shared_ptr<cluster_config> config,
                                   const kspp::connect::connection_params &cp,
                                   int32_t http_batch_size,
                                   std::chrono::milliseconds http_timeout)
      : kspp::topic_sink<kspp::generic_avro, kspp::generic_avro>(), _good(true), _closed(false), _start_running(false),
        _exit(false), _work(std::make_unique<boost::asio::io_service::work>(_ios)),
        _asio_thread([this] { _ios.run(); }), _bg([this] { _thread(); }), _cp(cp), _http_handler(_ios, http_batch_size),
        _batch_size(http_batch_size), _next_time_to_send(kspp::milliseconds_since_epoch() + 100), _next_time_to_poll(0),
        _http_timeout(http_timeout), _http_bytes("http_bytes", "bytes"), _http_requests("http_request", "msg"),
        _http_timeouts("http_timeout", "msg"), _http_error("http_error", "msg"), _http_ok("http_ok", "msg") {
    this->add_metric(&_lag);
    this->add_metric(&_http_bytes);
    this->add_metric(&_http_requests);
    this->add_metric(&_http_timeouts);
    this->add_metric(&_http_error);
    this->add_metric(&_http_ok);
    this->add_metrics_label(KSPP_PROCESSOR_TYPE_TAG, "prometheus_sink");

    curl_global_init(CURL_GLOBAL_NOTHING); /* minimal */
    _http_handler.set_user_agent(
        "Mozilla/5.0 (Windows NT 10.0; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/56.0.2924.87 Safari/537.36");

    _start_running = true;
  }

  prometheus_sink::~prometheus_sink() {
    if (!_closed)
      close();
    _exit = true;
    _http_handler.close();
    _work = nullptr;
    _asio_thread.join();
    _bg.join();
  }

  std::string prometheus_sink::log_name() const {
    return "prometheus_sink";
  }

  bool prometheus_sink::eof() const {
    return ((this->_queue.size() == 0) && (_pending_for_delete.size() == 0));
  }

  size_t prometheus_sink::process(int64_t tick) {
    size_t sz = 0;
    while (!_pending_for_delete.empty()) {
      ++sz;
      _pending_for_delete.pop_front();
    }
    return sz;
  }

  void prometheus_sink::close() {
    if (!_closed) {
      _closed = true;
    }
    //TODO??
  }

  void prometheus_sink::_thread() {

    while (!_exit) {
      if (_closed)
        break;

      // connected
      if (!_start_running) {
        std::this_thread::sleep_for(1s);
        continue;
      }

      if (this->_queue.empty()) {
        std::this_thread::sleep_for(100ms);
        continue;
      }

      size_t items_to_copy = std::min<size_t>(this->_queue.size(), _batch_size);

      std::string url;
      if (_cp.user.size()) {
        url = _cp.url + "?db=" + _cp.database_name + "&u=" + _cp.user + "&p=" + _cp.password;
      } else {
        url = _cp.url + "?db=" + _cp.database_name;
      }
      std::shared_ptr<kspp::http::request> request(
          new kspp::http::request(kspp::http::POST, url, {"Content-Encoding: snappy"}, _http_timeout));

      size_t msg_in_batch = 0;
      size_t bytes_in_batch = 0;
      event_queue<kspp::generic_avro, kspp::generic_avro> in_batch;

      prometheus::WriteRequest prequest;
      while (!this->_queue.empty() && msg_in_batch < _batch_size) {
        auto msg = this->_queue.pop_front_and_get(); // this will loose messages at retry... TODO
        //make sure no nulls gets to us


        // this is from influxdb go code
        // prometheusNameTag is the tag key that Prometheus uses for metric names
        //prometheusNameTag = "__name__"

        // measurementTagKey is the tag key that all measurement names use in the new storage processor
        //measurementTagKey = "_measurement"

        // so lets merge



        auto event_v = msg->record()->value();
        // protect agains null and +- inf
        if (event_v) {
          try {
            auto
                record = event_v->record();
            //LOG(INFO) << to_json(*event_v);

            double metrics_value = NAN;
            auto gd_value = record.get_generic_datum("value");
            if (gd_value.isUnion()) {
              switch (gd_value.type()) {
                case avro::AVRO_LONG:
                  metrics_value = gd_value.value<int64_t>();
                  break;
                case avro::AVRO_INT:
                  metrics_value = gd_value.value<int32_t>();
                  break;
                case avro::AVRO_DOUBLE:
                  metrics_value = gd_value.value<double>();
                  break;
              }
            }

            //auto metrics_value = record.get<double>("value");

            auto timestamp = record.get<int64_t>("timestamp");
            if (std::isfinite(metrics_value)) {
              auto ts = prequest.add_timeseries();
              auto l = ts->add_labels();
              l->set_name("__name__");
              auto ns = record.get<std::string>("ns");
              auto name = record.get<std::string>("name");
              if (ns.empty())
                l->set_value(name);
              else
                l->set_value(ns + "_" + name);

              std::vector<avro::GenericDatum> labels = record.get<avro::GenericArray>(
                  "labels").value(); // access later on crashed if we return a reference here -- seems there are som tmp objects
              for (auto &i: labels) {
                if (i.type() == avro::AVRO_RECORD) {
                  generic_avro::generic_record label(i.value<avro::GenericRecord>());
                  auto l = ts->add_labels();
                  l->set_name(label.get<std::string>("key"));
                  l->set_value(label.get<std::string>("value"));
                }
              }
              auto s = ts->add_samples();
              s->set_timestamp(timestamp);
              s->set_value(metrics_value);
            }
            ++msg_in_batch;
            in_batch.push_back(msg);
          }

          catch (std::exception &e) {
            LOG(INFO) << "exception: " << e.what();
          }
        }
      }
      std::string uncompressed;
      prequest.SerializeToString(&uncompressed);
      std::string compressed;
      snappy::Compress(uncompressed.data(), uncompressed.size(), &compressed);
      request->append(compressed);
      bytes_in_batch += request->tx_content_length();

      //DLOG(INFO) << request->uri();
      //DLOG(INFO) << request->tx_content();

      auto ts0 = kspp::milliseconds_since_epoch();
      // retry sent till we have to exit
      while (!_exit) {
        auto res = _http_handler.perform(request);

        // NOT OK?
        if (!res->ok()) {
          if (!res->transport_result()) {
            LOG(ERROR) << "HTTP timeout";
            std::this_thread::sleep_for(10s);
            continue;
          }
          LOG(ERROR) << "HTTP error (skipping) : " << res->rx_content();
          // we have a partial write that is evil - if we have a pare error int kafka the we will stoip forever here if we don't skip that.
          // for now skip the error and contine as if it worked
          //std::this_thread::sleep_for(10s);
          //continue;
        }

        // OK...
        //auto ts1 = kspp::milliseconds_since_epoch();
        //LOG(INFO) << "HTTP call nr of msg: " << msg_in_batch  << ", (" << ts1 - ts0 << ") ms";
        LOG_EVERY_N(INFO, 100) << "http post: " << request->uri() << " sent nr_of_metrics: " << in_batch.size()
                               << ", content lenght: " << request->tx_content_length() << " bytes, time="
                               << request->milliseconds() << " ms";

        while (!in_batch.empty()) {
          _pending_for_delete.push_back(in_batch.pop_front_and_get());
        }

        //_msg_cnt += msg_in_batch;
        _http_bytes += bytes_in_batch;
        break;
      }
    } // while (!exit)
    LOG(INFO) << "exiting thread";
  }

  void prometheus_sink::flush() {
    while (!eof()) {
      process(kspp::milliseconds_since_epoch());
      poll(0);
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    while (true) {
      int ec = 0; // TODO fixme
      //auto ec = _impl.flush(1000);
      if (ec == 0)
        break;
    }
  }
} // namespace

