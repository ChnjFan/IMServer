#include "JsonUtils.h"
#include <nlohmann/json.hpp>
#include <vector>
#include <sstream>

using json = nlohmann::json;

namespace imserver {
namespace tool {

/**
 * @brief 分割路径字符串
 * @param path 路径字符串
 * @param delimiter 分隔符
 * @return 分割后的路径部分
 */
static std::vector<std::string> splitPath(const std::string& path, const std::string& delimiter) {
    std::vector<std::string> parts;
    size_t start = 0;
    size_t end = path.find(delimiter);
    
    while (end != std::string::npos) {
        parts.push_back(path.substr(start, end - start));
        start = end + delimiter.length();
        end = path.find(delimiter, start);
    }
    
    parts.push_back(path.substr(start));
    return parts;
}



bool JsonUtils::jsonToMetadata(const std::string& json_str, std::unordered_map<std::string, std::string>& metadata) {
    try {
        // 解析JSON字符串
        json msg_json = json::parse(json_str);
        
        // 清空输出map
        metadata.clear();
        
        // 遍历JSON对象，转换为unordered_map
        for (auto& [key, value] : msg_json.items()) {
            if (value.is_string()) {
                metadata[key] = value.get<std::string>();
            } else if (value.is_number()) {
                metadata[key] = std::to_string(value.get<double>());
            } else if (value.is_boolean()) {
                metadata[key] = value.get<bool>() ? "true" : "false";
            } else if (value.is_object()) {
                // 递归处理嵌套对象
                std::unordered_map<std::string, std::string> nested_metadata;
                std::string nested_json = value.dump();
                if (jsonToMetadata(nested_json, nested_metadata)) {
                    for (const auto& [nested_key, nested_value] : nested_metadata) {
                        metadata[key + "." + nested_key] = nested_value;
                    }
                }
            } else if (value.is_array()) {
                // 数组转换为逗号分隔的字符串
                std::string array_str;
                for (size_t i = 0; i < value.size(); ++i) {
                    if (i > 0) {
                        array_str += ",";
                    }
                    if (value[i].is_string()) {
                        array_str += value[i].get<std::string>();
                    } else if (value[i].is_number()) {
                        array_str += std::to_string(value[i].get<double>());
                    } else if (value[i].is_boolean()) {
                        array_str += value[i].get<bool>() ? "true" : "false";
                    }
                }
                metadata[key] = array_str;
            }
        }
        
        return true;
    } catch (const json::exception& e) {
        return false;
    }
}

bool JsonUtils::metadataToJson(const std::unordered_map<std::string, std::string>& metadata, std::string& json_str) {
    try {
        // 创建JSON对象
        json root;
        
        // 遍历metadata map，转换为JSON
        for (const auto& [key, value] : metadata) {
            // 检查是否是嵌套路径
            size_t dot_pos = key.find('.');
            if (dot_pos != std::string::npos) {
                // 处理嵌套路径
                std::string parent_key = key.substr(0, dot_pos);
                std::string child_key = key.substr(dot_pos + 1);
                
                // 递归处理嵌套
                if (!root.contains(parent_key)) {
                    root[parent_key] = json::object();
                }
                
                json& parent = root[parent_key];
                parent[child_key] = value;
            } else {
                // 直接设置顶级键值对
                root[key] = value;
            }
        }
        
        // 转换为字符串
        json_str = root.dump(2); // 带缩进
        return true;
    } catch (const json::exception& e) {
        return false;
    }
}

bool JsonUtils::getJsonValue(const std::string& json_str, const std::string& path, std::string& value) {
    try {
        // 解析JSON字符串
        json msg_json = json::parse(json_str);
        
        // 处理路径
        std::vector<std::string> path_parts = splitPath(path, ".");
        
        json current = msg_json;
        
        // 遍历路径部分
        for (const auto& part : path_parts) {
            if (current.contains(part)) {
                current = current[part];
            } else {
                return false;
            }
        }
        
        // 转换为字符串
        if (current.is_string()) {
            value = current.get<std::string>();
        } else if (current.is_number()) {
            value = std::to_string(current.get<double>());
        } else if (current.is_boolean()) {
            value = current.get<bool>() ? "true" : "false";
        } else {
            return false;
        }
        
        return true;
    } catch (const json::exception& e) {
        return false;
    }
}

bool JsonUtils::isValidJson(const std::string& json_str) {
    try {
        json::parse(json_str);
        return true;
    } catch (const json::exception& e) {
        return false;
    }
}

} // namespace tool
} // namespace imserver
