#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <deque>
#include <cstring>
#include <sstream>
#include <tgmath.h> 
using namespace std;

// Struct containing info for each thread
typedef struct ThreadInfo{
  int thread_id;
  long instr_count;
  deque<long> read_list;
  deque<long> write_list;
} threadinfo_t;

// ENUM for Cache Coherence Bus Operations
typedef enum
{
    BusRd,
    BusRdX,
    Flush
} bus_op_t;

// Cache Parameters
#define L1_DCACHE_SIZE     32768
#define L1_DCACHE_ASSOC    8
#define L1_DCACHE_LINESIZE 64
#define L1_DCACHE_SETS     (L1_DCACHE_SIZE / L1_DCACHE_ASSOC) / L1_DCACHE_LINESIZE

#define L2_CACHE_SIZE     262144
#define L2_CACHE_ASSOC    4
#define L2_CACHE_LINESIZE 64
#define L2_CACHE_SETS     (L2_CACHE_SIZE / L2_CACHE_ASSOC) / L2_CACHE_LINESIZE

#define NUM_CORES         8

// Estimate
#define L1_MISS_PENALTY   10

// Cache Structures
typedef struct {
    long tag[L1_DCACHE_ASSOC];      /* tag for the line */
    long opCount[L1_DCACHE_ASSOC];  /* latest operation which used the line */
    int  state[L1_DCACHE_ASSOC];    /* Cache Coherence State*/
} L1_line_t;                        // 0 -> Idle
                                    // 1 -> Shared
                                    // 2 -> Exclusive
                                    // 3 -> Modified

typedef struct {
    L1_line_t L1_Cache[L1_DCACHE_SETS]; /* Array of lines matching L1 Cache*/
    long memory_reads;       // Memory Reads
    long memory_writes;      // Memory Writes
    long count;              // Cycle Count
    long evictions;          // Evictions
    long response_bus;       // Responses to Bus Transactions
} cache_t;

// Global vector containing pointers to all threads
vector<threadinfo_t *> thread_list;

// Global vector containing pointer to cache
cache_t Cache[NUM_CORES];

// Global variable for total threads
long total_threads = 0;

// Global variable tracking interconnect traffic
long interconnect_traffic = 0;

// Bus Transaction Prototype
int BusTransaction(int source, long addr, bus_op_t op);

// Function to parse Trace file
int parseTrace() {
    
    FILE *fptr;

    fptr = fopen("/home/joshua/15418/CacheSimulator/mm.out", "r");

    if (fptr == NULL) {
        printf("Error Opening Trace File!");
        return -1;
    }

    char *line = NULL;
    size_t len = 0;
    size_t read;

    int count = 0;

    while ((read = getline(&line, &len, fptr)) != -1) {
        
        // End of trace
        if (strcmp("#eof\n", line) == 0) break;

        // Struct containing info about trace
        threadinfo_t *this_thread_info = new threadinfo_t();

        // Get thread_id
        char* tok = strtok(line, "(,");
        this_thread_info->thread_id = atoi(tok);

        // Get Instruction Count
        tok = strtok(NULL, "(,");
        this_thread_info->instr_count = atoi(tok);

        // Extract Read / Write Mem Traces
        tok = strtok(NULL, "[");
        char *read_mem_log = strtok(NULL, "]");
        tok = strtok(NULL, "[");
        char *write_mem_log = strtok(NULL, "]");

        // Parse read_mem_log
        char* term = strtok(read_mem_log, ",");
        while(term) {
            char *ptr;
            this_thread_info->read_list.push_back(strtol(term, &ptr, 10));
            term = strtok(NULL, ",");
        }

        // Parse write_mem_log
        term = strtok(write_mem_log, ",");
        while(term) {
            char *ptr;
            this_thread_info->write_list.push_back(strtol(term, &ptr, 10));
            term = strtok(NULL, ",");
        }

        // Store thread info
        thread_list.push_back(this_thread_info);
        count++;
    }

    printf("Total Threads: %d\n", count);
    total_threads = count;
    return 0;
}

