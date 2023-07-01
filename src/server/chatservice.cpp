#include "chatservice.hpp"
#include "public.hpp"
#include <muduo/base/Logging.h>
#include <vector>
#include <iostream>
using namespace std;
using namespace muduo;

bool ready = false; //全局标志位

//获取单例对象的接口函数
ChatService* ChatService::instance(){
    static ChatService service;
    return &service;
};

//注册消息以及对应的回调函数
ChatService::ChatService(){ 
    _msgHandlerMap.insert({LOGIN_MSG,std::bind(&ChatService::login,this,_1,_2,_3)});
    _msgHandlerMap.insert({REG_MSG,std::bind(&ChatService::reg,this,_1,_2,_3)});
    _msgHandlerMap.insert({LOGINOUT_MSG,std::bind(&ChatService::loginout,this,_1,_2,_3)});

    _msgHandlerMap.insert({ONE_CHAT_MSG,std::bind(&ChatService::oneChat,this,_1,_2,_3)});
    _msgHandlerMap.insert({ADD_FRIEND_MSG,std::bind(&ChatService::addFriend,this,_1,_2,_3)});
    
    _msgHandlerMap.insert({CREATE_GROUP_MSG,std::bind(&ChatService::createGroup,this,_1,_2,_3)});
    _msgHandlerMap.insert({ADD_GROUP_MSG,std::bind(&ChatService::addGroup,this,_1,_2,_3)});
    _msgHandlerMap.insert({GROUP_CHAT_MSG,std::bind(&ChatService::groupChat,this,_1,_2,_3)});

    _msgHandlerMap.insert({CHANGE_CHAR_ACK,std::bind(&ChatService::change_message,this,_1,_2,_3)});

    //连接redis服务器
    if(_redis.connect()){
        //设置上报消息的回调
        _redis.init_notify_handler(std::bind(&ChatService::handleRedisSubscribeMessage,this,_1,_2));
    }
};

//获取消息对应的处理器
MsgHandler ChatService::getHandler(int msgid){
    //记录错误日志，msgid没有对应的事件处理回调
    auto it = _msgHandlerMap.find(msgid);
    if(it == _msgHandlerMap.end()){
        //返回一个默认的处理器，空操作(防止异常,进程挂掉，退出服务器)
        return [=](const TcpConnectionPtr &conn,json &js,Timestamp){
            LOG_ERROR << "msgid:"<< msgid << "can not find handler";
        };
    }else{
        return _msgHandlerMap[msgid];
    }    
};

//登录 id pwd pwd
void ChatService::login(const TcpConnectionPtr &conn,json &js,Timestamp time){
    LOG_INFO << "do login service" ;
    int id = js["id"].get<int>();
    string pwd = js["password"];

    User user = _userModel.query(id);
    if(user.getId() == id && user.getPwd() == pwd){
        //登录成功
        if(user.getState() == "online"){
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 2;
            response["errmsg"] = "this account is using, input another!";
            conn->send(response.dump());
        }else{
            {   
                //登录成功，记录用户连接记录
                lock_guard<mutex> lock(_connMutex);
                _userConnMap.insert({id, conn});
            }

            //id用户登录成功后，向redis订阅channel(id)
            _redis.subscribe(id);

            //登录成功，修改登录状态
            user.setState("online");
            _userModel.updateState(user);

            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 0;
            response["id"] =  user.getId();
            response["name"] = user.getName();

            //服务器自动推送，查询该用户是否有离线消息
            vector<string> vec = _offlineMsgModel.query(id);
            if(!vec.empty()){
                response["offlinemsg"] = vec;
                //读取完之后将该用户的离线消息清除
                _offlineMsgModel.remove(id);
            }
            //查询该用户的好友情况
            vector<User> userVec = _friendModel.query(id);
            if(!userVec.empty()){
                vector<string> vec2;
                for(User &user : userVec){
                    json js;
                    js["id"] = user.getId();
                    js["name"] = user.getName();
                    js["state"] = user.getState();
                    vec2.push_back(js.dump());
                }
                response["friendmsg"] = vec2;
            }

            //查询该用户的群组情况
            vector<Group> groupVec = _groupModel.queryGroups(id);
            if(!groupVec.empty()){
                vector<string> groupV;
                for(Group &group : groupVec){
                    json groupjs;
                    groupjs["id"] = group.getId();
                    groupjs["groupname"] = group.getName();
                    groupjs["groupdesc"] = group.getDesc();

                    vector<string> GroupUserV;
                    for(GroupUser &user: group.getUsers()){
                        json userjs;
                        userjs["id"] = user.getId();
                        userjs["name"] = user.getName();
                        userjs["state"] = user.getState();
                        userjs["role"] = user.getRole();
                        GroupUserV.push_back(userjs.dump());
                    }
                    groupjs["users"] = GroupUserV;
                    groupV.push_back(groupjs.dump());
                }
                response["groupmsg"] = groupV;
            }
            
            //判断将发送数据大小
            cout << strlen(response.dump().c_str()) << endl;
            if(strlen(response.dump().c_str()) >= 1024){
                json change_size;
                change_size["msgid"] = CHANGE_CHAR_SIZE;
                int size = (strlen(response.dump().c_str())/1024 + 1)*1024;
                change_size["size"] = size;
                cout << change_size.dump() << endl;
                conn->send(change_size.dump());
            }else{
                conn->send(response.dump());
            }
            string change_send = response.dump();

            //单独线程去监听接收修改成功信号
            thread t([=](){
                cout << "thread recv success" << endl;
                unique_lock<mutex> lock(_connMutex);
                while(!ready){
                    change_cv.wait(lock);
                }
                conn->send(change_send);
            });
            t.detach();
        }    
    }else{
        //登录失败
        //该用户不存在，用户存在但是密码错误，密码失败
        json response;
        response["msgid"] = LOGIN_MSG_ACK;
        response["errno"] = 1;
        response["errmsg"] = "id or password is invalid";
        conn->send(response.dump());
    }
}

