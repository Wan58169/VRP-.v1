//
// Created by WAN on 2021/9/19.
//
#include "common.h"
#include "rpc.h"
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <thread>
#include <vector>

typedef struct sockaddr_in Addr;
typedef socklen_t AddrSize;

using namespace std;

vector<string> logs;

int get_vehicle_capacity(char vehcType)
{
    switch(vehcType) {
        case 'A':
            return 36;
        case 'B':
            return 52;
        case 'C':
            return 100;
        default:
            return -1;
    }
}

/* extract the location of depot */
void get_depot(const int sock, Location &depot)
{
    char buf[BUF_SIZE];

    read(sock, buf, BUF_SIZE);
    _extract_depot_rpc(buf, depot);
}

void tell_master_vehcType(const int sock, const char vehcType)
{
    char buf[8];

    sprintf(buf, "%c", vehcType);
    write(sock, buf, sizeof(buf));
}

/* ask task */
void request_master(const int sock, const Task &t, const int vehcCap, const int restCap)
{
    char buf[BUF_SIZE];

    _generate_request_rpc(buf, t, vehcCap, restCap);
    write(sock, buf, sizeof(buf));
}

/* get task */
void get_task(const int sock, Task &t, int &type)
{
    char buf[BUF_SIZE];
    std::stack<int> args;

    read(sock, buf, BUF_SIZE);
    _extract_reply_rpc(buf, args);
    _task_assignment_copy_from_args(args, t);
    type = args.top(); args.pop();
}

/* back to depot */
void back_to_depot(const Task &t, int &restCap, const int &vehcCap, Location &preLoc, const Location &depot)
{
//    this_thread::sleep_for(chrono::milliseconds(t.get_serviceTime()));
    restCap = vehcCap;
    preLoc = depot;

    string s = "back to depot (" + to_string(depot.get_x()) + "," + to_string(depot.get_y()) + ")\n";
    Dprint("%s", s.c_str());
    logs.push_back(s);
}

int main(int argc, char const *argv[])
{
    /* 声明服务器套接字 */
    int masterSock;

    /* 声明服务器地址 */
    Addr masterAddr;
    AddrSize masterAddrSize = sizeof(masterAddr);

    /* 无效输入 */
    if(argc != 6) {
        Dprint("Usage: %s <ip> <port> <type> <workerNo> <solutionNo>\n", argv[0]);
        exit(1);
    }

    masterSock = socket(PF_INET, SOCK_STREAM, 0);
    if(masterSock == -1) {
        Dprint("sock error\n");
        exit(1);
    }

    /* 初始化服务器地址 */
    memset(&masterAddr, 0, masterAddrSize);
    masterAddr.sin_family = AF_INET;
    masterAddr.sin_addr.s_addr = inet_addr(argv[1]);
    masterAddr.sin_port = htons(atoi(argv[2]));

    /* 尝试连接 */
    if(connect(masterSock, (struct sockaddr*)&masterAddr, masterAddrSize) == -1) {
        Dprint("connect error\n");
        exit(1);
    }
    else {
        Dprint("connected..\n");
    }

    string file = "result/tmp/" + string(argv[5]) + "/worker" + string(argv[4]) + ".txt";
    FILE *fp = fopen(file.c_str(), "w");

    if(fp) {
        Dprint("open %s\n", file.c_str());
    }

    string s = string(argv[3]) + "\n";
    fputs(s.c_str(), fp);       // trace the vehc type

    Location depot, preLoc;

    get_depot(masterSock, depot);
    preLoc = depot;
    s = "srcLoc..(" + to_string(preLoc.get_x()) + "," + to_string(preLoc.get_y()) + ")\n";
    Dprint("%s", s.c_str());
    fputs(s.c_str(), fp);
    Task t(-1, depot, 0, 0, 0, 0);

    char vehcType = argv[3][0];
    int vehcCap = get_vehicle_capacity(vehcType);
    int restCap = vehcCap;
    int taskType;

    tell_master_vehcType(masterSock, vehcType);
    Dprint("vehcType..%s\n", argv[3]);

    while(true) {
        request_master(masterSock, t, vehcCap, restCap);
        get_task(masterSock, t, taskType);

        if(taskType == TaskEnd) {
            s = "noting to do now, back to depot (" + to_string(depot.get_x()) + "," + to_string(depot.get_y()) + ")\n";
            Dprint("%s", s.c_str());
            logs.push_back(s);
            break;
        }
        else if(taskType == TaskWait) {
            this_thread::sleep_for(chrono::milliseconds(1));  // retry
        }
        else {
            if(taskType == TaskNonHandle) { /* should back to depot */
                back_to_depot(t, restCap, vehcCap, preLoc, depot);
            }
            /* regular task */
            Location xy = t.get_xy();
            s = "go to (" + to_string(xy.get_x()) + "," + to_string(xy.get_y()) + ") doing the task "
                    + to_string(t.get_no()) + "\n";
            Dprint("%s", s.c_str());
            logs.push_back(s);

            this_thread::sleep_for(chrono::milliseconds(t.get_serviceTime()));
            restCap -= t.get_demand();
            preLoc = Location(xy.get_x(), xy.get_y());
            Dprint("task %d done\n", t.get_no());
            Dprint("----------------------\n");
        }
    }

    // take logs into result/tmp/x/workery.txt
    for(auto &v : logs) {
        fputs(v.c_str(), fp);
    }

    close(masterSock);
    fclose(fp);

    return 0;
}
