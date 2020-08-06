#include "service_resolver.h"
#include <iostream>
#include <future>
#include <boost/make_shared.hpp>
#include <boost/thread.hpp>
#include <glog/logging.h>


#ifndef T_A
#define T_A    1
#endif

#ifndef T_SRV
#define T_SRV    33
#endif

#ifndef C_IN
#define C_IN    1
#endif

//Size of an RR portion(type, class, lenght and ttl)
#ifndef RRFIXEDSZ
#define RRFIXEDSZ 10
#endif

// Packet size in bytes
#ifndef PACKETSZ
#define PACKETSZ 512
#endif

//Size of the header
#ifndef HFIXEDSZ
#define HFIXEDSZ 12
#endif

//Size of the question portion(type and class)
#ifndef QFIXEDSZ
#define QFIXEDSZ 4
#endif

using namespace std::chrono_literals;

const unsigned char *
parse_question(const unsigned char *cursor, const unsigned char *abuf, int alen, std::string &name) {
  //int type, dnsclass, status;
  long len;

  /* Parse the question name. */
  char *qname;
  int status = ares_expand_name(cursor, abuf, alen, &qname, &len);
  if (status != ARES_SUCCESS)
    return NULL;
  name.assign(qname);
  ares_free_string(qname);
  cursor += len;

  /* Make sure there's enough data after the name for the fixed part
  * of the question.
  */
  if (cursor + QFIXEDSZ > abuf + alen)
    return NULL;

  //type     = DNS_QUESTION_TYPE(aptr);
  //dnsclass = DNS_QUESTION_CLASS(aptr);
  cursor += QFIXEDSZ;
  return cursor;
}


const unsigned char *display_rr(const unsigned char *cursor, const unsigned char *abuf, int alen) {
  const unsigned char *p;
  long len;
  char addr[46];
  char *name = NULL;

  /* Parse the RR name. */
  int status = ares_expand_name(cursor, abuf, alen, &name, &len);
  if (status != ARES_SUCCESS)
    return NULL;
  cursor += len;

  /* Make sure there is enough data after the RR name for the fixed
  * part of the RR.
  */
  if (cursor + RRFIXEDSZ > abuf + alen) {
    ares_free_string(name);
    return NULL;
  }

  /* Parse the fixed part of the RR, and advance to the RR data
  * field. */
  int type = DNS_RR_TYPE(cursor);
  int dnsclass = DNS_RR_CLASS(cursor);
  int ttl = DNS_RR_TTL(cursor);
  int dlen = DNS_RR_LEN(cursor);
  cursor += RRFIXEDSZ;
  if (cursor + dlen > abuf + alen) {
    ares_free_string(name);
    return NULL;
  }

  /* Display the RR name, class, and type. */
  //printf("\t%-15s.\t%d", name.as_char, ttl);
  //if (dnsclass != C_IN)
  //    printf("\t%s", class_name(dnsclass));
  //printf("\t%s", type_name(type));
  //ares_free_string(name.as_char);

  /* Display the RR data.  Don't touch aptr. */
  switch (type) {
    case T_A:
      /* The RR data is a four-byte Internet address. */
      if (dlen != 4)
        return NULL;
      printf("\t%s", ares_inet_ntop(AF_INET, cursor, addr, sizeof(addr)));
      break;

    case T_SRV:
      /* The RR data is three two-byte numbers representing the
      * priority, weight, and port, followed by a domain name.
      */

      printf("\t%d", (int) DNS__16BIT(cursor));
      printf(" %d", (int) DNS__16BIT(cursor + 2));
      printf(" %d", (int) DNS__16BIT(cursor + 4));

      status = ares_expand_name(cursor + 6, abuf, alen, &name, &len);
      if (status != ARES_SUCCESS)
        return NULL;
      printf("\t%s.", name);
      ares_free_string(name);
      break;
    default:
      printf("\t[Unknown RR; cannot parse]");
      break;
  }
  printf("\n");

  return cursor + dlen;
}

static void destroy_addr_list(struct ares_addr_node *head) {
  while (head) {
    struct ares_addr_node *detached = head;
    head = head->next;
    free(detached);
  }
}

static void append_addr_list(struct ares_addr_node **head,
                             struct ares_addr_node *node) {
  struct ares_addr_node *last;
  node->next = NULL;
  if (*head) {
    last = *head;
    while (last->next) {
      last = last->next;
    }
    last->next = node;
  } else
    *head = node;
}

