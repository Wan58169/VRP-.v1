#include<iostream>

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include "string"
#include "math.h"
#include <cstring>

#include <fstream>
#include <sstream>
#include <vector>
#include <limits.h>
using namespace std;

#define MAX_VEC_NUM 11
#define SLEEP_TIME 2
//#define INT_MAX 2147483647

const char* nums = "123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefg";
/* model st. cost */
const float KilmsCostVct[] = {0, 1.6, 1.75, 1.85};
const float DotsCostVct[] = {0, 25, 14, 5};
/*
    cost_worker_norm = [400, 450, 480]
    cost_kilometer_norm = [1.6, 1.75, 1.85]
    cost_every_depot_every_car = [20, 10, 0]  # 22 10 0
    cost_every_kilometer = [10, 13, 15]
*/
const float WorkersCostVct[]={0,400,450,480};
const float KilmsWorkersCostVct[]={0,12,15,18};

int find_wrong()
{
    const char * filename = "result/MAS.txt";
    string search = "wrong";
    string judge;
    int count = 0;
    string line;

    ifstream file;													/*绑定并打开文件*/
    file.open(filename);
    if (!file)													/*检查文件能否打开*/
    {
        cout << "please correct the file has been set with the exe programm" << endl;
        return -1;

    }
    getline(file, line);
    while (file)																				/*一行一行查找*/
    {
        if (line.find(search) < string::npos)
            count++;
        getline(file, line);
    }
    return count;
}

vector<float> cal_cost(int a,int b,int c){
    vector<float> cost(4,float(0));
    string filename = "result/1.txt";
    for(int i=0;i<MAX_VEC_NUM;i++){
        filename[7]=nums[i];
        std::ifstream fIn(filename.c_str());
        if (fIn)
        {
            std::string str;
            std::getline(fIn,str);
            cout<<filename<<endl;
            if(str!="connected.."){
                printf("%d %d %d-this sample has errors",a,b,c);
                vector<float> error(4,0);
                return error;
            }
            std::getline(fIn, str);
            std::cout << str << std::endl;
            string first = str.substr(0,str.find(" "));
            string second = str.substr(str.find(" "));
            int x = atoi(first.c_str());
            int y = atoi(second.c_str());
            int sum_dis=0;
            int dot_count=1;
            while (std::getline(fIn, str))
            {
                std::cout << str << std::endl;
                string first = str.substr(0,str.find(" "));
                string second = str.substr(str.find(" "));
                int now_x =atoi(first.c_str());
                int now_y = atoi(second.c_str());
                sum_dis += sqrt((now_x-x)*(now_x-x)+(now_y-y)*(now_y-y));
                dot_count++;
                //cout<<"first:"<<first<<"\tsecond:"<<second<<endl;
            }
            cout<<"this vechile's distance is:"<<sum_dis<<endl;
            int VEC=0;
            if(i<a)
                VEC=1;
            else if(i<b+a)
                VEC=2;
            else VEC=3;
            cost[VEC]+=sum_dis/10 * (KilmsCostVct[VEC]+KilmsWorkersCostVct[VEC]) + dot_count*DotsCostVct[VEC];
        }
        else
        {
            std::cout << "Open file faild." << std::endl;
        }

        fIn.close();
    }
    return cost;
}
int main(){
    int min_a=0,min_b=0,min_c=0;
    float min_cost = INT_MAX;
    float cost_all_a = INT_MAX,cost_all_b=INT_MAX,cost_all_c=INT_MAX;
    string a_vec = "./worker 127.0.0.1 8888 A>result/1.txt &";
    string b_vec = "./worker 127.0.0.1 8888 B>result/1.txt &";
    string c_vec = "./worker 127.0.0.1 8888 C>result/1.txt &";
    string mas_vecs = "./master 8888 12>result/MAS.txt &";
    //a b c vec numbers
    int a,b,c;
    if(MAX_VEC_NUM%10==0) { mas_vecs[15]='0'; }
    else { mas_vecs[15]=nums[MAX_VEC_NUM%10 -1]; }

    mas_vecs[14]=nums[MAX_VEC_NUM/10 -1];

    for(a=0;a<=MAX_VEC_NUM;a++){
        for(b=0;b<=MAX_VEC_NUM-a;b++){
            c = MAX_VEC_NUM-a-b;
            //start
            //kill master and worker
//            system("ps -ef|grep master|grep -v grep|cut -c 9-15|xargs kill -9");
//            system("ps -ef|grep worker|grep -v grep|cut -c 9-15|xargs kill -9");
            printf("before %s\n", mas_vecs.c_str());
            system(mas_vecs.c_str());
            printf("after %s\n", mas_vecs.c_str());

            for(int i=0;i<a;i++) {
                a_vec[35] = nums[i];
                system(a_vec.c_str());
            }
            for(int i=0;i<b;i++){
                b_vec[35] = nums[a+i];
                system(b_vec.c_str());
            }
            for(int i=0;i<c;i++){
                c_vec[35] = nums[a+b+i];
                system(c_vec.c_str());
            }

            sleep(SLEEP_TIME);
            printf("%d %d %d\n\n",a,b,c);
            //judge reasonable
            if(find_wrong()!=0)
                cout<<"wrong"<<endl;
            else{
                vector<float> cost = cal_cost(a,b,c);
                cout<<cost[1]<<"\t"<<cost[2]<<"\t"<<cost[3]<<endl;
                float cost_all =cost[1]+cost[2]+cost[3]+a*WorkersCostVct[1]+b*WorkersCostVct[2]+c*WorkersCostVct[3];
                if(cost_all<min_cost && (int)(cost[1]+cost[2]+cost[3]) > 0){
                    min_a=a;
                    min_b=b;
                    min_c=c;
                    min_cost=cost_all;
                }
                if(a==MAX_VEC_NUM)
                    cost_all_a=cost_all;
                else if(b==MAX_VEC_NUM)
                    cost_all_b=cost_all;
                else if(c==MAX_VEC_NUM)
                    cost_all_c=cost_all;
            }
            printf("here\n");
        }
    }
    printf("this dataset's min result is:%d\t%d\t%d\t%lf\n",min_a,min_b,min_c,min_cost);
    printf("result of single vechile type:%lf\t%lf\t%lf\n",cost_all_a,cost_all_b,cost_all_c);
	printf("%lf\t%lf\t%lf\t%d\t%d\t%d\t%lf\n",cost_all_a,cost_all_b,cost_all_c,min_a,min_b,min_c,min_cost);
}
