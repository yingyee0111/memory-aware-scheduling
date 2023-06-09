# Exploration and Analysis of Memory-aware Semi-Static Scheduling Algorithms

Our project aims to explore multi-core memory-aware scheduling algorithms that take into account both task computation time and memory accesses. While the computation time of tasks is the primary metric of schedulers, memory accesses can contribute significantly to the overall execution time in memory bound programs. We studied different memory-aware scheduling algorithms to take advantage of the memory access pattern of each tasks.  We constructed a framework to evaluate the effectiveness of different scheduling algorithms on parallel matrix multiplication. We first profiling each task using pintool to record the memory trace, before scheduling them onto cores and evaluating the program execution time. The two main scheduling algorithms discussed in this paper are the Deque Model Data Aware heuristic (DMDA) and Hierarchical Fair Packing (HFP) algorithm [M. Gonthier, L. Marchal, and S. Thibault], of which DMDA has shown to be more effective. We also wrote a multi-core cache coherence simulator for greater flexibility in accessing cache behavior.

Proposal [Here](https://github.com/yingyee0111/memory-aware-scheduling/blob/main/Proposal.pdf)

Milestone Report [Here](https://github.com/yingyee0111/memory-aware-scheduling/blob/main/Milestone%20Report.pdf)

Final Report [Here](https://github.com/yingyee0111/memory-aware-scheduling/blob/main/Final%20Report.pdf)

## Getting Started
### `/schedulers`
- Implementation of scheduling algorithms can be found in the python files here. 
- The profiling and memory trace of our sample programs are also included: `pinatrace_mm.out` contains the trace for matrix multiplication and `pinatrace_conv.out` contains the trace for image convolution
- `schedule.py` is a sample script that reads task profiles and runs the schedulers.
- `parallelmatmul.c` and `convolution.c` are our multi-threaded programs. The helper function `assign_thread_to_core()` is used to schedule threads according to our algorithm's assignment. These files need to be compiled with the `-lpthread` flag as in 
    ```
    gcc -pthread -o parallelmatmul -O0 parallelmatmul.c -lpthread
    ```

### `/CacheSimulator`
- Implementation of the multi-core cache simulator can be found here.
- Parameters such as L1 cache size and number of cores can be adjusted near the top of the file
- `fopen` under the `parseTrace` function should point to a memory trace generated by the Intel pintool and our custom pin script, such as the `.out` files under the `/schedulers` directory
- The results of the cache simulation will be directly printed to terminal, and can be redirected to a log file if necessary
- The cache simulator can be compiled using the following command
    ```
    gcc -o CacheSimulate -O0 cache.cpp -lstdc++
    ```

### `/PinTool`
- Contains custom script based on the Intel Pin tool which enabled us to generate instruction count and memory traces for each thread in a multithreaded program
- Based on example programs provided by the Intel Pin tool install
- Tested with Pin 3.27 on Linux
