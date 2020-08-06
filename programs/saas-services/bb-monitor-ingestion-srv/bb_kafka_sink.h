#include <assert.h>
#include <memory>
#include <functional>
#include <sstream>
#include <kspp/sinks/sink_defs.h>
#include <kspp/internal/event_queue.h>
#include <prometheus/registry.h>
#include <prometheus/gateway.h>
#include "bb_kafka_producer.h"

#pragma once
using namespace std::chrono_literals;

namespace bb {
  template<class K, class V>
  class bb_kafka_sink  {
    static constexpr const char* PROCESSOR_NAME = "bb_kafka_sink";
  public:
    enum { MAX_KEY_SIZE = 1000 };

    static std::string pushgateway_hostname_part(std::string s){
      return s.substr(0, s.find(":"));
    }

    static std::string pushgateway_port_part(std::string s){
      auto i = s.find(":");
      if (i==std::string::npos)
        return "9091";
      return s.substr(i+1);
    }

    bb_kafka_sink(std::shared_ptr<kspp::cluster_config> config,
                  std::string topic, int customer_id, std::map<std::string, std::string> labels)
        : _run(true)
        , _customer_id(customer_id)
        , _key_codec(config->avro_serdes())
        , _val_codec(config->avro_serdes())
        , _key_schema_id(-1)
        , _val_schema_id(-1)
        , _impl(config, topic)
        , _prometheus_registry(std::make_shared<prometheus::Registry>())
        , _gateway(pushgateway_hostname_part(config->get_pushgateway_uri()), pushgateway_port_part(config->get_pushgateway_uri()), "bb_monitor")
        , _inbound_queue_size("inbound_queue_size", "msg", labels, _prometheus_registry)
        , _outbound_queue_size("outbound_queue_size", "msg", labels, _prometheus_registry)
        , _kafka_processed_count("processed", "msg", labels, _prometheus_registry)
        , _active_session_count("active_sessions", "count", labels, _prometheus_registry)
        , _total_session_count("total_sessions", "count", labels, _prometheus_registry)
        , _timeout_count("timeout_count", "count", labels, _prometheus_registry)
        , _bg_thread([this]() { bg_thread(); }) {
      _gateway.RegisterCollectable(_prometheus_registry);
    }

    ~bb_kafka_sink(){
      _run = false;
      close();
      _bg_thread.join();
    }

    std::string log_name() const {
      return PROCESSOR_NAME;
    }

    std::string topic() const {
      return _impl.topic();
    }

    void close() {
      flush();
      return _impl.close();
    }

    size_t queue_size() const {
      return _queue.size() + _impl.queue_size();
    }

    inline void push_back(const K &key, const V &value, int64_t ts = kspp::milliseconds_since_epoch()) {
      this->_queue.push_back(std::make_shared<kspp::kevent<K,V>>(std::make_shared <kspp::krecord < K, V >> (key, value, ts)));
    }

    void bg_thread() {
      LOG(INFO) << "bg_thread init";
      while (_queue.size()==0 && _run) {
        std::this_thread::sleep_for(500ms);
      }
      LOG(INFO) << "bg_thread starting";

      int64_t next_time_to_send_metrics = kspp::milliseconds_since_epoch() + 10 * 1000;

      while (_run) {
        poll(0);
        auto count = process();
        if (count==0)
          std::this_thread::sleep_for(100ms);

        if (next_time_to_send_metrics <= kspp::milliseconds_since_epoch()) {
          _inbound_queue_size.set(_queue.size());
          _outbound_queue_size.set(_impl.queue_size());
          //LOG(INFO) << " before pushgateway send";
          _gateway.Push();
          //LOG(INFO) << " after pushgateway send";
          //schedule nex reporting event
          next_time_to_send_metrics += 10000;
          // if we are reaaly out of sync lets sleep at least 10 more seconds
          if (next_time_to_send_metrics <= kspp::milliseconds_since_epoch())
            next_time_to_send_metrics = kspp::milliseconds_since_epoch() + 10000;
        }
      }
      LOG(INFO) << "bg_thread exiting";
    }


