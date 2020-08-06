#include "bb_log_sink.h"
#include <kspp/internal/grpc/grpc_utils.h>
#include <bb_monitor_utils/grpc_utils.h>

using namespace std::chrono_literals;

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using bb_monitor::MonitorSink;
using bb_monitor::LogLine;
using bb_monitor::PutLogsResponse;


namespace kspp {
  bb_log_sink::bb_log_sink(std::shared_ptr<cluster_config> config,
                           std::shared_ptr<grpc::Channel> channel,
                           std::string api_key,
                           std::string secret_access_key)
      : kspp::topic_sink<void, bb_monitor::LogLine>(), _good(true), _closed(false), _start_running(false), _exit(false),
        _channel(channel), _bg([this] { _thread(); }), _api_key(api_key), _secret_access_key(secret_access_key) {
    this->add_metrics_label(KSPP_PROCESSOR_TYPE_TAG, "bb_log_sink2");
    _start_running = true;
  }

  bb_log_sink::~bb_log_sink() {
    if (!_closed)
      close();
    _exit = true;
    _bg.join();
  }

  std::string bb_log_sink::log_name() const {
    return "bb_log_sink";
  }

  bool bb_log_sink::eof() const {
    return ((this->_queue.size() == 0) && (_pending_for_delete.size() == 0));
  }

  size_t bb_log_sink::process(int64_t tick) {
    size_t sz = 0;
    while (!_pending_for_delete.empty()) {
      ++sz;
      _pending_for_delete.pop_front();
    }
    return sz;
  }

  void bb_log_sink::close() {
    if (!_closed) {
      _closed = true;
    }
    //TODO??
  }

  void bb_log_sink::_thread() {
    if (!_start_running && !_exit)
      std::this_thread::sleep_for(1s);

    event_queue<void, bb_monitor::LogLine> in_rpc;

    int64_t next_retry_at = 0;

    while (!_exit) {
      if (this->_queue.empty()) {
        std::this_thread::sleep_for(2000ms);
        continue;
      }

      if (next_retry_at > 0 && kspp::milliseconds_since_epoch() < next_retry_at) {
        continue;
      }

      next_retry_at = 0;

      bb_monitor::LogBundle bundle;
      while (!this->_queue.empty() && bundle.logline_size() < _batch_size) {
        auto msg = this->_queue.pop_front_and_get();
        in_rpc.push_back(msg);
        //make sure no nulls gets to us
        if (msg->record()->value())
          *bundle.add_logline() = *msg->record()->value();
      }

      if (bundle.logline_size() > 0) {
        auto stub = bb_monitor::MonitorSink::NewStub(_channel);
        PutLogsResponse response;
        grpc::ClientContext context;
        add_api_key_secret(context, _api_key, _secret_access_key);
        Status status = stub->PutLogs2(&context, bundle, &response);

        if (!status.ok()) {
          while (!in_rpc.empty())
            _queue.push_front(in_rpc.pop_front_and_get());
          LOG(ERROR) << "RPC msg_in_batch: " << bundle.logline_size() << ", failed, code: " << status.error_code()
                     << ": " << status.error_message() << " will retry in 60s";
          next_retry_at = kspp::milliseconds_since_epoch() + 60000;
        } else {
          LOG_EVERY_N(INFO, 100) << "RPC, msg_in_batch: " << bundle.logline_size() << " OK";
        }
        // push handled events forward
        while (!in_rpc.empty())
          _pending_for_delete.push_back(in_rpc.pop_front_and_get());
      }
    } // while !exit
    LOG(INFO) << "exiting thread";
  }

  void bb_log_sink::flush() {
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
