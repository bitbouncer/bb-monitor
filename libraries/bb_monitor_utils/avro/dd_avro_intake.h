#include <sstream>
#include "boost/any.hpp"
#include "avro/Specific.hh"
#include "avro/Encoder.hh"
#include "avro/Decoder.hh"
#include "avro/Compiler.hh"
#pragma once

struct dd_avro_intake_t {
  std::string agent;
  std::string data;
  int64_t timestamp;
  dd_avro_intake_t() :
    agent(std::string()),
    data(std::string()),
    timestamp(int64_t()){
  }

  //returns the string representation of the schema of self (avro extension for kspp avro serdes)
  static inline const char* schema_as_string() {
    return "{\"type\":\"record\",\"name\":\"dd_avro_intake_t\",\"doc\":\"thisismydoc\",\"fields\":[{\"name\":\"agent\",\"type\":\"string\"},{\"name\":\"data\",\"type\":\"string\"},{\"name\":\"timestamp\",\"type\":\"long\"}]}";
  } 

  //returns a valid schema of self (avro extension for kspp avro serdes)
  static std::shared_ptr<const ::avro::ValidSchema> valid_schema() {
    static const std::shared_ptr<const ::avro::ValidSchema> _validSchema(std::make_shared<const ::avro::ValidSchema>(::avro::compileJsonSchemaFromString(schema_as_string())));
    return _validSchema;
  }

  //returns the (type)name of self (avro extension for kspp avro serdes)
  static std::string avro_schema_name(){
    return "dd_avro_intake_t";
  }
};


namespace avro {
template<> struct codec_traits<dd_avro_intake_t> {
  static void encode(Encoder& e, const dd_avro_intake_t& v) {
    ::avro::encode(e, v.agent);
    ::avro::encode(e, v.data);
    ::avro::encode(e, v.timestamp);
  }
  static void decode(Decoder& d, dd_avro_intake_t& v) {
    if (::avro::ResolvingDecoder *rd =
      dynamic_cast<::avro::ResolvingDecoder *>(&d)) {
        const std::vector<size_t> fo = rd->fieldOrder();
        for (std::vector<size_t>::const_iterator it = fo.begin(); it != fo.end(); ++it) {
          switch (*it) {
          case 0:
          ::avro::decode(d, v.agent);
          break;
          case 1:
          ::avro::decode(d, v.data);
          break;
          case 2:
          ::avro::decode(d, v.timestamp);
          break;
            default:
            break;
          }
        }
    } else {
      ::avro::decode(d, v.agent);
      ::avro::decode(d, v.data);
      ::avro::decode(d, v.timestamp);
    }
  }
};

}
