#include "prometheus_utils.h"
#include <chrono>
#include <glog/logging.h>

static inline int64_t milliseconds_since_epoch() {
  return std::chrono::duration_cast<std::chrono::milliseconds>
      (std::chrono::system_clock::now().time_since_epoch()).count();
}

bb_monitor::Label parse_bb_metrics_label(std::string_view s) {
  bb_monitor::Label label;
  auto separator_pos = s.find('=');

  auto label_name = s.substr(0, separator_pos);
  // following is proably not needed (never seen a problem)
  // replace all ' ' to '_' influxdb line format seems to dislike spaces in tag names
  //TODO check if prometheus line format allows this
  //std::replace(label_name.begin(), label_name.end(), ' ', '_');
  label.set_key(std::string(label_name));

  if (separator_pos != std::string::npos) {
    auto begin = s.find_first_not_of("\" ", separator_pos + 1);
    if (begin == std::string::npos) {
      LOG(ERROR) << "bad tag" << s;
      return label;
    }

    auto end = s.find('"', begin + 1);
    if (end == std::string::npos) {
      LOG(ERROR) << "bad tag" << s;
      return label;
    }

    //std::string value(s.substr(begin, end-begin));
    // Label values may contain any Unicode characters.
    // needed since jmx metrics contains things as "name=G1 Young Generation"
    // replace all ' ' to '_' influxdb line format seems to dislike spaces in tag values
    //TODO check if prometheus line format allows this
    //std::replace(value.begin(), value.end(), ' ', '_');
    label.set_value(std::string(s.substr(begin, end - begin)));
  }

  return label;
}

static bool compare_by_label_name2(const bb_monitor::Label &a, const bb_monitor::Label &b) {
  return a.key() < b.key();
}

static std::vector<bb_monitor::Label>
parse_bb_metrics_labels(std::string_view job_id, std::string_view instance_id, std::string_view s) {
  size_t cursor = 0;
  std::vector<bb_monitor::Label> result;
  while (true) {
    if (cursor == s.size())
      break;

    auto tag_separator_pos = s.find(',', cursor);

    bb_monitor::Label label = parse_bb_metrics_label(s.substr(cursor, tag_separator_pos));
    if (label.key().size())
      result.push_back(label);

    if (tag_separator_pos == std::string::npos)
      break;

    cursor = tag_separator_pos + 1;
  }

  // we should always have a job_id
  bb_monitor::Label label1;
  label1.set_key("job");
  label1.set_value(std::string(job_id));
  result.push_back(label1);

  // we usually have an instance_id
  if (instance_id.size() > 0) {
    bb_monitor::Label label2;
    label2.set_key("instance");
    label2.set_value(std::string(instance_id));
    result.push_back(label2);
  }

  std::sort(result.begin(), result.end(), compare_by_label_name2);
  return result;
}

/*
 # HELP http_requests_total The total number of HTTP requests.
# TYPE http_requests_total counter
http_requests_total{method="post",code="200"} 1027 1395066363000
http_requests_total{method="post",code="400"}    3 1395066363000

# Escaping in label values:
msdos_file_access_time_seconds{path="C:\\DIR\\FILE.TXT",error="Cannot find file:\n\"FILE.TXT\""} 1.458255915e9

# Minimalistic line:
metric_without_timestamp_and_labels 12.47

# A weird metric from before the epoch:
something_weird{problem="division by zero"} +Inf -3982045

# A histogram, which has a pretty complex representation in the text format:
# HELP http_request_duration_seconds A histogram of the request duration.
# TYPE http_request_duration_seconds histogram
http_request_duration_seconds_bucket{le="0.05"} 24054
http_request_duration_seconds_bucket{le="0.1"} 33444
http_request_duration_seconds_bucket{le="0.2"} 100392
http_request_duration_seconds_bucket{le="0.5"} 129389
http_request_duration_seconds_bucket{le="1"} 133988
http_request_duration_seconds_bucket{le="+Inf"} 144320
http_request_duration_seconds_sum 53423
http_request_duration_seconds_count 144320

# Finally a summary, which has a complex representation, too:
# HELP rpc_duration_seconds A summary of the RPC duration in seconds.
# TYPE rpc_duration_seconds summary
rpc_duration_seconds{quantile="0.01"} 3102
rpc_duration_seconds{quantile="0.05"} 3272
rpc_duration_seconds{quantile="0.5"} 4773
rpc_duration_seconds{quantile="0.9"} 9001
rpc_duration_seconds{quantile="0.99"} 76656
rpc_duration_seconds_sum 1.7560473e+07
rpc_duration_seconds_count 2693
 */
