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
#include <stack>

typedef struct sockaddr_in Addr;
typedef socklen_t AddrSize;

using namespace std;
#define BUF_SIZE 256

struct Task {
    int no_;
    int x_;
    int y_;
    int demand_;
    int readyTime_;
    int dueTime_;
    int serviceTime_;

    Task(){}

    Task(int no, int x, int y, int demand, int readyTime, int dueTime, int serviceTime)
        : no_(no), x_(x), y_(y), demand_(demand), readyTime_(readyTime), dueTime_(dueTime), serviceTime_(serviceTime) {}
};

struct Location {
    int x_, y_;

    Location() {}

    Location(int x, int y) : x_(x), y_(y) {}
};

Location depot, preLoc;

int get_vehicle_capacity(char vehicleType)
{
    int ret;

    switch(vehicleType) {
        case 'A':
            ret = 90;
            break;
        case 'B':
            ret = 150;
            break;
        case 'C':
            ret = 240;
            break;
        default:
            ret = -1;
            break;
    }
    return ret;
}

/* @request rpc: task's no, x, y, demand, readyTime, dueTime, serviceTime; vehcCap */
void _generate_request_rpc(char msg[], const Task &t, const int vehcCap)
{
    sprintf(msg, "%d,%d,%d,%d,%d,%d,%d,%d", t.no_, t.x_, t.y_, t.demand_, t.readyTime_, t.dueTime_, t.serviceTime_, vehcCap);
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

/* @depot rpc: x, y */
void _extract_depot_rpc(char msg[])
{
    char *splitPtr;

    splitPtr = strrchr(msg, ',');
    depot.y_ = atoi(splitPtr+1);
    *splitPtr = '\0';
    depot.x_ = atoi(msg);
}

/* @reply rpc: task's no, x, y, demand, readyTime, dueTime, serviceTime */
void _extract_reply_rpc(char msg[], std::stack<int> &args)
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

/* back to depot */
void back_to_depot(const Task &t, int &restCap, const int &vehcCap)
{
    this_thread::sleep_for(chrono::milliseconds(t.serviceTime_));
    restCap = vehcCap;
    preLoc = depot;
    printf("back to depot (%d,%d)\n", depot.x_, depot.y_);
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
    /* extract the location of depot */
    read(masterSock, buf, BUF_SIZE);
    _extract_depot_rpc(buf);
    preLoc = depot;
    printf("srcLoc..(%d,%d)\n", preLoc.x_, preLoc.y_);
    Task t(-1, depot.x_, depot.y_, 0, 0, 0, 0);

    std::stack<int> args;
    int vehcCap = get_vehicle_capacity(argv[3][0]);
    int restCap = vehcCap;

    while(true) {
        _generate_request_rpc(buf, t, restCap);
        write(masterSock, buf, sizeof(buf));
        read(masterSock, buf, BUF_SIZE);
        _extract_reply_rpc(buf, args);
        _task_assignment_copy_from_args(args, t);

        if(t.no_ > 0) {     /* regular task */
            printf("go to (%d,%d) doing the task %d, serviceTime %d\n", t.x_, t.y_, t.no_, t.serviceTime_);
            /* the travel time + the service time */
            this_thread::sleep_for(chrono::milliseconds(t.serviceTime_));
            restCap -= t.demand_;
            t.demand_ = 0;
            preLoc.x_ = t.x_; preLoc.y_ = t.y_;
            printf("task %d done\n", t.no_);
            if(restCap == 0) {  /* should back to depot */
                back_to_depot(t, restCap, vehcCap);
            }
            printf("----------------------\n");
        }
        else if(t.no_ == 0) {   /* force back to depot */
            back_to_depot(t, restCap, vehcCap);
        }
        else {
            printf("noting to do now, back to depot (%d,%d)\n", depot.x_, depot.y_);
            break;
        }
    }

    close(masterSock);
    return 0;
}
