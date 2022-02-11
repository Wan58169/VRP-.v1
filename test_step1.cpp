//
// Created by WAN on 2021/12/3.
//
#include "common.h"
#include "rpc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <thread>

using namespace std;

int generate_solutions(const string file, const int workerNum)
{
    string master = "./master";
    string worker = "./worker";
    string ip = "127.0.0.1";
    int port = 9033;
    string cmdRest = " &";
    int workerNo = 1;
    int solutionNo = 0;

    /* drop all result first */
    system("cd result/tmp/ && rm -rf *");
    system("cd ../..");

    for(int a=0; a<=workerNum; a++) {
        for(int b=0; b<=workerNum-a; b++) {
            int c = workerNum-a-b;

            solutionNo++;

            string cmd = "cd result/tmp/ && mkdir " + to_string(solutionNo);
            system(cmd.c_str());    // mkdir for solution

            /* ./master 9033 14 .csv solutionNo & */
            cmd = master + " " + to_string(port) + " " + to_string(workerNum) + " " + file + " "
                    + to_string(solutionNo) + cmdRest;
            system(cmd.c_str());

            /* start worker A */
            for(int i=0; i<a; i++) {
                /* ./worker 127.0.0.1 9033 A workerNo solutionNo & */
                cmd = worker + " " + ip + " " + to_string(port) + " A "
                        + to_string(workerNo) + " " + to_string(solutionNo) + cmdRest;
                system(cmd.c_str());
                workerNo++;
            }
            /* start worker B */
            for(int i=0; i<b; i++) {
                /* ./worker 127.0.0.1 9033 B workerNo solutionNo & */
                cmd = worker + " " + ip + " " + to_string(port) + " B "
                        + to_string(workerNo) + " " + to_string(solutionNo) + cmdRest;
                system(cmd.c_str());
                workerNo++;
            }
            /* start worker C */
            for(int i=0; i<c; i++) {
                /* ./worker 127.0.0.1 9033 C workerNo solutionNo & */
                cmd = worker + " " + ip + " " + to_string(port) + " C "
                        + to_string(workerNo) + " " + to_string(solutionNo) + cmdRest;
                system(cmd.c_str());
                workerNo++;
            }

            workerNo = 1;
            port++;

            printf("solutions[%d] is done\n", solutionNo);

            this_thread::sleep_for(chrono::milliseconds(100));

            system("cd ../..");
        }
    }

    return solutionNo;
}

int main(int argc, char const *argv[])
{
    if(argc != 3) {
        printf("Usage: %s <fileName> <workerNum>\n", argv[0]);
        exit(1);
    }

    const string file = argv[1];      // the path of .csv
    const int workerNum = atoi(argv[2]);  // the WorkerNum

    int solutionNum = generate_solutions(file, workerNum);

    char buf[16];
    sprintf(buf, "%d %d\n", workerNum, solutionNum);
    printf("%s", buf);

    FILE *fp = fopen("result/total.txt", "w");
    fputs(buf, fp);

    return 0;
}

