# IMServer 工具模块设计文档

## 1. 模块概述

工具模块提供IMServer所需的各种辅助功能，包括日志系统、配置管理、线程池、加密工具、时间处理等。这些工具类和函数为其他模块提供基础支持，提高代码复用性和可维护性。

## 2. 核心组件设计

### 2.1 Logger

#### 2.1.1 功能描述
日志系统负责记录系统运行过程中的各种信息，包括调试信息、错误信息、警告信息等。

#### 2.1.2 日志级别
```cpp
enum class LogLevel {
    DEBUG = 0,
    INFO = 1,
    WARN = 2,
    ERROR = 3,
    FATAL = 4
};
```

#### 2.1.3 实现细节
- 支持控制台和文件两种输出方式
- 可配置日志级别
- 日志格式包含时间戳、日志级别、线程ID、文件名、行号等信息
- 线程安全设计
- 支持日志轮转

```cpp
class Logger {
private:
    static Logger* instance_;
    static std::mutex instance_mutex_;
    
    LogLevel log_level_;
    std::ofstream log_file_;
    std::string log_file_path_;
    size_t max_log_size_; // 最大日志文件大小
    size_t max_log_files_; // 最大日志文件数量
    bool console_output_; // 是否输出到控制台
    std::mutex log_mutex_;
    
    Logger();
    
    // 格式化日志消息
    std::string formatLogMessage(LogLevel level, const std::string& file, int line, const std::string& message);
    
    // 检查并轮转日志文件
    void checkLogRotation();
    
    // 打开日志文件
    bool openLogFile(const std::string& file_path);
    
    // 关闭日志文件
    void closeLogFile();

public:
    ~Logger();
    
    // 获取单例实例
    static Logger* getInstance();
    
    // 初始化日志系统
    bool init(LogLevel level, const std::string& file_path, 
              size_t max_log_size = 10 * 1024 * 1024, // 默认10MB
              size_t max_log_files = 10, // 默认10个文件
              bool console_output = true);
    
    // 设置日志级别
    void setLogLevel(LogLevel level);
    
    // 日志输出函数
    void log(LogLevel level, const std::string& file, int line, const std::string& message);
    
    // 日志宏定义，方便使用
#define LOG_DEBUG(message) Logger::getInstance()->log(LogLevel::DEBUG, __FILE__, __LINE__, message)
#define LOG_INFO(message) Logger::getInstance()->log(LogLevel::INFO, __FILE__, __LINE__, message)
#define LOG_WARN(message) Logger::getInstance()->log(LogLevel::WARN, __FILE__, __LINE__, message)
#define LOG_ERROR(message) Logger::getInstance()->log(LogLevel::ERROR, __FILE__, __LINE__, message)
#define LOG_FATAL(message) Logger::getInstance()->log(LogLevel::FATAL, __FILE__, __LINE__, message)
};
```

### 2.2 Config

#### 2.2.1 功能描述
配置管理器负责加载和解析配置文件，提供统一的配置访问接口。

#### 2.2.2 实现细节
- 支持INI格式的配置文件
- 支持配置项的获取和设置
- 支持不同类型的配置值（字符串、整数、布尔值等）
- 配置文件自动重载

```cpp
class Config {
private:
    static Config* instance_;
    static std::mutex instance_mutex_;
    
    std::string config_file_path_;
    time_t last_modified_time_;
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> configs_;
    std::mutex config_mutex_;
    
    Config();
    
    // 加载配置文件
    bool loadConfigFile(const std::string& file_path);
    
    // 检查配置文件是否修改
    bool checkConfigFileModified();
    
    // 解析INI格式的配置行
    void parseConfigLine(const std::string& line, std::string& section, 
                         std::string& key, std::string& value);

public:
    ~Config();
    
    // 获取单例实例
    static Config* getInstance();
    
    // 初始化配置系统
    bool init(const std::string& file_path);
    
    // 获取配置值
    std::string getString(const std::string& section, const std::string& key, 
                          const std::string& default_value = "");
    
    int getInt(const std::string& section, const std::string& key, 
               int default_value = 0);
    
    long getLong(const std::string& section, const std::string& key, 
                 long default_value = 0);
    
    bool getBool(const std::string& section, const std::string& key, 
                 bool default_value = false);
    
    double getDouble(const std::string& section, const std::string& key, 
                     double default_value = 0.0);
    
    // 设置配置值
    bool setString(const std::string& section, const std::string& key, const std::string& value);
    
    // 保存配置到文件
    bool saveConfig();
};
```

### 2.3 ThreadPool

#### 2.3.1 功能描述
线程池负责管理线程的创建、任务分配和线程回收，提高线程的复用率和系统性能。

#### 2.3.2 实现细节
- 预创建一定数量的线程
- 任务队列管理
- 支持同步和异步任务
- 支持任务优先级
- 线程安全设计

