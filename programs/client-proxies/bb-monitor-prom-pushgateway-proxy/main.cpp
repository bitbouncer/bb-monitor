#include <cstring>
#include <glog/logging.h>
#include <boost/program_options.hpp>
#include <restinio/all.hpp>
#include <kspp/topology_builder.h>
#include <kspp/sources/mem_stream_source.h>
#include <kspp/processors/flat_map.h>
#include <kspp/kspp.h>
#include <kspp/utils/env.h>
#include <bb_monitor_client_utils/prometheus_utils.h>
#include <bb_monitor_client_utils/bb_metric_sink.h>

#define SERVICE_NAME   "bb_pushgateway"
#define DEFAULT_PORT   "9091"

#define INTERNAL_K8S_URI "monitor-api-srv:50071"

/* Exit flag for main loop */
static bool run = true;
static void sigterm(int sig) {
  run = false;
}

template < typename RESP >
RESP
init_resp( RESP resp )
{
  resp.append_header( restinio::http_field::server, "restino /v.0.5" );
  resp.append_header_date_field();
  return resp;
}

restinio::request_handling_status_t request_handler(kspp::mem_stream_source<void, bb_monitor::Metric> &stream, restinio::request_handle_t req) {
  auto path = req->header().path();
  // eg path.starts_with()
  if ((path.size()>13) && (path.compare(0, 13,  "/metrics/job/")==0)){
    if (req->header().method() == (restinio::http_method_post()) || req->header().method() == (restinio::http_method_put())) {
      path.remove_prefix(13);
        // those are// of form  "/metrics/job/some_job/instance/some_instance"
        // those are of form  "some_job/instance/some_instance"
        // or  "some_job
        std::string_view job_id;
        std::string_view instance_id;
        auto first_slash = path.find('/');
        if (first_slash!=std::string::npos){
          job_id = path.substr(0, first_slash);
          auto second_slash = path.find('/', first_slash+1);
          if (second_slash==std::string::npos){
            //TODO set status here
            return restinio::request_rejected();
          }
          instance_id = path.substr(second_slash+1);
        } else if (path.size()){
          job_id = path;
        } else {
          // TODO set bad request status
          return restinio::request_rejected();
        }

        //LOG(INFO) << "job: " << job_id << ", instance:" << instance_id;
        //auto encodning = req->header().get_field(restinio::http_field::content_encoding);
        //if (encodning == "snappy") {  auto body = decompress_snappy(req->body());]

        auto v = parse_prometheus_line_format_2_bb_metrics(job_id, instance_id, req->body());

        for (const auto& i : v){
          insert(stream, i);
        }

        req->create_response(restinio::status_created())
            .append_header( restinio::http_field::content_type, "text/plain; charset=utf-8" )
            .set_body( "\r\n")
            .done();
      return restinio::request_accepted();

    } else if (req->header().method() == restinio::http_method_delete()){
      return restinio::request_accepted();
    }
      return restinio::request_rejected();
  }
  return restinio::request_rejected();
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
      ("port", boost::program_options::value<std::string>()->default_value(get_env_and_log("PORT", DEFAULT_PORT)), "port")
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
  LOG(INFO) << "port                         : " << port;
  LOG(INFO) << "monitor_uri                  : " << monitor_uri;
  LOG(INFO) << "monitor_api_key              : " << monitor_api_key;
  if (monitor_secret_access_key.size()>0)
    LOG(INFO) << "monitor_secret_access_key    : " << "[hidden]";
  LOG(INFO) << "max_queue                    : " << max_queue;
  LOG(INFO) << "discovering facts...";

  std::shared_ptr<grpc::Channel> channel;
  grpc::ChannelArguments channelArgs;
  // special for easier debugging
  if (monitor_uri == INTERNAL_K8S_URI) {
    LOG(WARNING) << "using insecure channel for " << monitor_uri;
    channel = grpc::CreateCustomChannel(monitor_uri, grpc::InsecureChannelCredentials(), channelArgs);
  } else {
    auto channel_creds = grpc::SslCredentials(grpc::SslCredentialsOptions());
    channel = grpc::CreateCustomChannel(monitor_uri, channel_creds, channelArgs);
  }

  kspp::topology_builder builder(config);
  auto topology = builder.create_topology();
  auto source = topology->create_processor<mem_stream_source<void, bb_monitor::Metric>>(0);
  auto sink = topology->create_sink<bb_metric_sink>(source, channel, monitor_api_key, monitor_secret_access_key, max_queue);

  std::signal(SIGINT, sigterm);
  std::signal(SIGTERM, sigterm);
  std::signal(SIGPIPE, SIG_IGN);

  topology->add_labels( {
                            { "app_name", SERVICE_NAME },
                            { "hostname", default_hostname() },
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
          restinio::on_thread_pool(1)  // std::thread::hardware_concurrency()
              .port(atoi(port.c_str()))
              .address("0.0.0.0")
              .request_handler([source](restinio::request_handle_t req){
                return request_handler(*source, req);
              }));
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