// Simulate BusRd Behavior on specific Cache
int BusRd_Cache(int dest, long addr) {

    // Calculate Bits
    int L1_block_bits = log2(L1_DCACHE_LINESIZE);
    int L1_idx_bits = log2(L1_DCACHE_SETS);
    int L1_tag_bits = 64 - L1_idx_bits - L1_block_bits;

    // Compute terms
    long set = (long)(((unsigned long)(addr << L1_tag_bits)) >> (L1_tag_bits + L1_block_bits));
    long tag = (long)((unsigned long)addr >> L1_idx_bits + L1_block_bits);

    long count = Cache[dest].count;

    int i;
    int found_matching = 0;

    // Search for matching address in cache (either in exclusive, shared,
    // modified)
    for (i = 0; i < L1_DCACHE_ASSOC; i++) {
        if (Cache[dest].L1_Cache[set].tag[i] == tag &&
            (Cache[dest].L1_Cache[set].state[i] == 1 ||
             Cache[dest].L1_Cache[set].state[i] == 2 ||
             Cache[dest].L1_Cache[set].state[i] == 3)) {
            
            // Mark found_matching
            found_matching = 1;
            
            if (Cache[dest].L1_Cache[set].state[i] == 1) {
                // Do nothing if state is shared
                break;
            }

            // Increase cache response (not for shared state)
            Cache[dest].response_bus += 1;

            if (Cache[dest].L1_Cache[set].state[i] == 2) {
                // Transition from exclusive to shared
                Cache[dest].L1_Cache[set].state[i] = 1;
                break;
            }

            if (Cache[dest].L1_Cache[set].state[i] == 3) {
                // Transition from modified to shared
                Cache[dest].L1_Cache[set].state[i] = 1;

                //Flush
                bus_op_t op = Flush;
                BusTransaction(dest, addr, op);

                break;
            }

            break;
        }
    }

    // Increment internal count because cache operation was done
    Cache[dest].count = count + found_matching;

    // Return if shared entires were found
    return found_matching;
}

// Simulate BusRd Behavior on specific Cache
int BusRdx_Cache(int dest, long addr) {

    // Calculate Bits
    int L1_block_bits = log2(L1_DCACHE_LINESIZE);
    int L1_idx_bits = log2(L1_DCACHE_SETS);
    int L1_tag_bits = 64 - L1_idx_bits - L1_block_bits;

    // Compute terms
    long set = (long)(((unsigned long)(addr << L1_tag_bits)) >> (L1_tag_bits + L1_block_bits));
    long tag = (long)((unsigned long)addr >> L1_idx_bits + L1_block_bits);

    long count = Cache[dest].count;

    int i;
    int found_matching = 0;

    // Search for matching address in cache (either in exclusive, shared,
    // modified)
    for (i = 0; i < L1_DCACHE_ASSOC; i++) {
        if (Cache[dest].L1_Cache[set].tag[i] == tag &&
            (Cache[dest].L1_Cache[set].state[i] == 1 ||
             Cache[dest].L1_Cache[set].state[i] == 2 ||
             Cache[dest].L1_Cache[set].state[i] == 3)) {
            
            // Mark found_matching
            found_matching = 1;

            // Increase cache response
            Cache[dest].response_bus += 1;

            // Increase cache evictions
            Cache[dest].evictions += 1;
            
            if (Cache[dest].L1_Cache[set].state[i] == 1) {
                // Transition from exclusive to invalid
                Cache[dest].L1_Cache[set].state[i] = 0;
                break;
            }

            if (Cache[dest].L1_Cache[set].state[i] == 2) {
                // Transition from exclusive to invalid
                Cache[dest].L1_Cache[set].state[i] = 0;
                break;
            }

            if (Cache[dest].L1_Cache[set].state[i] == 3) {
                // Transition from modified to invalid
                Cache[dest].L1_Cache[set].state[i] = 0;

                //Flush
                bus_op_t op = Flush;
                BusTransaction(dest, addr, op);
                break;
            }

            break;
        }
    }

    // Increment internal count because cache operation was done
    Cache[dest].count = count + found_matching;

    // Return if shared entires were found
    return found_matching;
}

