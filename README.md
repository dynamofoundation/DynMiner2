# DynMiner2

## Building

* Windows: build using VS2019 project. Dependencies most easily resolved with VCPKG.
* Linux (e.g. Ubuntu): use CMake:

```
mkdir build
cd build
cmake ..
make -j4
```

## Deployment

* Each GPU or CPU used requires a `-miner` parameter.
* Type `dyn_miner2` with no parameters for usage.

Example:

```
./dyn_miner2 -mode stratum -server web.letshash.it -port 5966 -user dy1qyc3lkpe8ysns5z65u3t5j0remfpdxxxxxxxxxx -pass d=2 -miner GPU,16384,128,0,1
```

