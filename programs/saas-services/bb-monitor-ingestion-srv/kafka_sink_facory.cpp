#include "kafka_sink_factory.h"
#include <kspp/kspp.h>
#include <kspp/topology_builder.h>
#include <kspp/sources/kafka_source.h>
#include <kspp/processors/visitor.h>
#include <kspp/utils/kafka_utils.h>

#define AUTH_TOPIC "BB_MONITOR_AUTH_VIEW"

kafka_sink_factory::kafka_sink_factory(std::shared_ptr<kspp::cluster_config> config, std::string auth_topic, std::map<std::string, std::string> labels)
    : _config(config)
    , _labels(labels)
    , _authorizer(config, auth_topic) {
}

std::shared_ptr<bb::bb_kafka_sink<std::string, bb_avro_metric_t>> kafka_sink_factory::get_metrics_sink(std::string api_key, std::string secret_api_key){
  // we could check if we recogize the apinat all and
  // return Status(grpc::StatusCode::UNAUTHENTICATED, "authentication failed")

  kspp::spinlock::scoped_lock xxx(_lock);
  {
    //std::string topic_and_api_key = "metrics#" + api_key;

    auto auth_res = _authorizer.authorize_write(api_key, secret_api_key, "metrics");

    if (auth_res.second==0){
      LOG(INFO) << "auth failed for apikey: " << api_key;
      return nullptr;
    }

    auto item = _topic2producer_cache.find(auth_res.first);

    if (item != _topic2producer_cache.end()) {
      //LOG(INFO) << "reusing sink from cache: " << auth_res.first;
      return item->second;
    }

    std::map<std::string, std::string> labels =
        {
            { "client_id", std::to_string(auth_res.second) },
            { "logical_topic",  "metrics"},
            { "physical_topic",  auth_res.first},
            { "processor", "sink" }
        };
    labels.insert(_labels.begin(), _labels.end());

    auto p = std::make_shared<bb::bb_kafka_sink<std::string, bb_avro_metric_t>>(_config, auth_res.first, auth_res.second, labels);
    _topic2producer_cache[auth_res.first] = p;
    return p;
  }
}

std::shared_ptr<bb::bb_kafka_sink<std::string, bb_avro_logline>> kafka_sink_factory::get_log_sink(std::string api_key, std::string secret_api_key){
  // we could check if we recogize the apinat all and
  // return Status(grpc::StatusCode::UNAUTHENTICATED, "authentication failed")

  kspp::spinlock::scoped_lock xxx(_lock);
  {
    //std::string topic_and_api_key = "logs#" + api_key;

    auto auth_res  = _authorizer.authorize_write(api_key, secret_api_key, "logs");

    if (auth_res.second==0){
      LOG(INFO) << "auth failed for apikey: " << api_key;
      return nullptr;
    }

    auto item = _topic2logproducer_cache.find(auth_res.first);

    if (item != _topic2logproducer_cache.end()) {
      //LOG(INFO) << "reusing sink from cache: " << auth_res.first;
      return item->second;
    }

    // add instance from somewhere
    std::map<std::string, std::string> labels =
        {
            { "client_id", std::to_string(auth_res.second) },
            { "logical_topic",  "logs"},
            { "physical_topic",  auth_res.first},
            { "usage", "sink" }
        };

    auto p = std::make_shared<bb::bb_kafka_sink<std::string, bb_avro_logline>>(_config, auth_res.first, auth_res.second, labels);
    _topic2logproducer_cache[auth_res.first] = p;
    return p;
  }
}

std::shared_ptr<bb::bb_kafka_sink<std::string, dd_avro_intake_t>> kafka_sink_factory::get_intake_sink(std::string api_key, std::string secret_api_key) {
  kspp::spinlock::scoped_lock xxx(_lock);
  {
    //std::string topic_and_api_key = "intake#" + api_key;

    auto auth_res = _authorizer.authorize_write(api_key, secret_api_key, "intake");

    if (auth_res.second == 0) {
      LOG(INFO) << "auth failed for apikey: " << api_key;
      return nullptr;
    }

    auto item = _topic2intakeproducer_cache.find(auth_res.first);

    if (item != _topic2intakeproducer_cache.end()) {
      //DLOG(INFO) << "reusing sink from cache: " << auth_res.first;
      return item->second;
    }

    // add instance from somewhere
    std::map<std::string, std::string> labels =
        {
            {"logical_topic",  "intake"},
            {"physical_topic", auth_res.first},
            {"usage",          "sink"}
        };
    auto p = std::make_shared<bb::bb_kafka_sink<std::string, dd_avro_intake_t>>(_config, auth_res.first, auth_res.second, labels);
    _topic2intakeproducer_cache[auth_res.first] = p;
    return p;
  }
}

/*std::shared_ptr<bb::bb_kafka_sink<std::string, nagios_check_control_t>> kafka_sink_factory::get_nagios_control_sink(std::string api_key, std::string secret_api_key){
  kspp::spinlock::scoped_lock xxx(_lock);
  {
    //std::string topic_and_api_key = "intake#" + api_key;

    auto auth_res = _authorizer.authorize_write(api_key, secret_api_key, "nagios_control");

    if (auth_res.second == 0) {
      LOG(INFO) << "auth failed for apikey: " << api_key;
      return nullptr;
    }

    auto item = _topic2nagioscontrol_cache.find(auth_res.first);

    if (item != _topic2nagioscontrol_cache.end()) {
      //DLOG(INFO) << "reusing sink from cache: " << auth_res.first;
      return item->second;
    }

    // add instance from somewhere
    std::map<std::string, std::string> labels =
        {
            {"logical_topic",  "nagios_control"},
            {"physical_topic", auth_res.first},
            {"usage",          "sink"}
        };

    auto p = std::make_shared<bb::bb_kafka_sink<std::string, nagios_check_control_t>>(_config, auth_res.first, auth_res.second, labels);
    _topic2nagioscontrol_cache[auth_res.first] = p;
    return p;
  }
}
 */

