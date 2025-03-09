const {app} = require('electron')
const LoginWindow = require('../windows/js/login')

class IMApp {
    constructor() {
        this.loginWindow = null;
    }

    init() {
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

    createLoginWindow() {
        this.loginWindow = new LoginWindow();
        this.loginWindow.show();
    }
}

new IMApp().init();
