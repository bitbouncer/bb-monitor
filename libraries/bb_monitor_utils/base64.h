#include <string>
#include <vector>
#include <boost/uuid/uuid.hpp>
#pragma once

std::string encode64(const std::string &val);
std::string encode64(const boost::uuids::uuid &val);
std::string encode64(const std::vector<std::uint8_t>& data);
std::string rnd_uuid_base64();


