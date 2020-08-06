#include <cstdlib>
#include <iostream>
#include <chrono>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/program_options.hpp>
#include <openssl/md5.h>
#include <kspp/sources/mem_stream_source.h>
#include <kspp/utils/env.h>
#include <kspp/topology_builder.h>
#include <kspp/utils/env.h>
#include <bb_monitor_client_utils/kv_filter.h>
#include <bb_monitor_client_utils/bb_log_sink.h>
#include <bb_monitor_utils/time.h>

#define CONNECTION_TIMEOUT_S 60
#define SERVICE_NAME         "dd_dog_log_srv"
#define DEFAULT_PORT         "10513"
//#define DEFAULT_DD_API_KEY   "2cc3d365-c765-4d3e-933e-f1bb9abd4419"

using boost::asio::ip::tcp;
using namespace std::chrono_literals;

const std::string CRLF("\r\n");
static kspp::kv_filter dd_kv_parser("\",", ':');

/* Exit flag for main loop */
static bool run = true;
static void sigterm(int sig) {
  run = false;
}

//static std::string s_dd_api_key = DEFAULT_DD_API_KEY;

class session : public std::enable_shared_from_this<session>
{
public:
  session(boost::asio::io_service& io_service, std::shared_ptr<kspp::mem_stream_source<void, bb_monitor::LogLine>> stream)
      : socket_(io_service)
      , _connection_dead_timer(io_service)
      , _stream(stream) {
  }

  ~session() {
    close();
  }

  tcp::socket& socket()
  {
    return socket_;
  }

  void start()
  {
      boost::system::error_code ec;
      remote_address_ = socket_.remote_endpoint(ec).address().to_string();
      if (ec){
          LOG(ERROR) << "no remote endpoint - closing";
          close();
          return;
      }

    LOG(INFO) << "session created, remote_address:" <<  remote_address_;
    socket_.async_read_some(boost::asio::buffer(buffer_, max_length),
                            boost::bind(&session::handle_read, shared_from_this(),
                                        boost::asio::placeholders::error,
                                        boost::asio::placeholders::bytes_transferred));

    _connection_dead_timer.expires_from_now(boost::posix_time::seconds(CONNECTION_TIMEOUT_S));
    _connection_dead_timer.async_wait(boost::bind(&session::handle_connection_dead_timer, shared_from_this(), boost::asio::placeholders::error));
  }

private:
  void close() {
    boost::system::error_code ignored_ec;
    _connection_dead_timer.cancel(ignored_ec);

    boost::system::error_code ec;
    if(socket_.is_open()) {
      LOG(INFO) << "session closed, remote_address:" <<  remote_address_;
      socket_.close(ec);
    }
  }

  void handle_connection_dead_timer(const boost::system::error_code &ec) {
    if(ec)
      return;
    LOG(INFO) << "handle_connection_dead_timer expired - closing...";
    boost::system::error_code ignored_ec;
    if(socket_.is_open())
      socket_.close(ignored_ec);
  }

  void handle_read(const boost::system::error_code& error, size_t bytes_transferred)
  {
    if (!error)
    {
      _connection_dead_timer.expires_from_now(boost::posix_time::seconds(CONNECTION_TIMEOUT_S));
      _connection_dead_timer.async_wait(boost::bind(&session::handle_connection_dead_timer, shared_from_this(), boost::asio::placeholders::error));

      size_t len = remaining_ + bytes_transferred;
      char LF = '\n';
      auto ptr = memchr(buffer_, '\n', len);
      if (ptr==nullptr){
        line_.append(buffer_, 0, len);
        remaining_  = 0;
      } else {
        size_t bytes_2_copy = ((char*) ptr) - buffer_;
        line_.append(buffer_, 0, bytes_2_copy);
        remaining_ = (len - bytes_2_copy) - 1; // skip LF
        if (remaining_>0){
          memmove(buffer_, ((char*) ptr) +1,  remaining_);
        }
        handle_line();
        line_.erase();
      }

      size_t max_bytes_to_read = max_length - remaining_;
      if (remaining_>0) {
        //std::cerr << "reading max:  <" << max_bytes_to_read << ">\n";
        //std::cerr << "current content:" << std::string(buffer_, remaining_) << std::endl;
      }


      socket_.async_read_some(boost::asio::buffer(&buffer_[remaining_], max_bytes_to_read),
                              boost::bind(&session::handle_read, shared_from_this(),
                                          boost::asio::placeholders::error,
                                          boost::asio::placeholders::bytes_transferred));
    }
  }

