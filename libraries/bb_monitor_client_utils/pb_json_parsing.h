#include <vector>
#include <string>
#include <bb_monitor_sink.grpc.pb.h>

#pragma once

std::vector<bb_monitor::Metric> parse_datadog_series2pb(std::string buffer);

std::vector<bb_monitor::Metric> parse_datadog_check_run2pb(std::string buffer);
