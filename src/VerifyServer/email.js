const nodemailer = require('nodemailer');
const config_mod = require('./config');

/**
 * 创建发送邮件代理
 */
const transporter = nodemailer.createTransport({
    host: 'smtp.163.com',
    port: 465,
    secure: true,
    auth: {
        user: config_mod.email_user,    // 发送邮件的邮箱账号
        pass: config_mod.email_password,// 发送邮件的邮箱密码或授权码
    },
});

/**
 * 发送邮件
 * @param {*} mailOptions 邮件选项
 * @returns 
 */
function sendEmail(mailOptions) {
    return new Promise((resolve, reject) => {
        transporter.sendMail(mailOptions, (error, info) => {
            if (error) {
                console.error('发送邮件失败:', error);
                reject(error);
            } else {
                console.log('发送邮件成功:', info);
                console.log('邮件发送链接:', nodemailer.getTestMessageUrl(info));
                resolve(info);
            }
        });
    });
}

module.exports.SendEmail = sendEmail;