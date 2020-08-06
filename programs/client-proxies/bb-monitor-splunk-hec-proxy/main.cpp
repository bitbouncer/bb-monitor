#include <cstring>
#include <glog/logging.h>
#include <boost/program_options.hpp>
#include <restinio/all.hpp>
#include <kspp/kspp.h>
#include <kspp/sources/mem_stream_source.h>
#include <kspp/processors/flat_map.h>
#include <kspp/metrics/prometheus_pushgateway_reporter.h>
#include <kspp/utils/env.h>
#include <bb_monitor_client_utils/bb_log_sink.h>
#include <bb_monitor_utils/compression.h>
#include <bb_monitor_utils/splunk/splunk_utils.h>

#define SERVICE_NAME       "bb-splunk-hec-proxy"
#define DEFAULT_PORT       "8088"

using namespace std::chrono_literals;
using namespace kspp;

/* Exit flag for main loop */
static bool run = true;
static void sigterm(int sig) {
  run = false;
}

std::string get_decompressed_body(restinio::request_handle_t req){
  if (req->header().has_field(restinio::http_field::content_encoding)) {
    auto encodning = req->header().get_field(restinio::http_field::content_encoding);
    if (encodning == "deflate") {
      return decompress_deflate(req->body());
    } else if (encodning == "snappy") {
      return decompress_snappy(req->body());
    } else if (encodning == "gzip") {
      return decompress_gzip(req->body());
    } else {
      LOG(ERROR) << "dont know what to do with content_encoding: " << encodning;
      return req->body();
    }
  }
  return req->body();
}

using router_t = restinio::router::express_router_t<>;

auto create_request_handler(std::shared_ptr<kspp::mem_stream_source<void, bb_monitor::LogLine>> stream)
{
  auto router = std::make_unique<router_t>();

  router->http_post(
      "/services/collector/event/1.0",
      [stream]( auto req, auto ){
        auto body = get_decompressed_body(req);
        auto v = parse_splunk_event(body);
        for(auto m : v)
          insert(*stream, m);

        // it seems th splunk docker code requires a 200 here
        req->create_response(restinio::status_ok())
            .done();
        return restinio::request_accepted();
      } );

  router->http_get(
      "/healthz",
      []( auto req, auto ){
        req->create_response(restinio::status_ok())
            .append_header( restinio::http_field::content_type, "text/plain; charset=utf-8" )
            .set_body("\r\n")
            .done();
        return restinio::request_accepted();
      } );

  router->non_matched_request_handler(
      [](auto req){
        LOG(INFO) << req->header().method() << ", " << req->header().path();

        if (strcmp("OPTIONS", req->header().method().c_str())==0){
          req->create_response(restinio::status_ok())
              .append_header( restinio::http_field::allow, "POST" )
              .set_body("\r\n")
              .done();
          return restinio::request_accepted();
        }

        return req->create_response(restinio::status_not_found()).connection_close().done();
      });

  return router;
}

int
main(int argc, char *argv[])
{
  FLAGS_logtostderr = 1;
  google::InitGoogleLogging(argv[0]);

  boost::program_options::options_description desc("options");
  desc.add_options()
      ("help", "produce help message")
      ("port", boost::program_options::value<std::string>()->default_value(get_env_and_log("PORT", DEFAULT_PORT)), "port")
      ("monitor_api_key", boost::program_options::value<std::string>()->default_value(get_env_and_log("MONITOR_API_KEY", "")), "monitor_api_key")
      ("monitor_secret_access_key", boost::program_options::value<std::string>()->default_value(get_env_and_log("MONITOR_SECRET_ACCESS_KEY", "")),"monitor_secret_access_key")
      ("monitor_uri", boost::program_options::value<std::string>()->default_value(get_env_and_log("MONITOR_URI", "lb.bitbouncer.com:30111")),"monitor_uri")
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

  std::string monitor_uri;
  if (vm.count("monitor_uri")) {
    monitor_uri = vm["monitor_uri"].as<std::string>();
  }

  std::string consumer_group(SERVICE_NAME);

  auto config = std::make_shared<kspp::cluster_config>(consumer_group, kspp::cluster_config::NONE);
  config->load_config_from_env();

  LOG(INFO) << "port               : " << port;
  LOG(INFO) << "monitor_api_key    : " << monitor_api_key;
  if (monitor_secret_access_key.size()>0)
    LOG(INFO) << "monitor_secret_access_key  : " << "[hidden]";

  LOG(INFO) << "monitor_secret_access_key    : " << monitor_secret_access_key;

  LOG(INFO) << "monitor_uri        : " << monitor_uri;
  LOG(INFO) << "discovering facts...";

  kspp::topology_builder builder(config);
  auto topology = builder.create_topology();
  auto source = topology->create_processor<mem_stream_source<void, bb_monitor::LogLine>>(0);

  std::shared_ptr<grpc::Channel> channel;
  grpc::ChannelArguments channelArgs;
  auto channel_creds = grpc::SslCredentials(grpc::SslCredentialsOptions());
  channel = grpc::CreateCustomChannel(monitor_uri, channel_creds, channelArgs);

  auto sink = topology->create_sink<bb_log_sink>(source, channel, monitor_api_key, monitor_secret_access_key);

  std::signal(SIGINT, sigterm);
  std::signal(SIGTERM, sigterm);
  std::signal(SIGPIPE, SIG_IGN);

  topology->start(kspp::OFFSET_END); // has to be something - since we feed events from web totally irrelevant

  std::thread t([topology, sink]() {

    int64_t next_log = kspp::milliseconds_since_epoch()+60000;

    while (run) {
      if (topology->process(kspp::milliseconds_since_epoch()) == 0) {
        std::this_thread::sleep_for(2000ms);
        topology->commit(false);
      }

      if (kspp::milliseconds_since_epoch() > next_log){
        next_log = kspp::milliseconds_since_epoch()+6000;
        if (sink->queue_size()>1000) {
          LOG(INFO) << "items in queue" << sink->queue_size();
        }
      }
    }
    LOG(INFO) << "flushing events..";
    topology->flush(true, 10000); // 10sec max
    LOG(INFO) << "flushing events done";
  });

  LOG(INFO) << "status is up";
  try {

    using traits_t =
    restinio::traits_t<
        restinio::asio_timer_manager_t,
        restinio::null_logger_t,
        router_t >;

    restinio::run(
        restinio::on_this_thread< traits_t >()
            .port(atoi(port.c_str()))
            .address("0.0.0.0")
            .request_handler(create_request_handler(source))
            .read_next_http_message_timelimit( 10s )
            .write_http_response_timelimit( 5s )
            .handle_request_timeout( 2s )
    );
  }

  catch (const std::exception &ex) {
    LOG(ERROR) << "exception: " << ex.what();
    run = false;
  }

  LOG(INFO) << "status is down";

  topology->commit(true);
  topology->close();
  return 0;
}

