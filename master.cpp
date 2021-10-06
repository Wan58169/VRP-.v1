//
// Created by WAN on 2021/9/19.
//
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <iostream>
#include <thread>
#include <vector>
#include <queue>
#include <stack>
#include <set>
#include <condition_variable>
#include <future>
#include <mutex>

#define BUF_SIZE 256

typedef struct sockaddr_in Addr;
typedef socklen_t AddrSize;

const int TaskEnd = -1;
const int TaskWait = 0;

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

    void set_task_type(int type) {
        switch(type) {
            case TaskEnd:
                no_ = -1;
                break;
            case TaskWait:
                no_ = 0;
                break;
            default:
                break;
        }
    }
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
/* timer */
std::chrono::steady_clock::time_point StartTime;

/* TaskQueue and mutex */
std::mutex Mtx;
std::multiset<Task, TaskCmp> TaskQ;

/* thread sync */
std::mutex CvMtx;
std::condition_variable Cv;
int WeakUpNum = 0;

/* the number of workers */
int WorkerNum = 0;
int WorkerNumLimt = 0;

class Method {
public:
    virtual void run(int, Task&, long long, int)=0;
};

class TimeFirstMethod : public Method {
public:
    void run(int sock, Task &t, long long timeStamp, int vehcCap) {
        auto targetIte = TaskQ.begin();
        t = *targetIte;
        TaskQ.erase(targetIte);
        printf("task %d readyTime %d dueTime %d dispatch worker %d timeStamp %lld ", t.no_, t.readyTime_, t.dueTime_, sock, timeStamp);
        if(timeStamp>=t.readyTime_ && timeStamp<=t.dueTime_) { printf("right time\n"); }
        else { printf("wrong time\n"); }
    }
};

class XYFirstMethod : public Method {
public:
    void run(int sock, Task &t, long long timeStamp, int vehcCap) {
        auto maxReadyTimeIte = TaskQ.begin();
        /* locate the maximum readyTime of task, unreachable! */
        while(true) {
            if(maxReadyTimeIte->readyTime_>timeStamp || maxReadyTimeIte==TaskQ.end()) { break; }
            else { maxReadyTimeIte++; }
        }
        /* find the nearest task and this worker can handle */
        auto nearestIte = TaskQ.begin();
        int nearestDist = INT32_MAX;
        for(auto ite=TaskQ.begin(); ite!=maxReadyTimeIte; ite++) {
            if(ite->readyTime_<=timeStamp && ite->dueTime_>=timeStamp) {    /* available task */
                int dist = abs(ite->x_-t.x_) + abs(ite->y_-t.y_);
                if(dist<nearestDist && ite->demand_<=vehcCap) { nearestDist = dist; nearestIte = ite; }
            }
        }
        t = *nearestIte;
        TaskQ.erase(nearestIte);
        printf("task %d readyTime %d dueTime %d dispatch worker %d timeStamp %lld ", t.no_, t.readyTime_, t.dueTime_, sock, timeStamp);
        if(timeStamp>=t.readyTime_ && timeStamp<=t.dueTime_) { printf("right time\n"); }
        else { printf("wrong time\n"); }
    }
};

class DispatchAlgorithm {
private:
    Method *impl_;

public:
    DispatchAlgorithm() {}

    DispatchAlgorithm(Method *impl) : impl_(impl) {}

    void run(int sock, Task &t, int timeStamp, int vehcCap) {
        impl_->run(sock, t, timeStamp, vehcCap);
    }
};
DispatchAlgorithm *DpAlg = nullptr;

/* choose the type of dispatch algorithm */
void choose_dispatch_algorithm(char type)
{
    switch(type) {
        case 'A':
            DpAlg = new DispatchAlgorithm(new TimeFirstMethod());
            break;
        case 'B':
            DpAlg = new DispatchAlgorithm(new XYFirstMethod());
            break;
        default:
            break;
    }
}

struct Location {
    int x_, y_;

    Location() {}

    Location(int x, int y) : x_(x), y_(y) {}
};
/* the location of depot */
Location depot;

/* timer start thread */
void timer_run()
{
    while(WorkerNum != WorkerNumLimt)
        ;
    StartTime = std::chrono::steady_clock::now();
    Cv.notify_all();
    while(WeakUpNum != WorkerNumLimt)
        ;
    printf("notify all\n");
}

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
            TaskQ.insert(t);
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

/*  */
void worker_handle(int sock)
{
    char buf[BUF_SIZE];
    std::stack<int> args;
    Task t;
    int vehcCap;

    /* tell worker about depot */
    _generate_depot_rpc(buf);
    write(sock, buf, sizeof(buf));

    {
        std::unique_lock<std::mutex> lk(CvMtx);
//        printf("I'm %d get lock\n", sock);
        Cv.wait(lk);
    }
//    printf("I'm %d weak up\n", sock);
    WeakUpNum++;

    while(true) {
        if(read(sock, buf, BUF_SIZE) == 0) {
            break;
        }
        _extract_request_rpc(buf, args);
        _task_assignment_copy_from_args(args, t);
        vehcCap = args.top(); args.pop();

        Mtx.lock();
        if(!TaskQ.empty()) {
            auto timeStamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-StartTime).count();
            while(true) {
                if(timeStamp < TaskQ.begin()->readyTime_) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    timeStamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-StartTime).count();
                }
                else {
                    break;
                }
            }
            DpAlg->run(sock, t, timeStamp, vehcCap);
        }
        else {
            t.set_task_type(TaskEnd);
            printf("no task to do, worker %d\n", sock);
        }
        Mtx.unlock();
        _generate_reply_rpc(buf, t);
        write(sock, buf, sizeof(buf));
    }

    printf("worker %d disconnect!\n", sock);
    Mtx.lock();
    WorkerNum--;
    Mtx.unlock();
    close(sock);
}

int main(int argc, char const *argv[])
{
    int masterSock;
    Addr masterAddr;
    AddrSize masterAddrSize = sizeof(masterAddr);

    /* unvalid input */
    if(argc != 4) {
        printf("Usage: %s <port> <type> <workers>\n", argv[0]);
        exit(1);
    }

    masterSock = socket(PF_INET, SOCK_STREAM, 0);
    if(masterSock == -1) {
        printf("sock error\n");
        exit(1);
    }

    memset(&masterAddr, 0, masterAddrSize);
    masterAddr.sin_family = AF_INET;
    masterAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    masterAddr.sin_port = htons(atoi(argv[1]));

    if(bind(masterSock, (struct sockaddr*)&masterAddr, masterAddrSize) == -1) {
        printf("bind error\n");
        exit(1);
    }

    if(listen(masterSock, BUF_SIZE) == -1) {
        printf("listen error\n");
        exit(1);
    }

    int workerSock;
    Addr workerAddr;
    AddrSize workerAddrSize;

    /* pre-process */
    scan_from_csv();

    /* choose the type of dispatch algorithm */
    choose_dispatch_algorithm(argv[2][0]);

    /* set the WorkerNumLimt */
    WorkerNumLimt = atoi(argv[3]);

    for(int i=0; i<WorkerNumLimt; i++) {
        /* try to accept the request */
        workerSock = accept(masterSock, (struct sockaddr*)&workerAddr, &workerAddrSize);
        if(workerSock == -1) {
            printf("accept error");
            exit(1);
        }
        ++WorkerNum;
        printf("new worker %d connected\n", workerSock);
        /* create thread for worker */
        std::thread(worker_handle, workerSock).detach();
    }
    /* run timer */
    std::async(std::launch::async, timer_run);
    while(true)
        ;

    return 0;
}

