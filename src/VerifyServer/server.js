const grpc = require('@grpc/grpc-js');
const message_proto = require('./proto');
const emailModule = require('./email');
const const_module = require('./const');
const { v4: uuidv4 } = require('uuid');
const config_mod = require('./config');
const redis_module = require('./redis');

async function GetVerifyCode(call, callback) {
    console.log('GetVerifyCode email:', call.request.email);
    try {
        // 现在 Redis 查询是否有没有过期的验证码
        console.log('Redis GET ', config_mod.code_prefix + call.request.email);
        let query_res = await redis_module.GetRedis(config_mod.code_prefix + call.request.email);
        console.log("query_res is ", query_res);
        let uniqueId = query_res;
        if (query_res == null) {
            uniqueId = uuidv4(); // 生成唯一ID
            if (uniqueId.length > 4) {// 截取前 4 位作为验证码
                uniqueId = uniqueId.substring(0, 4);
            }
            // 设置 10 分钟的过期时间
            let bres = await redis_module.SetRedisExpire(config_mod.code_prefix + call.request.email, uniqueId, 600);
             if (!bres) {
                 callback(null, {
                     email: call.request.emcall.request.emailail,
                     error: const_module.Errors.RedisErr
                 });
                 return;
             }
             console.log('Redis Expired with ', config_mod.code_prefix + config_mod.email_user);
        }

        console.log('uniqueId:', uniqueId);
        let text_str = '您的验证码是 ' + uniqueId + '，请在 3 分钟内输入';
        // 发送邮件
        let mailOptions = {
            from: config_mod.email_user,
            to: call.request.email,
            subject: '【账户注册】验证码',
            text: text_str,
        };

        let send_result = await emailModule.SendEmail(mailOptions);
        console.log('send_result:', send_result);

        callback(null, {
            email: call.request.email,
            error: const_module.Errors.SUCCESS
        });
    } catch (error) {
        console.error('发送邮件失败:', error);
        callback(null, {
            email: call.request.email,
            error: const_module.Errors.Exception
        });
    }
}

function main() {
    console.log('VerifyServer is running...');
    const server = new grpc.Server();
    server.addService(message_proto.VerifyService.service, {
        GetVerifyCode: GetVerifyCode,
    });
    server.bindAsync('0.0.0.0:50051', grpc.ServerCredentials.createInsecure(), (err) => {
        if (err) {
            console.error('绑定失败:', err);
            process.exit(1);
        }
        // server.start();
        console.log('VerifyServer is running on 0.0.0.0:50051');
    });
}

main();