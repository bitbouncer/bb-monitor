#include <zlib.h>
#include <string>
#pragma once

std::string compress_gzip(const std::string& str, int compressionlevel = Z_BEST_COMPRESSION);
std::string compress_deflate(const std::string& str, int compressionlevel = Z_BEST_COMPRESSION);

std::string decompress_deflate(const std::string& str);
std::string decompress_gzip(const std::string& str);
std::string decompress_snappy(const std::string& str);