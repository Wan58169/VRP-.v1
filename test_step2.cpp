//
// Created by WAN on 2021/12/6.
//
#include "rpc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

using namespace std;

/* model st. cost */
const float KilmsCostVct[] = {0, 1.6, 1.75, 1.85};
const float DotsCostVct[] = {0, 25, 20, 15};

bool _is_right_solution(const int idx, const int workerNum)
{
    string file = "result/tmp/" + to_string(idx) + "/master.txt";
    FILE *fp = fopen(file.c_str(), "r");
    char buf[256];

    /* ignore workerNum worker and one "notify all" statement */
    for(int i=0; i<=workerNum; i++) {
        fgets(buf, sizeof(buf), fp);
    }

    /* check is right or wrong */
    while(fgets(buf, sizeof(buf), fp)) {
        if(strstr(buf, "wrong")) {
            fclose(fp);
            return false;
        }
    }

    fclose(fp);
    return true;
}

float _cal_solution_cost(const int idx, const int workerNum)
{
    float cost = 0;

    for(int i=1; i<=workerNum; i++) {
        string file = "result/tmp/" + to_string(idx) + "/worker" + to_string(i) + ".txt";
        FILE *fp = fopen(file.c_str(), "r");

        char buf[256], *p, *q;
        Location preLoc;
        int vehcType;

        /* read the first line, ignore */
        fgets(buf, sizeof(buf), fp);
        /* read the second line, extract the depot */
        fgets(buf, sizeof(buf), fp); buf[strlen(buf)-1] = '\0';
        p = strchr(buf, '('); q = strchr(buf, ','); *q = '\0';
        int x = atoi(p+1); int y = atoi(q+1);
        preLoc = Location(x, y);
        /* read the third line, extract the vehcType */
        fgets(buf, sizeof(buf), fp); buf[strlen(buf)-1] = '\0';
        switch(buf[strlen(buf)-1]) {
            case 'A':
                vehcType = 1; break;
            case 'B':
                vehcType = 2; break;
            case 'C':
                vehcType = 3; break;
        }
        /* read the points */
        while(fgets(buf, sizeof(buf), fp)) {
            p = strchr(buf, '(');
            if(!p) { continue; }
            q = strchr(buf, ','); *q = '\0';
            x = atoi(p+1); *q = ',';
            p = strchr(buf, ')'); *p = '\0';
            y = atoi(q+1);
            cost += (abs(x-preLoc.get_x())+abs(y-preLoc.get_y()))*KilmsCostVct[vehcType] + DotsCostVct[vehcType];
            preLoc = Location(x, y);
        }
        fclose(fp);
    }

    return cost;
}

int choose_the_best_solution(const int solutionNum, const int workerNum)
{
    int minIdx = -1;
    float minCost = INT32_MAX;

    for(int i=1; i<=solutionNum; i++) {
        if(_is_right_solution(i, workerNum)) {
            float cost = _cal_solution_cost(i, workerNum);
            printf("solutions[%d] is right, cost..%f\n", i, cost);
            if(cost < minCost) {
                minCost = cost;
                minIdx = i;
            }
        } else {
            printf("solutions[%d] is wrong\n", i);
        }
    }

    return minIdx;
}

int main(int argc, char const *argv[])
{
    if(argc != 3) {
        printf("Usage: %s <workers> <solutions>\n", argv[0]);
        exit(1);
    }

    const int workerNum = atoi(argv[1]);
    const int solutionNum = atoi(argv[2]);

    int bestSolutionIdx = choose_the_best_solution(solutionNum, workerNum);

    printf("best solution is %d\n", bestSolutionIdx);

    return 0;
}

