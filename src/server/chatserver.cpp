#include "chatserver.hpp"
#include "chatservice.hpp"
#include "json.hpp"

#include <functional>
#include <string>
#include <iostream>

using namespace std;
using namespace placeholders;
using json = nlohmann::json;

ChatServer::ChatServer(EventLoop *loop,
                       const InetAddress& listenAddr,
                       const string& nameArg)
    : _server(loop,listenAddr,nameArg), _loop(loop)
{
    //注册连接回调
    _server.setConnectionCallback(bind(&ChatServer::onConnection,this,_1));
    //注册读写回调
    _server.setMessageCallback(bind(&ChatServer::onMessage,this,_1,_2,_3));
    //初始化反应堆线程数
    _server.setThreadNum(4);
};


void ChatServer::start(){
    _server.start();
}

void ChatServer::onConnection(const TcpConnectionPtr& conn){
    if(conn->connected()){
        
    }else{
        //客户端异常断开连接
        ChatService::instance()->clientCloseException(conn);
        //客户端断开连接
        conn->shutdown();
    }
}
    
//回调服务器的业务事件
void ChatServer::onMessage(const TcpConnectionPtr& conn,Buffer* buffer,Timestamp time){
    string buf = buffer->retrieveAllAsString();
    //数据反序列化
    json js = json::parse(buf);
    //通过ChatSerive去分发业务,获取业务handler 传入=> conn,js,time
    auto msgHandler = ChatService::instance()->getHandler(js["msgid"].get<int>());
    //回调消息绑定好的事件处理器，来执行相应的业务处理
    msgHandler(conn, js, time);
}