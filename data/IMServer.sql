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
  `status` tinyint NOT NULL DEFAULT '0' COMMENT '申请状态 0-待审批 1-已同意 2-已拒绝 3-已过期',
  PRIMARY KEY (`id`),
  UNIQUE KEY `idx_apply_friend` (`uid`,`friend_id`) COMMENT '唯一索引：一个用户对一个好友只有一条申请记录',
  KEY `idx_uid` (`uid`),
  KEY `idx_friend_id` (`friend_id`)
) ENGINE=InnoDB AUTO_INCREMENT=4 DEFAULT CHARSET=utf16 COMMENT='IM添加好友申请表';

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
  `created_at` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '成为好友时间',
  `updated_at` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT '更新时间',
  PRIMARY KEY (`id`),
  UNIQUE KEY `idx_user_friend` (`uid`,`friend_id`) COMMENT '唯一索引：一个用户对一个好友只有一条记录',
  KEY `idx_friend_id` (`friend_id`),
  KEY `idx_status` (`uid`,`status`) COMMENT '查询正常好友/拉黑列表专用索引'
) ENGINE=InnoDB AUTO_INCREMENT=9 DEFAULT CHARSET=utf16 COMMENT='IM好友关系表';

-- ----------------------------
-- Table structure for message
-- ----------------------------
DROP TABLE IF EXISTS `message`;
CREATE TABLE `message` (
  `id` int NOT NULL AUTO_INCREMENT COMMENT '主键ID',
  `conv_id` varchar(64) NOT NULL COMMENT '所属会话',
  `sender_uid` int NOT NULL COMMENT '发送者用户ID',
  `msg_type` tinyint NOT NULL COMMENT '消息类型 1:文本,2:图片/文件',
  `content` text COMMENT '消息内容/资源URL',
  `msg_id` int NOT NULL COMMENT '消息ID',
  `status` tinyint DEFAULT '0' COMMENT '消息状态 0:发送中,1:已送达,2:已读',
  `created_at` datetime(6) DEFAULT CURRENT_TIMESTAMP(6) COMMENT '创建时间',
  PRIMARY KEY (`id`),
  UNIQUE KEY `idx_conv_msg` (`conv_id`,`msg_id`) COMMENT '会话内消息ID索引'
) ENGINE=InnoDB AUTO_INCREMENT=5 DEFAULT CHARSET=utf16 COMMENT='IM消息表';

