#include <grpcpp/grpcpp.h>
#include <google/protobuf/repeated_field.h>
#include <google/protobuf/map.h>
#pragma once

void bb_set_channel_args(grpc::ChannelArguments& channelArgs);
void bb_set_channel_args(grpc::ServerBuilder& serverBuilder);

inline int64_t milliseconds_since_epoch() {
  return std::chrono::duration_cast<std::chrono::milliseconds>
      (std::chrono::system_clock::now().time_since_epoch()).count();
}

std::string to_pg(const ::google::protobuf::RepeatedPtrField<::std::string>& v);
std::string to_pg(const ::google::protobuf::Map< ::std::string, ::std::string >& m);

void pg2proto(std::string s, google::protobuf::RepeatedPtrField<::std::string>* p);
void pg2proto(std::string s, google::protobuf::Map< ::std::string, ::std::string >* p);

std::string to_json(const ::google::protobuf::RepeatedPtrField<::std::string>& v);
std::string to_json(const ::google::protobuf::Map< ::std::string, ::std::string >& m);
