#ifndef GROUPUSER_H
#define GROUPUSER_H

#include "user.hpp"

//群组角色，多了一个角色信息，从User类直接继承, 复用User的其他信息
class GroupUser : public User{
public:
    void setRole(string role){this->_role = role;};
    string getRole(){return this->_role;};
private:
    string _role;
};

#endif