std::vector<bb_monitor::Metric>
parse_prometheus_line_format_2_bb_metrics(std::string_view job_id, std::string_view instance_id, std::string_view s) {
  std::vector<bb_monitor::Metric> result;
  int line_cursor = 0;
  while (true) {
    bb_monitor::Metric metric;
    metric.set_ns(std::string(job_id));

    if (line_cursor >= s.size())
      break;
    auto next_lf = s.find('\n', line_cursor);

    //if (next_lf == std::string::npos)
    // next_lf = s.size();

    // done?
    if (next_lf == std::string::npos)
      break;

    //empty line?
    if (line_cursor == next_lf) {
      //LOG(INFO) << "skipping empty line";
      line_cursor = next_lf + 1;
      //TODO push histogram and buckets
      continue;
    }

    //comment?
    if (s[line_cursor] == '#') {
      //auto comment = s.substr(line_cursor, next_lf - line_cursor);
      //LOG(INFO) << "skipping comment ->" << comment;
      line_cursor = next_lf + 1;
      continue;
    }

    //parse line
    auto line = s.substr(line_cursor, next_lf - line_cursor);
    //LOG(INFO) << "found metric - parsing " << line;

    size_t start_of_tags = line.find('{');
    size_t end_of_tags = std::string::npos;
    //has tags?
    if (start_of_tags != std::string::npos) {
      start_of_tags += 1;
      end_of_tags = line.find('}', start_of_tags);
      if (end_of_tags == std::string::npos) {
        LOG(ERROR) << "failed to find end of tags '}' -> skipping " << line;
        line_cursor = next_lf + 1;
        continue;
      }
      auto tags = line.substr(start_of_tags, (end_of_tags - start_of_tags));
      auto v = parse_bb_metrics_labels(job_id, instance_id, tags);
      for (auto &&i  : v)
        *metric.add_labels() = i;
      //LOG(INFO) << "tags" << tags;
    }

    size_t value_begin = std::string::npos;
    size_t value_end = std::string::npos;

    if (end_of_tags != std::string::npos) {
      metric.set_name(std::string(line.substr(0, start_of_tags - 1)));
      value_begin = line.find_first_not_of(' ', end_of_tags + 1);
    } else {
      auto first_space = line.find(' ');
      metric.set_name(std::string(line.substr(0, first_space)));
      if (first_space != std::string::npos)
        value_begin = line.find_first_not_of(' ', first_space + 1);
    }
    // find value
    std::string value;
    if (value_begin != std::string::npos) {
      value_end = line.find(' ', value_begin);
      if (value_end != std::string::npos)
        value = line.substr(value_begin, value_end - value_begin);
      else
        value = line.substr(value_begin);
    } else {
      LOG(ERROR) << "no value -- todo...";
      line_cursor = next_lf + 1;
      continue;
    }

    //TODO How to handle NoN??
    //std::nan("");

    // we only set double type of value
    auto sample = metric.mutable_sample();
    sample->set_doublevalue(atof(value.c_str()));

    //LOG(INFO) << "value: " << value;

    std::string ts;
    if (value_end != std::string::npos) {
      auto ts_begin = line.find_first_not_of(' ', value_end + 1);
      if (ts_begin != std::string::npos) {
        ts = line.substr(ts_begin);
      }
    }

    //TODO unit??
    if (ts.size()) {
      metric.set_timestamp(atoll(ts.c_str())); // UNIT??
    } else {
      metric.set_timestamp(milliseconds_since_epoch());
    }

    line_cursor = next_lf + 1;
    result.push_back(metric);
  }

  return result;
}

