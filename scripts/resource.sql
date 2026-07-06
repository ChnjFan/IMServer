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

-- ----------------------------
-- Table structure for resource_meta
-- ----------------------------
DROP TABLE IF EXISTS `resource_meta`;
CREATE TABLE `resource_meta` (
     `resource_id`     varchar(32) NOT NULL COMMENT '资源唯一ID，格式: res_{8位随机hex}',
     `conv_id`         varchar(64) NOT NULL COMMENT '来源会话ID',
     `uploader_uid`    int NOT NULL COMMENT '上传者用户ID',
     `md5`             char(32) NOT NULL COMMENT '文件MD5（小写32位hex）',
     `file_size`       bigint NOT NULL DEFAULT 0 COMMENT '文件大小(字节)',
     `file_name`       varchar(255) NOT NULL COMMENT '原始文件名',
     `file_path`       varchar(512) NOT NULL COMMENT '物理存储相对路径',
     `thumb_path`      varchar(512) DEFAULT NULL COMMENT '缩略图相对路径',
     `resource_type`   tinyint NOT NULL COMMENT '1:图片 2:文件 3:语音 4:视频 5:其他',
     `status`          tinyint NOT NULL DEFAULT 0 COMMENT '0:正常 1:上传中 2:已删除',
     `reference_count` int NOT NULL DEFAULT 1 COMMENT '引用计数',
     `width`           int DEFAULT NULL COMMENT '图片/视频宽度(px)',
     `height`          int DEFAULT NULL COMMENT '图片/视频高度(px)',
     `duration`        int DEFAULT NULL COMMENT '音视频时长(ms)',
     `create_time`     datetime(3) DEFAULT CURRENT_TIMESTAMP(3),
     `update_time`     datetime(3) DEFAULT CURRENT_TIMESTAMP(3) ON UPDATE CURRENT_TIMESTAMP(3),
     PRIMARY KEY (`resource_id`),
     KEY `idx_md5` (`md5`) COMMENT '秒传查询',
     KEY `idx_conv_id` (`conv_id`) COMMENT '会话资源列表',
     KEY `idx_uploader` (`uploader_uid`) COMMENT '上传者查询'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='资源元数据表';