-- ----------------------------
-- Table structure for user
-- ----------------------------
DROP TABLE IF EXISTS `user`;
CREATE TABLE `user` (
  `id` int NOT NULL AUTO_INCREMENT COMMENT '主键ID',
  `uid` int NOT NULL COMMENT '用户唯一ID',
  `name` varchar(64) NOT NULL COMMENT '用户昵称',
  `email` varchar(128) NOT NULL COMMENT '邮箱',
  `pwd` varchar(128) NOT NULL COMMENT '加密密码',
  `salt` varchar(64) NOT NULL COMMENT '密码加密盐值',
  `avatar` varchar(256) DEFAULT '' COMMENT '头像URL',
  `gender` tinyint NOT NULL DEFAULT 0 COMMENT '0未知 1男 2女',
  `create_time` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',
  `update_time` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT '更新时间',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=11 DEFAULT CHARSET=utf16 COMMENT='用户主表';

CREATE TABLE `user_profile` (
  `id` int NOT NULL AUTO_INCREMENT COMMENT '主键ID',
  `uid` int NOT NULL COMMENT '关联用户唯一ID',
  `signature` varchar(256) DEFAULT '' COMMENT '个性签名',
  `birthday` date DEFAULT NULL COMMENT '生日',
  `region` varchar(64) DEFAULT '' COMMENT '地区，省市区',
  `self_intro` text COMMENT '个人简介',
  `privacy_friend` tinyint NOT NULL DEFAULT 1 COMMENT '加好友权限：0禁止 1需验证 2直接添加',
  `privacy_chat` tinyint NOT NULL DEFAULT 1 COMMENT '陌生人私信权限',
  `blacklist_switch` tinyint NOT NULL DEFAULT 1 COMMENT '是否开启黑名单拦截',
  `extra_json` text COMMENT '预留扩展字段，JSON存储临时配置',
  `create_time` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `update_time` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`),
  UNIQUE KEY `idx_ user_id` (`uid`) COMMENT '用户信息ID索引'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='用户扩展资料表';
-- ----------------------------
-- Records of user
-- ----------------------------
BEGIN;
INSERT INTO `user` (`id`, `uid`, `name`, `email`, `pwd`, `salt`) VALUES (1, 0, 'admin', 'admin@163.com', '123456', '');
COMMIT;

-- ----------------------------
-- Table structure for user_conversation
-- ----------------------------
DROP TABLE IF EXISTS `user_conversation`;
CREATE TABLE `user_conversation` (
  `id` int NOT NULL AUTO_INCREMENT COMMENT '主键ID',
  `uid` int NOT NULL COMMENT '当前用户ID',
  `conv_id` varchar(64) NOT NULL COMMENT '会话唯一ID',
  `conv_type` tinyint NOT NULL COMMENT '会话类型 1:C2C,2:Group',
  `to_uid` bigint NOT NULL COMMENT '对方ID/群ID',
  `unread_count` int DEFAULT '0' COMMENT '未读数',
  `is_top` tinyint DEFAULT '0' COMMENT '是否置顶',
  `is_mute` tinyint DEFAULT '0' COMMENT '是否静音',
  `last_msg_id` int DEFAULT NULL COMMENT '最新消息ID',
  `last_msg_content` text COMMENT '最新消息摘要',
  `last_time` datetime(6) DEFAULT NULL COMMENT '最新消息时间',
  `created_at` datetime DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',
  `updated_at` datetime DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT '更新时间',
  PRIMARY KEY (`id`),
  UNIQUE KEY `idx_user_conv` (`uid`,`conv_id`) COMMENT '用户会话索引',
  KEY `idx_user_time` (`uid`,`last_time`) COMMENT '用户最新消息时间索引'
) ENGINE=InnoDB AUTO_INCREMENT=35 DEFAULT CHARSET=utf16 COMMENT='IM用户会话表';

-- ----------------------------
-- Table structure for user_id
-- ----------------------------
DROP TABLE IF EXISTS `user_id`;
CREATE TABLE `user_id` (
  `id` int NOT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf16;

-- ----------------------------
-- Records of user_id
-- ----------------------------
BEGIN;
INSERT INTO `user_id` (`id`) VALUES (1);
COMMIT;

-- ----------------------------
-- Procedure structure for add_friend_relation
-- ----------------------------
DROP PROCEDURE IF EXISTS `add_friend_relation`;
delimiter ;;
CREATE PROCEDURE `add_friend_relation`(IN `auth_uid` INT, 
    IN `apply_uid` INT, 
    OUT `result` INT)
BEGIN
    -- 如果在执行过程中遇到任何错误，则回滚事务
    DECLARE EXIT HANDLER FOR SQLEXCEPTION
    BEGIN
        -- 回滚事务
        ROLLBACK;
        -- 设置返回值为-1，表示错误
        SET result = -1;
    END;
    
    -- 开始事务
    START TRANSACTION;

    -- 1. 更新好友申请状态为 已同意(1)
    IF EXISTS (SELECT 1 FROM friend_apply WHERE uid = apply_uid) THEN
				UPDATE friend_apply SET status = 1 WHERE uid = apply_uid AND friend_id = auth_uid AND status = 0;
				-- 2. 新增双向好友关系
				INSERT INTO friend_relation(uid,friend_id,alias) VALUES(auth_uid,apply_uid,'');
				INSERT INTO friend_relation(uid,friend_id,alias) VALUES(apply_uid,auth_uid,'');
				SET result = 0;
        COMMIT;
    ELSE
				SET result = -1;	-- 没有申请记录
				COMMIT;
    END IF;
    
END
;;
delimiter ;

-- ----------------------------
-- Procedure structure for reg_user
-- ----------------------------
DROP PROCEDURE IF EXISTS `reg_user`;
delimiter ;;
CREATE PROCEDURE `reg_user`(IN `new_name` VARCHAR(64),
    IN `new_email` VARCHAR(128),
    IN `new_pwd` VARCHAR(128),
    IN `pwd_salt` VARCHAR(64),
    OUT `result` INT)
BEGIN
    -- 如果在执行过程中遇到任何错误，则回滚事务
    DECLARE EXIT HANDLER FOR SQLEXCEPTION
    BEGIN
        -- 回滚事务
        ROLLBACK;
        -- 设置返回值为-1，表示错误
        SET result = -1;
    END;
    
    -- 开始事务
    START TRANSACTION;

    -- 检查用户名是否已存在
    IF EXISTS (SELECT 1 FROM `user` WHERE `name` = new_name) THEN
        SET result = 0; -- 用户名已存在
        COMMIT;
    ELSE
        -- 用户名不存在，检查email是否已存在
        IF EXISTS (SELECT 1 FROM `user` WHERE `email` = new_email) THEN
            SET result = 0; -- email已存在
            COMMIT;
        ELSE
            -- email也不存在，更新user_id表
            UPDATE `user_id` SET `id` = `id` + 1;
            
            -- 获取更新后的id
            SELECT `id` INTO @new_id FROM `user_id`;
            
            -- 在user表中插入新记录
            INSERT INTO `user` (`uid`, `name`, `email`, `pwd`, `salt`) VALUES (@new_id, new_name, new_email, new_pwd, pwd_salt);
            -- 设置result为新插入的uid
            SET result = @new_id; -- 插入成功，返回新的uid
            COMMIT;
        
        END IF;
    END IF;
    
END
;;
delimiter ;

-- ----------------------------
-- Procedure structure for save_chat_message
-- ----------------------------
DROP PROCEDURE IF EXISTS `save_chat_message`;
delimiter ;;
CREATE PROCEDURE `save_chat_message`(IN `p_conv_id` VARCHAR(64), 
    IN `sender_uid` INT, 
    IN `recver_uid` INT,
    IN `msg_type` TINYINT, 
    IN `msg_id` INT,
    IN `stat` TINYINT,
    IN `content` TEXT,
    OUT `result` INT)
BEGIN
    -- 如果在执行过程中遇到任何错误，则回滚事务
    DECLARE EXIT HANDLER FOR SQLEXCEPTION
    BEGIN
        -- 回滚事务
        ROLLBACK;
        -- 设置返回值为-1，表示错误
        SET result = -1;
				SHOW ERRORS;
    END;
    
    -- 开始事务
    START TRANSACTION;

    -- 检查消息是否已存在
    IF EXISTS (SELECT 1 FROM `message` WHERE `conv_id` = p_conv_id AND `msg_id` = msg_id) THEN
        SET result = 1; -- 消息已存在
        COMMIT;
    ELSE
				SET result = 0;
        -- 消息不存在，插入新消息
				INSERT INTO message(conv_id, sender_uid, msg_type, content, msg_id, status)
				VALUES (p_conv_id, sender_uid, msg_type, content, msg_id, stat);
			  -- 更新发送方会话表
				UPDATE  user_conversation 
				SET last_msg_id = msg_id, last_msg_content = content, last_time = NOW()
				WHERE conv_id = p_conv_id AND uid = sender_uid;
				
				-- 接收方如果没有创建会话，主动创建并更新
				IF EXISTS (SELECT 1 FROM user_conversation WHERE uid = recver_uid AND conv_id = p_conv_id) THEN
						-- 有会话：更新
						UPDATE user_conversation
						SET last_msg_id = msg_id, last_msg_content = content, unread_count = unread_count+1, last_time = NOW()
						WHERE conv_id = p_conv_id AND uid = recver_uid;
				ELSE
						-- 无会话：自动创建（关键！）
						INSERT INTO user_conversation(uid, conv_id, conv_type, to_uid, unread_count, last_msg_id, last_msg_content,last_time)
						VALUES (recver_uid, p_conv_id, 1, sender_uid, 1, msg_id, content, NOW());
				END IF;
    END IF;
    COMMIT;
END
;;
delimiter ;

SET FOREIGN_KEY_CHECKS = 1;
