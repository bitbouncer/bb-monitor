#include <iostream>
#include <csignal>
#include <boost/program_options.hpp>
#include <kspp/utils/env.h>
#include <kspp/utils/kafka_utils.h>
#include <kspp/processors/visitor.h>
#include <kspp/topology_builder.h>
#include <bb_monitor_utils/avro/bb_avro_metrics.h>
#include <kspp/processors/visitor.h>
#include <kspp/connect/bitbouncer/grpc_avro_source.h>

#define SERVICE_NAME     "bb_kafka_exporter"
#define DEFAULT_SRC_URI  "lb.bitbouncer.com:30112"
#define DEBUG_URI        "localhost:50063"

using namespace std::chrono_literals;
using namespace kspp;

static bool run = true;
static void sigterm(int sig) {
  run = false;
}

int main(int argc, char** argv) {
  FLAGS_logtostderr = 1;
  google::InitGoogleLogging(argv[0]);

  boost::program_options::options_description desc("options");
  desc.add_options()
      ("help", "produce help message")
      ("src_uri", boost::program_options::value<std::string>()->default_value(get_env_and_log("SRC_URI", DEFAULT_SRC_URI)), "src_uri")
      ("api_key", boost::program_options::value<std::string>()->default_value(get_env_and_log_hidden("API_KEY", "")), "api_key")
      ("secret_access_key", boost::program_options::value<std::string>()->default_value(get_env_and_log_hidden("SECRET_ACCESS_KEY", "")), "secret_access_key")
      ("topic", boost::program_options::value<std::string>()->default_value("metrics"), "topic")
      ("offset_storage", boost::program_options::value<std::string>()->default_value(get_env_and_log("OFFSET_STORAGE", "")), "offset_storage")
      ("oneshot", "run to eof and exit")
      ;

  boost::program_options::variables_map vm;
  boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc), vm);
  boost::program_options::notify(vm);

  if (vm.count("help")) {
    std::cout << desc << std::endl;
    return 0;
  }

  std::string consumer_group(SERVICE_NAME);
  auto config = std::make_shared<kspp::cluster_config>(consumer_group, kspp::cluster_config::NONE);
  config->load_config_from_env();

  std::string src_uri;
  if (vm.count("src_uri")) {
    src_uri = vm["src_uri"].as<std::string>();
  } else {
    std::cerr << "--src_uri must specified" << std::endl;
    return -1;
  }

  std::string api_key;
  if (vm.count("api_key")) {
    api_key = vm["api_key"].as<std::string>();
  }

  if (api_key.size()==0) {
    std::cerr << "--api_key must specified" << std::endl;
    return -1;
  }

  std::string secret_access_key;
  if (vm.count("secret_access_key")) {
    secret_access_key = vm["secret_access_key"].as<std::string>();
  }

  std::string offset_storage;
  if (vm.count("offset_storage"))
    offset_storage = vm["offset_storage"].as<std::string>();

  if (offset_storage.empty())
    offset_storage = config->get_storage_root() + "/" + SERVICE_NAME + "-import-metrics.offset";

  std::string topic;
  if (vm.count("topic")) {
    topic = vm["topic"].as<std::string>();
  }

  bool oneshot=false;
  if (vm.count("oneshot"))
    oneshot=true;

  LOG(INFO) << "src_uri          : " << src_uri;
  LOG(INFO) << "api_key          : " << api_key;
  if (secret_access_key.size()>0)
    LOG(INFO) << "secret_access_key: " << "[hidden]";
  LOG(INFO) << "offset_storage   : " << offset_storage;
  LOG(INFO) << "topic            : " << topic;
  LOG(INFO) << "discovering facts...";
  if (oneshot)
    LOG(INFO) << "oneshot          : TRUE";


  std::shared_ptr<grpc::Channel> channel;
  grpc::ChannelArguments channelArgs;
  // special for easier debugging
  if (src_uri == DEBUG_URI) {
    channel = grpc::CreateCustomChannel(src_uri, grpc::InsecureChannelCredentials(), channelArgs);
  } else {
    auto channel_creds = grpc::SslCredentials(grpc::SslCredentialsOptions());
    channel = grpc::CreateCustomChannel(src_uri, channel_creds, channelArgs);
  }


  kspp::topology_builder generic_builder(config);
  auto live = generic_builder.create_topology();
  auto offset_provider = get_offset_provider(offset_storage);
  auto source = live->create_processor<kspp::grpc_avro_source<std::string, bb_avro_metric_t>>(0, topic, offset_provider, channel, api_key, secret_access_key);
  live->create_processor<kspp::visitor<std::string, bb_avro_metric_t>>(source, [](auto ev){
    if (ev.value()){
      std::cout << ev.value()->name << std::endl;
    }
    //std::cout << to_json(*ev.value()) << std::endl;
  });

  live->start(kspp::OFFSET_END);

  std::signal(SIGINT, sigterm);
  std::signal(SIGTERM, sigterm);
  std::signal(SIGPIPE, SIG_IGN);

  while (run && source->good()) {
    auto sz = live->process(kspp::milliseconds_since_epoch());

    if (sz == 0) {
      std::this_thread::sleep_for(100ms);
      continue;
    }

  }

  LOG(INFO) << "exiting";
  return 0;
}