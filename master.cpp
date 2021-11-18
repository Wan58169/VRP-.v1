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

/* min demand */
const int MinDemand = 10;

/* model st. cost */
const float KilmsCostVct[] = {0, 0.85, 1.15, 1.50};
const float DotsCostVct[] = {0, 15, 11.5, 8.5};

class Method {
public:
    virtual void run(const int, Task&, const long long, const int, const int)=0;
};

class XYFirstMethod : public Method {
public:
    void run(const int sock, Task &t, const long long timeStamp, const int vehcCap, const int kilms) {
        auto maxReadyTimeIte = TaskQ.begin();
        /* locate the maximum readyTime of task, unreachable! */
        while(true) {
            if(maxReadyTimeIte->get_readyTime()>timeStamp || maxReadyTimeIte==TaskQ.end()) { break; }
            else { maxReadyTimeIte++; }
        }
        /* find the best task and this worker can handle */
        auto bestIte = TaskQ.begin();
        auto secondIte = TaskQ.begin();
        int bestDist = INT32_MAX;
        int secondDist = INT32_MAX;
        for(auto ite=TaskQ.begin(); ite!=maxReadyTimeIte; ite++) {
            /* available task */
            if(ite->get_readyTime()<=timeStamp && ite->get_dueTime()>=timeStamp && ite->get_demand()<=vehcCap) {
                Location xy1 = t.get_xy();
                Location xy2 = ite->get_xy();
                int dist = abs(xy1.get_x()-xy2.get_x()) + abs(xy1.get_y()-xy2.get_y());
                int g = kilms + dist;   /* passed value */
                int h = abs(xy2.get_x()-Depot.get_x()) + abs(xy2.get_y()-Depot.get_y());    /* hope value */
                int f = g + h;          /* total value */
                if(f <= bestDist) { bestDist = f; bestIte = ite; }
                if(f<=secondDist && f>=bestDist) { secondDist = f; secondIte = ite; }
            }
        }
        /* compare best and second */
        int dDemand = secondIte->get_demand()-bestIte->get_demand();
        if(dDemand > MinDemand) {
            int dCost = (secondDist-bestDist) * KilmsCostVct[t.get_no()];
            if(dCost > DotsCostVct[t.get_no()]*(dDemand/MinDemand)) {   /* choose the second */
                t = *secondIte;
                TaskQ.erase(secondIte);
                return;
            }
        }
        t = *bestIte;
        TaskQ.erase(bestIte);
    }
};

class DispatchAlgorithm {
private:
    Method *impl_;

public:
    DispatchAlgorithm() {}

    DispatchAlgorithm(Method *impl) : impl_(impl) {}

    void run(const int sock, Task &t, const long long timeStamp, const int vehcCap, const int kilms) {
        impl_->run(sock, t, timeStamp, vehcCap, kilms);
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
void respond_worker(const int sock, char buf[], Task &t, int &vehcCap, int &kilms)
{
    std::stack<int> args;

    _extract_request_rpc(buf, args);
    _task_assignment_copy_from_args(args, t);
    vehcCap = args.top(); args.pop();
    kilms = args.top(); args.pop();
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
    int vehcCap, kilms;

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
        respond_worker(sock, buf, t, vehcCap, kilms);

        Mtx.lock();
        if(!TaskQ.empty()) {
            auto timeStamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-StartTime).count();
            while(timeStamp < TaskQ.begin()->get_readyTime()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                timeStamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-StartTime).count();
            }
            DpAlg->run(sock, t, timeStamp, vehcCap, kilms);
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
    WorkerNum--;
    close(sock);
}

/* prepare data and preprocess */
void scan_from_csv(std::multiset<Task, TaskCmp> &taskQ, Location &depot)
{
    /* data set */
    FILE *fp;
    char buf[BUF_SIZE];
    std::stack<int> args;      /* @args: no, xy, demand, readyTime, dueTime, serviceTime */
    Task t;

    if( (fp=fopen("data.csv", "r")) ) {
        fseek(fp, 66L, SEEK_SET);   /* locate the second line */
        /* get the location of depot */
        fgets(buf, BUF_SIZE, fp);
        _extract_taskInfo_from_csv(buf, args);
        /* assignment the location of depot */
        args.pop();
        int x = args.top(); args.pop();
        int y = args.top(); args.pop();
        depot = Location(x, y);
        /* clear the args */
        while(!args.empty()) { args.pop(); }
        /* scan the cluster */
        while( fgets(buf, BUF_SIZE, fp) ) {
            buf[strlen(buf)-1] = '\0';  /* replace the end of str: '\n'->'\0' */
            _extract_taskInfo_from_csv(buf, args);
            _task_assignment_copy_from_args(args, t);
            /* fix readyTime */
            int delayTime = t.get_dueTime() - t.get_serviceTime();
//            printf("no:%d, dueTime:%d, serviceTime:%d, delayTime:%d\n", t.get_no(), t.get_dueTime(), t.get_serviceTime(), delayTime);
            if(delayTime >= t.get_readyTime()) {
                t.set_readyTime(delayTime);
            }
            taskQ.insert(t);
        }
    }

    fclose(fp);
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

    close(masterSock);

    return 0;
}

