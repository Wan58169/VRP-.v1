//
// Created by WAN on 2021/10/9.
//

#include "rpc.h"

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
void _extract_depot_rpc(char msg[], Location &depot)
{
    char *splitPtr;
    int x, y;

    splitPtr = strrchr(msg, ',');
    y = atoi(splitPtr+1);
    *splitPtr = '\0';
    x = atoi(msg);
    depot = Location(x, y);
}

/* @task: no, x, y, demand, readyTime, dueTime, serviceTime */
void _extract_taskInfo_from_csv(char msg[], std::stack<int> &args)
{
    __extract_func(msg, args);
}

/* @request rpc: no, x, y, demand, readyTime, dueTime, serviceTime, vehcCap */
void _extract_request_rpc(char msg[], std::stack<int> &args)
{
    __extract_func(msg, args);
}

/* @reply rpc: task's no, x, y, demand, readyTime, dueTime, serviceTime */
void _extract_reply_rpc(char msg[], std::stack<int> &args)
{
    __extract_func(msg, args);
}

/* task assignment copy */
void _task_assignment_copy_from_args(std::stack<int> &args, Task &t)
{
    int no = args.top(); args.pop(); int x = args.top(); args.pop(); int y = args.top(); args.pop();
    int demand = args.top(); args.pop(); int readyTime = args.top(); args.pop();
    int dueTime = args.top(); args.pop(); int serviceTime = args.top(); args.pop();
    t = Task(no, x, y, demand, readyTime, dueTime, serviceTime);
}

/* @depot rpc: x, y */
void _generate_depot_rpc(char msg[], const Location &depot)
{
    int x = depot.get_x(); int y = depot.get_y();
    sprintf(msg, "%d,%d", x, y);
}

/* @request rpc: task's no, x, y, demand, readyTime, dueTime, serviceTime; vehcCap */
void _generate_request_rpc(char msg[], const Task &t, const int vehcCap)
{
    sprintf(msg, "%d,%d,%d,%d,%d,%d,%d,%d", t.get_no(), t.get_x(), t.get_y(), t.get_demand(), t.get_readyTime(), t.get_dueTime(), t.get_serviceTime(), vehcCap);
}

/* @reply rpc: task's no, x, y, demand, readyTime, dueTime, serviceTime */
void _generate_reply_rpc(char msg[], const Task &t)
{
    sprintf(msg, "%d,%d,%d,%d,%d,%d,%d", t.get_no(), t.get_x(), t.get_y(), t.get_demand(), t.get_readyTime(), t.get_dueTime(), t.get_serviceTime());
}

/* prepare data */
void scan_from_csv(std::multiset<Task, TaskCmp> &taskQ, Location &depot)
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
            taskQ.insert(t);
        }
    }
}
