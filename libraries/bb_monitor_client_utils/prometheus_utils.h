#include <bb_monitor_sink.grpc.pb.h>

#pragma once

std::vector<bb_monitor::Metric>
parse_prometheus_line_format_2_bb_metrics(std::string_view job_id, std::string_view instance_id, std::string_view s);