  void handle_line() {
    std::string key;
    bb_monitor::LogLine value;

    //11223344556677 <46>0 2018-07-31T09:27:41.745847338Z saka-ThinkPad-P51 syslog - - [dd ddsource="syslog"][dd ddsourcecategory="syslog"][dd ddtags="filename:syslog"] Jul 31 11:28:47 saka-ThinkPad-P51 agent[1396]: 2018-07-31 11:28:47 CEST | INFO | (runner.go:309 in work) | Done running check ntp
    //LOG(INFO) << "GOT " << line_.size() << "chars: " << line_;

    auto end_of_api_key = line_.find_first_of(' ', 0);
    if (end_of_api_key == std::string::npos)
      return;

    std::string api_key = line_.substr(0, end_of_api_key);

    /*if (api_key!=s_dd_api_key){
      LOG(WARNING) << "got " <<api_key << ", expected: " << s_dd_api_key;
      LOG(WARNING) << "not authorized - disconneting, " <<  socket_.remote_endpoint().address().to_string();
      close();
      return;
    }
    */

    value.set_agent("datadog");

    auto end_46_thing = line_.find_first_of(' ', end_of_api_key+1);
    if (end_46_thing == std::string::npos)
      return;

    auto end_of_ts = line_.find_first_of(' ', end_46_thing+1);
    if (end_of_ts == std::string::npos)
      return;
    size_t collection_time_sz = end_of_ts - end_46_thing -1;
    std::string collection_time = line_.substr(end_46_thing+1, collection_time_sz);

    value.set_timestamp_ns(nanoseconds_since_epoch()); // WRONG PARSE collection_time...

    auto end_of_hostname = line_.find_first_of(' ', end_of_ts+1);
    if (end_of_hostname == std::string::npos)
      return;
    size_t hostname_sz = end_of_hostname - end_of_ts -1;
    value.set_host(line_.substr(end_of_ts+1, hostname_sz));

    auto end_of_source = line_.find_first_of(' ', end_of_hostname+1);
    if (end_of_source == std::string::npos)
      return;
    size_t source_sz = end_of_source - end_of_hostname -1;
    std::string source =  line_.substr(end_of_hostname+1, source_sz);
    value.set_source(source);

    auto end_of_unknown_1 = line_.find_first_of(' ', end_of_source+1);
    if (end_of_unknown_1 == std::string::npos)
      return;
    size_t unknown_1_sz = end_of_unknown_1 - end_of_source -1;
    std::string unknown_1 =  line_.substr(end_of_source+1, unknown_1_sz);

    auto end_of_unknown_2 = line_.find_first_of(' ', end_of_unknown_1+1);
    if (end_of_unknown_2 == std::string::npos)
      return;
    size_t unknown_2_sz = end_of_unknown_2 - end_of_unknown_1 -1;
    std::string unknown_2 =  line_.substr(end_of_unknown_1+1, unknown_2_sz);

    // [dd ddsource="syslog"][dd ddsourcecategory="syslog"][dd ddtags="filename:syslog"]

    auto begin_ddsource = line_.find("dd ddsource", end_of_unknown_2+1);
    if (begin_ddsource == std::string::npos)
      return;
    auto end_of_ddsource = line_.find_first_of('"', begin_ddsource+13);
    size_t ddsource_sz = end_of_ddsource - begin_ddsource - 13;
    std::string ddsource =  line_.substr(begin_ddsource+13, ddsource_sz);
    {
      bb_monitor::Label label;
      label.set_key("ddsource");
      label.set_value(ddsource);
      *value.add_labels() = label;
    }

    auto begin_ddsourcecategory = line_.find("dd ddsourcecategory", end_of_unknown_2+1);
    if (begin_ddsourcecategory == std::string::npos)
      return;
    auto end_ddsourcecategory = line_.find_first_of('"', begin_ddsourcecategory+21);
    size_t ddsource_category_sz = end_ddsourcecategory - begin_ddsourcecategory - 21;
    std::string ddsource_category =  line_.substr(begin_ddsourcecategory+21, ddsource_category_sz);
    {
      bb_monitor::Label label;
      label.set_key("ddsourcecategory");
      label.set_value(ddsource_category);
      *value.add_labels() = label;
    }

    auto begin_ddstags= line_.find("dd ddtags=", end_of_unknown_2+1);
    if (begin_ddstags == std::string::npos)
      return;
    auto end_ddtags = line_.find_first_of(']', begin_ddstags+10);
    if (end_ddtags==std::string::npos)
      return;

    size_t end_ddtags_sz = end_ddtags - begin_ddstags - 10;
    std::string ddtags =  line_.substr(begin_ddstags+10, end_ddtags_sz);
    std::vector<kspp::kv> tags;
    int ec = dd_kv_parser.match(ddtags, tags);

    for (auto t : tags){
      bb_monitor::Label label;
      label.set_key(t.key);
      label.set_value(t.val);
      *value.add_labels() = label;
    }

    value.set_line(line_.substr(end_ddtags+1));

    // should we care about collection time??
    // if we skip it we might have bad logs without ts in data - those will have the same id
    // if we  add it we will double count same logs if we restart collection agent
    MD5_CTX ctx;
    MD5_Init(&ctx);
    MD5_Update(&ctx, ddsource.data(), ddsource.size());
    MD5_Update(&ctx, ddsource_category.data(), ddsource_category.size());
    MD5_Update(&ctx, value.line().data(), value.line().size());
    // SHOULD TS be part of this???
    boost::uuids::uuid uuid;
    MD5_Final(uuid.data, &ctx);
    value.set_id(to_string(uuid));
    insert(*_stream, value);
  }

