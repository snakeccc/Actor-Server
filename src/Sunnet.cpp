#include <iostream>
#include <thread>
#include "../include/Sunnet.h"
#include <assert.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>

using namespace std;
class Sunnet;
Sunnet* Sunnet::inst;
Sunnet::Sunnet() {
cout << "Hello Sunnet"<<endl;
}

void Sunnet::Start(){
     inst = this;
     //锁
     pthread_rwlock_init(&servicesLock, NULL);
     pthread_spin_init(&globalLock, PTHREAD_PROCESS_PRIVATE);
     //开启Worker
     StartWorker();

     //开启Socket线程
     StartSocket();

     assert(pthread_rwlock_init(&connsLock, NULL)==0);
     //唤醒线程
     /*pthread_cond_init(＆sleepCond, NULL);
     pthread_mutex_init(＆sleepMtx, NULL);*/
}

void Sunnet::StartWorker() {
//WORKER_NUM
for (int i = 0; i < inst->WORKER_NUM; i++) {
cout << "start worker thread:" << i << endl;
//创建线程对象
Worker* worker = new Worker();
worker->id = i;
worker->eachNum = 2 << i;
//创建线程
thread* wt = new thread(*worker);

//添加到数组

/*
Worker worker;
worker.id = i;
worker.eachNum = 2 << i;
thread wt(worker,i);
*/

 inst->workers.push_back(worker);
 inst->workerThreads.push_back(wt);
}
  

}
//等待
void Sunnet::Wait() {
if( workerThreads[0]) {
workerThreads[0]->join();
}
}

//新建服务
uint32_t Sunnet::NewService(shared_ptr<string> type){
     auto srv = make_shared<Service>();
     srv->type = type;
     pthread_rwlock_wrlock(&servicesLock);
     {
          srv->id = maxId;
          maxId++;
          services.emplace(srv->id, srv); 
     }
     pthread_rwlock_unlock(&servicesLock);
     srv->OnInit(); //初始化
     return srv->id;
}

//由id查找服务
shared_ptr<Service> Sunnet::GetService(uint32_t id){
     shared_ptr<Service> srv = nullptr;
     pthread_rwlock_rdlock(&servicesLock);
     {
         unordered_map<uint32_t,shared_ptr<Service>>::iterator iter = services.find(id);
          if(iter != services.end()){
               srv = iter->second;
          }
     }
     pthread_rwlock_unlock(&servicesLock);
     return srv;
}

//删除服务
//只能service自己调自己，因为会调用不加锁的srv->OnExit和srv->isExiting
void Sunnet::KillService(uint32_t id) {
     shared_ptr<Service> srv = GetService(id);
     if(!srv){
          return;
     }
     //退出前
     srv->OnExit();
     srv->isExiting = true;
     //删列表
     pthread_rwlock_wrlock(&servicesLock);
     {
     services.erase(id);
     }
     pthread_rwlock_unlock(&servicesLock);
}

//弹出全局队列
shared_ptr<Service> Sunnet::PopGlobalQueue(){
     shared_ptr<Service> srv = NULL;
     pthread_spin_lock(&globalLock);
     {
     if (!globalQueue.empty()) {
     srv = globalQueue.front();
     globalQueue.pop();
     globalLen--;
          }
     }
     pthread_spin_unlock(&globalLock);
     return srv;
}

//插入全局队列
void Sunnet::PushGlobalQueue(shared_ptr<Service> srv){
     pthread_spin_lock(&globalLock);
     {
          globalQueue.push(srv);
          globalLen++;
     }
     pthread_spin_unlock(&globalLock);
}

//发送消息
void Sunnet::Send(uint32_t toId, shared_ptr<BaseMsg> msg){
     shared_ptr<Service> toSrv = GetService(toId);

     if(!toSrv){
     cout << "Send fail, toSrv not exist toId:" << toId << endl;
     return;
     }
     //插入目标服务的消息队列
     toSrv->PushMsg(msg);
     //检查并放入全局队列
     bool hasPush = false;
     pthread_spin_lock(&toSrv->inGlobalLock);
     {
     if(!toSrv->inGlobal) {
     PushGlobalQueue(toSrv);
     toSrv->inGlobal = true;
     hasPush = true;
       }
     }
     pthread_spin_unlock(&toSrv->inGlobalLock);
     
    //唤起进程，不放在临界区里面
    if(hasPush){
     CheckAndWeakUp();
    }
}