service_resolver::service_resolver(boost::asio::io_service &ios)
        : _ios(ios)
          , _is_init(false)
          , _time_to_die(false)
          , _thread(boost::bind(&service_resolver::run_task, this)) {
  int status = ares_library_init(ARES_LIB_INIT_ALL);
  if (status != ARES_SUCCESS) {
    LOG(INFO) << "ares_library_init failed: " << ares_strerror(status);
    return;
  }

  if ((status = ares_init(&_channel)) != ARES_SUCCESS) {
    LOG(ERROR) << "ares_init failed: " << ares_strerror(status);
    return;
  }

  _is_init = true;
}

service_resolver::~service_resolver() {
  close();
  ares_destroy(_channel);
}

void service_resolver::close() {
  if (_thread.joinable()) {
    _time_to_die = true;
    _thread.join();
  }
}

bool service_resolver::set_dns_server(const std::string &dns_server) {
  int optmask = ARES_OPT_FLAGS;

  struct ares_options options;
  options.flags = ARES_FLAG_NOCHECKRESP;
  options.servers = NULL;
  options.nservers = 0;

  int status = ares_init_options(&_channel, &options, optmask);

  if (status != ARES_SUCCESS) {
    LOG(ERROR) << "ares_init_options failed: " << ares_strerror(status);
    return false;
  }

  struct in_addr ip;
  int res;

  struct ares_addr_node *srvr, *servers = NULL;
  struct hostent *hostent;

  /* User-specified name servers override default ones. */
  srvr = (ares_addr_node *) malloc(sizeof(struct ares_addr_node));
  if (!srvr) {
    LOG(FATAL) << "Out of memory";
    destroy_addr_list(servers);
    return false;
  }
  append_addr_list(&servers, srvr);
  if (ares_inet_pton(AF_INET, dns_server.c_str(), &srvr->addr.addr4) > 0)
    srvr->family = AF_INET;
    //else if (ares_inet_pton(AF_INET6, dns_server, &srvr->addr.addr6) > 0)
    //    srvr->family = AF_INET6;
  else {
    hostent = gethostbyname(dns_server.c_str());
    if (!hostent) {
      LOG(ERROR) << "gethostbyname, server: " << dns_server << " not found";
      destroy_addr_list(servers);
      return false;
    }
    switch (hostent->h_addrtype) {
      case AF_INET:
        srvr->family = AF_INET;
        memcpy(&srvr->addr.addr4, hostent->h_addr, sizeof(srvr->addr.addr4));
        break;
      case AF_INET6:
        srvr->family = AF_INET6;
        memcpy(&srvr->addr.addr6, hostent->h_addr, sizeof(srvr->addr.addr6));
        break;
      default:
        fprintf(stderr, "adig: server %s unsupported address family.\n", dns_server.c_str());
        destroy_addr_list(servers);
        return false;
    }
  }
  /* Notice that calling ares_init_options() without servers in the
  * options struct and with ARES_OPT_SERVERS set simultaneously in
  * the options mask, results in an initialization with no servers.
  * When alternative name servers have been specified these are set
  * later calling ares_set_servers() overriding any existing server
  * configuration. To prevent initial configuration with default
  * servers that will be discarded later, ARES_OPT_SERVERS is set.
  * If this flag is not set here the result shall be the same but
  * ares_init_options() will do needless work. */
  //optmask |= ARES_OPT_SERVERS;

  if (servers) {
    res = ares_set_servers(_channel, servers);
    destroy_addr_list(servers);
    if (res != ARES_SUCCESS) {
      LOG(ERROR) << "ares_set_servers failed: " << ares_strerror(res);
      return false;
    }
  }
  return true;
}

