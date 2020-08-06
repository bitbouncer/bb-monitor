#include "pg_worker.h"
#include <glog/logging.h>

using namespace std::chrono_literals;

//std::string connect_string = "user=" + c.user + " password=" + c.password + "  dbname=" + c.database_name + " host=" + c.host;

pg_worker::pg_worker(std::string connection_string)
 : _connection_string(connection_string)
 , _fg_work(std::make_unique<boost::asio::io_service::work>(_fg_ios))
 , _bg_work(std::make_unique<boost::asio::io_service::work>(_bg_ios))
 , _fg([&] { _fg_ios.run(); })
 , _bg([&] { _bg_ios.run(); })
 , _executor([&]{ _executor_f(); }){
  _connection = std::make_shared<postgres_asio::connection>(_fg_ios, _bg_ios);
  //std::string connect_string = "user=postgres password=postgres dbname=test";
  //std::string connect_string = "user=postgres password=docker dbname=bb_monitor host=localhost";
  auto res = _connection->connect(_connection_string);
}

pg_worker::~pg_worker(){
    exit_ = true;
    _fg.join();
    _bg.join();
    _executor.join();
}

void pg_worker::_executor_f() {
    while (!exit_) {
        sql_task task;
        if (queue_.try_pop(task, 500ms)) {
            LOG(INFO) << "exec..";
            auto res = _connection->exec(task.statement);
            task.cb(res.first, res.second);
            continue;
        }
        //LOG(INFO) << "doing nothing";

    }
    LOG(INFO) << "exiting pg executor";
}
