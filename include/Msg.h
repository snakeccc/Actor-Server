#pragma once
#include <memory>
using namespace std;
//消息基类
class BaseMsg {

public:
    enum TYPE {
        SERVICE = 1,
        SOCKET_ACCEPT = 2,
        SOCKET_RW = 3,
    };
    uint8_t type;

    char load[999999]{};//用于检测内存泄漏，仅用于调试

    virtual ~BaseMsg(){};

};


class ServiceMsg:public BaseMsg
{
private:
   
public:
    uint32_t source;//消息发送方
    shared_ptr<char> buff; //消息内容
    size_t size;//消息内容大小
};

class SocketAcceptMsg : public BaseMsg {

public:
    //socket描述符(编号)
    int listenFd;
    int clientFd;   
};

//可读可写
class SocketRWMsg : public BaseMsg {
public:
    //socket描述符(编号)
    int fd;
    bool isRead = false;
    bool isWrite = false;
};

