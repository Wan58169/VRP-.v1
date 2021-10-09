# VRP
read algorithm.xmind
![alt 图片](https://github.com/Wan58169/VRP/blob/master/algorithm.png)

This program needs one master, twelve workers.

compile master.cpp: `g++ master.cpp rpc.cpp -std=c++11 -o master`

run master: `./master 9022 B 12`
  
compile worker.cpp: `g++ worker.cpp rpc.cpp -std=c++11 -o worker`

run workerX: `./worker 127.0.0.1 9022 A`


  


