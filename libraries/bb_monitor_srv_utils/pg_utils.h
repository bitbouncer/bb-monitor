#include <string>
#include <optional>
#include <postgresql/libpq-fe.h>

// default not implemented
template<class T>
std::optional<T> pg_get_value(PGresult* pgres, size_t row, std::string column_name);

template<>
std::optional<std::string> pg_get_value(PGresult* pgres, size_t row, std::string column_name);

template<>
std::optional<bool> pg_get_value(PGresult* pgres, size_t row, std::string column_name);

template<>
std::optional<int32_t> pg_get_value(PGresult* pgres, size_t row, std::string column_name);

template<>
std::optional<int64_t> pg_get_value(PGresult* pgres, size_t row, std::string column_name);

std::string sql_strip_bad_chars(std::string s);