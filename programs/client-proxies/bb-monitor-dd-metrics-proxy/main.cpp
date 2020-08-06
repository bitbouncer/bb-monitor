#include <cstring>
#include <glog/logging.h>
#include <boost/program_options.hpp>
#include <restinio/all.hpp>
#include <kspp/kspp.h>
#include <kspp/sources/mem_stream_source.h>
#include <kspp/processors/flat_map.h>
#include <kspp/metrics/prometheus_pushgateway_reporter.h>
#include <kspp/utils/env.h>
#include <bb_monitor_client_utils/pb_json_parsing.h>
#include <bb_monitor_client_utils/prometheus_utils.h>
#include <bb_monitor_client_utils/bb_metric_sink.h>
#include <bb_monitor_client_utils/bb_intake_sink.h>
#include <bb_monitor_utils/compression.h>

#define SERVICE_NAME       "dd_api_srv"
#define DEFAULT_PORT       "8080"
//#define DEFAULT_DD_API_KEY "2cc3d365-c765-4d3e-933e-f1bb9abd4419"

/* Exit flag for main loop */
static bool run = true;
static void sigterm(int sig) {
  run = false;
}

//static std::string s_dd_api_key = DEFAULT_DD_API_KEY;

/*
 * bool authorize(mg_connection *conn){
  const char* api_key = mg_get_header(conn, "Dd-Api-Key");
  if (!api_key) {
    send_status(conn, 403); // Unauthorized
    mg_printf(conn, "\r\n");
    return false;
  }

  if (s_dd_api_key != api_key) {
    send_status(conn, 403); // Unauthorized
    mg_printf(conn, "\r\n");
    return false;
  }

  return true;
}
*/

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

auto create_request_handler(std::shared_ptr<kspp::mem_stream_source<void, bb_monitor::Metric>> metrics_source, std::shared_ptr<kspp::mem_stream_source<void, bb_monitor::Intake>> intake_stream)
{
  auto router = std::make_unique<router_t>();
  router->http_post(
      "/api/v1/series",
      [metrics_source]( auto req, auto ){
        auto body = get_decompressed_body(req);
        //LOG(INFO) << body;
        auto v = parse_datadog_series2pb(body);
        for(auto m : v)
          insert(*metrics_source, m);

        req->create_response(restinio::status_created())
            .append_header( restinio::http_field::content_type, "text/plain; charset=utf-8" )
            .set_body("\r\n")
            .done();
        return restinio::request_accepted();
      } );

  router->http_post(
      "/api/v1/check_run",
      [metrics_source]( auto req, auto ){
        auto body = get_decompressed_body(req);
        //LOG(INFO) << body;// remove me
        auto v = parse_datadog_check_run2pb(body);
        for(auto m : v)
          insert(*metrics_source, m);

        req->create_response(restinio::status_created())
            .append_header( restinio::http_field::content_type, "text/plain; charset=utf-8" )
            .set_body("\r\n")
            .done();
        return restinio::request_accepted();
      } );

  router->http_post(
      "/intake/",
      [intake_stream]( auto req, auto ){
        auto body = get_decompressed_body(req);
        //LOG(INFO) << body; // remove me
        bb_monitor::Intake intake;
        intake.set_agent("datadog");
        intake.set_data(body);
        intake.set_timestamp(kspp::milliseconds_since_epoch());
        insert(*intake_stream, intake);

        req->create_response(restinio::status_created())
            .append_header( restinio::http_field::content_type, "text/plain; charset=utf-8" )
            .set_body("\r\n")
            .done();
        return restinio::request_accepted();
      } );

  // not implemented
  router->http_post(
      "/api/v1/validate",
      []( auto req, auto ){
        LOG(INFO) << "not implemented";
        req->create_response(restinio::status_created())
            .append_header( restinio::http_field::content_type, "text/plain; charset=utf-8" )
            .set_body("\r\n")
            .done();
        return restinio::request_accepted();
      } );

  router->http_post(
      "/api/v1/comments",
      []( auto req, auto ){
        LOG(INFO) << "not implemented";
        req->create_response(restinio::status_created())
            .append_header( restinio::http_field::content_type, "text/plain; charset=utf-8" )
            .set_body("\r\n")
            .done();
        return restinio::request_accepted();
      } );

  router->http_post(
      "/api/v1/events",
      []( auto req, auto ){
        LOG(INFO) << "not implemented";
        req->create_response(restinio::status_created())
            .append_header( restinio::http_field::content_type, "text/plain; charset=utf-8" )
            .set_body("\r\n")
            .done();
        return restinio::request_accepted();
      } );

  router->http_post(
      "/api/v1/tags/hosts",
      []( auto req, auto ){
        LOG(INFO) << "not implemented";
        req->create_response(restinio::status_created())
            .append_header( restinio::http_field::content_type, "text/plain; charset=utf-8" )
            .set_body("\r\n")
            .done();
        return restinio::request_accepted();
      } );


  router->http_post(
      "/api/v1/container",
      []( auto req, auto ){
        LOG(INFO) << "not implemented";
        req->create_response(restinio::status_created())
            .append_header( restinio::http_field::content_type, "text/plain; charset=utf-8" )
            .set_body("\r\n")
            .done();
        return restinio::request_accepted();
      } );

  router->http_get(
      "/healthz",
      []( auto req, auto ){
        LOG(INFO) << "healthz called";
        req->create_response(restinio::status_ok())
            .append_header( restinio::http_field::content_type, "text/plain; charset=utf-8" )
            .set_body("\r\n")
            .done();
        return restinio::request_accepted();
      } );

  return router;
}


