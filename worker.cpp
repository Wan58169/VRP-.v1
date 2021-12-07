//
// Created by WAN on 2021/9/19.
//
#include "rpc.h"
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <thread>

typedef struct sockaddr_in Addr;
typedef socklen_t AddrSize;

using namespace std;

int get_vehicle_capacity(char vehcType)
{
    switch(vehcType) {
        case 'A':
            return 90;
        case 'B':
            return 150;
        case 'C':
            return 240;
        default:
            return -1;
    }
}

/* extract the location of depot */
void get_depot(const int sock, char buf[], Location &depot)
{
    read(sock, buf, BUF_SIZE);
    _extract_depot_rpc(buf, depot);
}

/* ask task */
void request_master(const int sock, const Task &t, const int restCap)
{
    char buf[BUF_SIZE];

    _generate_request_rpc(buf, t, restCap);
    write(sock, buf, sizeof(buf));
}

/* get task */
void get_task(const int sock, Task &t)
{
    char buf[BUF_SIZE];
    std::stack<int> args;

    read(sock, buf, BUF_SIZE);
    _extract_reply_rpc(buf, args);
    _task_assignment_copy_from_args(args, t);
}

/* back to depot */
void back_to_depot(const Task &t, int &restCap, const int &vehcCap, Location &preLoc, const Location &depot)
{
    this_thread::sleep_for(chrono::milliseconds(t.get_serviceTime()));
    restCap = vehcCap;
    preLoc = depot;
    printf("back to depot (%d,%d)\n", depot.get_x(), depot.get_y());
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
    Location depot, preLoc;

    get_depot(masterSock, buf, depot);
    preLoc = depot;
    printf("srcLoc..(%d,%d)\n", preLoc.get_x(), preLoc.get_y());
    Task t(-1, depot, 0, 0, 0, 0);

    char vehcType = argv[3][0];
    int vehcCap = get_vehicle_capacity(vehcType);
    int restCap = vehcCap;
    printf("vehcType..%c\n", vehcType);

    while(true) {
        request_master(masterSock, t, restCap);
        get_task(masterSock, t);

        if(t.get_no() > 0) {     /* regular task */
            Location xy = t.get_xy();
            printf("go to (%d,%d) doing the task %d\n", xy.get_x(), xy.get_y(), t.get_no());
            this_thread::sleep_for(chrono::milliseconds(t.get_serviceTime()));

            restCap -= t.get_demand();
            preLoc = Location(xy.get_x(), xy.get_y());
            printf("task %d done\n", t.get_no());

            if(restCap == 0) {  /* should back to depot */
                back_to_depot(t, restCap, vehcCap, preLoc, depot);
            }
            printf("----------------------\n");
        }
        else if(t.get_no() == TaskEnd) {
            printf("noting to do now, back to depot (%d,%d)\n", depot.get_x(), depot.get_y());
            break;
        }
    }

    close(masterSock);
    return 0;
}
