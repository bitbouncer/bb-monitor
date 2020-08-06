#include <vector>
#include <string>
#include <bb_monitor_utils/avro/bb_avro_metrics.h>

#pragma once

// this is not needed anymore???

std::vector<bb_avro_metric_t> parse_datadog_series2avro(std::string buffer);

std::vector<bb_avro_metric_t> parse_datadog_check_run2avro(std::string buffer);
