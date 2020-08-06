#include <vector>
#include <string>
#include <bb_monitor_sink.grpc.pb.h>
#pragma once

std::vector<bb_monitor::LogLine> parse_splunk_event(std::string buffer);
