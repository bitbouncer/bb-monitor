#include <grpcpp/grpcpp.h>
#include <bb_monitor_sink.grpc.pb.h>
#include <bb_monitor_srv_utils/grpc_apikey_auth.h>
#include <kspp/kspp.h>

#pragma once

namespace kspp {
  class bb_log_sink : public topic_sink<void, bb_monitor::LogLine> {
  public:
    bb_log_sink(std::shared_ptr<cluster_config> config,
                std::shared_ptr<grpc::Channel> channel,
                std::string api_key,
                std::string secret_access_key);

    ~bb_log_sink() override;

    std::string log_name() const override;

    bool eof() const override;

    size_t process(int64_t tick) override;

    void close() override;

    void flush() override;

  private:
    void _thread();

    bool _exit;
    bool _start_running;
    bool _good;
    bool _closed;
    size_t _batch_size = 1000;
    std::thread _bg; // performs the send loop
    event_queue<void, bb_monitor::LogLine> _pending_for_delete;
    std::shared_ptr<grpc::Channel> _channel;
    std::string _api_key;
    std::string _secret_access_key;
  };
}





