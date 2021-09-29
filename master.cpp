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

std::mutex mtx;
/* 任务队列 */
std::queue<Task> taskQ;

/* 客户数量 */
int workerNum = 0;
const int workerNumLimt = 3;

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
        while( fgets(buf, BUF_SIZE, fp) ) {
            buf[strlen(buf)-1] = '\0';  /* replace the end of str: '\n'->'\0' */
            _extract_taskInfo_from_csv(buf, args);
            _task_assignment_copy_from_args(args, t);
//            printf("%d,%d,%d,%d,%d,%d,%d\n", no, x, y, demand, readyTime, dueTime, serviceTime);
            taskQ.push(t);
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

    /* 读取该客户端发来的消息 */
    while(true) {
        if(workerNum == workerNumLimt) {
            if(read(sock, buf, BUF_SIZE) == 0) {
                break;
            }
            _extract_request_rpc(buf, args);
            _task_assignment_copy_from_args(args, t);
            vehcCap = args.top(); args.pop();

            if(t.no_>0 && t.demand_>0) {    /* task rest */
                mtx.lock();
                printf("add task %d\n", t.no_);
                taskQ.push(t);
                mtx.unlock();
            }

            if(!taskQ.empty()) {
                mtx.lock();
                t = taskQ.front();
                _generate_reply_rpc(buf, t);
                write(sock, buf, sizeof(buf));
                taskQ.pop();
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
    taskQ.pop();

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

