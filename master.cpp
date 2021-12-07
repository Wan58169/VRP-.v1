//
// Created by WAN on 2021/9/19.
//
#include "rpc.h"
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include <thread>
#include <vector>
#include <bitset>
#include <tuple>
#include <condition_variable>
#include <atomic>
#include <mutex>

typedef struct sockaddr_in Addr;
typedef socklen_t AddrSize;

/* the location of depot */
static Location Depot;

/* timer */
static std::chrono::steady_clock::time_point StartTime;

/* TaskQueue and mutex */
static std::mutex Mtx;
static std::vector<Task> TaskQ;

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
    void run(const int sock, Task &t, const long long timeStamp, const int vehcCap) {
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
                int g = abs(xy1.get_x()-xy2.get_x()) + abs(xy1.get_y()-xy2.get_y());        /* try value */
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
            if(dCost > DotsCostVct[t.get_no()]) {   /* choose the second */
                t = *secondIte;
                TaskQ.erase(secondIte);
                return;
            }
        }
        t = *bestIte;
        TaskQ.erase(bestIte);
    }
};

static Method method;

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
            method.run(sock, t, timeStamp, vehcCap);
            printf("task %d readyTime %d dueTime %d dispatch worker %d timeStamp %lld ",
                   t.get_no(), t.get_readyTime(), t.get_dueTime(), sock, timeStamp);
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

/* prepare data */
void scan_from_csv(const char fileName[])
{
    /* data set */
    FILE *fp;
    char buf[BUF_SIZE];
    std::string path = "dataset/";
    std::stack<int> args;      /* @args: no, xy, demand, readyTime, dueTime, serviceTime */
    Task t;

    path += fileName;   // generate the path of .csv

    if( (fp=fopen(path.c_str(), "r")) ) {
        fseek(fp, 66L, SEEK_SET);   /* locate the second line */
        /* get the location of depot */
        fgets(buf, BUF_SIZE, fp);
        _extract_taskInfo_from_csv(buf, args);
        /* assignment the location of depot */
        args.pop();
        int x = args.top(); args.pop();
        int y = args.top(); args.pop();
        Depot = Location(x, y);
        /* clear the args */
        while(!args.empty()) { args.pop(); }
        /* scan the cluster */
        while( fgets(buf, BUF_SIZE, fp) ) {
            _extract_taskInfo_from_csv(buf, args);
            _task_assignment_copy_from_args(args, t);
            /* pre fix readyTime */
            t.set_needFix(false);
            int delayTime = t.get_dueTime() - t.get_serviceTime();
            if(delayTime > t.get_readyTime()) {
                t.set_needFix(true);
            }

            TaskQ.push_back(t);
        }
    }
    fclose(fp);
    /* sort taksQ based readyTime */
    std::sort(TaskQ.begin(), TaskQ.end(), TaskCmp());
}

void fix_readyTime()
{
    std::bitset<1200> map;
    std::vector<Task> fixeds, needFixs;

    srand(time(NULL));  // true random

    /* fixeds & needFixs ctor, map counter */
    for(int i=0; i<TaskQ.size(); i++) {
        if(!TaskQ[i].get_needFix()) {
            map[TaskQ[i].get_readyTime()] = 1;
            fixeds.push_back(TaskQ[i]);
        } else {
            needFixs.push_back(TaskQ[i]);
        }
    }

    /* insert the need fix task into fixeds */
    for(auto &v : needFixs) {
        while(true) {
            int delayTime = v.get_dueTime() - v.get_serviceTime();
            int readyTime = v.get_readyTime();
            /* set the need fix task's readyTime */
            int time = rand() % (delayTime-readyTime+1) + readyTime;     // readyTime <= time <= delayTime
            if(map[time] == 0) {
                v.set_readyTime(time);
                v.set_needFix(false);
                fixeds.push_back(v);
                map[time] = 1;
                break;
            }
        }
    }

    TaskQ.clear();
    TaskQ.assign(fixeds.begin(), fixeds.end());
    /* sort taksQ based readyTime */
    std::sort(TaskQ.begin(), TaskQ.end(), TaskCmp());
}

void print_TaskQ()
{
    for(auto &v : TaskQ) {
        printf("no: %d, readyTime: %d, dueTime: %d, needFix: %d\n", v.get_no(), v.get_readyTime(), v.get_dueTime(), v.get_needFix());
    }
}

int main(int argc, char const *argv[])
{
    int masterSock;
    Addr masterAddr;
    AddrSize masterAddrSize = sizeof(masterAddr);

    /* unvalid input */
    if(argc != 4) {
        printf("Usage: %s <port> <workers> <fileName>\n", argv[0]);
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

    scan_from_csv(argv[3]);

    fix_readyTime();

//    print_TaskQ();

    /* set the WorkerNumLimt */
    WorkerNumLimt = atoi(argv[2]);

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

