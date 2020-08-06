#include <bb_monitor_srv_utils/postgres_asio.h>
#include <kspp/utils/concurrent_queue.h>
#pragma once

class pg_worker {
public:
  //using pg_callback = std::function <void(std::pair<int, std::shared_ptr<PGresult>>)>;
  //std::function<void(int ec, std::shared_ptr<PGresult>)> on_query_callback

  pg_worker(std::string connection_string);

  ~pg_worker();

  void exec(std::string statement, postgres_asio::connection::on_query_callback cb){
    queue_.push({ statement, cb});
  }

  std::string get_last_error() const {
    return _connection->last_error();
  }

private:
  struct sql_task{
    std::string statement;
    postgres_asio::connection::on_query_callback cb;
  };

  void _executor_f();

  bool exit_ = false;
  boost::asio::io_service _fg_ios;
  boost::asio::io_service _bg_ios;
  concurrent_queue<sql_task> queue_;
  std::unique_ptr<boost::asio::io_service::work> _fg_work;
  std::unique_ptr<boost::asio::io_service::work> _bg_work;
  std::thread _fg;
  std::thread _bg;
  std::thread _executor;
  std::shared_ptr<postgres_asio::connection> _connection;
  std::string _connection_string;
};