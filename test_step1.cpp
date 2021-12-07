//
// Created by WAN on 2021/12/3.
//
#include "rpc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <thread>

using namespace std;

/* model st. cost */
const float KilmsCostVct[] = {0, 1.6, 1.75, 1.85};
const float DotsCostVct[] = {0, 25, 20, 15};

int generate_solutions(const string file, const int workerNum)
{
    string master = "./master";
    string worker = "./worker";
    string ip = "127.0.0.1";
    int port = 9033;
    string outPath = "> result/tmp/";
    string cmdRest = ".txt &";
    int workerNo = 1;
    int solutionNo = 0;

    /* compile master.cpp and worker.cpp */
    system("g++ master.cpp rpc.cpp -o master -std=c++11 -lpthread");
    system("g++ worker.cpp rpc.cpp -o worker -std=c++11");

    /* drop all result first */
    system("cd result/tmp/ && rm -rf *");
    system("cd ../..");

    for(int a=0; a<=workerNum; a++) {
        for(int b=0; b<=workerNum-a; b++) {
            int c = workerNum-a-b;

            solutionNo++;

            string cmd = "cd result/tmp/ && mkdir " + to_string(solutionNo);
            system(cmd.c_str());    // mkdir for solution

            /* ./master 9033 14 .csv > result/tmp/solutionNo/master.txt & */
            cmd = master + " " + to_string(port) + " " + to_string(workerNum) + " " + file + " " +
                    outPath + to_string(solutionNo) + "/master" + cmdRest;
            system(cmd.c_str());

            /* start worker A */
            for(int i=0; i<a; i++) {
                /* ./worker 127.0.0.1 9033 A > result/tmp/solutionNo/workerx.txt & */
                cmd = worker + " " + ip + " " + to_string(port) + " A " +
                        outPath + to_string(solutionNo) + "/worker" + to_string(workerNo) + cmdRest;
                system(cmd.c_str());
                workerNo++;
            }
            /* start worker B */
            for(int i=0; i<b; i++) {
                /* ./worker 127.0.0.1 9033 B > result/tmp/solutionNo/workerx.txt & */
                cmd = worker + " " + ip + " " + to_string(port) + " B " +
                        outPath + to_string(solutionNo) + "/worker" + to_string(workerNo) + cmdRest;
                system(cmd.c_str());
                workerNo++;
            }
            /* start worker C */
            for(int i=0; i<c; i++) {
                /* ./worker 127.0.0.1 9033 C > result/tmp/solutionNo/workerx.txt & */
                cmd = worker + " " + ip + " " + to_string(port) + " C " +
                        outPath + to_string(solutionNo) + "/worker" + to_string(workerNo) + cmdRest;
                system(cmd.c_str());
                workerNo++;
            }

            workerNo = 1;
            port++;

//            printf("solutions[%d]: a..%d, b..%d, c..%d\n", solutionNo, a, b, c);
            system("cd ../..");
        }
    }

    return solutionNo;
}

int main(int argc, char const *argv[])
{
    if(argc != 3) {
        printf("Usage: %s <fileName> <workers>\n", argv[0]);
        exit(1);
    }

    const string file = argv[1];      // the path of .csv
    const int workerNum = atoi(argv[2]);  // the WorkerNum

    int solutionNum = generate_solutions(file, workerNum);

    printf("%d %d", workerNum, solutionNum);

    return 0;
}

