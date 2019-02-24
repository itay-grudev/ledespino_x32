#ifndef UI_WEB_SERVER_H
#define UI_WEB_SERVER_H

class UIWebServerPrivate;

class UIWebServer {
public:
    static void start();
    static void stop();

private:
    static UIWebServerPrivate *d;
};

#endif // UI_WEB_SERVER_H