  tcp::socket socket_;
  std::shared_ptr<kspp::mem_stream_source<void, bb_monitor::LogLine>> _stream;
  boost::asio::deadline_timer _connection_dead_timer;

  enum { max_length =1024*64 };
  size_t remaining_=0;
  char  buffer_[max_length];
  std::string line_;
  std::string remote_address_ = "unknown";
};

class server
{
public:
  server(boost::asio::io_service& io_service, short port, std::shared_ptr<kspp::mem_stream_source<void, bb_monitor::LogLine>> stream)
      : _io_service(io_service)
      , _acceptor(io_service, tcp::endpoint(tcp::v4(), port))
      , _stream(stream) {
    start_accept();
  }

private:
  void start_accept()
  {
    std::shared_ptr<session> new_session = std::make_shared<session>(_io_service, _stream);
    _acceptor.async_accept(new_session->socket(),
                           boost::bind(&server::handle_accept, this, new_session,
                                       boost::asio::placeholders::error));
  }

  void handle_accept(std::shared_ptr<session> new_session,
                     const boost::system::error_code& error)
  {
    if (!error)
      new_session->start();
    start_accept();
  }

  boost::asio::io_service& _io_service;
  tcp::acceptor _acceptor;
  std::shared_ptr<kspp::mem_stream_source<void, bb_monitor::LogLine>> _stream;
};

using namespace kspp;

int main(int argc, char* argv[])
{
  FLAGS_logtostderr = 1;
  google::InitGoogleLogging(argv[0]);

  boost::program_options::options_description desc("options");
  desc.add_options()
      ("help", "produce help message")
      ("port", boost::program_options::value<std::string>()->default_value(get_env_and_log("PORT", DEFAULT_PORT)), "port")
      //("dd_api_key", boost::program_options::value<std::string>()->default_value(get_env_and_log("DD_API_KEY", DEFAULT_DD_API_KEY)), "dd_api_key")
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

  /*if (vm.count("dd_api_key")) {
    s_dd_api_key = vm["dd_api_key"].as<std::string>();
  }
  */

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
  auto config = std::make_shared<kspp::cluster_config>(consumer_group, kspp::cluster_config::PUSHGATEWAY);
  config->load_config_from_env();

  LOG(INFO) << "port                      : " << port;
  //LOG(INFO) << "dd_api_key                : " << s_dd_api_key;
  LOG(INFO) << "monitor_api_key           : " << monitor_api_key;
  if (monitor_secret_access_key.size()>0)
    LOG(INFO) << "monitor_secret_access_key: " << "[hidden]";

  LOG(INFO) << "monitor_uri               : " << monitor_uri;

  LOG(INFO) << "discovering facts...";

  std::shared_ptr<grpc::Channel> channel;
  grpc::ChannelArguments channelArgs;
  auto channel_creds = grpc::SslCredentials(grpc::SslCredentialsOptions());
  channel = grpc::CreateCustomChannel(monitor_uri, channel_creds, channelArgs);

  kspp::topology_builder builder(config);
  auto topology = builder.create_topology();
  auto source = topology->create_processor<mem_stream_source<void, bb_monitor::LogLine>>(0);
  auto sink = topology->create_sink<bb_log_sink>(source, channel, monitor_api_key, monitor_secret_access_key);

  std::signal(SIGINT, sigterm);
  std::signal(SIGTERM, sigterm);
  std::signal(SIGPIPE, SIG_IGN);

  topology->add_labels( {
                            { "app_name", SERVICE_NAME },
                            { "hostname", default_hostname() }
                        });

  topology->start(kspp::OFFSET_END); // has to be something - since we feed events from web totally irrelevant
  LOG(INFO) << "status: up";
  // output metrics and run...
  std::thread t([&]()
  {
    //auto metrics_reporter = std::make_shared<kspp::influx_metrics_reporter>(builder, "kspp_metrics", "kspp", "") << topology;
    while (run) {
      if (topology->process(kspp::milliseconds_since_epoch()) == 0) {
        std::this_thread::sleep_for(5000ms);
        topology->commit(false);
      }
    }
  });

  try
  {
    boost::asio::io_service io_service;
    server s(io_service, atoi(port.c_str()), source);
    io_service.run();
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  run = false;
  LOG(INFO) << "stopping work thread";
  t.join();
  LOG(INFO) << "status is down";
  topology->commit(true);
  topology->close();
  return 0;
}
