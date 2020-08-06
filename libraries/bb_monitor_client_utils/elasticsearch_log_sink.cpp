#include "elasticsearch_log_sink.h"
#include <bb_monitor_utils/time.h>

// "rancher/calico-node@sha256:2f46157483904649f334da83e6b5e522bf20cbd3ea6ec9f43456673933021d5a/k8s_calico-node_canal-5z9cm_kube-system_d899e3a6-8b8c-11e9-9185-086266e237a8_4/e6e5e52ad403";
int split_splunk_tag(std::string s, std::string& container_ns, std::string& container_name){
  container_ns="";
  container_name="";

  size_t last = s.find_last_of('/');
  if (last == std::string::npos || last < 2)
    return -1;
  size_t second_last = s.find_last_of('/', last-1);

  // if there are no / the we do not have an image first
  if (second_last == std::string::npos)
    second_last = 0;

  size_t image_separator = s.find_first_of(":@");

  //if (second_last>0 && image_separator != std::string::npos)
  //  image_name = s.substr(0, image_separator);

  std::string_view container(s);
  // if we start with a slash we have to remove it.
  if (second_last)
    container = container.substr(second_last+1, last-second_last-1);
  else
    container = container.substr(0, last);

  // start with k8s_
  if (container.rfind("k8s_", 0) == 0) {
    size_t last_underscore_in_container = container.find_last_of('_');
    if (last_underscore_in_container == std::string::npos || last_underscore_in_container < 10)
      return -1;

    size_t second_last_underscore_in_container = container.find_last_of('_', last_underscore_in_container-1);
    if (second_last_underscore_in_container == std::string::npos || second_last_underscore_in_container < 1)
      return -1;

    size_t third_last_underscore_in_container = container.find_last_of('_', second_last_underscore_in_container-1);
    if (third_last_underscore_in_container == std::string::npos || third_last_underscore_in_container < 1)
      return -1;

    container_ns = container.substr(third_last_underscore_in_container+1, second_last_underscore_in_container-third_last_underscore_in_container-1);

    //size_t hyphen_after_name = container.find_last_of('-', third_last_underscore_in_container-1);
    //container_name = container.substr(0, hyphen_after_name);
    container_name = container.substr(0, third_last_underscore_in_container);
  } else {
    // this is normal docker without k8s
    container_name = container;
    return 0;
  }

  return 0;
}


namespace kspp {
  elasticsearch_log_sink::elasticsearch_log_sink(std::shared_ptr<cluster_config> config, std::string remote_write_url, std::string username, std::string password, size_t max_http_connection){
    _impl = std::make_shared<kspp::elasticsearch_producer<std::string, bb_avro_logline>>(
        remote_write_url,
        username,
        password,
        [](const std::string& key){
          return key;
        },
        [](const bb_avro_logline& value){

          std::string k8s_ns;
          std::string k8s_name;

          for (auto i : value.tags) {
            // if this is a splunk tag then we might find good stuff here
            if (i.key == "tag") {
              split_splunk_tag(i.value, k8s_ns, k8s_name);
            }
          }

          std::string result = "{";
          result += "\"timestamp\": \"" + date_from_nanos(std::chrono::nanoseconds(value.timestamp_ns))+ "\"";
          result += ", \"agent\": \"" + kspp::escape_json(value.agent) + "\"";
          result += ", \"host\": \"" + kspp::escape_json(value.host) + "\"";
          result += ", \"source\": \"" + kspp::escape_json(value.source) + "\"";
          result += ", \"line\": \"" + kspp::escape_json(value.line) + "\"";

          if (k8s_ns.size()){
            result += ", \"k8s_ns\": \"" + kspp::escape_json(k8s_ns) + "\"";
          }

          if (k8s_name.size()){
            result += ", \"k8s_name\": \"" + kspp::escape_json(k8s_name) + "\"";
          }

          for (auto i : value.tags){
            result += ", \"" + kspp::escape_json(i.key) +"\": \"" + kspp::escape_json(i.value) + "\"";
          }
          result += "}";
          //LOG(INFO) << result;
          return result;
        },
        max_http_connection);

    // debug test
    //std::string ns;
    //std::string name;
    //split_splunk_tag("bb-elk/1445e0d6e846", ns, name);
    //LOG(INFO) << "ns:" << ns << ", name " << name;

    //reduce number of logs
    _impl->set_batch_log_interval(1000);
    _impl->set_http_log_interval(100000);

    this->add_metrics_label(KSPP_PROCESSOR_TYPE_TAG, PROCESSOR_NAME);
    this->add_metrics_label("remote_write_url", remote_write_url);

    // register sub component metrics
    this->register_metrics(this);
  }

  elasticsearch_log_sink::~elasticsearch_log_sink(){
    close();
  }

  /*bool good() const override {
    return this->good();
  }*/

  std::string elasticsearch_log_sink::log_name() const  {
    return PROCESSOR_NAME;
  }

  bool elasticsearch_log_sink::good() const {
    return _impl->good();
  }

  void elasticsearch_log_sink::register_metrics(kspp::processor* parent){
    _impl->register_metrics(parent);
  }

  void elasticsearch_log_sink::close()  {
    if (!_exit) {
      _exit = true;
    }
    _impl->close();
  }

  bool elasticsearch_log_sink::eof() const  {
    return this->_queue.size()==0 && _impl->eof();
  }

  size_t elasticsearch_log_sink::queue_size() const  {
    return this->_queue.size();
  }

  size_t elasticsearch_log_sink::outbound_queue_len() const  {
    return this->_impl->queue_size();
  }

  int64_t elasticsearch_log_sink::next_event_time() const  {
    return this->_queue.next_event_time();
  }

  size_t elasticsearch_log_sink::process(int64_t tick)  {
    if (this->_queue.empty())
      return 0;
    size_t processed=0;
    while(!this->_queue.empty()) {
      auto p = this->_queue.front();
      if (p==nullptr || p->event_time() > tick)
        return processed;
      this->_queue.pop_front();
      _impl->insert(p);
      ++(this->_processed_count);
      ++processed;
      this->_lag.add_event_time(tick, p->event_time());
    }
    return processed;
  }

  std::string elasticsearch_log_sink::topic() const  {
    return _impl->topic();
  }

  void elasticsearch_log_sink::poll(int timeout)  {
    _impl->poll();
  }

  void elasticsearch_log_sink::flush()  {
    while (!eof()) {
      process(kspp::milliseconds_since_epoch());
      poll(0);
      std::this_thread::sleep_for(std::chrono::milliseconds(10)); // TODO the deletable messages should be deleted when poill gets called an not from background thread 3rd queue is needed...
    }

    while (true) {
      int ec=0; // TODO fixme
      //auto ec = _impl.flush(1000);
      if (ec == 0)
        break;
    }
  }
}
