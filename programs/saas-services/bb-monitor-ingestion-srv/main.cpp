#include <memory>
#include <iostream>
#include <string>
#include <thread>
#include <grpc++/grpc++.h>
#include <glog/logging.h>
#include <boost/program_options.hpp>
#include <kspp/utils/env.h>
#include <kspp/utils/kafka_utils.h>
#include <bb_monitor_utils/grpc_utils.h>
#include "kafka_sink_factory.h"

#define BB_METRIC_ENABLE_STREAMING 0

#ifdef BB_METRIC_ENABLE_STREAMING
#include "put_metrics_call_data.h"
#include "put_intake_call_data.h"
#include "put_logs_call_data.h"
#endif

#include "put_metrics2_call_data.h"
#include "put_intake2_call_data.h"
#include "put_logs2_call_data.h"


using namespace std::chrono_literals;
using namespace kspp;

using grpc::Server;
using grpc::ServerAsyncResponseWriter;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerCompletionQueue;
using grpc::Status;

#define SERVICE_NAME "bb_metrics_srv"
#define DEFAULT_PORT "50071"



/* Exit flag for main loop */
static bool run = true;
static void sigterm(int sig) {
  run = false;
}



class ServerImpl final {
public:
  ServerImpl(std::string bindTo, std::shared_ptr<kafka_sink_factory> factory)
  : _server_address(bindTo)
  , _factory(factory){
  }

  ~ServerImpl() {
    server_->Shutdown();
    // Always shutdown the completion queue after the server.
    cq_->Shutdown();
  }

  // There is no shutdown handling in this code.
  void Run() {
    ServerBuilder builder;
    bb_set_channel_args(builder);
    // Listen on the given address without any authentication mechanism.
    builder.AddListeningPort(_server_address, grpc::InsecureServerCredentials());
    //builder.AddListeningPort(server_address, call_creds);
    // Register "service_" as the instance through which we'll communicate with
    // clients. In this case it corresponds to an *asynchronous* service.
    builder.RegisterService(&metrics_service_);
    // Get hold of the completion queue used for the asynchronous communication
    // with the gRPC runtime.
    cq_ = builder.AddCompletionQueue();
    // Finally assemble the server.
    server_ = builder.BuildAndStart();
    LOG(INFO) << "Server listening on " << _server_address;

    // Proceed to the server's main loop.
    HandleRpcs();
  }

private:


  // This can be run in multiple threads if needed.
  void HandleRpcs() {
    // Spawn a new CallData instance to serve new clients.
#ifdef BB_METRIC_ENABLE_STREAMING
    //new put_metrics_call_data(&metrics_service_, cq_.get(), _factory);
    //new put_intake_call_data(&metrics_service_, cq_.get(), _factory);
    //new put_logs_call_data(&metrics_service_, cq_.get(), _factory);
#endif
    // non - streaming version
    new put_metrics2_call_data(&metrics_service_, cq_.get(), _factory);
    new put_intake2_call_data(&metrics_service_, cq_.get(), _factory);
    new put_logs2_call_data(&metrics_service_, cq_.get(), _factory);

    void *tag;  // uniquely identifies a request.
    bool ok;
    while (run) { // TODO maybe this is the wrong way to stop an application - what about pending calls
      // Block waiting to read the next event from the completion queue. The
      // event is uniquely identified by its tag, which in this case is the
      // memory address of a CallData instance.
      // The return value of Next should always be checked. This return value
      // tells us whether there is any kind of event or cq_ is shutting down.
      tag = nullptr;
      if ((cq_->Next(&tag, &ok)) == false) {
        LOG(ERROR) << "cq_->Next failed";
        if (tag)
          static_cast<call_data *>(tag)->set_error();
      }

      //LOG(INFO) << "next msg";

      if (!ok) {
        if (tag) {
          call_data* p = static_cast<call_data *>(tag);
          LOG(ERROR) << "request failed " << tag;
          if (p->get_state() == call_data::READING){
            p->set_state(call_data::READING_FAILED);
            //LOG(ERROR) << "READING_FAILED - prepare reply";
          } else {
            LOG(ERROR) << "request failed";
          }
        }
      }
      //GPR_ASSERT(ok);
      static_cast<call_data *>(tag)->process_state();
    }
  }

  std::string _server_address;
  std::shared_ptr<kafka_sink_factory> _factory;
  std::unique_ptr<ServerCompletionQueue> cq_;
  MonitorSink::AsyncService metrics_service_;
  std::unique_ptr<Server> server_;
};

int main(int argc, char **argv) {
  FLAGS_logtostderr = 1;
  google::InitGoogleLogging(argv[0]);

  boost::program_options::options_description desc("options");
  desc.add_options()
      ("help", "produce help message")
      ("app_realm", boost::program_options::value<std::string>()->default_value(get_env_and_log("APP_REALM", "DEV")), "app_realm")
      ("port", boost::program_options::value<std::string>()->default_value(get_env_and_log("PORT", DEFAULT_PORT)), "port")
      ("topic_auth_topic", boost::program_options::value<std::string>()->default_value(kspp::get_env_and_log("TOPIC_AUTH_TOPIC")), "topic_auth_topic")
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

  std::string topic_auth_topic;
  if (vm.count("topic_auth_topic")) {
    topic_auth_topic = vm["topic_auth_topic"].as<std::string>();
  }
  if (topic_auth_topic.size()==0){
    std::cerr << "topic_auth_topic must be specified" << std::endl;
    return -1;
  }

  auto config = std::make_shared<kspp::cluster_config>();
  config->load_config_from_env();
  config->set_producer_buffering_time(1000ms);
  config->set_consumer_buffering_time(500ms);
  config->validate();
  config->log();
  config->get_schema_registry();

  LOG(INFO) << "port               : " << port;
  LOG(INFO) << "topic_auth_topic   : " << topic_auth_topic;
  LOG(INFO) << "discovering facts...";

  std::string hostname = default_hostname();

  std::map<std::string, std::string> labels = {
      { "app_name", SERVICE_NAME },
      { "port", port },
      { "host", hostname},
      { "app_realm", app_realm }
  };

  std::signal(SIGINT, sigterm);
  std::signal(SIGTERM, sigterm);
  std::signal(SIGPIPE, SIG_IGN);

  auto factory = std::make_shared<kafka_sink_factory>(config, topic_auth_topic, labels);
  std::string bindTo = "0.0.0.0:" + port;
  ServerImpl server(bindTo, factory);
  server.Run();
  LOG(INFO) << "exiting";
  gflags::ShutDownCommandLineFlags(); // make valgrind happy
  return 0;
}
