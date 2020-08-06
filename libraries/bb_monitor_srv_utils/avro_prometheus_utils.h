#include <bb_monitor_utils/avro/bb_avro_metrics.h>
#pragma once

bb_avro_metric_label_t parse_prometheus_tag(const std::string s);
std::vector<bb_avro_metric_label_t> parse_prometheus_tags(std::string& job_id, std::string& instance_id, const std::string& s);



