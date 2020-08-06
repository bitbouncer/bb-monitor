#include <memory>
#include <iostream>
#include <string>
#include <thread>
#include <grpc++/grpc++.h>
#include <glog/logging.h>
//#include <kspp/impl/sources/kafka_consumer.h>
#include <kspp/cluster_config.h>
#include <bb_monitor_sink.grpc.pb.h>
#include <bb_monitor_srv_utils/grpc_topic_authorizer.h>
#include <bb_monitor_srv_utils/grpc_apikey_auth.h>
#include "kafka_sink_factory.h"
#include "call_data.h"
#pragma once

using namespace std::chrono_literals;

using grpc::Server;
using grpc::ServerAsyncResponseWriter;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerCompletionQueue;
using grpc::Status;


using bb_monitor::MonitorSink;
using bb_monitor::Metric;
using bb_monitor::PutMetricsResponse;
class put_metrics_call_data : public call_data {

public:
  // Take in the "service" instance (in this case representing an asynchronous
  // server) and the completion queue "cq" used for asynchronous communication
  // with the gRPC runtime.std::shared_ptr<kafka_sink_factory>
  put_metrics_call_data(bb_monitor::MonitorSink::AsyncService* service, grpc::ServerCompletionQueue* cq, std::shared_ptr<kafka_sink_factory> factory)
      : service_(service)
      , cq_(cq)
      , stream_(&ctx_)
      , status_(START)
      , _sink_factory(factory)
      , _msg_count(0) {
    // Invoke the serving logic right away.
    status_ = START;
    service_->RequestPutMetrics(&ctx_, &stream_, cq_, cq_, this);
  }

  ~put_metrics_call_data(){
    if (_sink)
      LOG(INFO) << "metrics rpc done, client: " << _sink->client_id() << ", source: " << _source_ip << ", nr_of_msg: " << _msg_count;

    if (_sink)
      _sink->_active_session_count.decr(1.0);
  }

  void set_state(CallStatus e) override {
    status_ = e;
  }

  CallStatus get_state() override {
    return status_;
  }

  void set_error() override {
    status_ = FINISH;

    // is this true??
    if (_sink)
      ++_sink->_timeout_count;

    LOG(INFO) << "set-error";
  }

  void process_state() override {
    switch (status_) {
      case START: {
        // Spawn a new CallData instance to serve new clients while we process
        // the one for this CallData. The instance will deallocate itself as
        // part of its FINISH state.
        new put_metrics_call_data(service_, cq_, _sink_factory);

        std::string api_key = get_api_key(ctx_);
        std::string api_secret = get_api_secret(ctx_);
        _source_ip = get_forwarded_for(ctx_);
        //LOG(INFO) << "headers: " << get_metadata(ctx_);
        _sink = _sink_factory->get_metrics_sink(api_key, api_secret);

        if (_sink == nullptr){
          LOG(WARNING) << "PERMISSION_DENIED topic: metrics, api_key: " << api_key << ", ip: " << _source_ip;
          Status authentication_error(grpc::StatusCode::UNAUTHENTICATED, "authentication failed for key: " + api_key);
          stream_.FinishWithError(authentication_error, this);
          status_ = FINISH;
          return;
        }

        LOG(INFO) << "rpc start, client: " <<  _sink->client_id() << " topic: " << _sink->topic() << ", ip: " << _source_ip;

        _sink->_active_session_count.incr(1.0);
        ++_sink->_total_session_count;

        // the event.
        status_ = READING;
        stream_.Read(&request_, this);
      }
        break;

      case READING: {
        size_t sz = request_.metrics_size();
        for (size_t i =0; i!=sz; ++i){
          const auto& metric = request_.metrics(i);
          bb_avro_metric_t m;
          m.ns = metric.ns();
          m.name = metric.name();
          for (int i = 0; i != metric.labels_size(); ++i) {
            bb_avro_metric_label_t label;
            const auto &l = metric.labels(i);
            label.key = l.key();
            label.value = l.value();
            m.labels.push_back(label);
          }

          m.timestamp = metric.timestamp();

          auto s = metric.sample();
          switch (s.MeasurementOneof_case()) {
            case bb_monitor::Measurement::kLongValue:
              m.value.set_long(s.longvalue());
              _sink->push_back(m.ns + m.name, m);
              break;
            case bb_monitor::Measurement::kDoubleValue:
              m.value.set_double(s.doublevalue());
              _sink->push_back(m.ns + m.name, m);
              break;
            case bb_monitor::Measurement::kLatLonValue: {
              const auto &pbpos = s.latlonvalue();
              bb_avro_metric_pos_t avropos;
              avropos.lat = pbpos.latitude();
              avropos.lon = pbpos.longitude();
              m.value.set_bb_avro_metric_pos_t(avropos);
              _sink->push_back(m.ns + m.name, m);
            }
              break;
            default:
              break;
          }
          ++_msg_count;
        }

        stream_.Read(&request_, this);
      }
        break;

      case  READING_FAILED: {
        //LOG(INFO) << "write response";
        status_ = FINISH;
        reply_.set_count(_msg_count);
        stream_.Finish(reply_, grpc::Status(), this);
      }
        break;

      case FINISH:
      default:{
        //LOG(INFO) << "finish response callback";
        GPR_ASSERT(status_ == FINISH);
        // Once in the FINISH state, deallocate ourselves (CallData).
        delete this;
        return;
      }
        break;
    }
  }

private:
  // The means of communication with the gRPC runtime for an asynchronous
  // server.
  bb_monitor::MonitorSink::AsyncService* service_;
  // The producer-consumer queue where for asynchronous server notifications.
  grpc::ServerCompletionQueue* cq_;
  // Context for the rpc, allowing to tweak aspects of it such as the use
  // of compression, authentication, as well as to send metadata back to the
  // client.
  ServerContext ctx_;

  // What we get from the client.
  bb_monitor::MetricsBundle request_;
  // What we send back to the client.
  std::string _source_ip;
  int64_t _msg_count;
  bb_monitor::PutMetricsResponse reply_;

  // The means to get back to the client.
  ::grpc::ServerAsyncReader<bb_monitor::PutMetricsResponse, bb_monitor::MetricsBundle> stream_;

  CallStatus status_;  // The current serving state.
  std::shared_ptr<kafka_sink_factory> _sink_factory;
  std::shared_ptr<bb::bb_kafka_sink<std::string, bb_avro_metric_t>> _sink;
};