//
// Created by WAN on 2021/9/19.
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <iostream>
#include <thread>

typedef struct sockaddr_in Addr;
typedef socklen_t AddrSize;

using namespace std;
#define BUF_SIZE 256

int get_vehicle_capacity(char vehicleType)
{
    int ret;

    switch(vehicleType) {
        case 'A':
            ret = 3;
            break;
        case 'B':
            ret = 5;
            break;
        case 'C':
            ret = 8;
            break;
        default:
            ret = -1;
            break;
    }
    return ret;
}

/* @request rpc: taskNo, taskSize, vehicleCap */
void generate_request_rpc(char msg[], const int taskNo, const int taskSize, const int vehcCap)
{
    sprintf(msg, "%d,%d,%d", taskNo, taskSize, vehcCap);
}

/* @reply rpc: taskNo,taskSize */
void extract_reply_rpc(char msg[], int &taskNo, int &taskSize)
{
    char *splitPtr = strchr(msg, ',');
    taskSize = atoi(splitPtr+1);
    *splitPtr = '\0';
    taskNo = atoi(msg);
}

int main(int argc, char const *argv[])
{
    /* 声明服务器套接字 */
    int masterSock;

    /* 声明服务器地址 */
    Addr masterAddr;
    AddrSize masterAddrSize = sizeof(masterAddr);

    /* 无效输入 */
    if(argc != 4) {
        printf("Usage: %s <ip> <port> <type>\n", argv[0]);
        exit(1);
    }

    masterSock = socket(PF_INET, SOCK_STREAM, 0);
    if(masterSock == -1) {
        printf("sock error\n");
        exit(1);
    }

    /* 初始化服务器地址 */
    memset(&masterAddr, 0, masterAddrSize);
    masterAddr.sin_family = AF_INET;
    masterAddr.sin_addr.s_addr = inet_addr(argv[1]);
    masterAddr.sin_port = htons(atoi(argv[2]));

    /* 尝试连接 */
    if(connect(masterSock, (struct sockaddr*)&masterAddr, masterAddrSize) == -1) {
        printf("connect error\n");
        exit(1);
    }
    else {
        printf("connected..\n");
    }

    char buf[BUF_SIZE];
    int taskNo = -1;
    int taskSize = 0;
    int vehcCap = get_vehicle_capacity(argv[3][0]);

    while(true) {
        /* @request rpc: taskNo, taskSize, vehicleCap */
        generate_request_rpc(buf, taskNo, taskSize, vehcCap);
        write(masterSock, buf, sizeof(buf));
        read(masterSock, buf, BUF_SIZE);
        /* @reply rpc: taskNo,taskSize */
        extract_reply_rpc(buf, taskNo, taskSize);

        if(taskNo>0 && taskSize>0) {
            printf("I'm %d, working task %d, size %d\n", masterSock, taskNo, taskSize);
            this_thread::sleep_for(chrono::seconds(1));
            taskSize -= vehcCap;
            printf("task %d, rest %d\n", taskNo, taskSize);
            if(taskSize <= 0) { /* task done */
                taskNo = -1;
                taskSize = 0;
            }
        }
        else if(taskNo==-1 && taskSize==0) {
            printf("noting to do now\n");
            break;
        } else {

        }
    }

    close(masterSock);
    return 0;
}
