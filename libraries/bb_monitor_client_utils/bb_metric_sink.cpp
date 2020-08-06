#include "bb_metric_sink.h"
#include <kspp/internal/grpc/grpc_utils.h>
#include <bb_monitor_utils/grpc_utils.h>

using namespace std::chrono_literals;

//using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using bb_monitor::MonitorSink;
using bb_monitor::Metric;
using bb_monitor::PutMetricsResponse;

namespace kspp {
  bb_metric_sink::bb_metric_sink(std::shared_ptr<cluster_config> config,
                                 std::shared_ptr<grpc::Channel> channel,
                                 std::string api_key,
                                 std::string secret_access_key,
                                 int64_t max_queue)
      : kspp::topic_sink<void, bb_monitor::Metric>(), _channel(channel), _bg([this] { _thread(); }), _api_key(api_key),
        _secret_access_key(secret_access_key), _max_queue(max_queue) {
    this->add_metrics_label(KSPP_PROCESSOR_TYPE_TAG, "bb_metric_sink");
    _start_running = true;
  }

  bb_metric_sink::~bb_metric_sink() {
    if (!_closed)
      close();
    _exit = true;
    _bg.join();
  }

  std::string bb_metric_sink::log_name() const {
    return "bb_metric_sink";
  }

  bool bb_metric_sink::eof() const {
    return ((this->_queue.size() == 0) && (_pending_for_delete.size() == 0));
  }

  size_t bb_metric_sink::process(int64_t tick) {
    size_t sz = 0;
    // mutex
    while (!_pending_for_delete.empty()) {
      ++sz;
      _pending_for_delete.pop_front();
    }
    return sz;
  }

  void bb_metric_sink::close() {
    if (!_closed) {
      _closed = true;
    }
    //TODO??
  }

  void bb_metric_sink::_thread() {
    if (!_start_running && !_exit)
      std::this_thread::sleep_for(1s);

    event_queue<void, bb_monitor::Metric> in_rpc;

    int64_t next_retry_at = 0;
    int64_t next_purge_at = 0;

    while (!_exit) {
      if (this->_queue.empty()) {
        std::this_thread::sleep_for(2000ms);
        continue;
      }

      if (next_retry_at > 0 && kspp::milliseconds_since_epoch() < next_retry_at) {

        // we need to purge data if we're failing to connect to server
        if (kspp::milliseconds_since_epoch() > next_purge_at) {
          next_purge_at = kspp::milliseconds_since_epoch() + 10000;
          if (this->_queue.size() > _max_queue) {
            size_t items_to_purge = this->_queue.size() - _max_queue;
            LOG(WARNING) << "purging metrics in memory - loosing data, queue.size: " << this->_queue.size()
                         << ", purging: " << items_to_purge;
            for (size_t i = 0; i != items_to_purge; ++i)
              this->_queue.pop_front();
          }
        }
        continue;
      }
      next_retry_at = 0;

      bb_monitor::MetricsBundle bundle;
      bundle.clear_metrics();
      while (!this->_queue.empty() && bundle.metrics_size() < _batch_size) {
        auto msg = this->_queue.pop_front_and_get();
        in_rpc.push_back(msg);
        //make sure no nulls gets to us
        if (msg->record()->value()) {
          *bundle.add_metrics() = *msg->record()->value();
        }
      }

      if (bundle.metrics_size() > 0) {
        PutMetricsResponse response;
        grpc::ClientContext context;
        kspp::add_api_key_secret(context, _api_key, _secret_access_key);
        auto stub = bb_monitor::MonitorSink::NewStub(_channel);
        int64_t t0 = kspp::milliseconds_since_epoch();
        Status status = stub->PutMetrics2(&context, bundle, &response);
        if (!status.ok()) {
          while (!in_rpc.empty())
            _queue.push_front(in_rpc.pop_front_and_get());
          LOG(ERROR) << "RPC msg_in_batch: " << bundle.metrics_size() << ", failed, code: " << status.error_code()
                     << ": " << status.error_message() << " will retry in 60s";
          next_retry_at = kspp::milliseconds_since_epoch() + 60000;
        } else {
          int64_t t1 = kspp::milliseconds_since_epoch();
          LOG_EVERY_N(INFO, 100) << "RPC, msg_in_batch: " << bundle.metrics_size() << " OK, took " << t1 - t0
                                 << " ms, #" << google::COUNTER;
        }
        // push handled events forward
        while (!in_rpc.empty())
          _pending_for_delete.push_back(in_rpc.pop_front_and_get());
      }
    }
    LOG(INFO) << "exiting thread";
  }

  void bb_metric_sink::flush() {
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
