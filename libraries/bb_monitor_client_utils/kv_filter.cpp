#include "kv_filter.h"

namespace kspp {
  int kv_filter::match(const std::string &s, std::vector<kv> &result) {
    result.clear();
    int cursor = 0;
    while (true) {
      kspp::kv kv;
      auto first_pos = s.find_first_of(separators_, cursor);
      if (first_pos == std::string::npos)
        break;

      // first found is "key1=value key2=value"
      if (s[first_pos] == kv_separator_) {
        kv.key = s.substr(cursor, first_pos - cursor);
        auto second_pos = s.find_first_of(separators_, first_pos + 1);
        if (second_pos == std::string::npos) {
          kv.val = s.substr(first_pos + 1);
          result.push_back(kv);
          break;
        } else {
          kv.val = s.substr(first_pos + 1, second_pos - (first_pos + 1));
          result.push_back(kv);
          cursor = second_pos + 1;
          continue;
        }
      } else {
        // first found is "text text key1=value key2=value"
        cursor = first_pos + 1;
      }
    }

    if (result.size() == 0)
      return 1;
    return 0;
  }
}