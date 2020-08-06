#include "avro_json_parsing.h"
#include <iostream>
#include <algorithm>
#include <rapidjson/document.h>
#include <glog/logging.h>

using namespace rapidjson;

//int64_t get_ts()
// we try to parse the name of the metric accoring to prometheus naming standard.
// all . in metric name is converted to _
//
std::vector<bb_avro_metric_t> parse_datadog_series2avro(std::string buffer) {
  std::vector<bb_avro_metric_t> result;
  StringStream s(buffer.c_str());
  Document d;
  d.ParseStream(s);
  if (d.HasParseError()) {
    LOG(WARNING) << "parse error";
    return result;
  }

  if (d.HasMember("series") && d["series"].IsArray()) {
    auto serie = d["series"].GetArray();
    for (auto const &s : serie) {
      bb_avro_metric_t m;

      if (s.IsObject()) {
        if (s.HasMember("metric") && s["metric"].IsString()) {
          std::string raw_name = s["metric"].GetString();
          // split name in name and prefix
          auto separator_pos = raw_name.find_first_of('.', 0);
          if (separator_pos != std::string::npos) {
            m.ns = raw_name.substr(0, separator_pos);
            m.name = raw_name.substr(separator_pos + 1);
          } else {
            m.ns = "undef";
            m.name = raw_name;
          }
          std::replace(m.name.begin(), m.name.end(), '.', '_');
        } else
          continue;

        // TODO
        if (s.HasMember("tags") && s["tags"].IsArray()) {
          //"tags": ["version:6.3.3"]
          auto tags = s["tags"].GetArray();
          for (auto const &t : tags) {
            if (t.IsString()) {
              std::string s = t.GetString();
              auto separator_pos = s.find_first_of(':', 0);
              if (separator_pos != std::string::npos) {
                bb_avro_metric_label_t label;
                label.key = s.substr(0, separator_pos);
                // following is proably not needed (never seen a problem)
                std::replace(label.key.begin(), label.key.end(), ' ',
                             '_'); // replace all ' ' to '_' influxdb line format seems to dislike spaces in tag names
                label.value = s.substr(separator_pos + 1);
                // needed since jmx metrics contains things as "name=G1 Young Generation"
                std::replace(label.value.begin(), label.value.end(), ' ',
                             '_'); // replace all ' ' to '_' influxdb line format seems to dislike spaces in tag values
                m.labels.push_back(label);
              }
            }
          }

          //metrics_tag_t
          //std::cerr << buffer << std::endl;
        }

        if (s.HasMember("host") && s["host"].IsString()) {
          bb_avro_metric_label_t label;
          label.key = "dd_host";
          label.value = s["host"].GetString();
          m.labels.push_back(label);
        }

        if (s.HasMember("type") && s["type"].IsString()) {
          bb_avro_metric_label_t label;
          label.key = "dd_type";
          label.value = s["type"].GetString();
          m.labels.push_back(label);
        }

        if (s.HasMember("interval") && s["interval"].IsInt()) {
          bb_avro_metric_label_t label;
          label.key = "dd_interval";
          label.value = std::to_string(s["interval"].GetInt());
          m.labels.push_back(label);
          // TODO what should we do with this
        }

        if (s.HasMember("device") && s["device"].IsString()) {
          bb_avro_metric_label_t label;
          label.key = "dd_device";
          label.value = s["device"].GetString();
          m.labels.push_back(label);
        }

        /*if (s.HasMember("source_type_name") && s["source_type_name"].IsString()) {
          m.source_type_name = s["source_type_name"].GetString();
          //m.tags.emplace_back("source_type_name", s["source_type_name"].GetString());
        }*/


        // TODO add sanity checks here

        if (s.HasMember("points") && s["points"].IsArray()) {
          auto points = s["points"].GetArray();
          for (auto const &pa : points) {
            if (pa.IsArray()) {
              if (pa.Size() == 2) { // TODO loop over pair of measurements here
                // get ts
                if (pa[0].IsInt())
                  m.timestamp = ((int64_t) pa[0].GetInt() * 1000); // ms

                //get sample
                if (pa[1].IsDouble())
                  m.value.set_double(pa[1].GetDouble());
                else if (pa[1].IsInt())
                  m.value.set_long(pa[1].GetInt());
                else if (pa[1].IsInt64())
                  m.value.set_long(pa[1].GetInt64());

                result.push_back(m);
              }
            }
          }
        }
      } // is object
    }
  }
  return result;
}


