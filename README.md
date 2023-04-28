# COMP 730 Project
by Ernest Ermongkonchai, Amin Zamani, Alissa

Our Adapted Lock-Free Linked List is located at `xenium/xenium/xenium/lock_free_730.hpp`.

The gtests can be found at `xenium/xenium/test/lock_free_730_test.cpp`.

# How to build

Ensure that you can build C++ projects (eg. have gcc and cmake).

In terminal where `730Project` is:
```
cd xenium/xenium
mkdir build
cd build
cmake ..
make benchmark
make gtest
```
# How to run gtest
Ensure that you have build the project with the instruction above.

In terminal where `730Project` is:
```
cd xenium/xenium/build
./gtest --gtest_filter=*LockFree730*  
```

# How to run benchmark
Ensure that you have build the project with the instruction above.

In terminal where `730Project` is:

```
cd xenium/xenium/build
./benchmark <benchmark config file.json path> 
```

For example, if you want to run `xenium/xenium/benchmarks/examples/EBR.json`, inside the `xenium/xenium/build` folder, you can simply run `./benchmark ../benchmarks/examples/EBR.json`. 

The config files with the names `EBR.json`, `QSBR.json`, `dynamicHP.json`, and `staticHP.json` in the folder `730Project/xenium/xenium/benchmarks/examples/` are all the configs for benchmarking our adapted lock-free linked list with the memory reclamation schemes epoch-based, quiescent-based, dynamic hazard pointer, and static hazard pointer, respectively.
