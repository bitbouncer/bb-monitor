#include <chrono>
#include <kspp/kspp.h>
#include <kspp/utils/async.h>
#include <kspp/utils/http_client.h>
#include <kspp/connect/connection_params.h>
#include <bb_monitor_utils/avro/bb_avro_metrics.h>

#pragma once

namespace kspp {
  class prometheus_avro_metrics_sink :
      public kspp::topic_sink<std::string, bb_avro_metric_t> {
  public:
    prometheus_avro_metrics_sink(std::shared_ptr<cluster_config> config,
                                 std::string remote_write_uri,
                                 int32_t http_batch_size,
                                 std::chrono::milliseconds http_timeout,
                                 std::chrono::seconds max_exported_age
    );

    ~prometheus_avro_metrics_sink() override;

    std::string log_name() const override;

    bool eof() const override;

    size_t process(int64_t tick) override;

    void close() override;

    void flush() override;

    size_t outbound_queue_len() const override {
      return this->_queue.size();
    }

  private:
    void _thread();

    bool exit_ = false;
    bool start_running_ = false;
    //bool good_ = true;
    bool closed_ = false;
    int64_t max_exported_age_;

    boost::asio::io_service ios_;
    std::unique_ptr<boost::asio::io_service::work> work_;
    std::thread asio_thread_; // internal to http client

    std::thread bg_; // performs the send loop

    event_queue<std::string, bb_avro_metric_t> pending_for_delete_;

    const std::string remote_write_uri_;
    kspp::http::client http_handler_;
    size_t batch_size_;
    std::chrono::milliseconds http_timeout_;
//    kspp::metric_streaming_lag lag_;
    kspp::metric_counter http_requests_;
    kspp::metric_counter http_timeouts_;
    kspp::metric_counter http_error_;
    kspp::metric_counter http_ok_;
    kspp::metric_counter http_bytes_;
  };
} // namespace
