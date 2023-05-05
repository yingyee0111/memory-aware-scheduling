/*
 * Copyright (C) 2004-2021 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */

/*
 *  This file contains an ISA-portable PIN tool for tracing memory accesses.
 */

#include <stdio.h>
#include <vector>
#include <tuple>
#include "pin.H"
using namespace std;

FILE* trace;
PIN_LOCK globalLock;

// Force each thread's data to be in its own data cache line so that
// multiple threads do not contend for the same data cache line.
// This avoids the false sharing problem.
#define PADSIZE 40 // 64 byte line size: 64-8

typedef vector<unsigned long> memory_access_t;

// a running count of the instructions
class thread_data_t
{
  public:
    thread_data_t() : _count(0) {}
    UINT64 _count;
    memory_access_t read_mem_log;
    memory_access_t write_mem_log;
    UINT8 _pad[PADSIZE];
};

// key for accessing TLS storage in the threads. initialized once in main()
static TLS_KEY tls_key = INVALID_TLS_KEY;

// This function is called before every block
VOID PIN_FAST_ANALYSIS_CALL docount(UINT32 c, THREADID threadid)
{
    thread_data_t* tdata = static_cast< thread_data_t* >(PIN_GetThreadData(tls_key, threadid));
    tdata->_count += c;
}

VOID ThreadStart(THREADID threadid, CONTEXT* ctxt, INT32 flags, VOID* v)
{
    thread_data_t* tdata = new thread_data_t;
    if (PIN_SetThreadData(tls_key, tdata, threadid) == FALSE)
    {
        printf("PIN_SetThreadData failed\n");
        PIN_ExitProcess(1);
    }
}

// Pin calls this function every time a new basic block is encountered.
// It inserts a call to docount.
VOID Trace(TRACE trace, VOID* v)
{
    // Visit every basic block  in the trace
    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl))
    {
        // Insert a call to docount for every bbl, passing the number of instructions.
        BBL_InsertCall(bbl, IPOINT_ANYWHERE, (AFUNPTR)docount, IARG_FAST_ANALYSIS_CALL, IARG_UINT32, BBL_NumIns(bbl),
                       IARG_THREAD_ID, IARG_END);
    }
}

// This function is called when the thread exits
VOID ThreadFini(THREADID threadIndex, const CONTEXT* ctxt, INT32 code, VOID* v)
{
    thread_data_t* tdata = static_cast< thread_data_t* >(PIN_GetThreadData(tls_key, threadIndex));
    PIN_GetLock(&globalLock, threadIndex);

    fprintf(trace, "(%d, %lu, [", threadIndex, tdata->_count);

    // Print read list
    // returns length of vector as unsigned int
    unsigned int vecSize = tdata->read_mem_log.size();
    if (vecSize != 0) {
        fprintf(trace, "%lu", tdata->read_mem_log[0]);
    }
    // run for loop from 0 to vector size
    for(unsigned int i = 0; i < vecSize; i++) {
        fprintf(trace, ", %lu", tdata->read_mem_log[i]);
    }
    fprintf(trace, "], [");

    // Print write list
    // returns length of vector as unsigned int
    vecSize = tdata->write_mem_log.size();
    if (vecSize != 0) {
        fprintf(trace, "%lu", tdata->write_mem_log[0]);
    }
    // run for loop from 0 to vector size
    for(unsigned int i = 0; i < vecSize; i++) {
        fprintf(trace, ", %lu", tdata->write_mem_log[i]);
    }
    fprintf(trace, "])\n");

    PIN_ReleaseLock(&globalLock);

    delete tdata;
}
// Print a memory read record
VOID RecordMemRead(THREADID threadId, VOID* ip, VOID* addr) {
    thread_data_t* tdata = static_cast< thread_data_t* >(PIN_GetThreadData(tls_key, threadId));
    tdata->read_mem_log.push_back((unsigned long)addr);
}

// Print a memory write record
VOID RecordMemWrite(THREADID threadId, VOID* ip, VOID* addr) {
    thread_data_t* tdata = static_cast< thread_data_t* >(PIN_GetThreadData(tls_key, threadId));
    tdata->write_mem_log.push_back((unsigned long)addr);
}

// Is called for every instruction and instruments reads and writes
VOID Instruction(INS ins, VOID* v)
{
    // Instruments memory accesses using a predicated call, i.e.
    // the instrumentation is called iff the instruction will actually be executed.
    //
    // On the IA-32 and Intel(R) 64 architectures conditional moves and REP
    // prefixed instructions appear as predicated instructions in Pin.
    UINT32 memOperands = INS_MemoryOperandCount(ins);

    // Iterate over each memory operand of the instruction.
    for (UINT32 memOp = 0; memOp < memOperands; memOp++)
    {
        if (INS_MemoryOperandIsRead(ins, memOp))
        {
            INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)RecordMemRead, IARG_THREAD_ID, IARG_INST_PTR, IARG_MEMORYOP_EA, memOp,
                                     IARG_END);
        }
        // Note that in some architectures a single memory operand can be
        // both read and written (for instance incl (%eax) on IA-32)
        // In that case we instrument it once for read and once for write.
        if (INS_MemoryOperandIsWritten(ins, memOp))
        {
            INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)RecordMemWrite, IARG_THREAD_ID, IARG_INST_PTR, IARG_MEMORYOP_EA, memOp,
                                     IARG_END);
        }
    }
}

VOID Fini(INT32 code, VOID* v)
{
    fprintf(trace, "#eof\n");
    fclose(trace);
}

/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */

INT32 Usage()
{
    PIN_ERROR("This Pintool prints a trace of memory addresses\n" + KNOB_BASE::StringKnobSummary() + "\n");
    return -1;
}

/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */

int main(int argc, char* argv[])
{
    if (PIN_Init(argc, argv)) return Usage();

    // Obtain  a key for TLS storage.
    tls_key = PIN_CreateThreadDataKey(NULL);
    if (tls_key == INVALID_TLS_KEY)
    {
        printf("number of already allocated keys reached the MAX_CLIENT_TLS_KEYS limit\n");
        PIN_ExitProcess(1);
    }

    trace = fopen("pinatrace.out", "w");

    // Initiate Lock for File access
    PIN_InitLock(&globalLock);

    // Register Instruction to be called to instrument instructions.
    // This is for detecting memory accesses
    INS_AddInstrumentFunction(Instruction, 0);
    PIN_AddFiniFunction(Fini, 0);

    // Register ThreadStart to be called when a thread starts.
    PIN_AddThreadStartFunction(ThreadStart, NULL);

    // Register ThreadFini to be called when thread exits.
    PIN_AddThreadFiniFunction(ThreadFini, NULL);

    // Register Trace to be called to instrument blocks.
    // This is for counting Instructions
    TRACE_AddInstrumentFunction(Trace, NULL);

    // Never returns
    PIN_StartProgram();

    return 0;
}
