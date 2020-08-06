#include "splunk_utils.h"
#include <iostream>
#include <algorithm>
#include <rapidjson/document.h>
#include <glog/logging.h>
#include <kspp/kspp.h>
#include <openssl/md5.h>
#include <bb_monitor_utils/time.h>

using namespace rapidjson;

// should parse this - note that this is not valid json - it's not an array
// kParseStopWhenDoneFlag solves this
//{"event":{"line":"Mon Sep 23 07:27:21 UTC 2019","source":"stdout","tag":"d799f3fdbbd9"},"time":"1569223641.959321","host":"linux"}{"event":{"line":"another log","source":"stdout","tag":"d799f3fdbbd9"},"time":"1569223641.959406","host":"linux"}

//{"event":{"line":"Mon Sep 23 09:30:37 UTC 2019","source":"stdout","tag":"keen_heisenberg/08ba12ba25a00b94ba4e4c9e8016f2e60bb5abfe0924fe8f56c1174c25cd55a7","attrs":{"TEST":"false","location":"west"}},"time":"1569231037.639682","host":"linux"}

std::vector<bb_monitor::LogLine> parse_splunk_event(std::string buffer){
  std::vector<bb_monitor::LogLine> result;
  StringStream s(buffer.c_str());
  Document d;
  while(true) {
    d.ParseStream<kParseStopWhenDoneFlag>(s);
    if (d.HasParseError()) {
      return result;
    }

    bb_monitor::LogLine ll;
    if (d.IsObject()) {
      if (d.HasMember("event") && d["event"].IsObject()) {
        auto ev = d["event"].GetObject();
        if (ev.HasMember("line") && ev["line"].IsString()) {
          //std::string line = ev["line"].GetString();
          ll.set_line(ev["line"].GetString());
        }
        if (ev.HasMember("source") && ev["source"].IsString()) {
          ll.set_source(ev["source"].GetString());
        }
        if (ev.HasMember("tag") && ev["tag"].IsString()) {
          bb_monitor::Label label;
          label.set_key("tag");
          label.set_value(ev["tag"].GetString());
          *ll.add_labels() = label;
        }
        // iterate over all keys and add the as labels
        if (ev.HasMember("attrs") && ev["attrs"].IsObject()) {
          auto dict = ev["attrs"].GetObject();
          for (auto i = dict.begin(); i != dict.end(); ++i) {
            bb_monitor::Label label;
            if (i->name.IsString() && i->value.IsString()) {
              label.set_key(i->name.GetString());
              label.set_value(i->value.GetString());
              *ll.add_labels() = label;
            }
          }
        }

        if (d.HasMember("time")) {
          if (d["time"].IsString()) {
            std::string time = d["time"].GetString();
            double ts_d = std::stod(time); // this is supposed to be 1569223641.959321
            int64_t ts_ns = ts_d * 1000000; // get microseconds
            ts_ns =ts_ns*1000; // nanosec
            ll.set_timestamp_ns(ts_ns);
            //  m.set_timestamp(((int64_t) c["timestamp"].IsInt() * 1000)); // ms
            //m.set_timestamp(c["timestamp"].IsInt());
            //} else {
            //  m.set_timestamp(kspp::milliseconds_since_epoch());
            //}
          } else { //todo if this is actually a float or an integer??
            ll.set_timestamp_ns(nanoseconds_since_epoch());
          }
          ll.set_timestamp_ns(nanoseconds_since_epoch());
        }

        if (d.HasMember("host") && d["host"].IsString()) {
          ll.set_host(d["host"].GetString());
        }

        ll.set_agent("splunk");

        MD5_CTX ctx;
        MD5_Init(&ctx);
        MD5_Update(&ctx, ll.source().data(), ll.source().size());
        MD5_Update(&ctx, ll.host().data(), ll.host().size());
        MD5_Update(&ctx, ll.line().data(), ll.line().size());

        for (int i =0; i!=ll.labels_size(); ++i){
          MD5_Update(&ctx, ll.labels(i).key().data(), ll.labels(i).key().size());
          MD5_Update(&ctx, ll.labels(i).value().data(), ll.labels(i).value().size());
        }

        int64_t ts = ll.timestamp_ns();
        MD5_Update(&ctx, &ts, sizeof(ts));
        // SHOULD TS be part of this???
        boost::uuids::uuid uuid;
        MD5_Final(uuid.data, &ctx);
        ll.set_id(to_string(uuid));

        result.push_back(ll);
      }
    }
  }

    return result;
  }
