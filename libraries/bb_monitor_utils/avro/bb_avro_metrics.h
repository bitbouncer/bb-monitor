#include <sstream>
#include "boost/any.hpp"
#include "avro/Specific.hh"
#include "avro/Encoder.hh"
#include "avro/Decoder.hh"
#include "avro/Compiler.hh"
#pragma once

struct bb_avro_metric_label_t {
  std::string key;
  std::string value;
  bb_avro_metric_label_t() :
    key(std::string()),
    value(std::string()){
  }

};

struct bb_avro_metric_pos_t {
  double lat;
  double lon;
  bb_avro_metric_pos_t() :
    lat(double()),
    lon(double()){
  }

};

struct _bb_avro_metrics_schema_Union__0__ {
private:
  size_t idx_;
  boost::any value_;
public:
  size_t idx() const { return idx_; }
  bool is_null() const {
    return (idx_ == 0);
  }
  void set_null() {
    idx_ = 0;
    value_ = boost::any();
  }
  int64_t get_long() const;
  void set_long(const int64_t& v);
  double get_double() const;
  void set_double(const double& v);
  bb_avro_metric_pos_t get_bb_avro_metric_pos_t() const;
  void set_bb_avro_metric_pos_t(const bb_avro_metric_pos_t& v);
  _bb_avro_metrics_schema_Union__0__();
};

struct bb_avro_metric_t {
  typedef _bb_avro_metrics_schema_Union__0__ value_t;
  std::string ns;
  std::string name;
  std::vector<bb_avro_metric_label_t> labels;
  value_t value;
  int64_t timestamp;
  bb_avro_metric_t() :
    ns(std::string()),
    name(std::string()),
    labels(std::vector<bb_avro_metric_label_t>()),
    value(value_t()),
    timestamp(int64_t()){
  }

  //returns the string representation of the schema of self (avro extension for kspp avro serdes)
  static inline const char* schema_as_string() {
    return "{\"type\":\"record\",\"name\":\"bb_avro_metric_t\",\"doc\":\"thisismydoc\",\"fields\":[{\"name\":\"ns\",\"type\":\"string\"},{\"name\":\"name\",\"type\":\"string\"},{\"name\":\"labels\",\"type\":{\"type\":\"array\",\"items\":{\"type\":\"record\",\"name\":\"bb_avro_metric_label_t\",\"fields\":[{\"name\":\"key\",\"type\":\"string\"},{\"name\":\"value\",\"type\":\"string\"}]}}},{\"name\":\"value\",\"type\":[\"null\",\"long\",\"double\",{\"type\":\"record\",\"name\":\"bb_avro_metric_pos_t\",\"fields\":[{\"name\":\"lat\",\"type\":\"double\"},{\"name\":\"lon\",\"type\":\"double\"}]}]},{\"name\":\"timestamp\",\"type\":\"long\"}]}";
  } 

  //returns a valid schema of self (avro extension for kspp avro serdes)
  static std::shared_ptr<const ::avro::ValidSchema> valid_schema() {
    static const std::shared_ptr<const ::avro::ValidSchema> _validSchema(std::make_shared<const ::avro::ValidSchema>(::avro::compileJsonSchemaFromString(schema_as_string())));
    return _validSchema;
  }

  //returns the (type)name of self (avro extension for kspp avro serdes)
  static std::string avro_schema_name(){
    return "bb_avro_metric_t";
  }
};

inline
int64_t _bb_avro_metrics_schema_Union__0__::get_long() const {
  if (idx_ != 1) {
    throw ::avro::Exception("Invalid type for union");
  }
  return boost::any_cast<int64_t >(value_);
}

inline
void _bb_avro_metrics_schema_Union__0__::set_long(const int64_t& v) {
  idx_ = 1;
  value_ = v;
}

inline
double _bb_avro_metrics_schema_Union__0__::get_double() const {
  if (idx_ != 2) {
    throw ::avro::Exception("Invalid type for union");
  }
  return boost::any_cast<double >(value_);
}

inline
void _bb_avro_metrics_schema_Union__0__::set_double(const double& v) {
  idx_ = 2;
  value_ = v;
}

inline
bb_avro_metric_pos_t _bb_avro_metrics_schema_Union__0__::get_bb_avro_metric_pos_t() const {
  if (idx_ != 3) {
    throw ::avro::Exception("Invalid type for union");
  }
  return boost::any_cast<bb_avro_metric_pos_t >(value_);
}

