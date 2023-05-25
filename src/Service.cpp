#include <iostream>
#include "Service.h"
#include "Sunnet.h"
#include <unistd.h>
#include <string.h>

Service::Service(){
    //初始化锁
    pthread_spin_init(&queueLock, PTHREAD_PROCESS_PRIVATE);
    pthread_spin_init(&inGlobalLock, PTHREAD_PROCESS_PRIVATE);
}

Service::~Service(){
    pthread_spin_destroy(&queueLock);
    pthread_spin_destroy(&inGlobalLock);
}

//插入消息
void Service::PushMsg(shared_ptr<BaseMsg> msg){
    
    pthread_spin_lock(&queueLock);
    {
        msgQueue.push(msg);
    }
    pthread_spin_unlock(&queueLock);
}

//取出消息
shared_ptr<BaseMsg> Service::PopMsg(){
        shared_ptr<BaseMsg> msg = NULL;
        //取一条消息
        pthread_spin_lock(&queueLock);
        {
            if(!msgQueue.empty()){
                 msg = msgQueue.front();
                 msgQueue.pop();
            }
        }
        pthread_spin_unlock(&queueLock);
        return msg;
}

//创建服务后触发
void Service::OnInit() {
    
    cout << "[" << id <<"] OnInit"<< endl;
    //开启监听
    Sunnet::inst->Sunnet::Listen(8002, id);
}

//收到消息时触发
void Service::OnMsg(shared_ptr<BaseMsg> msg) {

//测试用
    if(msg->type == BaseMsg::TYPE::SERVICE){
        auto m = dynamic_pointer_cast<ServiceMsg>(msg);
        cout << "[" << id <<"] OnMsg " << m->buff << endl;
        auto msgRet = Sunnet::inst->MakeMsg(id,
        new char[ 9999999] { 'p', 'i', 'n', 'g', '\0' },
        9999999);

        Sunnet::inst->Send(m->source,msgRet);
    }
    else
    {
        cout << "[" << id <<"] OnMsg" << endl;
    }

    //SOCKET_ACCEPT
    if(msg->type == BaseMsg::TYPE::SOCKET_ACCEPT) {
        auto m = dynamic_pointer_cast<SocketAcceptMsg>(msg);
        cout << "new conn " << m->clientFd << endl;
    }
    //SOCKET_RW
    if(msg->type == BaseMsg::TYPE::SOCKET_RW) {
        auto m = dynamic_pointer_cast<SocketRWMsg>(msg);
    if(m->isRead) {
        char buff[512];
        int len = read(m->fd, &buff, 512);
    if(len > 0) {
        char wirteBuff[3] = {'l','p','y'};
        write(m->fd, &wirteBuff, 3);
    }
    else
    {
        cout << "close " << m->fd << strerror(errno) <<endl;
    Sunnet::inst->CloseConn(m->fd);
    }
    }
    }
}

//退出服务时触发
void Service::OnExit() {
cout << "[" << id <<"] OnExit"<< endl;
}

//处理一条消息，返回值代表是否处理
bool Service::ProcessMsg(){
    shared_ptr<BaseMsg> msg = PopMsg();
    if(msg){
        OnMsg(msg);
        return true;
    }else{
        return false;//返回值预示着队列是否为空
    }
}

void Service::ProcessMsgs(int max){
    for(int i=0; i<max;i++){
        bool succ = ProcessMsg();
        if(!succ){
            break;
        }
    }

}



void Service::SetInGlobal(bool isIn){
    pthread_spin_lock(&inGlobalLock);
    {
        inGlobal = isIn;
    }
    pthread_spin_unlock(&inGlobalLock);
}

//收到其他服务发来的消息
void Service::OnServiceMsg(shared_ptr<ServiceMsg> msg) {
    cout << "OnServiceMsg " << endl;
}
//新连接
void Service::OnAcceptMsg(shared_ptr<SocketAcceptMsg> msg) {
    cout << "OnAcceptMsg " << msg->clientFd << endl;
}


//套接字可读可写
void Service::OnRWMsg(shared_ptr<SocketRWMsg> msg) {
    int fd = msg->fd;
    //可读
    if(msg->isRead) {
    const int BUFFSIZE = 512;
    char buff[BUFFSIZE];
    int len = 0;
    do {
        len = read(fd, &buff, BUFFSIZE);
        if(len > 0){
OnSocketData(fd, buff, len);
}
    } while(len == BUFFSIZE);

    if(len <= 0 && errno != EAGAIN){
        if(Sunnet::inst->GetConn(fd)){
         OnSocketClose(fd);
         Sunnet::inst->CloseConn(fd);
        }
    }
  }

    //可写
    if(msg->isWrite){
        if(Sunnet::inst->GetConn(fd)){
        OnSocketWritable(fd);
        }
    }

}

//收到客户端数据
void Service::OnSocketData(int fd, const char* buff, int len) {
    cout << "OnSocketData" << fd << " buff: " << buff << endl;
    //echo
   /* char wirteBuff[3] = {'l','p','y'};
    write(fd, &wirteBuff, 3);
    cout << "OnSocketData" << fd << " buff: " << buff << endl;
    //echo
    char wirteBuff[3] = {'l','p','y'};
    write(fd, &wirteBuff, 3);
    //等待15秒，继续发送
    usleep(15000000); //15秒
    char wirteBuff2[3] = {'n','e','t'};
    int r = write(fd, &wirteBuff2, 3);
    cout << "write2 r:" << r << " " << strerror(errno) << endl;
//等待1秒，继续发送
    usleep(1000000); //1秒
    char wirteBuff3[2] = {'n','o'};
    r = write(fd, &wirteBuff3,2);
    cout << "write3 r:" << r << " " << strerror(errno) << endl;
    
    
    
    */
    char * wirteBuff = new char[4200000];
    wirteBuff[4200000-1] = 'e';
    int r = write(fd, wirteBuff, 4200000);
    cout << "write r:" << r << " " << strerror(errno) << endl;
}
//套接字可写
void Service::OnSocketWritable(int fd) {
    cout << "OnSocketWritable " << fd << endl;
}
//关闭连接前
void Service::OnSocketClose(int fd) {
    cout << "OnSocketClose " << fd << endl;
}