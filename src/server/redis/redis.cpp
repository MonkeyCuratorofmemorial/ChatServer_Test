#include "redis.hpp"
#include <iostream>
using namespace std;

Redis::Redis():_publish_context(nullptr), _subscribe_context(nullptr)
{
};

Redis::~Redis(){
    if(_publish_context!=nullptr){
        redisFree(_publish_context);
    }

    if(_subscribe_context!=nullptr){
        redisFree(_subscribe_context);
    }
};

//连接redis服务器
bool Redis::connect(){
    //负责publish发布消息的上下文连接
    _publish_context = redisConnect("127.0.0.1", 6379);
    if(nullptr == _publish_context){
        cerr << "redis connect publish fail" <<endl;
        return false;
    }

    //负责subscribe发布消息的上下文连接
    _subscribe_context = redisConnect("127.0.0.1", 6379);
    if(nullptr == _subscribe_context){
        cerr << "redis connect subscribe fail" <<endl;
        return false;
    }

    //在单独的线程中。监听通道上的事件，有消息给业务层进行上报
    //因为subscribe在reids中会阻塞该通道等待publish发布
    thread t([&](){
        observer_channel_message();
    });
    t.detach();

    cout << "redis connect success!" << endl;
    return true;
};

//向redis指定的通道channel发布消息
bool Redis::publish(int channel, string message){
    //redisCommand 相当于 redisAppendCommand + redisBufferWrite + redisGetReply
    redisReply *reply = (redisReply *) redisCommand(_publish_context, "PUBLISH %d %s", channel, message.c_str());
    if(nullptr == reply){
        cerr << "publish command failed" << endl;
        return false;
    }
    
    freeReplyObject(reply);
    return true;
};

//从redis指定的通道channel订阅消息
bool Redis::subscribe(int channel){
    //redisAppendCommand + redisBufferWrite + redisGetReply
    //这里只做订阅消息，不接收消息
    //只负责发送命令，不阻塞接收redis server响应消息，否则和notifyMsg线程抢占相应资源
    //redisAppendCommand基于pipe的调用方式只将待发送的命令写入到上下文对象的输出缓冲区中
    if(REDIS_ERR == redisAppendCommand(this->_subscribe_context, "SUBSCRIBE %d", channel)){
        cerr << "subscribe command failed" << endl;
        return false;
    }
    //redisBufferWrite可以循环将缓冲区数据发送，直到缓冲区数据发送完毕(done被置为1)
    int done = 0;
    while(!done){
        if(REDIS_ERR == redisBufferWrite(this->_subscribe_context,&done)){
            cerr << "subscribe write failed" << endl;
            return false;
        }
    }
    //redisGetReply专门以阻塞的方式读取_subscribe_context传输的数据
    //所以创建独立的线程去接收数据
    return true;
};

//从redis指定的通道channelq取消订阅消息
bool Redis::unsubscribe(int channel){
    //redisCommand 相当于 redisAppendCommand + redisBufferWrite + redisGetReply
    if(REDIS_ERR == redisAppendCommand(this->_subscribe_context, "UNSUBSCRIBE %d", channel)){
        cerr << "unsubscribe command failed" << endl;
        return false;
    }
    //redisBufferWrite可以循环发送缓冲区，直到缓冲区数据发送完毕(done被置为1)
    int done = 0;
    while(!done){
        if(REDIS_ERR == redisBufferWrite(this->_subscribe_context,&done)){
            cerr << "unsubscribe write failed" << endl;
            return false;
        }
    }

    //redisGetReply专门以阻塞的方式读取_subscribe_context传输的数据
    //所以创建独立的线程去接收数据
    return true;
};

//在独立线程中接收订阅通道中的消息
void Redis::observer_channel_message(){
    redisReply *reply = nullptr;
    while(REDIS_OK == redisGetReply(this->_subscribe_context, (void**)&reply)){
        //订阅收到的消息一个带三元素的数组
        if(reply != nullptr && reply->element[2] != nullptr && reply->element[2]->str!= nullptr){
            //给业务层上报通道上发生的消息
            _notify_message_handler(atoi(reply->element[1]->str), reply->element[2]->str);
        }
        freeReplyObject(reply);
    }
};

//初始化向业务层上报通道消息的回调对象
void Redis::init_notify_handler(function<void(int,string)> fn){
    this->_notify_message_handler = fn;
};
