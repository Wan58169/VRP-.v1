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
#include <string>

#define BUF_SIZE 256

typedef struct sockaddr_in Addr;
typedef socklen_t AddrSize;

struct Task {
    int taskNo;
    int taskSize;

    Task(int no, int size) : taskNo(no), taskSize(size) {}
};

std::mutex mtx;
/* 任务队列 */
std::queue<Task> taskQ;

/* 客户数量 */
int workerNum = 0;
const int workerNumLimt = 3;

/* @request rpc: taskNo, taskSize, vehcCap */
void extract_request_rpc(char msg[], int &taskNo, int &taskSize, int &vehcCap)
{
    char *splitPtr = strrchr(msg, ',');
    vehcCap = atoi(splitPtr+1);
    *splitPtr = '\0';
    splitPtr = strrchr(msg, ',');
    taskSize = atoi(splitPtr+1);
    *splitPtr = '\0';
    taskNo = atoi(msg);
}

/* @reply rpc: taskNo, taskSize */
void generate_reply_rpc(char msg[], const int taskNo, const int taskSize)
{
    sprintf(msg, "%d,%d", taskNo, taskSize);
}

/**
* 客户端线程
*/
void worker_handle(int sock)
{
    char buf[BUF_SIZE];     /* 消息缓冲区 */
    int taskNo, taskSize, vehcCap;

    /* 读取该客户端发来的消息 */
    while(true) {
        if(workerNum == workerNumLimt) {
            if(read(sock, buf, BUF_SIZE) == 0) {
                break;
            }
            extract_request_rpc(buf, taskNo, taskSize, vehcCap);

            if(taskNo>0 && taskSize>0) {    /* task rest */
                mtx.lock();
                taskQ.push(Task(taskNo, taskSize));
                mtx.unlock();
                printf("add task %d, size %d\n", taskNo, taskSize);
            }

            if(!taskQ.empty()) {
                mtx.lock();
                Task t = taskQ.front();
                generate_reply_rpc(buf, t.taskNo, t.taskSize);
                write(sock, buf, sizeof(buf));
                printf("dispatch task %d, size %d to worker %d\n", t.taskNo, t.taskSize, sock);
                taskQ.pop();
                mtx.unlock();
            }
            else {
                write(sock, "-1,0", 5);
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

    for(int i=1; i<=30; i++) {
        taskQ.push(Task(i, rand()%10+1));
    }

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

