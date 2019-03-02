#ifndef UI_WEB_SERVER
#define UI_WEB_SERVER

class UIWebServerPrivate;

class UIWebServer {
public:
    static void start();
    static void stop();

private:
    static UIWebServerPrivate *d;
};

#endif // UI_WEB_SERVER
