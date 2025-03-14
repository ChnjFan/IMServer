const { BrowserWindow, ipcMain } = require('electron');
const net = require('net');
const path = require('path');

class LoginWindow {
    constructor() {
        this.loginWindow = new BrowserWindow({
            // 添加webPreferences配置
            webPreferences: {
                nodeIntegration: true,
                contextIsolation: false,
                enableRemoteModule: true
            },
            width: 800,
            height: 600,
            title: 'Login',
            resizable: true,    // 后续不允许改变大小
            show: true,
            autoHideMenuBar: true,
            alwaysOnTop: true,
        });

        this.loginWindow.loadURL(`file://${path.join(__dirname, '../html/login.html')}`);

        // 监听渲染进程的登录请求
        this.loginWindow.webContents.on('did-finish-load', () => {
            // 初始化IPC通信
            ipcMain.once('perform-login', (event, credentials) => {
                const client = new net.Socket();
                
                // 建立TCP连接
                client.connect(10001, '192.168.3.13', () => {
                    console.log('Connected to server');
                    // 发送认证数据
                    client.write(JSON.stringify({
                        type: 'auth',
                        username: credentials.username,
                        password: credentials.password
                    }));
                });

                // 设置响应超时（5秒）
                client.setTimeout(5000);
                let hasHandled = false;
                // 接收服务端响应
                client.on('data', (data) => {
                    if (hasHandled) return;
                    hasHandled = true;
                    console.log('Received data:', data.toString());
                    try {
                        const response = JSON.parse(data.toString());
                        if (response.success) {
                            this.loginWindow.close();
                        } else {
                            event.sender.send('login-failed', response.message);
                        }
                    } catch (e) {
                        event.sender.send('login-failed', 'Invalid server response');
                    }
                    client.end(() => {
                        client.destroy();
                    });
                });

                // 错误处理
                client.on('error', () => {
                    if (hasHandled) return;
                    hasHandled = true;
                    let message = '服务器响应超时，请检查网络状况';
                    event.sender.send('connection-error', message);
                    client.end(() => {
                        client.destroy();
                    });
                });

                // 超时处理
                client.on('timeout', () => {
                    if (hasHandled) return;
                    hasHandled = true;
                    event.sender.send('connection-error', '服务器响应超时，请检查网络状况');
                    client.end(() => {
                        client.destroy();
                    });
                });
            });
        });
    }

    show() {
        this.loginWindow.show();
    }
}

module.exports = LoginWindow;