using namespace std::chrono_literals;
using namespace kspp;


int
main(int argc, char *argv[])
{
  FLAGS_logtostderr = 1;
  google::InitGoogleLogging(argv[0]);

  boost::program_options::options_description desc("options");
  desc.add_options()
      ("help", "produce help message")
      ("app_realm", boost::program_options::value<std::string>()->default_value(get_env_and_log("APP_REALM", "DEV")), "app_realm")
      ("port", boost::program_options::value<std::string>()->default_value(get_env_and_log("PORT", DEFAULT_PORT)), "port")
      //("dd_api_key", boost::program_options::value<std::string>()->default_value(get_env_and_log("DD_API_KEY", DEFAULT_DD_API_KEY)), "dd_api_key")
      ("monitor_api_key", boost::program_options::value<std::string>()->default_value(get_env_and_log("MONITOR_API_KEY", "")), "monitor_api_key")
      ("monitor_secret_access_key", boost::program_options::value<std::string>()->default_value(get_env_and_log("MONITOR_SECRET_ACCESS_KEY", "")),"monitor_secret_access_key")
      ("monitor_uri", boost::program_options::value<std::string>()->default_value(get_env_and_log("MONITOR_URI", "lb.bitbouncer.com:30111")),"monitor_uri")
      ("max_queue", boost::program_options::value<std::string>()->default_value(get_env_and_log("MAX_QUEUE", "100000")),"max_queue")
      ;

  boost::program_options::variables_map vm;
  boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc), vm);
  boost::program_options::notify(vm);

  if (vm.count("help")) {
    std::cout << desc << std::endl;
    return 0;
  }

  std::string app_realm;
  if (vm.count("app_realm")) {
    app_realm = vm["app_realm"].as<std::string>();
  }

  std::string port;
  if (vm.count("port")) {
    port = vm["port"].as<std::string>();
  }

  /*if (vm.count("dd_api_key")) {
    s_dd_api_key = vm["dd_api_key"].as<std::string>();
  }
  */

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


  std::string consumer_group(SERVICE_NAME);

  auto config = std::make_shared<kspp::cluster_config>(consumer_group, kspp::cluster_config::PUSHGATEWAY);
  config->load_config_from_env();
  //std::string src_topic = dd::raw_metrics_topic_name(tenant_id);
  //std::string dst_topic = dd::prometheus_metrics_topic_name(tenant_id);


  LOG(INFO) << "port                     : " << port;
  //LOG(INFO) << "dd_api_key             : " << s_dd_api_key;
  LOG(INFO) << "monitor_uri              : " << monitor_uri;
  LOG(INFO) << "monitor_api_key          : " << monitor_api_key;
  if (monitor_secret_access_key.size()>0)
    LOG(INFO) << "monitor_secret_access_key: " << "[hidden]";
  LOG(INFO) << "monitor_secret_access_key : " << monitor_secret_access_key;
  LOG(INFO) << "max_queue                 : " << max_queue;
  LOG(INFO) << "discovering facts...";

  kspp::topology_builder builder(config);
  auto topology = builder.create_topology();
  auto metrics_source = topology->create_processor<mem_stream_source<void, bb_monitor::Metric>>(0);

  std::shared_ptr<grpc::Channel> metrics_channel;
  {
    grpc::ChannelArguments channelArgs;
    auto channel_creds = grpc::SslCredentials(grpc::SslCredentialsOptions());
    metrics_channel = grpc::CreateCustomChannel(monitor_uri, channel_creds, channelArgs);
  }

  auto sink = topology->create_sink<bb_metric_sink>(metrics_source, metrics_channel, monitor_api_key, monitor_secret_access_key, max_queue);

  auto intake_source = topology->create_processor<mem_stream_source<void, bb_monitor::Intake>>(0);
  auto intake_sink = topology->create_sink<bb_intake_sink>(intake_source, metrics_channel, monitor_api_key, monitor_secret_access_key);

  std::signal(SIGINT, sigterm);
  std::signal(SIGTERM, sigterm);
  std::signal(SIGPIPE, SIG_IGN);

  topology->add_labels( {
                            { "app_name", SERVICE_NAME },
                            { "app_realm", app_realm },
                            { "hostname", default_hostname() }
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

    using traits_t =
    restinio::traits_t<
        restinio::asio_timer_manager_t,
        restinio::null_logger_t,
        router_t >;

    restinio::run(
        restinio::on_this_thread< traits_t >()
            .port(atoi(port.c_str()))
            .address("0.0.0.0")
            .request_handler(create_request_handler(metrics_source, intake_source))
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