void
service_resolver::ares_srv_callback(std::shared_ptr<service_resolver::srv_query_context> qc, int status, int timeouts,
                                    unsigned char *abuf, int alen) {
  std::vector<srv_record> v;

  if (status != ARES_SUCCESS) {
    printf("%s\n", ares_strerror(status));
    do_cb(qc, status, v);
    return;
  }

  /* Parse the answer header. */
  int id = DNS_HEADER_QID(abuf);
  int qr = DNS_HEADER_QR(abuf);
  int opcode = DNS_HEADER_OPCODE(abuf);
  int aa = DNS_HEADER_AA(abuf);
  int tc = DNS_HEADER_TC(abuf);
  int rd = DNS_HEADER_RD(abuf);
  int ra = DNS_HEADER_RA(abuf);
  int rcode = DNS_HEADER_RCODE(abuf);
  unsigned int qdcount = DNS_HEADER_QDCOUNT(abuf);
  unsigned int ancount = DNS_HEADER_ANCOUNT(abuf);
  unsigned int nscount = DNS_HEADER_NSCOUNT(abuf);
  unsigned int arcount = DNS_HEADER_ARCOUNT(abuf);

  const unsigned char *cursor = abuf + HFIXEDSZ;

  //we do not need to display the data but we need to trase the data to be able to advance the cursor

  /* parse the questions */
  for (int i = 0; i < qdcount; i++) {
    std::string qname;
    cursor = parse_question(cursor, abuf, alen, qname);
    //std::cerr << "qname: " << qname << std::endl;
    if (cursor == NULL) {
      do_cb(qc, status, v);
      return;
    }
  }

  /* Display the answers. */
  for (int i = 0; i < ancount; i++) {
    const unsigned char *p;
    long len;
    char addr[46];
    char *name = NULL;

    /* Parse the RR name. */
    int status = ares_expand_name(cursor, abuf, alen, &name, &len);
    if (status != ARES_SUCCESS) {
      do_cb(qc, status, v);
      return;
    }

    cursor += len;

    /* Make sure there is enough data after the RR name for the fixed
    * part of the RR.
    */
    if (cursor + RRFIXEDSZ > abuf + alen) {
      ares_free_string(name);
      {
        do_cb(qc, -1, v); // TBD return code
        return;
      }
    }

    /* Parse the fixed part of the RR, and advance to the RR data
    * field. */
    int type = DNS_RR_TYPE(cursor);
    int dnsclass = DNS_RR_CLASS(cursor);
    int ttl = DNS_RR_TTL(cursor);
    int dlen = DNS_RR_LEN(cursor);
    cursor += RRFIXEDSZ;
    if (cursor + dlen > abuf + alen) {
      ares_free_string(name);
      do_cb(qc, -1, v); // TBD return code
      return;
    }

    if (type != T_SRV) {
      do_cb(qc, -1, v); // TBD return code
      return;
    }

    /* The RR data is three two-byte numbers representing the
    * priority, weight, and port, followed by a domain name.
    */

    srv_record rec;

    rec.prio = DNS__16BIT(cursor);
    rec.weight = DNS__16BIT(cursor + 2);
    rec.port = DNS__16BIT(cursor + 4);
    status = ares_expand_name(cursor + 6, abuf, alen, &name, &len);
    if (status != ARES_SUCCESS) {
      ares_free_string(name);
      do_cb(qc, status, v);
      return;
    }
    rec.fqdn = name;
    v.push_back(rec);
    //printf("\t%s.", name);
    ares_free_string(name);
    cursor += dlen;
  }
  do_cb(qc, 0, v);
}

void service_resolver::do_cb(std::shared_ptr<service_resolver::srv_query_context> qc, int ec, std::vector<srv_record> v) {
  _ios.post([this, qc, ec, v]() {
    qc->cb(ec, v);
  });
}

void service_resolver::query_srv(const std::string &query, query_srv_callback cb) {
  srv_query_context *qc = new srv_query_context();
  qc->_this = this;
  qc->cb = cb;
  qc->query = query;
  boost::mutex::scoped_lock lock(_io_mutex);
  ares_query(_channel, query.c_str(), C_IN, T_SRV, _srv_callback, qc);
}

std::pair<int, std::vector<service_resolver::srv_record>> service_resolver::query_srv(const std::string &query) {
  std::promise<std::pair<int, std::vector<service_resolver::srv_record>>> p;
  std::future<std::pair<int, std::vector<service_resolver::srv_record>>> f = p.get_future();
  query_srv(query, [&p](int ec, std::vector<srv_record> result) {
    p.set_value(std::make_pair(ec, result));
  });
  f.wait();
  return f.get();
}

void service_resolver::ares_hostbyname_callback(std::shared_ptr<service_resolver::hostbyname_query_context> qc
        , int status
        , int timeouts
        , struct hostent *host) {
  std::vector<std::string> v;

  if (!host || status != ARES_SUCCESS) {
    LOG(ERROR) << "Failed to lookup " << ares_strerror(status);
    _ios.post([this, qc, status, v]() {
      qc->cb(status, v);
    });
    return;
  }


  char ip[INET6_ADDRSTRLEN];
  int i = 0;

  for (i = 0; host->h_addr_list[i]; ++i) {
    inet_ntop(host->h_addrtype, host->h_addr_list[i], ip, sizeof(ip));
    v.push_back(ip);
    //printf("%s\n", ip);
  }

  //DLOG(INFO) << "Found address name: " << host->h_name;

  _ios.post([this, qc, status, v]() {
    qc->cb(status, v);
  });
}

void service_resolver::get_host_by_name(const std::string &hostname, hostbyname_callback cb) {
  hostbyname_query_context *qc = new hostbyname_query_context();
  qc->_this = this;
  qc->cb = cb;
  qc->query = hostname;
  boost::mutex::scoped_lock lock(_io_mutex);
  ares_gethostbyname(_channel, qc->query.c_str(), AF_INET, _ares_callback, qc);
}