// Simulate Interconnect Behavior
int BusTransaction(int source, long addr, bus_op_t op) {
    
    int i;

    int return_value = 0;
    
    // Simulate effects of bus transaction on cache
    for (i = 0; i < NUM_CORES; i++) {

        if (i == source) {
            // We ignore effects of bus transaction on own cache
            continue;
        }

        if (op == BusRd) {
            // Carry our BusRd operation on cache
            if (BusRd_Cache(i, addr)) {
                return_value = 1;
            }
        }
        else if (op == BusRdX) {
            // Carry our BusRdx operation on cache
            if (BusRdx_Cache(i, addr)) {
                return_value = 1;
            }
        }
    }

    // Increment Interconnect Traffic
    // NOTE: Flush does nothing but increment this counter
    interconnect_traffic += 1;

    // Return return value (whether there is shared state)
    // So that we know if we're exclusive or shared
    return return_value;
}

// Process Cache Read
void processCacheRead(int core, long addr) {

    // Calculate Bits
    int L1_block_bits = log2(L1_DCACHE_LINESIZE);
    int L1_idx_bits = log2(L1_DCACHE_SETS);
    int L1_tag_bits = 64 - L1_idx_bits - L1_block_bits;

    // Compute terms
    long set = (long)(((unsigned long)(addr << L1_tag_bits)) >> (L1_tag_bits + L1_block_bits));
    long tag = (long)((unsigned long)addr >> L1_idx_bits + L1_block_bits);

    long count = Cache[core].count;

    // Update Memory Reads
    Cache[core].memory_reads += 1;

    int found_free = 0;
    int found_match = 0;
    int i;

    // Search for matching cache (either in exclusive, shared, modified)
    for (i = 0; i < L1_DCACHE_ASSOC; i++) {
        if (Cache[core].L1_Cache[set].tag[i] == tag &&
            (Cache[core].L1_Cache[set].state[i] == 1 ||
             Cache[core].L1_Cache[set].state[i] == 2 ||
             Cache[core].L1_Cache[set].state[i] == 3)) {          
            Cache[core].L1_Cache[set].opCount[i] = count;
            found_match = 1; 

            Cache[core].count = count + 1;

            break;
        }
    }

    // If not, search for empty way
    if (found_match == 0) {
        // Search for free cache
        for (i = 0; i < L1_DCACHE_ASSOC; i++) {
            if (Cache[core].L1_Cache[set].state[i] == 0) {
                Cache[core].L1_Cache[set].tag[i] = tag;
                Cache[core].L1_Cache[set].opCount[i] = count;
                
                // Update State for Read Transaction
                bus_op_t op = BusRd;
                if (BusTransaction(core, addr, op)) {
                    // There is shared state
                    Cache[core].L1_Cache[set].state[i] = 1;
                }
                else {
                    Cache[core].L1_Cache[set].state[i] = 2;
                }

                Cache[core].count = count + L1_MISS_PENALTY;

                found_free = 1;

                break;
            }
        }
    }

    // Else, find one to evict
    if (found_match == 0 && found_free == 0) {

        // Search for oldest way (LRU)
        int oldest_way = 0;
        long oldest_count = count;
        for (i = 0; i < L1_DCACHE_ASSOC; i++) {
            if (Cache[core].L1_Cache[set].opCount[i] < (long)oldest_count) {
                oldest_way = i;
                oldest_count = Cache[core].L1_Cache[set].opCount[i];
            }
        }

        Cache[core].L1_Cache[set].tag[oldest_way] = tag;
        Cache[core].L1_Cache[set].opCount[oldest_way] = count;
        
        // Update State for Read Transaction
        bus_op_t op = BusRd;
        if (BusTransaction(core, addr, op)) {
            // There is shared state
            Cache[core].L1_Cache[set].state[i] = 1;
        }
        else {
            Cache[core].L1_Cache[set].state[i] = 2;
        }

        // Increase cache evictions
        Cache[core].evictions += 1;

        // Increase cache execution time
        Cache[core].count = count + L1_MISS_PENALTY;

    }

    return;
}