//仅测试用，buff须由new产生
shared_ptr<BaseMsg> Sunnet::MakeMsg(uint32_t source, char* buff,int len) {
     auto msg= make_shared<ServiceMsg>();

     msg->type = BaseMsg::TYPE::SERVICE;

     msg->source = source;

     //基本类型的对象没有析构函数，
     //所以用delete 或 delete[]都可以销毁基本类型数组；
     //智能指针默认使用delete销毁对象，
     //所以无须重写智能指针的销毁方法
     msg->buff = shared_ptr<char>(buff);

     msg->size = len;
     
     return msg;
}

//Worker线程调用，进入休眠
void Sunnet::WorkerWait(){
     pthread_mutex_lock(&sleepMtx);
     {
        sleepCount ++;
        //解锁进入休眠 线程被唤醒时加锁
        pthread_cond_wait(&sleepCond, &sleepMtx); 
        sleepCount--; 
     }
     pthread_mutex_unlock(&sleepMtx);
}

//检查并唤醒线程
void Sunnet::CheckAndWeakUp(){
     //unsafe
     if(sleepCount ==0){
          return;
     }
     if(WORKER_NUM - sleepCount <= globalLen){
          cout << "weakup" << endl;
          pthread_cond_signal(&sleepCond);
     }
}

//开启scoket网络线程
void Sunnet::StartSocket(){
     //创建线程对象
     socketWorker = new SocketWorker();
     //初始化
     socketWorker->Init();
     //创建线程
     socketThread = new thread(*socketWorker);     
}


//添加scoket连接
int Sunnet::AddConn(int fd, uint32_t id, Conn::TYPE type){
     auto conn = make_shared<Conn>();
     conn->fd = fd;
     conn->serviceId = id;
     conn->type = type;
     pthread_rwlock_wrlock(&connsLock);
     {
         conns.emplace(fd, conn); 
     }
     pthread_rwlock_unlock(&connsLock);
     return fd;
}

//通过id查找连接
shared_ptr<Conn> Sunnet::GetConn(int fd){
     shared_ptr<Conn> conn = nullptr;
     pthread_rwlock_wrlock(&connsLock);
     {
          unordered_map<uint32_t,shared_ptr<Conn>>::iterator iter = conns.find(fd);
          if(iter != conns.end()){
               conn = iter->second;
          }
     }
     pthread_rwlock_unlock(&connsLock);
     return conn;
}

//删除连接
bool Sunnet::RemoveConn(int fd) {
     int result;
     pthread_rwlock_wrlock(&connsLock);
     {
     result = conns.erase(fd);
     }
     pthread_rwlock_unlock(&connsLock);
     return result == 1;
}

//网络socket连接操作
int Sunnet::Listen(uint32_t port, uint32_t serviceId) {
     //步骤1：创建socket
     int listenFd = socket(AF_INET,SOCK_STREAM,0);
     
     if(listenFd <= 0){
          cout << "listen error, listenFd <= 0" << endl;
          return -1;
     }

     //步骤2：设置为非阻塞
     fcntl(listenFd, F_SETFL, O_NONBLOCK);

     //步骤3：bind
     struct sockaddr_in addr;//创建地址结构
     addr.sin_family = AF_INET; //ipv4
     addr.sin_port = htons(port);//port
     addr.sin_addr.s_addr = htonl(INADDR_ANY); //can be set any ip

     int r = bind(listenFd,(struct sockaddr*)&addr,sizeof(addr));
     if(r == -1){
          cout << "listen error, bind fail" << endl;
          return -1;
     }

     //步骤4：listen
     r = listen(listenFd,64);
     if(r<0){
          return -1;
     }

     //步骤5：添加到管理结构
     AddConn(listenFd,serviceId,Conn::TYPE::LISTEN);
     //步骤6：添加到epoll事件，跨线程
     socketWorker->AddEvent(listenFd);
     return listenFd;
}

void Sunnet::CloseConn(uint32_t fd) {
//删除conn对象
bool succ = RemoveConn(fd);
//关闭套接字
close(fd);
//删除epoll对象对套接字的监听（跨线程）
if(succ) {
socketWorker->RemoveEvent(fd);
}
}
