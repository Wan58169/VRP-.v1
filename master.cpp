//
// Created by WAN on 2021/9/19.
//
#include "rpc.h"
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include <thread>
#include <vector>
#include <queue>
#include <vector>
#include <condition_variable>
#include <mutex>

typedef struct sockaddr_in Addr;
typedef socklen_t AddrSize;

/* the location of depot */
static Location Depot;

/* timer */
static std::chrono::steady_clock::time_point StartTime;

/* TaskQueue and mutex */
static std::mutex Mtx;
static std::multiset<Task, TaskCmp> TaskQ;

/* thread sync */
static std::mutex CvMtx;
static std::condition_variable Cv;
static std::atomic<bool> Ready(false);
static std::atomic<int> WeakUpNum(0);

/* the number of workers */
static std::atomic<int> WorkerNum(0);
static std::atomic<int> WorkerNumLimt(0);

class Method {
public:
    virtual void run(const int, Task&, const long long, const int)=0;
};

class TimeFirstMethod : public Method {
public:
    void run(const int sock, Task &t, const long long timeStamp, const int vehcCap) {
        auto targetIte = TaskQ.begin();
        t = *targetIte;
        TaskQ.erase(targetIte);
    }
};

class XYFirstMethod : public Method {
public:
    void run(const int sock, Task &t, const long long timeStamp, const int vehcCap) {
        auto maxReadyTimeIte = TaskQ.begin();
        /* locate the maximum readyTime of task, unreachable! */
        while(true) {
            if(maxReadyTimeIte->get_readyTime()>timeStamp || maxReadyTimeIte==TaskQ.end()) { break; }
            else { maxReadyTimeIte++; }
        }
        /* find the nearest task and this worker can handle */
        auto nearestIte = TaskQ.begin();
        int nearestDist = INT32_MAX;
        for(auto ite=TaskQ.begin(); ite!=maxReadyTimeIte; ite++) {
            if(ite->get_readyTime()<=timeStamp && ite->get_dueTime()>=timeStamp) {    /* available task */
                int dist = abs(ite->get_x()-t.get_x()) + abs(ite->get_y()-t.get_y());
                if(dist<nearestDist && ite->get_demand()<=vehcCap) { nearestDist = dist; nearestIte = ite; }
            }
        }
        t = *nearestIte;
        TaskQ.erase(nearestIte);
    }
};

class DemandFirstMethod : public Method {
public:
    void run(const int sock, Task &t, const long long timeStamp, const int vehcCap) {
        auto maxReadyTimeIte = TaskQ.begin();
        /* locate the maximum readyTime of task, unreachable! */
        while(true) {
            if(maxReadyTimeIte->get_readyTime()>timeStamp || maxReadyTimeIte==TaskQ.end()) { break; }
            else { maxReadyTimeIte++; }
        }
        /* find the maximum demand task and this worker can handle */
        auto maxDemandtIte = TaskQ.begin();
        int maxDemand = -1;
        for(auto ite=TaskQ.begin(); ite!=maxReadyTimeIte; ite++) {
            if(ite->get_readyTime()<=timeStamp && ite->get_dueTime()>=timeStamp) {    /* available task */
                int demand = ite->get_demand();
                if(demand>maxDemand && demand<=vehcCap) { maxDemandtIte = ite; maxDemand = demand; }
            }
        }
        t = *maxDemandtIte;
        TaskQ.erase(maxDemandtIte);
    }
};

class DispatchAlgorithm {
private:
    Method *impl_;

public:
    DispatchAlgorithm() {}

    DispatchAlgorithm(Method *impl) : impl_(impl) {}

    void run(const int sock, Task &t, const long long timeStamp, const int vehcCap) {
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
        case 'C':
            DpAlg = new DispatchAlgorithm(new DemandFirstMethod());
            break;
        default:
            printf("unvalid method type\n");
            break;
    }
}

/* timer start thread */
void timer_run()
{
    while(WorkerNum != WorkerNumLimt)
        ;
    StartTime = std::chrono::steady_clock::now();
    {
        std::lock_guard<std::mutex> lg(Mtx);
        Ready = true;
        Cv.notify_all();
    }
    while(WeakUpNum != WorkerNumLimt)
        ;
    printf("notify all\n");
}

/* tell worker about depot */
void tell_worker_depot(const int sock, const Location &depot)
{
    char buf[BUF_SIZE];

    _generate_depot_rpc(buf, Depot);
    write(sock, buf, sizeof(buf));
}

/* respond to the worker's request */
void respond_worker(const int sock, char buf[], Task &t, int &vehcCap)
{
    std::stack<int> args;

    _extract_request_rpc(buf, args);
    _task_assignment_copy_from_args(args, t);
    vehcCap = args.top(); args.pop();
}

/* reply the worker */
void reply_worker(const int sock, const Task &t)
{
    char buf[BUF_SIZE];

    _generate_reply_rpc(buf, t);
    write(sock, buf, sizeof(buf));
}

void worker_handle(int sock)
{
    char buf[BUF_SIZE];
    Task t;
    int vehcCap;

    tell_worker_depot(sock, Depot);
    /* wait util timer start */
    {
        std::unique_lock<std::mutex> lk(CvMtx);
        Cv.wait(lk, []{ return Ready.load(); });
    }
    WeakUpNum++;

    while(true) {
        if(read(sock, buf, BUF_SIZE) == 0) {
            break;
        }
        respond_worker(sock, buf, t, vehcCap);

        Mtx.lock();
        if(!TaskQ.empty()) {
            auto timeStamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-StartTime).count();
            while(timeStamp < TaskQ.begin()->get_readyTime()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                timeStamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-StartTime).count();
            }
            DpAlg->run(sock, t, timeStamp, vehcCap);
            printf("task %d readyTime %d dueTime %d dispatch worker %d timeStamp %lld ", t.get_no(), t.get_readyTime(), t.get_dueTime(), sock, timeStamp);
            if(timeStamp>=t.get_readyTime() && timeStamp<=t.get_dueTime()) { printf("right time\n"); }
            else { printf("wrong time\n"); }
        }
        else {
            t = Task(TaskEnd);
            printf("no task to do, worker %d\n", sock);
        }
        Mtx.unlock();
        reply_worker(sock, t);
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
    scan_from_csv(TaskQ, Depot);

    /* choose the type of dispatch algorithm */
    choose_dispatch_algorithm(argv[2][0]);

    /* set the WorkerNumLimt */
    WorkerNumLimt = atoi(argv[3]);

    std::vector<std::thread> threads;

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
        threads.push_back(std::thread(worker_handle, workerSock));
    }
    /* run timer */
    threads.push_back(std::thread(timer_run));

    for(auto &v : threads) { v.join(); }

    return 0;
}

