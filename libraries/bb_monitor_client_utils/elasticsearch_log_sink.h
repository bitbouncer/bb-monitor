#include <kspp/connect/elasticsearch/elasticsearch_producer.h>
#include <kspp/connect/elasticsearch/elasticsearch_utils.h>
#include <bb_monitor_utils/avro/bb_avro_logline.h>
#include <kspp/utils/string_utils.h>
#pragma once

int split_splunk_tag(std::string s, std::string& container_ns, std::string& container_name);


namespace kspp {
  class elasticsearch_log_sink : public topic_sink<std::string, bb_avro_logline>  {
    static constexpr const char *PROCESSOR_NAME = "elasticsearch_log_sink";
  public:
    elasticsearch_log_sink(std::shared_ptr<cluster_config> config, std::string remote_write_url, std::string username, std::string password, size_t max_http_connection=20);

    ~elasticsearch_log_sink() override;

    std::string log_name() const override;

    bool good() const;

    void register_metrics(kspp::processor* parent);

    void close() override;

    bool eof() const override;

    size_t queue_size() const override;

    size_t outbound_queue_len() const override;

    int64_t next_event_time() const override;

    size_t process(int64_t tick) override;

    std::string topic() const override;

    void poll(int timeout) override;

    void flush() override;

  protected:
    bool _exit = false;
    std::shared_ptr<kspp::elasticsearch_producer<std::string, bb_avro_logline>> _impl;
  };
}
