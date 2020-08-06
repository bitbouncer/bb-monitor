#include "grpc_utils.h"
#include <boost/algorithm/string.hpp>

void bb_set_channel_args(grpc::ChannelArguments& channelArgs){
  channelArgs.SetInt(GRPC_ARG_HTTP2_BDP_PROBE, 1);

  channelArgs.SetInt(GRPC_ARG_KEEPALIVE_TIME_MS, 20000);
  channelArgs.SetInt(GRPC_ARG_KEEPALIVE_TIMEOUT_MS, 20000);
  channelArgs.SetInt(GRPC_ARG_KEEPALIVE_PERMIT_WITHOUT_CALLS, 1);

  channelArgs.SetInt(GRPC_ARG_HTTP2_MAX_PINGS_WITHOUT_DATA, 0); // unlimited
  //channelArgs.SetInt(GRPC_ARG_HTTP2_MIN_RECV_PING_INTERVAL_WITHOUT_DATA_MS, 5000); // not applicable for client
  channelArgs.SetInt(GRPC_ARG_HTTP2_MIN_SENT_PING_INTERVAL_WITHOUT_DATA_MS, 20000);
}

void bb_set_channel_args(grpc::ServerBuilder& serverBuilder){
  serverBuilder.AddChannelArgument(GRPC_ARG_HTTP2_BDP_PROBE, 1);
  serverBuilder.AddChannelArgument(GRPC_ARG_KEEPALIVE_TIME_MS, 20000);
  //serverBuilder.AddChannelArgument(GRPC_ARG_KEEPALIVE_TIME_MS, 3600000);  // 1h
  serverBuilder.AddChannelArgument(GRPC_ARG_KEEPALIVE_TIMEOUT_MS, 20000); // wait 10 s for reply
  serverBuilder.AddChannelArgument(GRPC_ARG_KEEPALIVE_PERMIT_WITHOUT_CALLS, 1);

  serverBuilder.AddChannelArgument(GRPC_ARG_HTTP2_MAX_PINGS_WITHOUT_DATA, 0); // unlimited
  serverBuilder.AddChannelArgument(GRPC_ARG_HTTP2_MIN_RECV_PING_INTERVAL_WITHOUT_DATA_MS, 10000);
  serverBuilder.AddChannelArgument(GRPC_ARG_HTTP2_MIN_SENT_PING_INTERVAL_WITHOUT_DATA_MS, 10000);
  serverBuilder.AddChannelArgument(GRPC_ARG_HTTP2_MAX_PING_STRIKES, 0); // unlimited

  //serverBuilder.SetDefaultCompressionLevel(GRPC_COMPRESS_LEVEL_MED);
}

// trim from start (in place)
static inline void ltrim(std::string &s) {
  s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) {
    return !std::isspace(ch);
  }));
}

// trim from end (in place)
static inline void rtrim(std::string &s) {
  s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) {
    return !std::isspace(ch);
  }).base(), s.end());
}

// trim from both ends (in place)
static inline void trim(std::string &s) {
  ltrim(s);
  rtrim(s);
}

static std::pair<std::string, std::string> split_key_val(std::string s, std::string delimiter) {
  size_t pos = 0;
  if ((pos = s.find(delimiter)) != std::string::npos){
    std::string key = s.substr(0, pos);
    std::string val = s.substr(pos + + delimiter.size());
    return {key, val};
  }
  return {};
}

static std::vector<std::string> split_string2vector(std::string text, char delimiter) {
  std::vector<std::string> results;
  boost::split(results, text, [delimiter](char c) { return c == delimiter; });
  return results;
}

static std::vector<std::string> split_string2vector(std::string s, std::string delimiter) {
  std::vector<std::string> results;
  size_t pos = 0;
  std::string token;
  while ((pos = s.find(delimiter)) != std::string::npos) {
    results.emplace_back(s.substr(0, pos));
    s.erase(0, pos + delimiter.length());
  }
  results.emplace_back(s);
  return results;
}

//  ARRAY ['linux-hosts']
std::string to_pg(const ::google::protobuf::RepeatedPtrField<::std::string>& v){
  //bool empty=true;
  std::string result = "ARRAY[";
  for (int i = 0; i != v.size(); ++i) {
    if (v[i].size()>0) {
      result += "'" + v[i] + "'" + ((i != v.size() - 1) ? ", " : "");
      //empty = false;
    }
  }
  result += "]::TEXT[]";
  return result;
}

//  '"_HOSTIPMI_USERNAME"=>"admin","_HOSTIPMI_PASSWORD"=>"admin"'
std::string to_pg(const ::google::protobuf::Map< ::std::string, ::std::string >& m){
  std::string result = "'";
  bool first = true;
  for (google::protobuf::Map< ::std::string, ::std::string >::const_iterator i = m.begin(); i != m.end(); ++i) {
    if (!first)
      result += ", ";
    result += "\"" + i->first + "\"=>\"" + i->second + "\"";
    first = false;
  }
  result += "'";
  return result;
}

std::string to_json(const ::google::protobuf::RepeatedPtrField<::std::string>& v){
  std::string result = "[";
  for (int i = 0; i != v.size(); ++i)
    result += "\"" + v[i] + + ((i !=v.size() - 1) ? "\", " : "\"");
  result += "]";
  return result;
}

std::string to_json(const ::google::protobuf::Map< ::std::string, ::std::string >& m){
  std::string result = "{";
  bool first = true;
  for (google::protobuf::Map< ::std::string, ::std::string >::const_iterator i = m.begin(); i != m.end(); ++i) {
    if (!first)
      result += ", ";
    result += "\"" + i->first + "\":\"" + i->second + "\"";
    first = false;
  }
  result += "}";
  return result;
}

void pg2proto(std::string s, google::protobuf::RepeatedPtrField<::std::string>* p){
  s.erase(std::remove(s.begin(), s.end(), '{'), s.end());
  s.erase(std::remove(s.begin(), s.end(), '}'), s.end());
  if (s.size()==0)
    return;
  std::vector<std::string> v = split_string2vector(s, ',');
  for (std::vector<std::string>::iterator i = v.begin(); i != v.end(); ++i) {
    trim(*i);
    //i->erase(std::remove_if(i->begin(), i->end(), ::isspace), i->end());
    // skip empty strings
    if (i->size())
      *p->Add() = *i;
  }
}

//TODO
//regexp -> https://www.daniweb.com/programming/software-development/threads/118708/boost-algorithm-split-with-string-delimeters
void pg2proto(std::string s, google::protobuf::Map< ::std::string, ::std::string >* p){
  std::vector<std::string> v = split_string2vector(s, ','); // this does not work with , embedded in strings - use regexp
  for (auto i : v) {
    auto kv = split_key_val(i, "=>");
    //kv.first.erase(std::remove_if(kv.first.begin(), kv.first.end(), ::isspace), kv.first.end());
    //kv.second.erase(std::remove_if(kv.second.begin(), kv.second.end(), ::isspace), kv.second.end());
    trim(kv.first); // might be leading spaces
    trim(kv.second);// might be leading spaces
    kv.first.erase(std::remove(kv.first.begin(), kv.first.end(), '"'), kv.first.end());
    kv.second.erase(std::remove(kv.second.begin(), kv.second.end(), '"'), kv.second.end());
    // skip empty keys
    if (kv.first.size()){
      (*p)[kv.first] = kv.second;
    }
  }
}