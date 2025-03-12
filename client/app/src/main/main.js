const {app, ipcMain} = require('electron')
const LoginWindow = require('../windows/js/login')

class IMApp {
    constructor() {
        this.loginWindow = null;
        this.connection = null;
    }

    init() {
        this.initApp();
        this.initIpc();
    }

    initApp() {
        app.on('ready', ()=> {
            this.createLoginWindow();
        });

        app.on('window-all-closed', ()=> {
            app.quit();
        });

        app.on('activate', ()=> {
            if (this.loginWindow === null) {
                this.createLoginWindow();
            }
            else {
                this.loginWindow.show();
            }
        });
    }

    initIpc() {
        ipcMain.on('badge-changed', (event, num) => {
            if (process.platform === "darwin") {
                app.dock.setBadge(num);
                if (num) {
                    this.tray.setTitle(` ${num}`);
                } else {
                    this.tray.setTitle('');
                }
            } else if (process.platform === "linux" || process.platform === "win32") {
                app.setBadgeCount(num);
                this.tray.setUnreadStat(num > 0? 1 : 0);
            } else {
                // 处理其他平台的逻辑
                console.log(`Unsupported platform: ${process.platform}`);
            }
        });
    }

    createLoginWindow() {
        this.loginWindow = new LoginWindow();
        
        // 添加开发者工具快捷键监听（F12）
        this.loginWindow.loginWindow.webContents.on('before-input-event', (event, input) => {
            if (input.key === 'F12') {
                this.toggleDevTools();
            }
        });

        this.loginWindow.show();
        // 默认打开开发者工具（开发环境建议）
        this.loginWindow.loginWindow.webContents.openDevTools();
    }

    toggleDevTools() {
        if (this.loginWindow) {
            const contents = this.loginWindow.webContents;
            if (contents.isDevToolsOpened()) {
                contents.closeDevTools();
            } else {
                contents.openDevTools();
            }
        }
    }
}

new IMApp().init();
