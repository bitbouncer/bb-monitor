#include <memory>
#include <iostream>
#include <string>
#include <thread>
#include <grpc++/grpc++.h>
#include <glog/logging.h>
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
using bb_monitor::Intake;
using bb_monitor::PutIntakeResponse;

class put_intake_call_data : public call_data {

public:
  // Take in the "service" instance (in this case representing an asynchronous
  // server) and the completion queue "cq" used for asynchronous communication
  // with the gRPC runtime.std::shared_ptr<kafka_sink_factory>
  put_intake_call_data(bb_monitor::MonitorSink::AsyncService* service, grpc::ServerCompletionQueue* cq, std::shared_ptr<kafka_sink_factory> factory)
      : service_(service)
      , cq_(cq)
      , responder_(&ctx_)
      , status_(START)
      , _sink_factory(factory)
      , _msg_count(0)
      , _client_id(-1) {
    // Invoke the serving logic right away.
    status_ = START;
    service_->RequestPutIntake(&ctx_, &responder_, cq_, cq_, this);
  }

  ~put_intake_call_data(){
    if (_sink)
      LOG(INFO) << "intake rpc done, client: " << _sink->client_id() << ", source: " << _source_ip << ", nr_of_msg: " << _msg_count;

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
        new put_intake_call_data(service_, cq_, _sink_factory);

        std::string api_key = get_api_key(ctx_);
        std::string api_secret = get_api_secret(ctx_);
        _source_ip = get_forwarded_for(ctx_);
        //LOG(INFO) << "headers: " << get_metadata(ctx_);
        _sink = _sink_factory->get_intake_sink(api_key, api_secret);

        if (_sink == nullptr){
          LOG(WARNING) << "PERMISSION_DENIED topic: intake, api_key: " << api_key << ", ip: " << _source_ip;
          Status authentication_error(grpc::StatusCode::UNAUTHENTICATED, "authentication failed for key: " + api_key);
          responder_.FinishWithError(authentication_error, this);
          status_ = FINISH;
          return;
        }

        LOG(INFO) << "rpc start, client: " <<  _sink->client_id() << " topic: " << _sink->topic() << ", ip: " << _source_ip;

        _sink->_active_session_count.incr(1.0);
        ++_sink->_total_session_count;

        // the event.
        status_ = READING;
        responder_.Read(&request_, this);
      }
        break;

      case READING: {
        size_t sz = request_.intake_size();
        for (size_t i =0; i!=sz; ++i){
          const auto& intake = request_.intake(i);
          dd_avro_intake_t ai;
          ai.agent = intake.agent();
          ai.data = intake.data();
          ai.timestamp = intake.timestamp();
          _sink->push_back("",  ai, kspp::milliseconds_since_epoch()); // or should be use collection ts?
          }
          ++_msg_count;
        responder_.Read(&request_, this);
      }
        break;

      case  READING_FAILED: {
        //LOG(INFO) << "write response";
        status_ = FINISH;
        reply_.set_count(_msg_count);
        responder_.Finish(reply_, grpc::Status(), this);
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

  int _client_id;

  // What we get from the client.
  bb_monitor::IntakeBundle request_;
  // What we send back to the client.
  std::string _source_ip;
  int64_t _msg_count;
  bb_monitor::PutIntakeResponse reply_;

  // The means to get back to the client.
  ::grpc::ServerAsyncReader<bb_monitor::PutIntakeResponse, bb_monitor::IntakeBundle> responder_;

  CallStatus status_;  // The current serving state.
  std::shared_ptr<kafka_sink_factory> _sink_factory;
  std::shared_ptr<bb::bb_kafka_sink<std::string, dd_avro_intake_t>> _sink;
};