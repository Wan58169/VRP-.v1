# VRP
read 运输算法.xmind

This program needs one master, three workers.

compile master.cpp: **g++ master.cpp -std=c++11 -o master**

run master: **./master 9022**
  
compile worker.cpp: **g++ worker.cpp -std=c++11 -o worker**

run workerA: **./worker 127.0.0.1 9022 A**

run workerB: **./worker 127.0.0.1 9022 B**

run workerC: **./worker 127.0.0.1 9022 C**

  


