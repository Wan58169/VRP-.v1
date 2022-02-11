//
// Created by WAN on 2021/10/9.
//

#ifndef VRP_RPC_H
#define VRP_RPC_H

#include <iostream>
#include <stack>
#include <stdio.h>
#include <string.h>

class Location {
private:
    int x_, y_;

public:
    Location() {}

    Location(int x, int y) : x_(x), y_(y) {}

    int get_x() const { return x_; }

    int get_y() const { return y_; }
};

class Task {
private:
    int no_;
    Location xy_;
    int demand_;
    int readyTime_;
    int dueTime_;
    int serviceTime_;
    bool needFix_;

public:
    Task() {}

    Task(int no) : no_(no) {}

    Task(int no, Location xy, int demand, int readyTime, int dueTime, int serviceTime)
    : no_(no), xy_(xy), demand_(demand), readyTime_(readyTime), dueTime_(dueTime), serviceTime_(serviceTime) {}

    int get_no() const { return no_; }

    Location get_xy() const { return xy_; }

    int get_demand() const { return demand_; }

    int get_readyTime() const { return readyTime_; }

    int get_dueTime() const { return dueTime_; }

    int get_serviceTime() const { return serviceTime_; }

    bool get_needFix() const { return needFix_; }

    void set_no(int no) { no_ = no; }

    void set_readyTime(int readyTime) { readyTime_ = readyTime; }

    void set_needFix(bool needFix) { needFix_ = needFix; }
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

/* for various kinds extraction */
void __extract_func(char msg[], std::stack<int> &args);

/* @depot rpc: x, y */
void _extract_depot_rpc(char msg[], Location &depot);

/* @task: no, x, y, demand, readyTime, dueTime, serviceTime */
void _extract_taskInfo_from_csv(char msg[], std::stack<int> &args);

/* @request rpc: no, x, y, demand, readyTime, dueTime, serviceTime; vehcCap, restCap */
void _extract_request_rpc(char msg[], std::stack<int> &args);

/* @reply rpc: task's no, x, y, demand, readyTime, dueTime, serviceTime */
void _extract_reply_rpc(char msg[], std::stack<int> &args);

/* task assignment copy */
void _task_assignment_copy_from_args(std::stack<int> &args, Task &t);

/* @depot rpc: x, y */
void _generate_depot_rpc(char msg[], const Location &depot);

/* @request rpc: task's no, x, y, demand, readyTime, dueTime, serviceTime; vehcCap, restCap */
void _generate_request_rpc(char msg[], const Task &t, const int vehcCap, const int restCap);

/* @reply rpc: task's no, x, y, demand, readyTime, dueTime, serviceTime; taskType */
void _generate_reply_rpc(char msg[], const Task &t, const int type);

#endif //VRP_RPC_H
