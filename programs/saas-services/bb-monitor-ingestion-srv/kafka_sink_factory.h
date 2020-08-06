#include <kspp/cluster_config.h>
#include <memory>
#include "bb_kafka_sink.h"
#include <bb_monitor_utils/avro/bb_avro_metrics.h>
#include <bb_monitor_utils/avro/bb_avro_logline.h>
#include <bb_monitor_utils/avro/dd_avro_intake.h>
#include <bb_monitor_srv_utils/grpc_topic_authorizer.h>
#pragma once

class kafka_sink_factory {
public:
  kafka_sink_factory(std::shared_ptr<kspp::cluster_config> config, std::string auth_topic, std::map<std::string, std::string> labels);

  std::shared_ptr<bb::bb_kafka_sink<std::string, bb_avro_metric_t>> get_metrics_sink(std::string api_key, std::string secret_api_key);
  std::shared_ptr<bb::bb_kafka_sink<std::string, bb_avro_logline>> get_log_sink(std::string api_key, std::string secret_api_key);
  std::shared_ptr<bb::bb_kafka_sink<std::string, dd_avro_intake_t>> get_intake_sink(std::string api_key, std::string secret_api_key);

  //non standard usage - ie no auth server to server
  //std::shared_ptr<bb::bb_kafka_sink<std::string, nagios_check_control_t>> get_nagios_control_sink(std::string api_key, std::string secret_api_key);
  std::map<std::string, std::string> _labels;
private:
  mutable kspp::spinlock _lock;
  std::shared_ptr<kspp::cluster_config> _config;
  grpc_topic_authorizer _authorizer;

  std::map<std::string, std::shared_ptr<bb::bb_kafka_sink<std::string, bb_avro_metric_t>>> _topic2producer_cache;
  std::map<std::string, std::shared_ptr<bb::bb_kafka_sink<std::string, bb_avro_logline>>> _topic2logproducer_cache;
  std::map<std::string, std::shared_ptr<bb::bb_kafka_sink<std::string, dd_avro_intake_t>>> _topic2intakeproducer_cache;
};
