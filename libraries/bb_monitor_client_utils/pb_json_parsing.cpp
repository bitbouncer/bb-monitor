#include "pb_json_parsing.h"
#include <iostream>
#include <algorithm>
#include <rapidjson/document.h>
#include <glog/logging.h>
#include <kspp/kspp.h>

using namespace rapidjson;

std::vector<bb_monitor::Metric> parse_datadog_series2pb(std::string buffer) {
  std::vector<bb_monitor::Metric> result;
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
      bb_monitor::Metric m;

      if (s.IsObject()) {
        if (s.HasMember("metric") && s["metric"].IsString()) {
          std::string raw_name = s["metric"].GetString();
          // split name in name and prefix
          auto separator_pos = raw_name.find_first_of('.', 0);
          std::string metric_name;
          if (separator_pos != std::string::npos) {
            m.set_ns(raw_name.substr(0, separator_pos));
            metric_name = raw_name.substr(separator_pos + 1);
          } else {
            m.set_ns("undef");
            metric_name = raw_name;
          }
          std::replace(metric_name.begin(), metric_name.end(), '.', '_');
          m.set_name(metric_name);
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
                std::string label_name = s.substr(0, separator_pos);
                // following is proably not needed (never seen a problem)
                std::replace(label_name.begin(), label_name.end(), ' ',
                             '_'); // replace all ' ' to '_' influxdb line format seems to dislike spaces in tag names

                std::string label_value = s.substr(separator_pos + 1);
                // needed since jmx metrics contains things as "name=G1 Young Generation"
                std::replace(label_value.begin(), label_value.end(), ' ',
                             '_'); // replace all ' ' to '_' influxdb line format seems to dislike spaces in tag values
                bb_monitor::Label label;
                label.set_key(label_name);
                label.set_value(label_value);
                *m.add_labels() = label;
                //m.labels.push_back(label);
              }
            }
          }

          //metrics_tag_t
          //std::cerr << buffer << std::endl;
        }

        if (s.HasMember("host") && s["host"].IsString()) {
          bb_monitor::Label label;
          label.set_key("dd_host");
          label.set_value(s["host"].GetString());
          *m.add_labels() = label;
        }

        if (s.HasMember("type") && s["type"].IsString()) {
          bb_monitor::Label label;
          label.set_key("dd_type");
          label.set_value(s["type"].GetString());
          *m.add_labels() = label;
        }

        if (s.HasMember("interval") && s["interval"].IsInt()) {
          bb_monitor::Label label;
          label.set_key("dd_interval");
          label.set_value(std::to_string(s["interval"].GetInt()));
          *m.add_labels() = label;
          // TODO what should we do with this
        }

        if (s.HasMember("device") && s["device"].IsString()) {
          bb_monitor::Label label;
          label.set_key("dd_device");
          label.set_value(s["device"].GetString());
          *m.add_labels() = label;
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
                  m.set_timestamp(((int64_t) pa[0].GetInt() * 1000)); // ms

                //get sample
                // we only set double type of value
                auto sample = m.mutable_sample();

                if (pa[1].IsDouble())
                  sample->set_doublevalue(pa[1].GetDouble());
                else if (pa[1].IsInt())
                  sample->set_longvalue(pa[1].GetInt());
                else if (pa[1].IsInt64())
                  sample->set_longvalue(pa[1].GetInt64());

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


std::vector<bb_monitor::Metric> parse_datadog_check_run2pb(std::string buffer) {
  std::vector<bb_monitor::Metric> result;
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
      bb_monitor::Metric m;
      //m.source_type_name ="Check";

      if (c.IsObject()) {
        if (c.HasMember("check") && c["check"].IsString())
          m.set_name(c["check"].GetString());
        else
          continue;

        if (c.HasMember("host_name") && c["host_name"].IsString()) {
          bb_monitor::Label label;
          label.set_key("dd_host_name");
          label.set_value(c["host_name"].GetString());
          *m.add_labels() = label;
        }

        if (c.HasMember("tags") && c["tags"].IsArray()) {
          //"tags": ["check:uptime"]
          auto tags = c["tags"].GetArray();
          for (auto const &t : tags) {
            if (t.IsString()) {
              std::string s = t.GetString();
              auto separator_pos = s.find_first_of(':', 0);
              if (separator_pos != std::string::npos) {
                std::string label_name = s.substr(0, separator_pos);
                // following is proably not needed (never seen a problem)
                std::replace(label_name.begin(), label_name.end(), ' ',
                             '_'); // replace all ' ' to '_' influxdb line format seems to dislike spaces in tag names
                std::string label_value = s.substr(separator_pos + 1);
                // needed since jmx metrics contains things as "name=G1 Young Generation"
                std::replace(label_value.begin(), label_value.end(), ' ',
                             '_'); // replace all ' ' to '_' influxdb line format seems to dislike spaces in tag values
                bb_monitor::Label label;
                label.set_key(label_name);
                label.set_value(label_value);
                *m.add_labels() = label;
              }
            }
          }
        }

        // we cannot do anything without a ts
        if (c.HasMember("timestamp") && c["timestamp"].IsInt()) {
          m.set_timestamp(((int64_t) c["timestamp"].IsInt() * 1000)); // ms
          //m.set_timestamp(c["timestamp"].IsInt());
        } else {
          m.set_timestamp(kspp::milliseconds_since_epoch());
        }

        if (c.HasMember("status") && c["status"].IsInt()) {
          auto sample = m.mutable_sample();
          sample->set_longvalue(c["status"].GetInt());
        }

        if (c.HasMember("message") && c["message"].IsString()) {
          //message
        }
        result.push_back(m);
      }
    } //for checks
  } // isArray

  return result;
}


