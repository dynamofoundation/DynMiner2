# DynMiner2

Sample command line:

dynminer2 -mode stratum -server web.letshash.it -port 5966 -user dy1qyc3lkpe8ysns5z65u3t5j0remfpdxxxxxxxxxx -pass d=2 -miner GPU,16384,128,0,1

Each GPU or CPU used requires a "-miner" parameter

Type dynminer2 with no parameters for usage

Build for windows using VS2019 project.  Dependencies most easily resolved with VCPKG.

Build for Ubuntu with:

```
g++-11 -I. -std=gnu++11 *.cpp -lpthread -L/opt/cuda/lib64 -lOpenCL -lcurl -o dyn_miner2
```
