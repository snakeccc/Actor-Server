#include "SocketWorker.h"
#include <iostream>
#include <unistd.h>
#include <assert.h>
#include <string.h>
//初始化
void SocketWorker::Init(){
    cout << "SocketWorker Init" << endl;
    //创建epoll对象
    int epollFd = epoll_create(1024);// 返回值：非负数表示成功创建的epoll对象的描述符，-1表示创建失败
    assert(epollFd > 0);

}

void SocketWorker::operator()(){
    while (true)
    {
        //阻塞等待
        const int EVENT_SIZE = 64;
        struct epoll_event events[EVENT_SIZE];
        int eventCount = epoll_wait(epollFd , events, EVENT_SIZE,-1);
       

       //取得事件
        for (int i=0; i<eventCount; i++) {
            epoll_event ev = events[i]; //当前要处理的事件
            OnEvent(ev);
        }

    }
    
}

//添加事件
void SocketWorker::AddEvent(int fd){
    cout << "AddEvent fd " << fd << endl;
    //添加到epoll对象
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = fd;
    if (epoll_ctl(epollFd, EPOLL_CTL_ADD, fd, &ev) == -1) {
    cout << "AddEvent epoll_ctl Fail:" << strerror(errno) <<
    endl;
    }
}

//跨线程调用
void SocketWorker::RemoveEvent(int fd) {
    cout << "RemoveEvent fd " << fd << endl;
    epoll_ctl(epollFd, EPOLL_CTL_DEL, fd, NULL);
}

void SocketWorker::OnEvent(epoll_event ev) {
cout << "OnEvent" << endl;
}