inline
void _bb_avro_metrics_schema_Union__0__::set_bb_avro_metric_pos_t(const bb_avro_metric_pos_t& v) {
  idx_ = 3;
  value_ = v;
}

inline _bb_avro_metrics_schema_Union__0__::_bb_avro_metrics_schema_Union__0__() : idx_(0) { }

namespace avro {
template<> struct codec_traits<bb_avro_metric_label_t> {
  static void encode(Encoder& e, const bb_avro_metric_label_t& v) {
    ::avro::encode(e, v.key);
    ::avro::encode(e, v.value);
  }
  static void decode(Decoder& d, bb_avro_metric_label_t& v) {
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

template<> struct codec_traits<bb_avro_metric_pos_t> {
  static void encode(Encoder& e, const bb_avro_metric_pos_t& v) {
    ::avro::encode(e, v.lat);
    ::avro::encode(e, v.lon);
  }
  static void decode(Decoder& d, bb_avro_metric_pos_t& v) {
    if (::avro::ResolvingDecoder *rd =
      dynamic_cast<::avro::ResolvingDecoder *>(&d)) {
        const std::vector<size_t> fo = rd->fieldOrder();
        for (std::vector<size_t>::const_iterator it = fo.begin(); it != fo.end(); ++it) {
          switch (*it) {
          case 0:
          ::avro::decode(d, v.lat);
          break;
          case 1:
          ::avro::decode(d, v.lon);
          break;
            default:
            break;
          }
        }
    } else {
      ::avro::decode(d, v.lat);
      ::avro::decode(d, v.lon);
    }
  }
};

template<> struct codec_traits<_bb_avro_metrics_schema_Union__0__> {
  static void encode(Encoder& e, _bb_avro_metrics_schema_Union__0__ v) {
    e.encodeUnionIndex(v.idx());
    switch (v.idx()) {
    case 0:
      e.encodeNull();
     break;
    case 1:
      ::avro::encode(e, v.get_long());
     break;
    case 2:
      ::avro::encode(e, v.get_double());
     break;
    case 3:
      ::avro::encode(e, v.get_bb_avro_metric_pos_t());
     break;
    }
  }
  static void decode(Decoder& d, _bb_avro_metrics_schema_Union__0__& v) {
    size_t n = d.decodeUnionIndex();
    if (n >= 4) { throw ::avro::Exception("Union index too big"); }
    switch (n) {
    case 0:
      d.decodeNull();
      v.set_null();
      break;
    case 1:
      {
          int64_t vv;
          ::avro::decode(d, vv);
          v.set_long(vv);
      }
      break;
    case 2:
      {
          double vv;
          ::avro::decode(d, vv);
          v.set_double(vv);
      }
      break;
    case 3:
      {
          bb_avro_metric_pos_t vv;
          ::avro::decode(d, vv);
          v.set_bb_avro_metric_pos_t(vv);
      }
      break;
    }
  }
};

template<> struct codec_traits<bb_avro_metric_t> {
  static void encode(Encoder& e, const bb_avro_metric_t& v) {
    ::avro::encode(e, v.ns);
    ::avro::encode(e, v.name);
    ::avro::encode(e, v.labels);
    ::avro::encode(e, v.value);
    ::avro::encode(e, v.timestamp);
  }
  static void decode(Decoder& d, bb_avro_metric_t& v) {
    if (::avro::ResolvingDecoder *rd =
      dynamic_cast<::avro::ResolvingDecoder *>(&d)) {
        const std::vector<size_t> fo = rd->fieldOrder();
        for (std::vector<size_t>::const_iterator it = fo.begin(); it != fo.end(); ++it) {
          switch (*it) {
          case 0:
          ::avro::decode(d, v.ns);
          break;
          case 1:
          ::avro::decode(d, v.name);
          break;
          case 2:
          ::avro::decode(d, v.labels);
          break;
          case 3:
          ::avro::decode(d, v.value);
          break;
          case 4:
          ::avro::decode(d, v.timestamp);
          break;
            default:
            break;
          }
        }
    } else {
      ::avro::decode(d, v.ns);
      ::avro::decode(d, v.name);
      ::avro::decode(d, v.labels);
      ::avro::decode(d, v.value);
      ::avro::decode(d, v.timestamp);
    }
  }
};

}