    void poll(int timeout) {
      return _impl.poll(timeout);
    }

    void flush() {
      while (!eof()) {
        process();
        poll(0);
      }
      while (true) {
        auto ec = _impl.flush(1000);
        if (ec == 0)
          break;
      }
    }

    bool eof() const {
      return this->_queue.size() == 0;
    }

    int handle_event(const kspp::krecord<K, V>& record) {
      // first time??
      // register schemas under the topic-key, topic-value name to comply with kafka-connect behavior
      if (this->_key_schema_id<0) {
        this->_key_schema_id = this->_key_codec->register_schema(this->topic() + "-key", record.key());
        LOG_IF(FATAL, this->_key_schema_id<0) << "Failed to register schema - aborting";
      }

      if (this->_val_schema_id<0 && record.value()) {
        this->_val_schema_id = this->_val_codec->register_schema(this->topic() + "-value", *record.value());
        LOG_IF(FATAL, this->_val_schema_id<0) << "Failed to register schema - aborting";
      }

      uint32_t partition_hash = kspp::get_partition_hash(record.key(), _key_codec);

      void *kp = nullptr;
      void *vp = nullptr;
      size_t ksize = 0;
      size_t vsize = 0;

      std::stringstream ks;
      ksize = this->_key_codec->encode(record.key(), ks);
      kp = malloc(ksize);  // must match the free in kafka_producer TBD change to new[] and a memory pool
      ks.read((char *) kp, ksize);

      if (record.value()) {
        std::stringstream vs;
        vsize = this->_val_codec->encode(*record.value(), vs);
        vp = malloc(vsize);   // must match the free in kafka_producer TBD change to new[] and a memory pool
        vs.read((char *) vp, vsize);
      }

      return this->_impl.produce(partition_hash, bb::kafka_producer::FREE, kp, ksize, vp, vsize, record.event_time());
    }

    // lets try to get as much as possible from queue to librdkafka - stop when queue is empty or librdkafka fails
    size_t process() {
      size_t count = 0;
      while (this->_queue.size()) {
        int ec = handle_event(*this->_queue.front()->record());
        if (ec == 0) {
          ++count;
          ++_kafka_processed_count;
          // ADD LAG
          _queue.pop_front();
          continue;
        } else if (ec == RdKafka::ERR__QUEUE_FULL) {
          // expected and retriable
          return count;
        } else  {
          LOG(ERROR) << "other error from rd_kafka ec:" << ec;
          // permanent failure - need to stop TBD
          return count;
        }
      } // while
      return count;
    }

   int32_t client_id(){
     return _customer_id;
    }

  protected:
    bool _run;
    const int32_t _customer_id;
    kspp::event_queue<K, V> _queue;
    std::shared_ptr<kspp::avro_serdes> _key_codec;
    std::shared_ptr<kspp::avro_serdes> _val_codec;
    int32_t _key_schema_id;
    int32_t _val_schema_id;
    bb::kafka_producer _impl;
    std::shared_ptr<prometheus::Registry> _prometheus_registry;
    prometheus::Gateway _gateway;
    kspp::metric_counter _kafka_processed_count;
  public: // this is ugly -- add public api for this (used from grpc hander)
    kspp::metric_gauge _inbound_queue_size;
    kspp::metric_gauge _outbound_queue_size;
    kspp::metric_gauge _active_session_count;
    kspp::metric_counter _total_session_count;
    kspp::metric_counter _timeout_count;
  private:
    std::thread _bg_thread;
  };
}


template<class K, class V>
void insert(bb::bb_kafka_sink<K, V>& sink, const K &k, const V &v, int64_t ts = kspp::milliseconds_since_epoch()){
  sink.push_back(k, v, ts);
}