std::pair<int, std::vector<std::string>> service_resolver::get_host_by_name(const std::string &hostname) {
  std::promise<std::pair<int, std::vector<std::string>>> p;
  std::future<std::pair<int, std::vector<std::string>>> f = p.get_future();
  get_host_by_name(hostname, [&p](int ec, std::vector<std::string> result) {
    p.set_value(std::make_pair(ec, result));
  });
  f.wait();
  return f.get();
}

void service_resolver::get_sockaddr_by_srv(const std::string &query, srvname2sockaddr_callback cb) {
  query_srv(query, [this, cb](int ec, std::vector<srv_record> v) {
    if (ec || v.size() == 0) {
      cb(ec, std::vector<boost::asio::ip::tcp::endpoint>());
      return;
    }
    int port = v[0].port; // we only resolve the first one... TBD otherwise we need a waterfall here...
    get_host_by_name(v[0].fqdn, [port, cb](int ec, std::vector<std::string> v) {
      if (ec) {
        cb(ec, std::vector<boost::asio::ip::tcp::endpoint>());
        return;
      }
      std::vector<boost::asio::ip::tcp::endpoint> res;
      for (std::vector<std::string>::const_iterator i = v.begin(); i != v.end(); ++i) {
        res.push_back(boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string(*i), port));
      }
      cb(ec, res);
    });
  });
}

std::pair<int, std::vector<boost::asio::ip::tcp::endpoint>>
service_resolver::get_sockaddr_by_srv(const std::string &query) {
  std::promise<std::pair<int, std::vector<boost::asio::ip::tcp::endpoint>>> p;
  std::future<std::pair<int, std::vector<boost::asio::ip::tcp::endpoint>>> f = p.get_future();
  get_sockaddr_by_srv(query, [&p](int ec, std::vector<boost::asio::ip::tcp::endpoint> result) {
    p.set_value(std::make_pair(ec, result));
  });
  f.wait();
  return f.get();
}

void service_resolver::get_sockaddr_by_name(const std::string &query, srvname2sockaddr_callback cb) {
  if (query.compare(0, 6, "srv://") == 0) {
    get_sockaddr_by_srv(query.substr(6), cb);
    return;
  } else {
    std::size_t pos = query.find(":");

    std::string port_s = query.substr(pos + 1);
    int port = atoi(port_s.c_str());
    if (pos == std::string::npos || port <= 0) {
      LOG(ERROR) << "get_sockaddr_by_name bad parameters " << query;
      _ios.post([this, cb]() { cb(-1, std::vector<boost::asio::ip::tcp::endpoint>()); });
      return;
    }

    if (pos != std::string::npos) {
      get_host_by_name(query.substr(0, pos), [cb, port](int ec, std::vector<std::string> v) {
        if (ec) {
          cb(ec, std::vector<boost::asio::ip::tcp::endpoint>());
          return;
        }
        std::vector<boost::asio::ip::tcp::endpoint> res;
        for (std::vector<std::string>::const_iterator i = v.begin(); i != v.end(); ++i) {
          res.push_back(boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string(*i), port));
        }
        cb(ec, res);
      });
    }
  }
}

std::pair<int, std::vector<boost::asio::ip::tcp::endpoint>>
service_resolver::get_sockaddr_by_name(const std::string &query) {
  std::promise<std::pair<int, std::vector<boost::asio::ip::tcp::endpoint>>> p;
  std::future<std::pair<int, std::vector<boost::asio::ip::tcp::endpoint>>> f = p.get_future();
  get_sockaddr_by_name(query, [&p](int ec, std::vector<boost::asio::ip::tcp::endpoint> result) {
    p.set_value(std::make_pair(ec, result));
  });
  f.wait();
  return f.get();
}

//this is just a quick and dirty for not bothering to do a correct boost asio thing here...
//we should probably cache resolves for some time and also do a normal ioloop here
void service_resolver::run_task() {
  while (!_is_init) {
    std::this_thread::sleep_for(100ms);
  }
  int nfds, count;
  fd_set readers, writers;
  timeval tv, *tvp;
  while (!_time_to_die) {
    while (1) {
      FD_ZERO(&readers);
      FD_ZERO(&writers);
      {
        boost::mutex::scoped_lock lock(_io_mutex);
        nfds = ares_fds(_channel, &readers, &writers);
      }
      if (nfds == 0)
        break;
      tvp = ares_timeout(_channel, NULL, &tv);
      count = select(nfds, &readers, &writers, NULL, tvp);
      {
        boost::mutex::scoped_lock lock(_io_mutex);
        ares_process(_channel, &readers, &writers);
      }
    }
    std::this_thread::sleep_for(15ms);
  }
}
