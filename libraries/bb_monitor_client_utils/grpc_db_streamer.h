#include <string>
#include <kspp/kspp.h>
#include <kspp/topology_builder.h>
#include <kspp/processors/visitor.h>
#include <kspp/connect/bitbouncer/grpc_avro_source.h>

#pragma once

template<class K, class V>
class grpc_db_streamer {
public:
  typedef std::function<void(const kspp::krecord<K, V> &record)> extractor;

  grpc_db_streamer(std::shared_ptr<kspp::cluster_config> config, std::shared_ptr<kspp::offset_storage> offset_store,
                   std::shared_ptr<grpc::Channel> channel, std::string api_key, std::string secret_access_key,
                   std::string topic, extractor f)
      : _config(config), _builder(config), _channel(channel), _api_key(api_key), _secret_access_key(secret_access_key),
        _topic(topic), _f(f), _next_log(0), _next_commit(kspp::milliseconds_since_epoch() + 10000) {
    _main = _builder.create_topology();
    _main_source = _main->create_processor<kspp::grpc_avro_source<K, V>>(0, _topic, offset_store, _channel, _api_key,
                                                                         _secret_access_key);
    _main->create_processor<kspp::visitor<K, V>>(_main_source, [&](const auto &in) {
      _f(in);
    });
    _main->start(kspp::OFFSET_STORED);


    /*
     * _tail = _builder.create_topology();
    _tail_source = _tail->create_processor<kspp::grpc_avro_source<K, V>>(0, _topic, nullptr, _channel, _api_key, _secret_access_key);
    _tail->create_processor<kspp::visitor<K, V>>(_tail_source, [&](const auto &in) {
      _f(in);
    });
    _tail->start(kspp::OFFSET_END);
     */
  }

  int64_t process() {
    int64_t tail_sz = 0;
    if (_tail) {
      tail_sz = _tail->process(kspp::milliseconds_since_epoch());

      if (kspp::milliseconds_since_epoch() > _next_log) {
        _next_log = kspp::milliseconds_since_epoch() + 10000;
        LOG(INFO) << "tail: " << _tail_source->offset() << ", backfill: " << _main_source->offset() << ", remaining: "
                  << _tail_source->offset() - _main_source->offset();
      }

      if (_tail_source->offset() > 0) {
        if ((_tail_source->offset() - _main_source->offset()) < 10000) {
          LOG(INFO) << "backfill offset < 10000 behind - closing tail, tail: " << _tail_source->offset() << ", head "
                    << _main_source->offset();
          _tail->close();
          _tail.reset();
          tail_sz = 0;
        }
      }
    }

    auto sz = _main->process(kspp::milliseconds_since_epoch());

    if (kspp::milliseconds_since_epoch() > _next_commit) {
      _next_commit = kspp::milliseconds_since_epoch() + 10000;
      _main->commit(false);
    }

    return sz + tail_sz;
  }

private:
  void init() {
    LOG(INFO) << "init";
  }

  std::shared_ptr<kspp::cluster_config> _config;
  kspp::topology_builder _builder;
  std::string _uri;
  std::string _api_key;
  std::string _secret_access_key;
  std::string _topic;
  extractor _f;
  int64_t _next_log;
  int64_t _next_commit;
  std::shared_ptr<grpc::Channel> _channel;
  std::shared_ptr<kspp::topology> _main;
  std::shared_ptr<kspp::grpc_avro_source<K, V>> _main_source;
  std::shared_ptr<kspp::topology> _tail;
  std::shared_ptr<kspp::grpc_avro_source<K, V>> _tail_source;
};