//注册 name password
void ChatService::reg(const TcpConnectionPtr &conn,json &js,Timestamp time){
    LOG_INFO << "do reg service" ; 
    string name = js["name"];
    string pwd = js["password"];

    User user;
    user.setName(name);
    user.setPwd(pwd);

    bool state = _userModel.insert(user);
    if(state){
        //注册成功, 回显到客户端
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 0;
        response["id"] =  user.getId();
        conn->send(response.dump());
    }else{
        //注册失败，回显到客户端
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 1;
        conn->send(response.dump());
    }
};

//客户端异常断开连接
void ChatService::clientCloseException(const TcpConnectionPtr &conn){
    User user;
    {
        lock_guard<mutex> lock(_connMutex); 
        for(auto it = _userConnMap.begin();it != _userConnMap.end();it++){
            if(it->second == conn){
                //从map表删除用户的链接信息
                user.setId(it->first);
                _userConnMap.erase(it);
                break;
            }
        }
    }

    //用户注销，在redis中取消订阅通道
    _redis.unsubscribe(user.getId());

    //更新用户的状态信息
    if(user.getId() != -1){
        user.setState("offline");
        _userModel.updateState(user);
    }
};

//一对一聊天业务
void ChatService::oneChat(const TcpConnectionPtr &conn,json &js,Timestamp time){
    int toid = js["toid"].get<int>();
    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(toid);
        if(it != _userConnMap.end()){
            //toid对方在线，转发消息
            //同一服务器主动推送消息给toid用户
            it->second->send(js.dump());
            return;
        }
    }

    //查询toid是否在线
    User user = _userModel.query(toid);
    if(user.getState() == "online"){
        _redis.publish(toid, js.dump());
        return ;
    }
    
    //toid对方不在线，存储离线消息
    _offlineMsgModel.insert(toid, js.dump());
};

//服务器端异常，业务重置方法
void ChatService::reset(){
    _userModel.resetState();
};

//添加好友业务
void ChatService::addFriend(const TcpConnectionPtr &conn,json &js,Timestamp time){
    int userid = js["id"].get<int>();
    int friendid = js["friendid"].get<int>();

    //增加好友信息
    _friendModel.insert(userid, friendid); 
};

//创建群组
void ChatService::createGroup(const TcpConnectionPtr &conn,json &js,Timestamp time){
    int userid = js["id"].get<int>();
    string name = js["groupname"];
    string desc = js["groupdesc"];

    Group group(-1,name,desc);
    if(_groupModel.createGroup(group)){
        _groupModel.addGroup(userid,group.getId(),"creator");
    }
};

//加入群组
void ChatService::addGroup(const TcpConnectionPtr &conn,json &js,Timestamp time){
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    _groupModel.addGroup(userid, groupid ,"normal");
};

//群组聊天
void ChatService::groupChat(const TcpConnectionPtr &conn,json &js,Timestamp time){
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();

    vector<int> useridVec = _groupModel.queryGroupUsers(userid,groupid);
    lock_guard<mutex> lock(_connMutex);
    for(int id : useridVec){
        auto it = _userConnMap.find(id);
        if(it != _userConnMap.end()){
            //toid对方在线，转发消息
            //服务器主动推送消息给id用户
            it->second->send(js.dump());
        }else{
            //查询toid是否在线
            User user = _userModel.query(id);
            if(user.getState() == "online"){
                _redis.publish(id, js.dump());
                continue;
            }

            //toid对方不在线，存储离线消息
            _offlineMsgModel.insert(id, js.dump());
        }
    }
};

//处理注销业务
void ChatService::loginout(const TcpConnectionPtr &conn,json &js,Timestamp time){
    int userid = js["id"].get<int>();
    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(userid);
        if(it!=_userConnMap.end()){
            _userConnMap.erase(it);
        }
    }

    //用户注销，相当于下线，在redis中取消订阅通道
    _redis.unsubscribe(userid);
    
    User user(userid,"","","offline");
    _userModel.updateState(user);
};

//从redis消息队列中获取订阅消息
void ChatService::handleRedisSubscribeMessage(int userid, string msg){
    lock_guard<mutex> lock(_connMutex);
    auto it = _userConnMap.find(userid);
    if(it!=_userConnMap.end()){
        it->second->send(msg);
        return;
    }

    //存储该用户的离线消息
    _offlineMsgModel.insert(userid, msg);
};

//修改字符串大小
void ChatService::change_message(const TcpConnectionPtr &conn,json &js,Timestamp time){
    int userid = js["id"].get<int>();

    unique_lock<mutex> lock(_connMutex);
    auto it = _userConnMap.find(userid);
    if(it != _userConnMap.end()){
        ready = true;
        change_cv.notify_all();
    }
}