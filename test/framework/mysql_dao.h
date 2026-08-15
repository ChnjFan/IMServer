#include "MysqlPool.h"
#include "ConfigMgr.h"

class TestMysqlDao : public Singleton<TestMysqlDao> {
    friend class Singleton<TestMysqlDao>;
public:
    MysqlPool* get() { return mysqlPool_.get(); }
private:
    TestMysqlDao() {
        auto& config = ConfigMgr::getInstance();
        std::string host = config["Mysql"]["Host"];
        std::string port = config["Mysql"]["Port"];
        std::string user = config["Mysql"]["User"];
        std::string pwd = config["Mysql"]["Password"];
        std::string schema = config["Mysql"]["Schema"];
        mysqlPool_ = std::make_unique<MysqlPool>(host + ":" + port, user, pwd, schema, 2);
    }

    std::unique_ptr<MysqlPool> mysqlPool_;
};