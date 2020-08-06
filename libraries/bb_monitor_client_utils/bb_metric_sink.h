#include <grpcpp/grpcpp.h>
#include <bb_monitor_sink.grpc.pb.h>
#include <kspp/kspp.h>

#pragma once

namespace kspp {
  class bb_metric_sink : public topic_sink<void, bb_monitor::Metric> {
  public:
    bb_metric_sink(std::shared_ptr<cluster_config> config,
                   std::shared_ptr<grpc::Channel> channel,
                   std::string api_key,
                   std::string secret_access_key,
                   int64_t max_queue);

    ~bb_metric_sink() override;

    std::string log_name() const override;

    bool eof() const override;

    size_t process(int64_t tick) override;

    void close() override;

    void flush() override;

  private:
    void _thread();

    bool _exit = false;
    bool _start_running = false;
    bool _closed = false;
    size_t _batch_size = 5000;
    std::thread _bg; // performs the send loop
    event_queue<void, bb_monitor::Metric> _pending_for_delete;
    std::shared_ptr<grpc::Channel> _channel;
    const std::string _api_key;
    const std::string _secret_access_key;
    const int64_t _max_queue;
  };
}





