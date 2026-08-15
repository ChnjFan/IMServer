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
-- Table structure for conversation
-- ----------------------------
DROP TABLE IF EXISTS `conversation`;
CREATE TABLE `conversation` (
    `conv_id` varchar(64) NOT NULL COMMENT '会话唯一ID，C2C格式: c2c_{小uid}_{大uid}',
    `conv_type` tinyint NOT NULL COMMENT '会话类型 1:C2C,2:Group(预留)',
    `status` tinyint NOT NULL DEFAULT 0 COMMENT '会话状态 0:正常,1:已解散',
    `last_msg_id` int DEFAULT 0 COMMENT '最新消息ID',
    `last_msg_content` text COMMENT '最新消息摘要',
    `last_time` datetime(6) DEFAULT NULL COMMENT '最新消息时间',
    `create_time` datetime DEFAULT CURRENT_TIMESTAMP,
    `update_time` datetime DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (`conv_id`),
    KEY `idx_update_time` (`update_time`)
) ENGINE=InnoDB DEFAULT CHARSET=utf16 COMMENT='IM会话主表';

-- ----------------------------
-- Table structure for user_conversation
-- ----------------------------
CREATE TABLE `user_conversation` (
     `id` int NOT NULL AUTO_INCREMENT COMMENT '主键ID',
     `uid` int NOT NULL COMMENT '用户ID',
     `conv_id` varchar(64) NOT NULL COMMENT '会话ID',
     `last_read_msg_id` int DEFAULT 0 COMMENT '最后已读消息ID，0表示无已读消息',
     `unread_count` int DEFAULT 0 COMMENT '未读消息数（冗余，避免每次COUNT）',
     `is_top` tinyint DEFAULT 0 COMMENT '是否置顶 0:否,1:是',
     `is_mute` tinyint DEFAULT 0 COMMENT '是否免打扰 0:否,1:是',
     `is_deleted` tinyint DEFAULT 0 COMMENT '是否已删除 0:否,1:是（软删除）',
     `create_time` datetime DEFAULT CURRENT_TIMESTAMP,
     `update_time` datetime DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
     PRIMARY KEY (`id`),
     UNIQUE KEY `idx_user_conv` (`uid`, `conv_id`),
     KEY `idx_uid_update_time` (`uid`, `update_time`)
) ENGINE=InnoDB DEFAULT CHARSET=utf16 COMMENT='IM用户会话表';

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
    `create_time` datetime(6) DEFAULT CURRENT_TIMESTAMP(6) COMMENT '创建时间',
    PRIMARY KEY (`id`),
    UNIQUE KEY `idx_conv_msg` (`conv_id`,`msg_id`) COMMENT '会话内消息ID索引'
) ENGINE=InnoDB AUTO_INCREMENT=1 DEFAULT CHARSET=utf16 COMMENT='IM消息表';


-- ----------------------------
-- Procedure structure for create_conversation
-- ----------------------------
DROP PROCEDURE IF EXISTS `create_conversation`;
CREATE PROCEDURE `create_conversation`(
    IN  `p_conv_id` VARCHAR(64),
    IN  `p_uid1`    INT,
    IN  `p_uid2`    INT,
    OUT `result`    CHAR(20)
)
BEGIN
    DECLARE EXIT HANDLER FOR SQLEXCEPTION BEGIN ROLLBACK; SET result = 'FAIL'; END;
    START TRANSACTION;

    -- 会话主表不存在则创建
    IF NOT EXISTS (SELECT 1 FROM `conversation` WHERE `conv_id` = p_conv_id) THEN
        INSERT INTO `conversation` (`conv_id`, `conv_type`, `status`)
        VALUES (p_conv_id, 1, 0);
    END IF;

    -- 双方各创建用户会话（已删除的则恢复）
    INSERT INTO `user_conversation` (`uid`, `conv_id`, `last_read_msg_id`, `unread_count`)
    VALUES (p_uid1, p_conv_id, 0, 0)
    ON DUPLICATE KEY UPDATE `is_deleted` = 0, `update_time` = NOW();

    INSERT INTO `user_conversation` (`uid`, `conv_id`, `last_read_msg_id`, `unread_count`)
    VALUES (p_uid2, p_conv_id, 0, 0)
    ON DUPLICATE KEY UPDATE `is_deleted` = 0, `update_time` = NOW();

    -- 获取创建时间
    SELECT `create_time` INTO result FROM `conversation` WHERE `conv_id` = p_conv_id;
    COMMIT;
END;

-- ----------------------------
-- Procedure structure for save_chat_message
-- ----------------------------
DROP PROCEDURE IF EXISTS `save_chat_message`;
CREATE PROCEDURE `save_chat_message`(
    IN  `p_conv_id`     VARCHAR(64),
    IN  `p_sender_uid`  INT,
    IN  `receive_uid`   INT,
    IN  `p_msg_type`    TINYINT,
    IN  `p_msg_id`      INT,          -- 客户端的消息 ID
    IN  `stat`          TINYINT,
    IN  `p_content`       TEXT,
    OUT `result`        INT
)
BEGIN
    DECLARE EXIT HANDLER FOR SQLEXCEPTION BEGIN ROLLBACK; SET result = -1; END;
    START TRANSACTION;

    IF EXISTS (SELECT 1 FROM `message` WHERE `conv_id` = p_conv_id AND `msg_id` = p_msg_id) THEN
        SET result = 0;
        COMMIT;
    ELSE
        -- 插入消息记录
        INSERT INTO `message` (`conv_id`, `sender_uid`, `msg_type`, `content`, `msg_id`, `status`)
        VALUES (p_conv_id, p_sender_uid, p_msg_type, p_content, p_msg_id, stat);
        -- 更新会话 last_* 字段（根据消息类型生成摘要：1-文本原样，2-[图片]，3-[文件]，4-[视频]）
        UPDATE `conversation`
        SET `last_msg_id` = p_msg_id,
            `last_msg_content` = CASE p_msg_type
                WHEN 1 THEN p_content
                WHEN 2 THEN '[图片]'
                WHEN 3 THEN '[文件]'
                WHEN 4 THEN '[视频]'
            END,
            `last_time` = NOW()
        WHERE `conv_id` = p_conv_id;
        -- 更新发送方的用户会话更新时间
        UPDATE `user_conversation`
        SET `update_time` = NOW()
        WHERE `conv_id` = p_conv_id AND `uid` = p_sender_uid;
        -- 更新接收方的用户会话计数
        IF EXISTS (SELECT 1 FROM `user_conversation` WHERE `uid` = receive_uid AND `conv_id` = p_conv_id) THEN
            UPDATE `user_conversation`
            SET `unread_count` = `unread_count` + 1, `update_time` = NOW()
            WHERE `conv_id` = p_conv_id AND `uid` = receive_uid;
        ELSE    -- 没有找到接收方会话后创建会话，并设置未读计数
            INSERT INTO `user_conversation` (`uid`, `conv_id`, `unread_count`, `last_read_msg_id`)
            VALUES (receive_uid, p_conv_id, 1, 0);
        END IF;

        SELECT `id` INTO result FROM `message` WHERE `conv_id` = p_conv_id AND `msg_id` = p_msg_id;
    END IF;
COMMIT;
END;
