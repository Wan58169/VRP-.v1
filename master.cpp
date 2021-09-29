//
// Created by WAN on 2021/9/19.
//
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <iostream>
#include <thread>
#include <future>
#include <vector>
#include <queue>
#include <stack>
#include <set>

#define BUF_SIZE 256

typedef struct sockaddr_in Addr;
typedef socklen_t AddrSize;

struct Task {
    int no_;
    int x_;
    int y_;
    int demand_;
    int readyTime_;
    int dueTime_;
    int serviceTime_;

    Task() {}

    Task(int no, int x, int y, int demand, int readyTime, int dueTime, int serviceTime)
        : no_(no), x_(x), y_(y), demand_(demand), readyTime_(readyTime), dueTime_(dueTime), serviceTime_(serviceTime) {}
};

struct TaskCmp : public std::binary_function<Task, Task, bool>
{
    bool operator() (const Task &lhs, const Task &rhs) const {
        if(lhs.readyTime_ < rhs.readyTime_) {
            return true;
        }
        else if(lhs.readyTime_ == rhs.readyTime_) {
            return lhs.dueTime_<rhs.dueTime_ ? true: false;
        }
        else {
            return false;
        }
    }
};

struct Location {
    int x_, y_;

    Location() {}

    Location(int x, int y) : x_(x), y_(y) {}
};

std::mutex mtx;
/* 任务队列 */
std::set<Task, TaskCmp> taskQ;
/* the location of depot */
Location depot;

/* 客户数量 */
int workerNum = 0;
const int workerNumLimt = 3;

/* @depot rpc: x, y */
void _generate_depot_rpc(char msg[])
{
    sprintf(msg, "%d,%d", depot.x_, depot.y_);
}

/* for various kinds extraction */
void __extract_func(char msg[], std::stack<int> &args)
{
    char *splitPtr;

    while(true) {
        splitPtr = strrchr(msg, ',');
        if(splitPtr == NULL) {
            args.push(atoi(msg));
            break;
        }
        args.push(atoi(splitPtr+1));
        *splitPtr = '\0';
    }
}

/* @task: no, x, y, demand, readyTime, dueTime, serviceTime */
void _extract_taskInfo_from_csv(char msg[], std::stack<int> &args)
{
    __extract_func(msg, args);
}

/* task assignment copy */
void _task_assignment_copy_from_args(std::stack<int> &args, Task &t)
{
    t.no_ = args.top(); args.pop(); t.x_ = args.top(); args.pop(); t.y_ = args.top(); args.pop();
    t.demand_ = args.top(); args.pop(); t.readyTime_ = args.top(); args.pop();
    t.dueTime_ = args.top(); args.pop(); t.serviceTime_ = args.top(); args.pop();
}

/* prepare data */
void scan_from_csv()
{
    /* data set */
    FILE *fp;
    char buf[BUF_SIZE];
    std::stack<int> args;      /* @args: no, x, y, demand, readyTime, dueTime, serviceTime */
    Task t;

    if( (fp=fopen("data.csv", "r")) ) {
        fseek(fp, 66L, SEEK_SET);   /* locate the second line */
        /* get the location of depot */
        fgets(buf, BUF_SIZE, fp);
        _extract_taskInfo_from_csv(buf, args);
        args.pop();
        depot.x_ = args.top(); args.pop();
        depot.y_ = args.top(); args.pop();
        while(!args.empty()) { args.pop(); }
        /* the cluster */
        while( fgets(buf, BUF_SIZE, fp) ) {
            buf[strlen(buf)-1] = '\0';  /* replace the end of str: '\n'->'\0' */
            _extract_taskInfo_from_csv(buf, args);
            _task_assignment_copy_from_args(args, t);
            taskQ.insert(t);
        }
    }
}

/* @request rpc: no, x, y, demand, readyTime, dueTime, serviceTime, vehcCap */
void _extract_request_rpc(char msg[], std::stack<int> &args)
{
    __extract_func(msg, args);
}

/* @reply rpc: task's no, x, y, demand, readyTime, dueTime, serviceTime */
void _generate_reply_rpc(char msg[], const Task &t)
{
    sprintf(msg, "%d,%d,%d,%d,%d,%d,%d", t.no_, t.x_, t.y_, t.demand_, t.readyTime_, t.dueTime_, t.serviceTime_);
}

/**
* 客户端线程
*/
void worker_handle(int sock)
{
    char buf[BUF_SIZE];
    std::stack<int> args;
    Task t;
    int vehcCap;

    /* tell worker about depot */
    _generate_depot_rpc(buf);
    write(sock, buf, sizeof(buf));

    /* 读取该客户端发来的消息 */
    while(true) {
        if(workerNum == workerNumLimt) {
            if(read(sock, buf, BUF_SIZE) == 0) {
                break;
            }
            _extract_request_rpc(buf, args);
            _task_assignment_copy_from_args(args, t);
            vehcCap = args.top(); args.pop();

            if(!taskQ.empty()) {
                mtx.lock();
                auto ite = taskQ.begin(); t = *ite;
                while(vehcCap < t.demand_) {
                    t = *(++ite);
                    if(ite == taskQ.end()) {
                        t = Task(0, -1, -1, 0, 0, 0, 0);
                        break;
                    }
                }
                _generate_reply_rpc(buf, t);
                write(sock, buf, sizeof(buf));
                taskQ.erase(ite);
                printf("task %d dispatch worker %d\n", t.no_, sock);
                mtx.unlock();
            }
            else {
                Task endTask(-1, -1, -1, 0, 0, 0, 0);
                _generate_reply_rpc(buf, endTask);
                write(sock, buf, sizeof(buf));
                printf("no task to do, worker %d\n", sock);
            }
        }
    }

    printf("worker %d disconnect!\n", sock);
    close(sock);
}

int main(int argc, char const *argv[])
{
    /* 声明服务器套接字 */
    int masterSock;

    /* 声明服务器地址 */
    Addr masterAddr;
    AddrSize masterAddrSize = sizeof(masterAddr);

    /* 无效输入 */
    if(argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    /* 创建服务器套接字 */
    masterSock = socket(PF_INET, SOCK_STREAM, 0);
    if(masterSock == -1) {
        printf("sock error\n");
        exit(1);
    }

    memset(&masterAddr, 0, masterAddrSize);
    masterAddr.sin_family = AF_INET;
    masterAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    masterAddr.sin_port = htons(atoi(argv[1]));

    /* 准备服务器套接字 */
    if(bind(masterSock, (struct sockaddr*)&masterAddr, masterAddrSize) == -1) {
        printf("bind error\n");
        exit(1);
    }

    /* 监听端口 */
    if(listen(masterSock, BUF_SIZE) == -1) {
        printf("listen error\n");
        exit(1);
    }

    /* 客户端套接字 */
    int workerSock;
    /* 客户端地址 */
    Addr workerAddr;
    AddrSize workerAddrSize;

    /* pre-process */
    scan_from_csv();

    while(true) {
        /* 尝试连接 */
        workerSock = accept(masterSock, (struct sockaddr*)&workerAddr, &workerAddrSize);
        if(workerSock == -1) {
            printf("accept error");
            exit(1);
        }
        ++workerNum;
        printf("new worker %d connected\n", workerSock);

        /* 为每个连接创建处理线程 */
        std::thread t(worker_handle, workerSock);
        t.detach();
    }
    close(masterSock);

    return 0;
}

