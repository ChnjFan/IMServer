const { ipcRenderer } = require('electron');
const errorElement = document.getElementById('error-message');

document.getElementById('login-form').addEventListener('submit', (e) => {
    e.preventDefault();
    errorElement.style.display = 'none'; // 清空错误消息
    
    const credentials = {
        username: document.getElementById('username').value,
        password: document.getElementById('password').value
    };

    ipcRenderer.send('perform-login', credentials);
});

// 事件监听
ipcRenderer.on('connection-error', (event, message) => {
    errorElement.textContent = `服务器网络异常：${message}`;
    errorElement.style.display = 'block';
});

ipcRenderer.on('login-failed', (event, message) => {
    errorElement.textContent = `登录失败：${message}`;
    errorElement.style.display = 'block';
});