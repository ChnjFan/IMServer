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
    `avatar_url` varchar(256) DEFAULT '' COMMENT '头像URL',
    `gender` tinyint NOT NULL DEFAULT 0 COMMENT '0未知 1男 2女',
    `create_time` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',
    `update_time` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT '更新时间',
    PRIMARY KEY (`id`),
    UNIQUE KEY `idx_uid` (`uid`) COMMENT '用户uid索引',
    UNIQUE KEY `idx_email` (`email`) COMMENT '用户email索引'
) ENGINE=InnoDB AUTO_INCREMENT=11 DEFAULT CHARSET=utf16 COMMENT='用户主表';

-- ----------------------------
-- Table structure for user_profile
-- ----------------------------
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
    UNIQUE KEY `idx_user_id` (`uid`) COMMENT '用户信息ID索引'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='用户扩展资料表';

-- ----------------------------
-- Table structure for user_id
-- ----------------------------
DROP TABLE IF EXISTS `user_id`;
CREATE TABLE `user_id` (
   `id` int NOT NULL,
   PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf16;

-- ----------------------------
-- Procedure structure for reg_user
-- ----------------------------
DROP PROCEDURE IF EXISTS `reg_user`;

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
        -- 在 user_profile 表中插入对应记录
        INSERT INTO `user_profile` (`uid`, `privacy_friend`,`privacy_chat`, `blacklist_switch`) VALUES (@new_id, 0, 1, 1);
        -- 设置result为新插入的uid
        SET result = @new_id; -- 插入成功，返回新的uid
        COMMIT;

        END IF;
    END IF;
END;

-- ----------------------------
-- Records of user_id
-- ----------------------------
BEGIN;
INSERT INTO `user_id` (`id`) VALUES (1001);
COMMIT;

-- ----------------------------
-- Records of user
-- ----------------------------
BEGIN;
INSERT INTO `user` (`id`, `uid`, `name`, `email`, `pwd`, `salt`) VALUES (1, 0, 'admin', 'admin@163.com', '123456', '');
INSERT INTO `user` (`id`, `uid`, `name`, `email`, `pwd`, `salt`) VALUES (2, 1, 'Fan', 'fan@163.com', '123456', '');
COMMIT;

-- ----------------------------
-- Records of user_profile
-- ----------------------------
BEGIN;
INSERT INTO `user_profile` (`id`, `uid`) VALUES (1, 0);
INSERT INTO `user_profile` (`id`, `uid`) VALUES (2, 1);
COMMIT;