std::vector<bb_avro_metric_t> parse_datadog_check_run2avro(std::string buffer) {
  std::vector<bb_avro_metric_t> result;
  StringStream s(buffer.c_str());
  Document d;
  d.ParseStream(s);
  if (d.HasParseError()) {
    LOG(WARNING) << "parse error";
    return result;
  }

  if (d.IsArray()) {
    auto checks = d.GetArray();
    for (auto const &c : checks) {
      bb_avro_metric_t m;
      //m.source_type_name ="Check";

      if (c.IsObject()) {
        if (c.HasMember("check") && c["check"].IsString())
          m.name = c["check"].GetString();
        else
          continue;

        if (c.HasMember("host_name") && c["host_name"].IsString()) {
          bb_avro_metric_label_t label;
          label.key = "dd_host_name";
          label.value = c["host_name"].GetString();
          m.labels.push_back(label);
        }

        if (c.HasMember("tags") && c["tags"].IsArray()) {
          //"tags": ["check:uptime"]
          auto tags = c["tags"].GetArray();
          for (auto const &t : tags) {
            if (t.IsString()) {
              std::string s = t.GetString();
              auto separator_pos = s.find_first_of(':', 0);
              if (separator_pos != std::string::npos) {
                bb_avro_metric_label_t label;
                label.key = s.substr(0, separator_pos);
                // following is proably not needed (never seen a problem)
                std::replace(label.key.begin(), label.key.end(), ' ',
                             '_'); // replace all ' ' to '_' influxdb line format seems to dislike spaces in tag names
                label.value = s.substr(separator_pos + 1);
                // needed since jmx metrics contains things as "name=G1 Young Generation"
                std::replace(label.value.begin(), label.value.end(), ' ',
                             '_'); // replace all ' ' to '_' influxdb line format seems to dislike spaces in tag values
                m.labels.push_back(label);
              }
            }
          }
        }

        if (c.HasMember("timestamp") && c["timestamp"].IsInt()) {
          m.timestamp = c["timestamp"].IsInt();
        }

        if (c.HasMember("status") && c["status"].IsInt()) {
          m.value.set_long(c["status"].IsInt());
        }

        if (c.HasMember("message") && c["message"].IsString()) {
          //message
        }
      }
    } //for checks
  } // isArray

  return result;
}


//todo - this must be wromng - it only seems to do jsopn parsing - no prometheus conversion
std::vector<bb_avro_metric_t> parse_resources(std::string buffer) {
  std::vector<bb_avro_metric_t> result;
  StringStream s(buffer.c_str());
  Document d;
  d.ParseStream(s);
  if (d.HasParseError()) {
    LOG(WARNING) << "parse error";
    return result;
  }
  return result;

}

static std::string get_influx_tags(const bb_avro_metric_t &m) {
  std::string s;
  for (std::vector<bb_avro_metric_label_t>::const_iterator i = m.labels.begin(); i != m.labels.end(); ++i) {
    s += i->key + "=" + i->value + ",";
  }
  if (s.size() > 0)
    s.pop_back();
  return s;
}

std::string to_influx_string(const bb_avro_metric_t &m) {
  std::string time_str = std::to_string(m.timestamp) + "000000";
  std::string tags = get_influx_tags(m);
  //std::string metrics_line = m.source_type_name + "," + tags + " " + m.metric_name + "=" + std::to_string(m.value) + " " + time_str;
  //auto dot_pos = m.metric_name.find_first_of('.', 0);
  //std::string group = "dd." + m.metric_name.substr(0, dot_pos);
  //std::string metrics_line = m.source_type_name + "," + tags + " " + m.metric_name + "=" + std::to_string(m.value) + " " + time_str;

  std::string value_string;
  switch (m.value.idx()) {
    case 0: // null
      value_string = "NaN";
      break;
    case 1: // long
      value_string = std::to_string(m.value.get_long());
      break;
    case 2: // double
      value_string = std::to_string(m.value.get_double());
      break;
  }

  std::string metrics_line = m.ns + "," + tags + " " + m.name + "=" + value_string + " " + time_str;
  return metrics_line;
}

static std::string get_prometheus_tags(const bb_avro_metric_t &m) {
  std::string s;
  if (m.labels.size() == 0)
    return s;

  s += "{";
  for (std::vector<bb_avro_metric_label_t>::const_iterator i = m.labels.begin(); i != m.labels.end(); ++i) {
    s += i->key + "=\"" + i->value + "\",";
  }
  s.pop_back();
  s += "}";
  return s;
}

//should look like this
// http_requests_total{method="post",code="200"} 1027 1395066363000
/*std::string to_prometheus_string(const avro_metrics_t& m){
  std::string tags = get_prometheus_tags(m);

  std::string value_string;
  switch (m.value.idx()){
    case 0: // null
      value_string = "NaN";
      break;
    case 1: // long
      value_string =  std::to_string(m.value.get_long());
      break;
    case 2: // double
      value_string =  std::to_string(m.value.get_double());
      break;
  }

  std::string metrics_line =  m.ns + "_" + m.name + tags + " " + value_string + " " + std::to_string(m.timestamp);
  return metrics_line;
}
 */


