const { BrowserWindow } = require('electron');
const path = require('path');

class LoginWindow {
    constructor() {
        this.loginWindow = new BrowserWindow({
            width: 800,
            height: 600,
            title: 'Login',
            resizable: false,
            show: true,
            autoHideMenuBar: true,
            alwaysOnTop: true,
        });

        this.loginWindow.loadURL(`file://${path.join(__dirname, '../html/login.html')}`);
    }

    show() {
        this.loginWindow.show();
    }
}

module.exports = LoginWindow;
