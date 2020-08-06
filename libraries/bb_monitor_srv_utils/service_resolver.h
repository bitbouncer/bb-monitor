#include <utility>
#include <functional>
#include <thread>
#include <boost/asio.hpp>
#include <boost/thread/mutex.hpp>
#include <ares_dns.h>
#include <ares.h>

#pragma once

class service_resolver {
public:
  struct srv_record {
    uint16_t prio;
    uint16_t weight;
    std::string fqdn;
    uint16_t port;
  };

  typedef std::function<void(int ec, std::vector<srv_record>)> query_srv_callback;
  typedef std::function<void(int ec, std::vector<std::string>)> hostbyname_callback;       // should NOT be string
  typedef std::function<void(int ec, std::vector<boost::asio::ip::tcp::endpoint>)> srvname2sockaddr_callback; // should NOT be string

  service_resolver(boost::asio::io_service &ios);

  ~service_resolver();

  void close();

  bool set_dns_server(const std::string &address); // list of servers???
  void query_srv(const std::string &query, query_srv_callback cb); // timeout??
  std::pair<int, std::vector<srv_record>> query_srv(const std::string &query);

  void get_host_by_name(const std::string &hostname, hostbyname_callback);

  std::pair<int, std::vector<std::string>> get_host_by_name(const std::string &hostname);

  void get_sockaddr_by_srv(const std::string &query, srvname2sockaddr_callback);

  std::pair<int, std::vector<boost::asio::ip::tcp::endpoint>> get_sockaddr_by_srv(const std::string &query);

  void get_sockaddr_by_name(const std::string &query, srvname2sockaddr_callback);

  std::pair<int, std::vector<boost::asio::ip::tcp::endpoint>> get_sockaddr_by_name(const std::string &query);

  struct srv_query_context {
    service_resolver *_this;
    std::string query;
    query_srv_callback cb;
  };

  struct hostbyname_query_context {
    service_resolver *_this;
    std::string query;
    hostbyname_callback cb;
  };

  struct srvname2sockaddr_query_context {
    service_resolver *_this;
    std::string query;
    srvname2sockaddr_callback cb;
  };

  void run_task();

private:
  static void _srv_callback(void *arg, int status, int timeouts, unsigned char *abuf, int alen) {
    std::shared_ptr<service_resolver::srv_query_context> qc((service_resolver::srv_query_context *) arg);
    ((service_resolver *) (qc->_this))->ares_srv_callback(qc, status, timeouts, abuf, alen);
  }

  static void _ares_callback(void *arg, int status, int timeouts, struct hostent *host) {
    std::shared_ptr<service_resolver::hostbyname_query_context> qc((service_resolver::hostbyname_query_context *) arg);
    ((service_resolver *) (qc->_this))->ares_hostbyname_callback(qc, status, timeouts, host);
  }

  void
  ares_srv_callback(std::shared_ptr<service_resolver::srv_query_context> qc, int status, int timeouts, unsigned char *abuf,
                    int alen);

  void ares_hostbyname_callback(std::shared_ptr<service_resolver::hostbyname_query_context> qc, int status, int timeouts,
                                struct hostent *host);

  void do_cb(std::shared_ptr<service_resolver::srv_query_context> qc, int ec, std::vector<srv_record> v);

  boost::asio::io_service &_ios;
  ares_channel _channel;
  bool _is_init;
  bool _time_to_die;
  std::thread _thread;
  boost::mutex _io_mutex;
};
