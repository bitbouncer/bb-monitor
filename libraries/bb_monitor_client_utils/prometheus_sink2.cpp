#include "prometheus_sink2.h"
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
  prometheus_avro_metrics_sink::prometheus_avro_metrics_sink(std::shared_ptr<cluster_config> config,
                                                             std::string remote_write_uri,
                                                             int32_t http_batch_size,
                                                             std::chrono::milliseconds http_timeout,
                                                             std::chrono::seconds max_exported_age)
      : kspp::topic_sink<std::string, bb_avro_metric_t>(), work_(std::make_unique<boost::asio::io_service::work>(ios_)),
        asio_thread_([this] { ios_.run(); }), bg_([this] { _thread(); }), remote_write_uri_(remote_write_uri),
        http_handler_(ios_, http_batch_size), batch_size_(http_batch_size), http_timeout_(http_timeout),
        max_exported_age_(max_exported_age.count() * 1000), http_bytes_("http_bytes", "bytes"),
        http_requests_("http_request", "msg"), http_timeouts_("http_timeout", "msg"), http_error_("http_error", "msg"),
        http_ok_("http_ok", "msg") {
//    this->add_metric(&lag_);
    this->add_metric(&http_bytes_);
    this->add_metric(&http_requests_);
    this->add_metric(&http_timeouts_);
    this->add_metric(&http_error_);
    this->add_metric(&http_ok_);
    this->add_metrics_label(KSPP_PROCESSOR_TYPE_TAG, "prometheus_sink");

    curl_global_init(CURL_GLOBAL_NOTHING); /* minimal */
    http_handler_.set_user_agent(
        "Mozilla/5.0 (Windows NT 10.0; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/56.0.2924.87 Safari/537.36");

    start_running_ = true;
  }

  prometheus_avro_metrics_sink::~prometheus_avro_metrics_sink() {
    if (!closed_)
      close();
    exit_ = true;
    http_handler_.close();
    work_ = nullptr;
    asio_thread_.join();
    bg_.join();
  }

  std::string prometheus_avro_metrics_sink::log_name() const {
    return "prometheus_avro_metrics_sink";
  }

  bool prometheus_avro_metrics_sink::eof() const {
    return ((this->_queue.size() == 0) && (pending_for_delete_.size() == 0));
  }

  size_t prometheus_avro_metrics_sink::process(int64_t tick) {
    size_t sz = 0;
    while (!pending_for_delete_.empty()) {
      ++sz;
      pending_for_delete_.pop_front();
    }
    return sz;
  }

  void prometheus_avro_metrics_sink::close() {
    if (!closed_) {
      closed_ = true;
    }
    //TODO??
  }

  void prometheus_avro_metrics_sink::_thread() {
    event_queue<std::string, bb_avro_metric_t> in_batch;

    while (!exit_) {
      if (closed_)
        break;

      // connected
      if (!start_running_) {
        std::this_thread::sleep_for(1s);
        continue;
      }

      if (this->_queue.empty()) {
        std::this_thread::sleep_for(10ms);
        continue;
      }

      auto ts0 = kspp::milliseconds_since_epoch();

      std::shared_ptr<kspp::http::request> request(
          new kspp::http::request(kspp::http::POST, remote_write_uri_, {"Content-Encoding: snappy"}, http_timeout_));

      //size_t msg_in_batch = 0;
      size_t bytes_in_batch = 0;

      prometheus::WriteRequest prequest;

      int64_t min_ts = 0;
      if (max_exported_age_ > 0) {
        min_ts = kspp::milliseconds_since_epoch() - max_exported_age_;
      }

      while (!this->_queue.empty() && in_batch.size() < batch_size_) {
        auto msg = this->_queue.pop_front_and_get(); // this will loose messages at retry... TODO
        in_batch.push_back(msg);

        const bb_avro_metric_t *metrics = msg->record()->value();

        // protect agains null and +- inf
        if (metrics) {
          auto timestamp = metrics->timestamp;
          // skip this measurement
          if (timestamp <= min_ts) {
            continue;
          }

          double metrics_value = NAN;
          switch (metrics->value.idx()) {
            case 0: // NULL
              metrics_value = NAN;
              break;
            case 1:   // LONG
              metrics_value = metrics->value.get_long();
              break;
            case 2:
              metrics_value = metrics->value.get_double();
              break;
          }


          if (std::isfinite(metrics_value)) {
            auto ts = prequest.add_timeseries();
            auto l = ts->add_labels();
            l->set_name("__name__");
            if (metrics->ns.empty())
              l->set_value(metrics->name);
            else
              l->set_value(metrics->ns + "_" + metrics->name);

            for (auto &i : metrics->labels) {
              auto l = ts->add_labels();
              l->set_name(i.key);
              l->set_value(i.value);
            }

            auto s = ts->add_samples();
            s->set_timestamp(timestamp);
            s->set_value(metrics_value);
          }
        }
      }

      // something to send??
      if (prequest.timeseries_size()) {
        auto ts1 = kspp::milliseconds_since_epoch();
        LOG_EVERY_N(INFO, 100) << "prepare1, nr_of_metrics: " << in_batch.size() << ", took ; (" << ts1 - ts0 << ") ms";

        std::string uncompressed;
        prequest.SerializeToString(&uncompressed);
        std::string compressed;
        snappy::Compress(uncompressed.data(), uncompressed.size(), &compressed);
        request->append(compressed);
        bytes_in_batch += request->tx_content_length();

        auto ts2 = kspp::milliseconds_since_epoch();
        LOG_EVERY_N(INFO, 100) << "prepare2, nr_of_metrics: " << in_batch.size() << ", took ; (" << ts2 - ts1 << ") ms";

        // retry sent till we have to exit
        while (!exit_) {
          auto res = http_handler_.perform(request);

          // NOT OK?
          if (!res->ok()) {
            if (!res->transport_result()) {
              LOG(ERROR) << "HTTP timeout";
              ++http_timeouts_;
              std::this_thread::sleep_for(10s);
              continue;
            }
            LOG(ERROR) << "HTTP error (skipping) : THIS SHOULD NEVER HAPPEN" << res->rx_content();
            ++http_error_;
            // we have a partial write that is evil - if we have a pare error int kafka the we will stoip forever here if we don't skip that.
            // for now skip the error and contine as if it worked
            //std::this_thread::sleep_for(10s);
            break;
          }
          ++http_ok_;
          LOG_EVERY_N(INFO, 100) << "http post: " << request->uri() << " sent nr_of_metrics: " << in_batch.size()
                                 << ", content lenght: " << request->tx_content_length() << " bytes, time="
                                 << request->milliseconds() << " ms";
          http_bytes_ += bytes_in_batch;
          break;
        }
      }

      // messages to delete??
      while (!in_batch.empty())
        pending_for_delete_.push_back(in_batch.pop_front_and_get());


    } // while (!exit)

    LOG(INFO) << "exiting thread";
  }

  void prometheus_avro_metrics_sink::flush() {
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

