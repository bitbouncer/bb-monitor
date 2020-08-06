#include <cstring>
#include <glog/logging.h>
#include <boost/program_options.hpp>
#include <kspp/topology_builder.h>
#include <kspp/sources/mem_stream_source.h>
#include <kspp/processors/flat_map.h>
#include <kspp/kspp.h>
#include <kspp/utils/env.h>
#include <bb_monitor_utils/compression.h>
#include <bb_monitor_client_utils/prometheus_utils.h>
#include <bb_monitor_client_utils/bb_metric_sink.h>
#include <remote.pb.h>
#include <restinio/all.hpp>

#define SERVICE_NAME   "bb_promstore_write_proxy"
#define DEFAULT_PORT   "8086"
#define INTERNAL_K8S_URI "monitor-sink-srv:50071"

/* Exit flag for main loop */
static bool run = true;

static void sigterm(int sig) {
  run = false;
}

// Create request handler.
restinio::request_handling_status_t request_handler(kspp::mem_stream_source<void, bb_monitor::Metric> &stream, restinio::request_handle_t req) {
  if (req->header().path() == "/api/v1/prom/write") {
    if (restinio::http_method_post() == req->header().method()) {
      //auto body = req->body();
      auto encodning = req->header().get_field(restinio::http_field::content_encoding);
      if (encodning == "snappy") {
        auto body = decompress_snappy(req->body());
        prometheus::WriteRequest request;
        request.ParseFromString(body);
        size_t sz = request.timeseries_size();
        for (size_t i = 0; i != sz; ++i) {
          bb_monitor::Metric metric;
          const auto &prom_ts = request.timeseries(i);
          size_t sample_sz = prom_ts.samples_size();
          for (size_t j = 0; j != sample_sz; ++j) {
            auto sample = prom_ts.samples(j);
            size_t label_sz = prom_ts.labels_size();
            for (size_t k = 0; k != label_sz; ++k) {
              auto &label = prom_ts.labels(k);
              if (label.name() == "__name__") {
                // split in ns & name??
                std::string s = label.value();
                metric.set_ns("");
                metric.set_name(label.value());
              } else if (label.name() == "prometheus_replica") {
                // skip this label otherwise we double the series if running redundant prometheus operators
              } else {
                auto bb_label = metric.add_labels();
                bb_label->set_key(label.name());
                bb_label->set_value(label.value());
              }
            }

            metric.set_timestamp(sample.timestamp());
            metric.mutable_sample()->set_doublevalue(sample.value());

            if (!metric.name().empty()) {
              insert(stream, metric, metric.timestamp());
            }
          }
        }
      }
      req->create_response().done();
      return restinio::request_accepted();
    } else if (restinio::http_method_delete() == req->header().method()) {
      // this in intentionally doing nothing
      req->create_response().done();
      return restinio::request_accepted();
    }
  }
  return restinio::request_rejected();
}

using namespace std::chrono_literals;
using namespace kspp;

int
main(int argc, char *argv[]) {
  FLAGS_logtostderr = 1;
  google::InitGoogleLogging(argv[0]);

  boost::program_options::options_description desc("options");
  desc.add_options()
      ("help", "produce help message")
      ("port", boost::program_options::value<std::string>()->default_value(get_env_and_log("PORT", DEFAULT_PORT)), "port")
      ("monitor_uri", boost::program_options::value<std::string>()->default_value(get_env_and_log("MONITOR_URI", "lb.bitbouncer.com:30111")),"monitor_uri")
      ("monitor_api_key", boost::program_options::value<std::string>()->default_value(get_env_and_log("MONITOR_API_KEY", "")), "monitor_api_key")
      ("monitor_secret_access_key", boost::program_options::value<std::string>()->default_value(get_env_and_log("MONITOR_SECRET_ACCESS_KEY", "")),"monitor_secret_access_key")
      ("max_queue", boost::program_options::value<std::string>()->default_value(get_env_and_log("MAX_QUEUE", "100000")),"max_queue")
  ;

  boost::program_options::variables_map vm;
  boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc), vm);
  boost::program_options::notify(vm);

  if (vm.count("help")) {
    std::cout << desc << std::endl;
    return 0;
  }

  std::string port;
  if (vm.count("port")) {
    port = vm["port"].as<std::string>();
  }

  std::string monitor_uri;
  if (vm.count("monitor_uri")) {
    monitor_uri = vm["monitor_uri"].as<std::string>();
  }

  std::string monitor_api_key;
  if (vm.count("monitor_api_key")) {
    monitor_api_key = vm["monitor_api_key"].as<std::string>();
  }

  if (monitor_api_key.size()==0){
    std::cerr << "monitor_api_key must be defined - exiting";
    return -1;
  }

  std::string monitor_secret_access_key;
  if (vm.count("monitor_secret_access_key")) {
    monitor_secret_access_key = vm["monitor_secret_access_key"].as<std::string>();
  }

  int64_t max_queue=0;
  if (vm.count("max_queue")) {
    auto s = vm["max_queue"].as<std::string>();
    max_queue = atoll(s.c_str());
  }

  if (max_queue<=0)
    max_queue = LONG_LONG_MAX;

  auto config = std::make_shared<kspp::cluster_config>("dummy");
  LOG(INFO) << "port                           : " << port;
  LOG(INFO) << "monitor_uri                    : " << monitor_uri;
  LOG(INFO) << "monitor_api_key                : " << monitor_api_key;
  if (monitor_secret_access_key.size() > 0)
    LOG(INFO) << "monitor_secret_access_key      : " << "[hidden]";
  LOG(INFO) << "max_queue                      : " << max_queue;
  LOG(INFO) << "discovering facts...";

  std::shared_ptr<grpc::Channel> metrics_channel;
  {
    grpc::ChannelArguments channelArgs;
    auto channel_creds = grpc::SslCredentials(grpc::SslCredentialsOptions());
    metrics_channel = grpc::CreateCustomChannel(monitor_uri, channel_creds, channelArgs);
  }

  kspp::topology_builder builder(config);
  auto topology = builder.create_topology();
  auto source = topology->create_processor<mem_stream_source<void, bb_monitor::Metric>>(0);
  auto sink = topology->create_sink<bb_metric_sink>(source, metrics_channel, monitor_api_key, monitor_secret_access_key, max_queue);

  std::signal(SIGINT, sigterm);
  std::signal(SIGTERM, sigterm);
  std::signal(SIGPIPE, SIG_IGN);

  topology->add_labels({
                           {"app_name", SERVICE_NAME},
                           {"hostname", default_hostname()},
                       });

  topology->start(kspp::OFFSET_END); // has to be something - since we feed events from web totally irrelevant

  std::thread t([topology]() {
    while (run) {
      if (topology->process(kspp::milliseconds_since_epoch()) == 0) {
        std::this_thread::sleep_for(5000ms);
        topology->commit(false);
      }
    }
    LOG(INFO) << "flushing events..";
    topology->flush(true, 10000); // 10sec max
    LOG(INFO) << "flushing events done";
  });

  LOG(INFO) << "status is up";
  try {
    restinio::run(
        restinio::on_thread_pool(2)  // std::thread::hardware_concurrency()
            .port(atoi(port.c_str()))
            .address("0.0.0.0")
            .request_handler([source](restinio::request_handle_t req){
              return request_handler(*source, req);
            }));
  }

  catch (const std::exception &ex) {
    std::cerr << "Error: " << ex.what() << std::endl;
    return 1;
  }

  t.join();

  LOG(INFO) << "status is down";

  topology->commit(true);
  topology->close();
  return 0;
}
