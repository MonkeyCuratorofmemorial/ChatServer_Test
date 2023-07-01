#ifndef CHATSERVER_H
#define CHATSERVER_H

#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
#include <string>
using namespace muduo;
using namespace muduo::net;

class ChatServer{
public:
    //初始化聊天服务器对象
    ChatServer(EventLoop *loop,
               const InetAddress& listenAddr,
               const string& nameArg);
    //开启聊天服务器
    void start();
private:
    //回调服务器的连接和断开事件
    void onConnection(const TcpConnectionPtr&);
    
    //回调服务器的读写事件
    void onMessage(const TcpConnectionPtr&,
                    Buffer*,
                    Timestamp);

    TcpServer _server;//组合muduo库，实现服务器功能的类对象
    EventLoop *_loop;//指向事件循环对象的指针 （epoll）
};

#endif