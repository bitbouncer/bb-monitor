#include "avro_prometheus_utils.h"
#include <chrono>
#include <glog/logging.h>

static inline int64_t milliseconds_since_epoch() {
  return std::chrono::duration_cast<std::chrono::milliseconds>
      (std::chrono::system_clock::now().time_since_epoch()).count();
}

bb_avro_metric_label_t parse_prometheus_tag(const std::string s) {
  bb_avro_metric_label_t label;
  auto separator_pos = s.find('=');

  label.key = s.substr(0, separator_pos);
  // following is proably not needed (never seen a problem)
  // replace all ' ' to '_' influxdb line format seems to dislike spaces in tag names
  //TODO check if prometheus line format allows this
  std::replace(label.key.begin(), label.key.end(), ' ', '_');

  if (separator_pos != std::string::npos) {
    auto begin = s.find_first_not_of("\" ", separator_pos + 1);
    if (begin==std::string::npos) {
      LOG(ERROR) << "bad tag" << s;
      return label;
    }

    auto end   = s.find('"', begin + 1);
    if (end==std::string::npos) {
      LOG(ERROR) << "bad tag" << s;
      return label;
    }
    label.value = s.substr(begin, end-begin);
    // needed since jmx metrics contains things as "name=G1 Young Generation"
    // replace all ' ' to '_' influxdb line format seems to dislike spaces in tag values
    //TODO check if prometheus line format allows this
    std::replace(label.value.begin(), label.value.end(), ' ', '_');
  }
  return label;
}

static bool compare_by_label_name(const bb_avro_metric_label_t &a, const bb_avro_metric_label_t &b){
  return a.key < b.key;
}

std::vector<bb_avro_metric_label_t> parse_prometheus_tags(std::string& job_id, std::string& instance_id, const std::string& s){
  size_t cursor=0;
  std::vector<bb_avro_metric_label_t> result;
  while (true) {
    if (cursor == s.size())
      break;

    auto tag_separator_pos = s.find(',', cursor);

    bb_avro_metric_label_t label = parse_prometheus_tag(s.substr(cursor, tag_separator_pos));
    if (label.key.size())
      result.push_back(label);

    if (tag_separator_pos == std::string::npos)
      break;

    cursor = tag_separator_pos + 1;
  }

  // we should always have a job_id
  bb_avro_metric_label_t label1;
  label1.key = "job";
  label1.value = job_id;
  result.push_back(label1);

  // we usually have a job_id
  if (instance_id.size()>0) {
    bb_avro_metric_label_t label2;
    label2.key = "instance";
    label2.value = instance_id;
    result.push_back(label2);
  }

  std::sort(result.begin(), result.end(), compare_by_label_name);
  return result;
}
