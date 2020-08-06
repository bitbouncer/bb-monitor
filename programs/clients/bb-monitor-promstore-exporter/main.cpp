#include <iostream>
#include <csignal>
#include <boost/program_options.hpp>
#include <kspp/utils/env.h>
#include <kspp/utils/kafka_utils.h>
#include <kspp/topology_builder.h>
#include <kspp/sources/mem_stream_source.h>
#include <bb_monitor_client_utils/grpc_db_streamer.h>
#include <bb_monitor_client_utils/prometheus_sink2.h>
#include <bb_monitor_utils/avro/bb_avro_metrics.h>
#include <kspp/utils/string_utils.h>

#define SERVICE_NAME              "bb_promstore_exporter"
#define DEFAULT_REMOTE_WRITE_URI  "http://127.0.0.1:8086/api/v1/prom/write?db=prometheus&u=monitor&p=monitor"
#define DEBUG_URI                 "localhost:50063"

/*
 * checkout irondb - compatible with prometheus - sligtly different format
https://www.irondb.io/docs/prometheus-ingestion-retrieval.html
 the uuid is a namespace that is required
 http://irondbnode:8112/module/prometheus/write/<accountid>/<uuid>

 postgres & timescaledb
 https://github.com/timescale/prometheus-postgresql-adapter
 http://<adapter-address>:9201/write
 http://<adapter-address>:9201/read

*/

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
      ("app_realm", boost::program_options::value<std::string>()->default_value(get_env_and_log("APP_REALM", "DEV")), "app_realm")
      ("kafka_proxy_uri", boost::program_options::value<std::string>()->default_value(get_env_and_log("KAFKKA_PROXY_URI", "lb.bitbouncer.com:30112")),"kafka_proxy_uri")
      ("monitor_api_key", boost::program_options::value<std::string>()->default_value(get_env_and_log("MONITOR_API_KEY", "")), "monitor_api_key")
      ("monitor_secret_access_key", boost::program_options::value<std::string>()->default_value(get_env_and_log_hidden("MONITOR_SECRET_ACCESS_KEY", "")), "monitor_secret_access_key")
      ("offset_storage", boost::program_options::value<std::string>()->default_value(get_env_and_log("OFFSET_STORAGE", "")), "offset_storage")
      ("start_offset", boost::program_options::value<std::string>()->default_value(get_env_and_log("START_OFFSET", "OFFSET_END")), "start_offset")
      ("remote_write_uri", boost::program_options::value<std::string>()->default_value(get_env_and_log("REMOTE_WRITE_URI", DEFAULT_REMOTE_WRITE_URI)), "remote_write_uri")
      ("http_batch_size", boost::program_options::value<int32_t>()->default_value(10000), "http_batch_size")
      ("http_timeout_ms", boost::program_options::value<int32_t>()->default_value(10000), "http_timeout_ms")
      ("max_exported_age", boost::program_options::value<std::string>()->default_value(get_env_and_log("MAX_EXPORTED_AGE", "0")), "max_exported_age")
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

  std::string app_realm;
  if (vm.count("app_realm")) {
    app_realm = vm["app_realm"].as<std::string>();
  }

  std::string kafka_proxy_uri;
  if (vm.count("kafka_proxy_uri")) {
    kafka_proxy_uri = vm["kafka_proxy_uri"].as<std::string>();
  } else {
    std::cerr << "--kafka_proxy_uri must specified" << std::endl;
    return -1;
  }

  std::string monitor_api_key;
  if (vm.count("monitor_api_key")) {
    monitor_api_key = vm["monitor_api_key"].as<std::string>();
  }

  if (monitor_api_key.size()==0) {
    std::cerr << "--monitor_api_key must specified" << std::endl;
    return -1;
  }

  std::string monitor_secret_access_key;
  if (vm.count("monitor_secret_access_key")) {
    monitor_secret_access_key = vm["monitor_secret_access_key"].as<std::string>();
  }

  std::string offset_storage;
  if (vm.count("offset_storage"))
    offset_storage = vm["offset_storage"].as<std::string>();

  if (offset_storage.empty())
    offset_storage = config->get_storage_root() + "/" + SERVICE_NAME + "-import-metrics.offset";

  kspp::start_offset_t start_offset=kspp::OFFSET_BEGINNING;
  try {
    if (vm.count("start_offset"))
      start_offset = kspp::to_offset(vm["start_offset"].as<std::string>());
  }
  catch(std::exception& e) {
    std::cerr << "start_offset must be one of OFFSET_BEGINNING / OFFSET_END / OFFSET_STORED";
    return -1;
  }

  std::string remote_write_uri;
  if (vm.count("remote_write_uri")) {
    remote_write_uri = vm["remote_write_uri"].as<std::string>();
  }

  int32_t http_batch_size;
  if (vm.count("http_batch_size")) {
    http_batch_size = vm["http_batch_size"].as<int32_t>();
  }

  std::chrono::milliseconds http_timeout;
  if (vm.count("http_timeout_ms")) {
    http_timeout = std::chrono::milliseconds(vm["http_timeout_ms"].as<int32_t>());
  }

  std::chrono::seconds max_exported_age = 0s;
  if (vm.count("max_exported_age")) {
    auto s = vm["max_exported_age"].as<std::string>();
    int v = atoi(s.c_str());
    max_exported_age = std::chrono::seconds(v);
  }

  bool oneshot=false;
  if (vm.count("oneshot"))
    oneshot=true;


  LOG(INFO) << "kafka_proxy_uri          : " << kafka_proxy_uri;
  LOG(INFO) << "monitor_api_key          : " << monitor_api_key;
  if (monitor_secret_access_key.size()>0)
    LOG(INFO) << "monitor_secret_access_key: " << "[hidden]";
  LOG(INFO) << "offset_storage   : " << offset_storage;
  LOG(INFO) << "start_offset     : " << kspp::to_string(start_offset);
  LOG(INFO) << "remote_write_uri : " << remote_write_uri;
  LOG(INFO) << "http_batch_size  : " << http_batch_size;
  LOG(INFO) << "http_timeout_ms  : " << http_timeout.count();
  if (max_exported_age.count()>0)
    LOG(INFO) << "max_exported_age : " << max_exported_age.count() << "(s)";
  else
    LOG(INFO) << "max_exported_age : UNLIMITED";

  LOG(INFO) << "discovering facts...";
  if (oneshot)
    LOG(INFO) << "oneshot          : TRUE";

  std::shared_ptr<grpc::Channel> channel;
  grpc::ChannelArguments channelArgs;
  // special for easier debugging
  if (kafka_proxy_uri == DEBUG_URI) {
    channel = grpc::CreateCustomChannel(kafka_proxy_uri, grpc::InsecureChannelCredentials(), channelArgs);
  } else {
    auto channel_creds = grpc::SslCredentials(grpc::SslCredentialsOptions());
    channel = grpc::CreateCustomChannel(kafka_proxy_uri, channel_creds, channelArgs);
  }

  kspp::topology_builder generic_builder(config);

  auto offset_provider = get_offset_provider(offset_storage);
  auto t = generic_builder.create_topology();

  auto source = t->create_processor<kspp::grpc_avro_source<std::string, bb_avro_metric_t>>(0, "metrics", offset_provider, channel, monitor_api_key, monitor_secret_access_key);
  auto sink = t->create_sink<prometheus_avro_metrics_sink>(source, remote_write_uri, http_batch_size, http_timeout, max_exported_age);
  t->start(start_offset);

  int64_t next_log = kspp::milliseconds_since_epoch() + 10000;
  int64_t next_commit = kspp::milliseconds_since_epoch() + 60000;

  while (run) {
    if (next_log < kspp::milliseconds_since_epoch()){
      next_log = kspp::milliseconds_since_epoch() + 10000;
    }

    int64_t sz0 = 0;
    auto sz1 = t->process(kspp::milliseconds_since_epoch());
    if (sz1 == 0 ) {
      std::this_thread::sleep_for(15ms);
    }

    if (next_commit < kspp::milliseconds_since_epoch()){
      next_commit = kspp::milliseconds_since_epoch() + 60000;
      t->commit(false);
    }
  }
  t->commit(true);
  LOG(INFO) << "exiting";
  return 0;
}