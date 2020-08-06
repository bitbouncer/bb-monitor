#include <vector>
#include <string>

#pragma once

namespace kspp {
  struct kv {
    std::string key;
    std::string val;
  };

  class kv_filter {
  public:
    inline kv_filter(std::string term_separators = " []", char kv_separator = '=') : kv_separator_(kv_separator) {
      separators_ += term_separators;
      separators_ += kv_separator_;
    }

    int match(const std::string &s, std::vector<kv> &result);

  private:
    //std::string term_separators_;
    char kv_separator_;
    std::string separators_;

  };
}