// Process Cache Write
void processCacheWrite(int core, long addr) {

    // Calculate Bits
    int L1_block_bits = log2(L1_DCACHE_LINESIZE);
    int L1_idx_bits = log2(L1_DCACHE_SETS);
    int L1_tag_bits = 64 - L1_idx_bits - L1_block_bits;

    // Compute terms
    long set = (long)(((unsigned long)(addr << L1_tag_bits)) >> (L1_tag_bits + L1_block_bits));
    long tag = (long)((unsigned long)addr >> L1_idx_bits + L1_block_bits);

    long count = Cache[core].count;

    // Update Memory Writes
    Cache[core].memory_writes += 1;

    int found_free = 0;
    int found_match = 0;
    int i;

    // Search for matching cache (either in exclusive, shared, modified)
    for (i = 0; i < L1_DCACHE_ASSOC; i++) {
        if (Cache[core].L1_Cache[set].tag[i] == tag &&
            (Cache[core].L1_Cache[set].state[i] == 1 ||
             Cache[core].L1_Cache[set].state[i] == 2 ||
             Cache[core].L1_Cache[set].state[i] == 3)) {
                  
            Cache[core].L1_Cache[set].opCount[i] = count;
            found_match = 1; 
            Cache[core].count = count + 1;

            // If exclusive, move to modified state
            if (Cache[core].L1_Cache[set].state[i] == 2) {
                Cache[core].L1_Cache[set].state[i] = 3;
            }

            // If shared, move to modified state and issue bus transaction
            if (Cache[core].L1_Cache[set].state[i] == 1) {
                Cache[core].L1_Cache[set].state[i] = 3;
                bus_op_t op = BusRdX;
                BusTransaction(core, addr, op);
            }

            break;
        }
    }

    // If not, search for empty way
    if (found_match == 0) {
        // Search for free cache
        for (i = 0; i < L1_DCACHE_ASSOC; i++) {
            if (Cache[core].L1_Cache[set].state[i] == 0) {
                
                // Update tag and op
                Cache[core].L1_Cache[set].tag[i] = tag;
                Cache[core].L1_Cache[set].opCount[i] = count;

                // Issue BusRdX
                bus_op_t op = BusRdX;
                BusTransaction(core, addr, op);
                
                // Update State to Modified
                Cache[core].L1_Cache[set].state[i] = 3;

                // Update Stats
                Cache[core].count = count + L1_MISS_PENALTY;
                found_free = 1;

                break;
            }
        }
    }

    // Else, find one to evict
    if (found_match == 0 && found_free == 0) {

        // Search for oldest way (LRU)
        int oldest_way = 0;
        long oldest_count = count;
        for (i = 0; i < L1_DCACHE_ASSOC; i++) {
            if (Cache[core].L1_Cache[set].opCount[i] < (long)oldest_count) {
                oldest_way = i;
                oldest_count = Cache[core].L1_Cache[set].opCount[i];
            }
        }

        // Update tag and op
        Cache[core].L1_Cache[set].tag[oldest_way] = tag;
        Cache[core].L1_Cache[set].opCount[oldest_way] = count;
        
        // Issue BusRdX
        bus_op_t op = BusRdX;
        BusTransaction(core, addr, op);
                
        // Update State to Modified
        Cache[core].L1_Cache[set].state[i] = 3;

        // Increase cache evictions
        Cache[core].evictions += 1;

        // Increase cache execution time
        Cache[core].count = count + L1_MISS_PENALTY;

    }

    return;
}

// Run part of trace for single task on core
int runTaskTrace(int core, int thread_id) {
    
    threadinfo_t *thread_info = NULL;
    
    // Find thread info
    for (threadinfo_t *i : thread_list) {
        if (i->thread_id == thread_id) thread_info = i;
    }

    // Return error if we cannot find thread_id
    if (thread_info == NULL) {
        printf("Thread Info not Found! Is thread_id correct?\n");
        return -1;
    }

    if (thread_info->read_list.size() != 0) {
        long mem_read_addr = thread_info->read_list.front();
        thread_info->read_list.pop_front();
        processCacheRead(core, mem_read_addr);
    }

    if (thread_info->write_list.size() != 0) {
        long mem_write_addr = thread_info->write_list.front();
        thread_info->write_list.pop_front();
        processCacheWrite(core, mem_write_addr);
    }

    return 0;
}

// Print Stats
void printStats() {
    printf("Interconnect Traffic: %ld\n\n", interconnect_traffic);

    int i;

    for (i = 0; i < NUM_CORES; i++) {
        printf("**** CORE %d ****\n", i);
        printf("Memory Reads: %ld\n", Cache[i].memory_reads);
        printf("Memory Writes: %ld\n", Cache[i].memory_writes);
        printf("Cycle Count: %ld\n", Cache[i].count);
        printf("Evictions: %ld\n", Cache[i].evictions);
        printf("Bus Responses: %ld\n\n", Cache[i].response_bus);
    }
}

