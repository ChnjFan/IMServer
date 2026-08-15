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

 Date: 07/06/2026 10:38:17
*/

SET NAMES utf8mb4;
SET FOREIGN_KEY_CHECKS = 0;


-- ----------------------------
-- Table structure for friend_apply
-- ----------------------------
DROP TABLE IF EXISTS `friend_apply`;
CREATE TABLE `friend_apply` (
    `id` int NOT NULL AUTO_INCREMENT COMMENT '主键ID',
    `uid` int NOT NULL COMMENT '当前用户ID',
    `friend_id` int NOT NULL COMMENT '好友ID',
    `msg` varchar(128) NOT NULL COMMENT '好友申请留言',
    `status` tinyint NOT NULL DEFAULT '0' COMMENT '申请状态 0-待审批 1-已同意 2-已拒绝 3-已过期',
    `expire_time` datetime DEFAULT CURRENT_TIMESTAMP COMMENT '过期时间（7天有效期）',
    `create_time` datetime DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',
    `update_time` datetime DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT '更新时间',
    PRIMARY KEY (`id`),
    UNIQUE KEY `idx_apply_friend` (`uid`,`friend_id`) COMMENT '唯一索引：一个用户对一个好友只有一条申请记录',
    KEY `idx_uid` (`uid`),
    KEY `idx_friend_id` (`friend_id`)
) ENGINE=InnoDB AUTO_INCREMENT=1 DEFAULT CHARSET=utf16 COMMENT='IM添加好友申请表';

-- ----------------------------
-- Table structure for friend_relation
-- ----------------------------
DROP TABLE IF EXISTS `friend_relation`;
CREATE TABLE `friend_relation` (
    `id` int NOT NULL AUTO_INCREMENT COMMENT '主键ID',
    `uid` int NOT NULL COMMENT '当前用户ID',
    `friend_id` int NOT NULL COMMENT '好友ID',
    `alias` varchar(64) DEFAULT '' COMMENT '好友备注名称',
    `status` tinyint NOT NULL DEFAULT '1' COMMENT '好友状态 1-正常 2-拉黑 3-删除 4-屏蔽消息',
    `is_star` tinyint NOT NULL DEFAULT '0' COMMENT '是否星标好友 0-否 1-是',
    `is_hide` tinyint NOT NULL DEFAULT '0' COMMENT '是否隐藏该好友 0-否 1-是',
    `create_time` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '成为好友时间',
    `update_time` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT '更新时间',
    PRIMARY KEY (`id`),
    UNIQUE KEY `idx_user_friend` (`uid`,`friend_id`) COMMENT '唯一索引：一个用户对一个好友只有一条记录',
    KEY `idx_friend_id` (`friend_id`),
    KEY `idx_status` (`uid`,`status`) COMMENT '查询正常好友/拉黑列表专用索引'
) ENGINE=InnoDB AUTO_INCREMENT=1 DEFAULT CHARSET=utf16 COMMENT='IM好友关系表';

