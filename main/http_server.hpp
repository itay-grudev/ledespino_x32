#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

class HTTPServerPrivate;

class HTTPServer {
public:
    static void start();
    static void stop();

private:
    static HTTPServerPrivate *d;
};

#endif // HTTP_SERVER_H
