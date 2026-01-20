#pragma once

#include <string>
#include <unordered_map>

namespace imserver {
namespace tool {

/**
 * @brief JSON工具类
 * 提供JSON字符串与元数据unordered_map<string, string>之间的转换功能
 */
class JsonUtils {
public:
    /**
     * @brief 将JSON字符串转换为元数据unordered_map
     * @param json_str JSON字符串
     * @param metadata 输出的元数据unordered_map
     * @return 是否转换成功
     */
    static bool jsonToMetadata(const std::string& json_str, std::unordered_map<std::string, std::string>& metadata);
    
    /**
     * @brief 将元数据unordered_map转换为JSON字符串
     * @param metadata 元数据unordered_map
     * @param json_str 输出的JSON字符串
     * @return 是否转换成功
     */
    static bool metadataToJson(const std::unordered_map<std::string, std::string>& metadata, std::string& json_str);
    
    /**
     * @brief 从JSON字符串中提取指定路径的值
     * @param json_str JSON字符串
     * @param path JSON路径，如 "user.name"
     * @param value 输出的值
     * @return 是否提取成功
     */
    static bool getJsonValue(const std::string& json_str, const std::string& path, std::string& value);
    
    /**
     * @brief 检查JSON字符串是否有效
     * @param json_str JSON字符串
     * @return 是否有效
     */
    static bool isValidJson(const std::string& json_str);
};

} // namespace tool
} // namespace imserver
