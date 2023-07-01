#include "usermodel.hpp"
#include "db.h"
#include <iostream>
#include <string>
using namespace std;

bool UserModel::insert(User &user){
    char sql[1024] = {0};
    sprintf(sql, "insert into user(name, password, state) values ('%s','%s','%s')",
        user.getName().c_str(), user.getPwd().c_str(), user.getState().c_str());
    
    MySQL mysql;
    if(mysql.connect()){
        if(mysql.update(sql)){
            //获取插入成功的用户
            user.setId(mysql_insert_id(mysql.getConnection()));
            return true;
        }
    }
    return false;
}

User UserModel::query(int id){
    char sql[1024] = {0};
    sprintf(sql, "select * from user where id = %d",id);

    MySQL mysql;
    if(mysql.connect()){
        MYSQL_RES* res = mysql.query(sql);
        if(res != nullptr){
            //从资源上把行拿出来
            MYSQL_ROW row = mysql_fetch_row(res);
            if(row != nullptr){
                User user;
                user.setId(atoi(row[0]));
                user.setName(row[1]);
                user.setPwd(row[2]);
                user.setState(row[3]);
                mysql_free_result(res);
                return user;
            }
        }
    }

    return User();
};

//更改用户信息
bool UserModel::updateState(User user){
    char sql[1024] = {0};
    sprintf(sql, "update user set state = '%s' where id = %d", user.getState().c_str(), user.getId());

    MySQL mysql;
    if(mysql.connect()){
        if(mysql.update(sql)){
            
            return true;
        }
    }
    return false;
};

//重置用户的状态
void UserModel::resetState(){
    char sql[1024] = "update user set state = 'offline' where state = 'online'";

    MySQL mysql;
    if(mysql.connect()){
        mysql.update(sql);
    }
};