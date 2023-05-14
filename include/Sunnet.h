#pragma once
#include <vector>
#include <thread>
#include <unordered_map>
#include "Worker.h"
#include "Service.h"
#include "SocketWorker.h"
#include "Conn.h"
class Sunnet
{
public:
    static Sunnet* inst;
    
    int WORKER_NUM = 3;
    vector<Worker*> workers;
    vector<thread*> workerThreads;

    //消息服务器列表
    unordered_map<uint32_t,shared_ptr<Service>> services;
    uint32_t maxId = 0;//最大ID
    pthread_rwlock_t servicesLock;//读写锁
    //增删服务
    uint32_t NewService(shared_ptr<string> type);
    void KillService(uint32_t id);//仅限服务自己调用
    

    //增删查Conn
    shared_ptr<Conn> GetConn(int fd);
    bool RemoveConn(int fd);

private:
    queue<shared_ptr<Service>> globalQueue;
    int globalLen = 0;// 列队长度
    pthread_spinlock_t globalLock;//锁

    //休眠和唤醒
    pthread_mutex_t sleepMtx;
    pthread_cond_t sleepCond;
    int sleepCount = 0;

    //socket 网络线程
    SocketWorker* socketWorker;
    thread* socketThread;
    
    //Conn列表
    unordered_map<uint32_t, shared_ptr<Conn>> conns;
    //读写锁
    pthread_rwlock_t connsLock;

private:
    //获取服务
    shared_ptr<Service> GetService(uint32_t id);
    
    //开启网络线程
    void StartSocket();

public:
    Sunnet();

    void Start();

    void Wait();

    
public:
    //开启工作线程
    void StartWorker();

    //发送消息
    void Send(uint32_t told,shared_ptr<BaseMsg> msg);

    //全局队列操作
    shared_ptr<Service> PopGlobalQueue();
    void PushGlobalQueue(shared_ptr<Service> srv);

    //仅测试用，buff须由new产生
    shared_ptr<BaseMsg> MakeMsg(uint32_t source, char* buff,int len);

    //唤醒工作线程
    void CheckAndWeakUp();

    //让工作线程等待（仅工作线程调用）
    void WorkerWait();


    //网络 增删查Conn
    int AddConn(int fd,uint32_t id, Conn::TYPE type);

    //网络连接操作接口
    int Listen(uint32_t port, uint32_t serviceId);
    void CloseConn(uint32_t fd);
};


