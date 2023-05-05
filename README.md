# Exploration and Analysis of Memory-aware Semi-Static Scheduling Algorithms

Our project aims to explore multi-core memory-aware scheduling algorithms that take into account both task computation time and memory accesses. While the computation time of tasks is the primary metric of schedulers, memory accesses can contribute significantly to the overall execution time in memory bound programs. We studied different memory-aware scheduling algorithms to take advantage of the memory access pattern of each tasks.  We constructed a framework to evaluate the effectiveness of different scheduling algorithms on parallel matrix multiplication. We first profiling each task using pintool to record the memory trace, before scheduling them onto cores and evaluating the program execution time. The two main scheduling algorithms discussed in this paper are the Deque Model Data Aware heuristic (DMDA) and Hierarchical Fair Packing (HFP) algorithm [M. Gonthier, L. Marchal, and S. Thibault], of which DMDA has shown to be more effective. We also wrote a multi-core cache coherence simulator for greater flexibility in accessing cache behavior.

Proposal [Here](https://github.com/yingyee0111/memory-aware-scheduling/blob/main/Proposal.pdf)

Milestone Report [Here](https://github.com/yingyee0111/memory-aware-scheduling/blob/main/Milestone%20Report.pdf)

## Getting Started
### `/schedulers`
- Implementation of scheduling algorithms can be found in the python files here. 
- The profiling and memory trace of our sample programs are also included: `pinatrace_mm.out` contains the trace for matrix multiplication and `pinatrace_conv.out` contains the trace for image convolution
- `schedule.py` is a sample script that reads task profiles and runs the schedulers.
- `parallelmatmul.c` and `convolution.c` are our multi-threaded programs. The helper function `assign_thread_to_core()` is used to schedule threads according to our algorithm's assignment. These files need to be compiled with the `-lpthread` flag as in 
    ```
    gcc -pthread -o parallelmatmul -O0 parallelmatmul.c -lpthread
    ```