int main() {

    // Parse memory trace
    if (parseTrace() == -1) {
        return -1;
    }

    // Task scheduling (back to front)
    vector<int> CPU0{2, 6};
    vector<int> CPU1{1, 19, 13, 7};
    vector<int> CPU2{4, 43, 38, 33, 28, 23, 17, 11};
    vector<int> CPU3{3, 44, 39, 34, 29, 24, 18, 12, 5};
    vector<int> CPU4{56, 53, 50, 47, 42, 37, 32, 27, 22, 16, 10};
    vector<int> CPU5{55, 52, 49, 46, 41, 36, 31, 26, 21, 15, 9};
    vector<int> CPU6{54, 51, 48, 45, 40, 35, 30, 25, 20, 14, 8};

    while (1) {
        if (CPU0.size() == 0 && CPU1.size() == 0 && CPU2.size() == 0 &&
            CPU3.size() == 0 && CPU4.size() == 0 && CPU5.size() == 0 &&
            CPU6.size() == 0) break;
        
        threadinfo_t *thread_info = NULL;

        if (!CPU0.empty()) {
            int thread_id = CPU0.back();

            // Run Trace
            runTaskTrace(0, thread_id);

            // Find thread info
            for (threadinfo_t *i : thread_list) {
                if (i->thread_id == thread_id) thread_info = i;
            }

            if (thread_info->read_list.size() == 0 &&
                thread_info->write_list.size() == 0) {
                CPU0.pop_back();
            }
        }

        if (!CPU1.empty()) {
            int thread_id = CPU1.back();

            // Run Trace
            runTaskTrace(1, thread_id);

            // Find thread info
            for (threadinfo_t *i : thread_list) {
                if (i->thread_id == thread_id) thread_info = i;
            }

            if (thread_info->read_list.size() == 0 &&
                thread_info->write_list.size() == 0) {
                CPU1.pop_back();
            }
        }

        if (!CPU2.empty()) {
            int thread_id = CPU2.back();

            // Run Trace
            runTaskTrace(2, thread_id);

            // Find thread info
            for (threadinfo_t *i : thread_list) {
                if (i->thread_id == thread_id) thread_info = i;
            }

            if (thread_info->read_list.size() == 0 &&
                thread_info->write_list.size() == 0) {
                CPU2.pop_back();
            }
        }

        if (!CPU3.empty()) {
            int thread_id = CPU3.back();

            // Run Trace
            runTaskTrace(3, thread_id);

            // Find thread info
            for (threadinfo_t *i : thread_list) {
                if (i->thread_id == thread_id) thread_info = i;
            }

            if (thread_info->read_list.size() == 0 &&
                thread_info->write_list.size() == 0) {
                CPU3.pop_back();
            }
        }

        if (!CPU4.empty()) {
            int thread_id = CPU4.back();

            // Run Trace
            runTaskTrace(4, thread_id);

            // Find thread info
            for (threadinfo_t *i : thread_list) {
                if (i->thread_id == thread_id) thread_info = i;
            }

            if (thread_info->read_list.size() == 0 &&
                thread_info->write_list.size() == 0) {
                CPU4.pop_back();
            }
        }

        if (!CPU5.empty()) {
            int thread_id = CPU5.back();

            // Run Trace
            runTaskTrace(5, thread_id);

            // Find thread info
            for (threadinfo_t *i : thread_list) {
                if (i->thread_id == thread_id) thread_info = i;
            }

            if (thread_info->read_list.size() == 0 &&
                thread_info->write_list.size() == 0) {
                CPU5.pop_back();
            }
        }

        if (!CPU6.empty()) {
            int thread_id = CPU6.back();

            // Run Trace
            runTaskTrace(6, thread_id);

            // Find thread info
            for (threadinfo_t *i : thread_list) {
                if (i->thread_id == thread_id) thread_info = i;
            }

            if (thread_info->read_list.size() == 0 &&
                thread_info->write_list.size() == 0) {
                CPU6.pop_back();
            }
        }

    }

    printStats();

    return 0;
}