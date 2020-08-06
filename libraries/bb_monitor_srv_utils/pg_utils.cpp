#include "pg_utils.h"
#include <algorithm>
#include <exception>

template<> std::optional<std::string> pg_get_value<std::string>(PGresult* pgres, size_t row, std::string column_name) {
  int column_index = PQfnumber(pgres, column_name.c_str());
  if (column_index < 0)
    throw std::invalid_argument("unknown column: " + column_name);

  if (PQgetisnull(pgres, row, column_index) == 1)
    return {};

  return PQgetvalue(pgres, row, column_index);
}

template<> std::optional<int32_t> pg_get_value<int32_t>(PGresult* pgres, size_t row, std::string column_name){
  int column_index = PQfnumber(pgres, column_name.c_str());
  if (column_index < 0)
    throw std::invalid_argument("unknown column: " + column_name);

  if (PQgetisnull(pgres, row, column_index) == 1)
    return {};

  return atoi(PQgetvalue(pgres, row, column_index));
}

template<> std::optional<int64_t> pg_get_value<int64_t>(PGresult* pgres, size_t row, std::string column_name){
  int column_index = PQfnumber(pgres, column_name.c_str());
  if (column_index < 0)
    throw std::invalid_argument("unknown column: " + column_name);

  if (PQgetisnull(pgres, row, column_index) == 1)
    return {};

  return atol(PQgetvalue(pgres, row, column_index));
}

template<> std::optional<bool> pg_get_value<bool>(PGresult* pgres, size_t row, std::string column_name){
  int column_index = PQfnumber(pgres, column_name.c_str());
  if (column_index < 0)
    throw std::invalid_argument("unknown column: " + column_name);

  if (PQgetisnull(pgres, row, column_index) == 1)
    return {};

  auto val = PQgetvalue(pgres, row, column_index);

  return (val[0] == 't' || val[0] == 'T' || val[0] == '1');
}

static inline bool is_bad_sql_char(char c){
  static const std::string chars = "\"'\r\n\t";
  return (chars.find(c) != std::string::npos);
}

//USAGE SHOULD BE REPLACED BY PREPARED STATEMENTS
std::string sql_strip_bad_chars(std::string s){
  s.erase(std::remove_if(s.begin(), s.end(), [](unsigned char x){return is_bad_sql_char(x);}), s.end());
  return s;
}

