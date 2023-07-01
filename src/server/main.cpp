#include "chatserver.hpp"
#include "chatservice.hpp"
#include <iostream>
#include <signal.h>
using namespace std;

//处理服务器ctrl+c结束后，重置user的登录状态
void resetHandler(int){
    ChatService::instance()->reset();
    exit(0);
}

int main(int argc, char* argv[]){
    
    if(argc < 3){
        cerr << "command invalid! example: ./ChatServer ip port" << endl;
        exit(-1);
    }
    
    //解析通过命令行参数传递的ip和port
    char* ip = argv[1];
    uint16_t port = atoi(argv[2]);
    
    signal(SIGINT, resetHandler);

    EventLoop loop;
    InetAddress addr(ip, port);
    ChatServer server(&loop, addr, "ChatServer");
    //开启服务器服务
    server.start();
    //开启循环监听事件
    loop.loop();
    return 0;
}