```cpp
class ThreadPool {
private:
    struct Task {
        std::function<void()> func;
        int priority; // 任务优先级，值越大优先级越高
        
        Task(std::function<void()> f, int p = 0) : func(std::move(f)), priority(p) {}
        
        bool operator<(const Task& other) const {
            return priority < other.priority;
        }
    };
    
    std::vector<std::thread> threads_;
    std::priority_queue<Task> task_queue_;
    std::mutex task_mutex_;
    std::condition_variable cv_;
    bool stop_;
    size_t thread_count_;

public:
    ThreadPool(size_t thread_count = std::thread::hardware_concurrency());
    ~ThreadPool();
    
    // 启动线程池
    void start();
    
    // 关闭线程池
    void stop();
    
    // 提交任务
    template<typename Func, typename... Args>
    auto submit(Func&& func, Args&&... args) -> std::future<decltype(func(args...))> {
        using ReturnType = decltype(func(args...));
        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            std::bind(std::forward<Func>(func), std::forward<Args>(args)...));
        
        std::future<ReturnType> result = task->get_future();
        
        {   
            std::lock_guard<std::mutex> lock(task_mutex_);
            if (stop_) {
                throw std::runtime_error("ThreadPool is stopped");
            }
            
            task_queue_.emplace([task]() { (*task)(); }, 0);
        }
        
        cv_.notify_one();
        
        return result;
    }
    
    // 提交带优先级的任务
    template<typename Func, typename... Args>
    auto submitWithPriority(int priority, Func&& func, Args&&... args) -> std::future<decltype(func(args...))> {
        using ReturnType = decltype(func(args...));
        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            std::bind(std::forward<Func>(func), std::forward<Args>(args)...));
        
        std::future<ReturnType> result = task->get_future();
        
        {   
            std::lock_guard<std::mutex> lock(task_mutex_);
            if (stop_) {
                throw std::runtime_error("ThreadPool is stopped");
            }
            
            task_queue_.emplace([task]() { (*task)(); }, priority);
        }
        
        cv_.notify_one();
        
        return result;
    }
};
```

### 2.4 Crypto

#### 2.4.1 功能描述
加密工具类提供各种加密和解密算法，包括哈希算法、对称加密、非对称加密等。

#### 2.4.2 实现细节
- 支持SHA-1、SHA-256、SHA-512等哈希算法
- 支持MD5算法
- 支持Base64编码和解码
- 支持AES对称加密
- 支持RSA非对称加密

```cpp
class Crypto {
public:
    // SHA-256哈希
    static std::string sha256(const std::string& input);
    
    // SHA-512哈希
    static std::string sha512(const std::string& input);
    
    // MD5哈希
    static std::string md5(const std::string& input);
    
    // Base64编码
    static std::string base64Encode(const std::string& input);
    
    // Base64解码
    static std::string base64Decode(const std::string& input);
    
    // AES加密
    static std::string aesEncrypt(const std::string& input, const std::string& key, const std::string& iv);
    
    // AES解密
    static std::string aesDecrypt(const std::string& input, const std::string& key, const std::string& iv);
    
    // RSA加密
    static std::string rsaEncrypt(const std::string& input, const std::string& public_key);
    
    // RSA解密
    static std::string rsaDecrypt(const std::string& input, const std::string& private_key);
    
    // 生成随机字符串
    static std::string generateRandomString(size_t length);
    
    // 密码加密（结合盐值）
    static std::string hashPassword(const std::string& password, const std::string& salt = "");
    
    // 验证密码
    static bool verifyPassword(const std::string& password, const std::string& hash, const std::string& salt);
};
```

### 2.5 TimeUtil

#### 2.5.1 功能描述
时间工具类提供各种时间处理函数，包括时间格式化、时间戳转换等。

#### 2.5.2 实现细节
```cpp
class TimeUtil {
public:
    // 获取当前时间戳（秒）
    static time_t getCurrentTimestamp();
    
    // 获取当前时间戳（毫秒）
    static int64_t getCurrentTimestampMs();
    
    // 获取当前时间戳（微秒）
    static int64_t getCurrentTimestampUs();
    
    // 格式化时间
    static std::string formatTime(const time_t& time, const std::string& format = "%Y-%m-%d %H:%M:%S");
    
    // 解析时间字符串
    static time_t parseTime(const std::string& time_str, const std::string& format = "%Y-%m-%d %H:%M:%S");
    
    // 获取当前时间的字符串表示
    static std::string getCurrentTimeString(const std::string& format = "%Y-%m-%d %H:%M:%S");
    
    // 时间戳转换为本地时间
    static tm timestampToLocalTime(const time_t& timestamp);
    
    // 时间戳转换为GMT时间
    static tm timestampToGmtTime(const time_t& timestamp);
};
```

### 2.6 StringUtil

#### 2.6.1 功能描述
字符串工具类提供各种字符串处理函数，包括字符串分割、大小写转换、trim等。

