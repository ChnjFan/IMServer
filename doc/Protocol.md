# IM通信协议

IM客户端使用自定义的请求结构和请求方式。

## 请求概述

- **通讯协议**：TCP+protobuf
- **请求结构**：请求结构主要由请求头、请求体和校验值组成。

### 请求头（Header）

#### Header参数

消息请求头应该尽量精简。参考 muduo 网络库中 Protobuf 传输方案，请求头的参数如下：

| 参数       | 参数说明                                  |
|:---------|---------------------------------------|
| length   | 消息长度，包括消息头和消息体，不包含checkSum长度，uint32类型 |
| typeLen  | 消息类型长度（包含\0），uint32类型                 |
| typeName | 消息类型，String类型                         |

消息类型采用字符串类型，以'\0'结尾，方便读取。

- Message - 基础消息类，用于所有消息类型的基类。
- LoginMessage - 用于登录消息。
- TextMessage - 用于文本消息。
- ImageMessage - 用于图片消息。
- AudioMessage - 用于音频消息。
- VideoMessage - 用于视频消息。
- FileMessage - 用于文件传输消息。
- LocationMessage - 用于位置分享消息。
- StickerMessage - 用于表情贴纸消息。
- CommandMessage - 用于命令或控制消息。
- NotificationMessage - 用于系统通知消息。
- StatusMessage - 用于状态更新消息，比如在线、离线状态。
- ContactMessage - 用于联系人请求或信息。
- GroupMessage - 用于群组消息。
- BroadcastMessage - 用于广播消息。
- HistoryMessage - 用于历史消息查询。
- TypingIndicatorMessage - 用于显示“正在输入”状态的消息。
- ReadReceiptMessage - 用于已读回执。
- DeliveryReceiptMessage - 用于消息送达确认。
- ErrorMessage - 用于错误消息。
- PresenceMessage - 用于表示用户在线状态的消息。

### 校验值

虽然 TCP 提供可靠传输，但是网络传输数据必须要考虑数据被破坏的情况，校验值使用adler32算法生成。

POCO 库使用 Poco::Checksum 类支持 adler32 和 CRC-32 校验和，adler32 计算速度比 CRC-32 要快，因此消息校验采用了 adler32 校验。

### 请求体（Body）

各个消息类型的请求体采用不同的消息参数。

#### 登录鉴权

提供密码鉴权和 Token 鉴权两种方式登录。密码登录可以使用手机号、邮箱或 IM 账号鉴权；Token 登录根据终端类型和 Token 鉴权。

| 参数       | 参数说明                               |
|----------|------------------------------------|
| accid    | IM账户名称，String类型，密码登录，最大长度32        |
| token    | 登录秘钥，String类型，可以只使用 token 登录，长度128 |
| email    | 登录邮箱，String类型，最大长度64               |
| mobile   | 手机号，String类型，最大长度32                |
| password | 密码，String类型，使用bcrypt生成单向哈希，固定长度60  |

#### 登录应答

