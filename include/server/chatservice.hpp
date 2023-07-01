#ifndef CHATSERVICE_H
#define CHATSERVICE_H

#include <unordered_map>
#include <muduo/net/TcpConnection.h>
#include <functional>
#include <mutex>
#include <condition_variable>
#include "usermodel.hpp"
#include "offlinemessagemodel.hpp"
#include "friendmodel.hpp"
#include "groupmodel.hpp"
#include "redis.hpp"
using namespace std;
using namespace muduo;
using namespace muduo::net;

#include "json.hpp"
using json = nlohmann::json;

//处理消息的事件回调方式类型
using MsgHandler = std::function<void(const TcpConnectionPtr &conn,
                                      json &js,
                                      Timestamp)>;
//聊天服务器业务类
class ChatService{
public:
    //获取单例对象的接口函数
    static ChatService* instance();
    //登录
    void login(const TcpConnectionPtr &conn,json &js,Timestamp time);
    //注册
    void reg(const TcpConnectionPtr &conn,json &js,Timestamp time);
    //一对一聊天业务
    void oneChat(const TcpConnectionPtr &conn,json &js,Timestamp time);
    //客户端异常断开连接
    void clientCloseException(const TcpConnectionPtr &conn);
    //服务器端异常，业务重置方法
    void reset();
    //添加好友业务
    void addFriend(const TcpConnectionPtr &conn,json &js,Timestamp time);
    //创建群组
    void createGroup(const TcpConnectionPtr &conn,json &js,Timestamp time);
    //加入群组
    void addGroup(const TcpConnectionPtr &conn,json &js,Timestamp time);
    //群聊天
    void groupChat(const TcpConnectionPtr &conn,json &js,Timestamp time);
    //处理注销业务
    void loginout(const TcpConnectionPtr &conn,json &js,Timestamp time);
    //从redis消息队列中获取订阅消息
    void handleRedisSubscribeMessage(int, string);
    //修改字符串大小
    void change_message(const TcpConnectionPtr &conn,json &js,Timestamp time);
    //获取消息对应的处理器
    MsgHandler getHandler(int msgid);
private:
    ChatService();
    //存储消息id和其对应的业务处理方法
    unordered_map<int,MsgHandler> _msgHandlerMap;

    //定义一把互斥锁，控制在多线程中互斥访问
    mutex _connMutex;

    //定义条件变量，控制字符扩容
    condition_variable change_cv;

    //存储在线用户的通信连接
    unordered_map<int, TcpConnectionPtr> _userConnMap;

    //数据操作用户类对象
    UserModel _userModel;
    //数据操作离线消息
    OfflineMsgModel _offlineMsgModel;
    //数据操作好友类对象
    FriendModel _friendModel;
    //数据操作群组类对象
    GroupModel _groupModel;

    //redis操作对象
    Redis _redis;
};

#endif