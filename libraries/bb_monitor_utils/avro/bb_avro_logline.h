#include <sstream>
#include "boost/any.hpp"
#include "avro/Specific.hh"
#include "avro/Encoder.hh"
#include "avro/Decoder.hh"
#include "avro/Compiler.hh"
#pragma once

struct bb_avro_logline_label_t {
  std::string key;
  std::string value;
  bb_avro_logline_label_t() :
    key(std::string()),
    value(std::string()){
  }

};

struct bb_avro_logline {
  std::string id;
  std::string agent;
  std::string host;
  std::string source;
  int64_t timestamp_ns;
  std::vector<bb_avro_logline_label_t> tags;
  std::string line;
  bb_avro_logline() :
    id(std::string()),
    agent(std::string()),
    host(std::string()),
    source(std::string()),
    timestamp_ns(int64_t()),
    tags(std::vector<bb_avro_logline_label_t>()),
    line(std::string()){
  }

  //returns the string representation of the schema of self (avro extension for kspp avro serdes)
  static inline const char* schema_as_string() {
    return "{\"type\":\"record\",\"name\":\"bb_avro_logline\",\"doc\":\"logline,inspiredbydatadoglogs&splunk\",\"fields\":[{\"name\":\"id\",\"type\":\"string\"},{\"name\":\"agent\",\"type\":\"string\"},{\"name\":\"host\",\"type\":\"string\"},{\"name\":\"source\",\"type\":\"string\"},{\"name\":\"timestamp_ns\",\"type\":\"long\"},{\"name\":\"tags\",\"type\":{\"type\":\"array\",\"items\":{\"type\":\"record\",\"name\":\"bb_avro_logline_label_t\",\"fields\":[{\"name\":\"key\",\"type\":\"string\"},{\"name\":\"value\",\"type\":\"string\"}]}}},{\"name\":\"line\",\"type\":\"string\"}]}";
  } 

  //returns a valid schema of self (avro extension for kspp avro serdes)
  static std::shared_ptr<const ::avro::ValidSchema> valid_schema() {
    static const std::shared_ptr<const ::avro::ValidSchema> _validSchema(std::make_shared<const ::avro::ValidSchema>(::avro::compileJsonSchemaFromString(schema_as_string())));
    return _validSchema;
  }

  //returns the (type)name of self (avro extension for kspp avro serdes)
  static std::string avro_schema_name(){
    return "bb_avro_logline";
  }
};


namespace avro {
template<> struct codec_traits<bb_avro_logline_label_t> {
  static void encode(Encoder& e, const bb_avro_logline_label_t& v) {
    ::avro::encode(e, v.key);
    ::avro::encode(e, v.value);
  }
  static void decode(Decoder& d, bb_avro_logline_label_t& v) {
    if (::avro::ResolvingDecoder *rd =
      dynamic_cast<::avro::ResolvingDecoder *>(&d)) {
        const std::vector<size_t> fo = rd->fieldOrder();
        for (std::vector<size_t>::const_iterator it = fo.begin(); it != fo.end(); ++it) {
          switch (*it) {
          case 0:
          ::avro::decode(d, v.key);
          break;
          case 1:
          ::avro::decode(d, v.value);
          break;
            default:
            break;
          }
        }
    } else {
      ::avro::decode(d, v.key);
      ::avro::decode(d, v.value);
    }
  }
};

template<> struct codec_traits<bb_avro_logline> {
  static void encode(Encoder& e, const bb_avro_logline& v) {
    ::avro::encode(e, v.id);
    ::avro::encode(e, v.agent);
    ::avro::encode(e, v.host);
    ::avro::encode(e, v.source);
    ::avro::encode(e, v.timestamp_ns);
    ::avro::encode(e, v.tags);
    ::avro::encode(e, v.line);
  }
  static void decode(Decoder& d, bb_avro_logline& v) {
    if (::avro::ResolvingDecoder *rd =
      dynamic_cast<::avro::ResolvingDecoder *>(&d)) {
        const std::vector<size_t> fo = rd->fieldOrder();
        for (std::vector<size_t>::const_iterator it = fo.begin(); it != fo.end(); ++it) {
          switch (*it) {
          case 0:
          ::avro::decode(d, v.id);
          break;
          case 1:
          ::avro::decode(d, v.agent);
          break;
          case 2:
          ::avro::decode(d, v.host);
          break;
          case 3:
          ::avro::decode(d, v.source);
          break;
          case 4:
          ::avro::decode(d, v.timestamp_ns);
          break;
          case 5:
          ::avro::decode(d, v.tags);
          break;
          case 6:
          ::avro::decode(d, v.line);
          break;
            default:
            break;
          }
        }
    } else {
      ::avro::decode(d, v.id);
      ::avro::decode(d, v.agent);
      ::avro::decode(d, v.host);
      ::avro::decode(d, v.source);
      ::avro::decode(d, v.timestamp_ns);
      ::avro::decode(d, v.tags);
      ::avro::decode(d, v.line);
    }
  }
};

}