#### 2.6.2 实现细节
```cpp
class StringUtil {
public:
    // 分割字符串
    static std::vector<std::string> split(const std::string& str, const std::string& delimiter);
    
    // 连接字符串
    static std::string join(const std::vector<std::string>& strs, const std::string& delimiter);
    
    // 转换为大写
    static std::string toUpper(const std::string& str);
    
    // 转换为小写
    static std::string toLower(const std::string& str);
    
    // 去除字符串前后的空格
    static std::string trim(const std::string& str);
    
    // 去除字符串前的空格
    static std::string trimLeft(const std::string& str);
    
    // 去除字符串后的空格
    static std::string trimRight(const std::string& str);
    
    // 替换字符串
    static std::string replace(const std::string& str, const std::string& from, const std::string& to);
    
    // 检查字符串是否以指定前缀开头
    static bool startsWith(const std::string& str, const std::string& prefix);
    
    // 检查字符串是否以指定后缀结尾
    static bool endsWith(const std::string& str, const std::string& suffix);
    
    // 检查字符串是否包含指定子串
    static bool contains(const std::string& str, const std::string& substr);
    
    // 字符串转整数
    static int toInt(const std::string& str, int default_value = 0);
    
    // 字符串转长整数
    static long toLong(const std::string& str, long default_value = 0);
    
    // 字符串转浮点数
    static double toDouble(const std::string& str, double default_value = 0.0);
    
    // 字符串转布尔值
    static bool toBool(const std::string& str, bool default_value = false);
    
    // 整数转字符串
    static std::string toString(int value);
    
    // 长整数转字符串
    static std::string toString(long value);
    
    // 浮点数转字符串
    static std::string toString(double value, int precision = 6);
    
    // 布尔值转字符串
    static std::string toString(bool value);
};
```

## 3. 线程安全设计

工具模块中的所有类都采用线程安全设计，主要通过以下方式实现：

- 使用互斥锁（std::mutex）保护共享资源
- 使用原子操作（std::atomic）处理简单的并发场景
- 使用条件变量（std::condition_variable）实现线程间通信
- 避免使用全局变量，或对全局变量进行适当的线程保护

## 4. 集成与使用

### 4.1 日志系统使用

```cpp
// 初始化日志系统
Logger::getInstance()->init(LogLevel::DEBUG, "logs/imserver.log");

// 输出日志
LOG_DEBUG("This is a debug message");
LOG_INFO("This is an info message");
LOG_WARN("This is a warning message");
LOG_ERROR("This is an error message");
LOG_FATAL("This is a fatal message");
```

### 4.2 配置系统使用

```cpp
// 初始化配置系统
Config::getInstance()->init("config/server.conf");

// 获取配置值
std::string server_host = Config::getInstance()->getString("server", "host", "0.0.0.0");
uint16_t server_port = Config::getInstance()->getInt("server", "port", 8080);
int thread_pool_size = Config::getInstance()->getInt("server", "thread_pool_size", 4);
bool enable_websocket = Config::getInstance()->getBool("protocol", "websocket", true);
```

### 4.3 线程池使用

```cpp
// 创建线程池
ThreadPool thread_pool(4);

// 启动线程池
thread_pool.start();

// 提交任务
auto future = thread_pool.submit([]() {
    // 任务内容
    LOG_INFO("Task executed in thread pool");
    return 42;
});

// 获取任务结果
int result = future.get();
LOG_INFO("Task result: " << result);

// 关闭线程池
thread_pool.stop();
```

### 4.4 加密工具使用

```cpp
// 密码加密
std::string password = "mypassword";
std::string salt = Crypto::generateRandomString(16);
std::string password_hash = Crypto::hashPassword(password, salt);
LOG_INFO("Password hash: " << password_hash);

// 验证密码
bool valid = Crypto::verifyPassword(password, password_hash, salt);
LOG_INFO("Password valid: " << (valid ? "true" : "false"));

// Base64编码
std::string original = "Hello, World!";
std::string encoded = Crypto::base64Encode(original);
std::string decoded = Crypto::base64Decode(encoded);
LOG_INFO("Original: " << original);
LOG_INFO("Encoded: " << encoded);
LOG_INFO("Decoded: " << decoded);
```

## 5. 性能优化

### 5.1 日志系统优化
- 使用异步日志记录，避免阻塞主线程
- 批量写入日志，减少I/O操作次数
- 使用内存缓冲区，提高写入效率

### 5.2 线程池优化
- 根据系统负载动态调整线程数量
- 使用任务优先级，确保重要任务优先执行
- 避免线程频繁创建和销毁

### 5.3 内存管理
- 使用对象池复用频繁创建和销毁的对象
- 避免内存泄漏
- 合理设置缓存大小，避免内存占用过大

## 6. 未来扩展

工具模块的未来扩展计划：

- 增加更多的加密算法支持
- 实现更完善的日志系统，支持日志级别过滤、日志聚合等功能
- 增加网络工具类，提供网络相关的辅助函数
- 实现更高效的线程池，支持任务取消、超时等功能
- 增加分布式锁支持

## 7. 总结

工具模块为IMServer提供了各种基础的辅助功能，包括日志系统、配置管理、线程池、加密工具等。这些工具类和函数提高了代码的复用性和可维护性，为其他模块提供了可靠的基础支持。工具模块采用线程安全设计，确保在多线程环境下的安全性和稳定性。