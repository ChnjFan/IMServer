const Redis = require('ioredis');
const config_module = require('./config')
const {code_prefix} = require("./const");

// 创建 Redis 客户端实例
const RedisClient = new Redis({
    host: config_module.redis_host,
    port: config_module.redis_port,
    password: config_module.redis_password,
});

// 监听错误信息
RedisClient.on("error", function (err) {
    console.log("RedisClient Connect Error: ", err);
    RedisClient.quit();
});

// 根据 key 获取 value
async function GetRedis(key) {
    try {
        // 等待操作结果，使用 await 要声明函数为异步 async
        const result = await RedisClient.get(key)
        if (null == result) {
            console.log('result:','<'+result+'>', 'This key is not found');
            return null
        }
        console.log('result:','<'+result+'>', 'Get key successfully');
        return result
    }
    catch (error) {
        console.log('GetRedis error is ', error);
        return null
    }
}

// 查询 key 值是否存在
async function QueryRedis(key) {
    try {
        const result = await RedisClient.exists(key)
        if (result === 0) { // 严格相等判断
            console.log('result:','<'+result+'>', 'Get key is null');
            return null
        }
        console.log('result:','<'+result+'>', 'whit this value');
        return result
    }
    catch (error) {
        console.log('GetRedis error is ', error);
        return null
    }
}

// 设置 key 和 value 并过期时间
async function SetRedisExpire(key, value, expires) {
    try {
        await RedisClient.set(key, value)
        RedisClient.expire(key, expires)
        return true
    }
    catch (error) {
        console.log('SetRedisExpire error is ', error);
        return false
    }
}

// 退出函数
function Quit() {
    RedisClient.quit();
}

module.exports = {
    GetRedis,
    QueryRedis,
    SetRedisExpire,
    Quit,
}