//
// Created by WAN on 2021/12/6.
//
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <thread>

using namespace std;

int main(int argc, char const *argv[])
{
    if(argc != 3) {
        printf("Usage: %s <fileName> <workerNum>\n", argv[0]);
        exit(1);
    }

    string file = argv[1];
    int workerNum = atoi(argv[2]);

    /* compile master.cpp and worker.cpp */
    system("g++ master.cpp rpc.cpp common.cpp -o master -std=c++11");
    system("g++ worker.cpp rpc.cpp common.cpp -o worker -std=c++11");

    /* compile test_step1.cpp and test_step2.cpp */
    system("g++ test_step1.cpp rpc.cpp common.cpp -o test_1 -std=c++11");
    system("g++ test_step2.cpp rpc.cpp common.cpp -o test_2 -std=c++11");

    /* prepare run test_1 */
    string cmd = "./test_1 " + file + " " + to_string(workerNum);
    printf("run %s\n", cmd.c_str());
    system(cmd.c_str());

    printf("sleep 5s\n");
    this_thread::sleep_for(chrono::seconds(5));

    cmd = "cat result/total.txt |xargs ./test_2 ";
    printf("run %s\n", cmd.c_str());
    system(cmd.c_str());

    return 0;
}