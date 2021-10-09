//
// Created by WAN on 2021/10/9.
//

#ifndef VRP_RPC_H
#define VRP_RPC_H

#include <iostream>
#include <stack>
#include <set>
#include <stdio.h>
#include <string.h>

#define BUF_SIZE 256

const int TaskEnd = -1;

class Task {
private:
    int no_;
    int x_;
    int y_;
    int demand_;
    int readyTime_;
    int dueTime_;
    int serviceTime_;

public:
    Task() {}

    Task(int no) : no_(no) {}

    Task(int no, int x, int y, int demand, int readyTime, int dueTime, int serviceTime)
    : no_(no), x_(x), y_(y), demand_(demand), readyTime_(readyTime), dueTime_(dueTime), serviceTime_(serviceTime) {}

    int get_no() const { return no_; }

    int get_x() const { return x_; }

    int get_y() const { return y_; }

    int get_demand() const { return demand_; }

    int get_readyTime() const { return readyTime_; }

    int get_dueTime() const { return dueTime_; }

    int get_serviceTime() const { return serviceTime_; }

    void demand_clear() { demand_ = 0; }
};

struct TaskCmp : public std::binary_function<Task, Task, bool> {
    bool operator() (const Task &lhs, const Task &rhs) const {
        if(lhs.get_readyTime() < rhs.get_readyTime()) {
            return true;
        }
        else if(lhs.get_readyTime() == rhs.get_readyTime()) {
            return lhs.get_dueTime()<rhs.get_dueTime() ? true: false;
        }
        else {
            return false;
        }
    }
};

class Location {
private:
    int x_, y_;

public:
    Location() {}

    Location(int x, int y) : x_(x), y_(y) {}

    int get_x() const { return x_; }

    int get_y() const { return y_; }
};

/* for various kinds extraction */
void __extract_func(char msg[], std::stack<int> &args);

/* @depot rpc: x, y */
void _extract_depot_rpc(char msg[], Location &depot);

/* @task: no, x, y, demand, readyTime, dueTime, serviceTime */
void _extract_taskInfo_from_csv(char msg[], std::stack<int> &args);

/* @request rpc: no, x, y, demand, readyTime, dueTime, serviceTime, vehcCap */
void _extract_request_rpc(char msg[], std::stack<int> &args);

/* @reply rpc: task's no, x, y, demand, readyTime, dueTime, serviceTime */
void _extract_reply_rpc(char msg[], std::stack<int> &args);

/* task assignment copy */
void _task_assignment_copy_from_args(std::stack<int> &args, Task &t);

/* @depot rpc: x, y */
void _generate_depot_rpc(char msg[], const Location &depot);

/* @request rpc: task's no, x, y, demand, readyTime, dueTime, serviceTime; vehcCap */
void _generate_request_rpc(char msg[], const Task &t, const int vehcCap);

/* @reply rpc: task's no, x, y, demand, readyTime, dueTime, serviceTime */
void _generate_reply_rpc(char msg[], const Task &t);

/* prepare data */
void scan_from_csv(std::multiset<Task, TaskCmp> &taskQ, Location &depot);

#endif //VRP_RPC_H
