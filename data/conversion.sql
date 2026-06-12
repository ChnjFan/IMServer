/*
 Navicat Premium Dump SQL

 Source Server         : IMServer
 Source Server Type    : MySQL
 Source Server Version : 90700 (9.7.0)
 Source Host           : localhost:3306
 Source Schema         : IMServer

 Target Server Type    : MySQL
 Target Server Version : 90700 (9.7.0)
 File Encoding         : 65001

 Date: 19/05/2026 19:53:33
*/

SET NAMES utf8mb4;
SET FOREIGN_KEY_CHECKS = 0;

-- ----------------------------
-- Table structure for user
-- ----------------------------
-- 用户会话表
CREATE TABLE IF NOT EXISTS `user_conversation` (
  `id` int NOT NULL AUTO_INCREMENT PRIMARY KEY COMMENT '主键ID',
  `uid` int NOT NULL COMMENT '当前用户ID',
  `conv_id` VARCHAR(64) NOT NULL COMMENT '会话唯一ID',
  `conv_type` TINYINT NOT NULL COMMENT '会话类型 1:C2C,2:Group',
  `to_uid` BIGINT NOT NULL COMMENT '对方ID/群ID',
  `unread_count` int DEFAULT 0 COMMENT '未读数',
  `is_top` TINYINT DEFAULT 0 COMMENT '是否置顶',
  `is_mute` TINYINT DEFAULT 0 COMMENT '是否静音',
  `last_msg_id` int COMMENT '最新消息ID',
  `last_msg_content` TEXT COMMENT '最新消息摘要',
  `last_time` DATETIME(6) COMMENT '最新消息时间',
  `created_at` DATETIME DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',
  `updated_at` DATETIME DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT '更新时间',
  UNIQUE INDEX idx_user_conv (uid, conv_id)  COMMENT '用户会话索引',
  INDEX idx_user_time (uid, last_time) COMMENT '用户最新消息时间索引'
) ENGINE=InnoDB DEFAULT CHARSET=utf16 COMMENT='IM用户会话表';

CREATE TABLE IF NOT EXISTS `message` (
  `id` int NOT NULL AUTO_INCREMENT PRIMARY KEY COMMENT '主键ID',
  `conv_id` VARCHAR(64) NOT NULL COMMENT '所属会话',    
  `sender_uid` int NOT NULL COMMENT '发送者用户ID',
  `msg_type` TINYINT NOT NULL COMMENT '消息类型 1:文本,2:图片/文件',
  `content` TEXT COMMENT '消息内容/资源URL',
  `msg_id` int NOT NULL COMMENT '消息ID',
  `status` TINYINT DEFAULT 0 COMMENT '消息状态 0:发送中,1:已送达,2:已读',
  `created_at` DATETIME(6) DEFAULT CURRENT_TIMESTAMP(6) COMMENT '创建时间',
  UNIQUE INDEX idx_conv_msg (conv_id, msg_id) COMMENT '会话内消息ID索引'
) ENGINE=InnoDB DEFAULT CHARSET=utf16 COMMENT='IM消息表';

-- ----------------------------
-- Records of message
-- ----------------------------
-- BEGIN;
-- INSERT INTO `message` (`id`, `conv_id`, `sender_uid`, `msg_type`, `msg_id`, `content`, `status`) VALUES (0, 0, 'admin', 'admin@163.com');
-- COMMIT;

SET FOREIGN_KEY_CHECKS = 1;
