/*BEGIN_LEGAL 
Intel Open Source License 

Copyright (c) 2002-2017 Intel Corporation. All rights reserved.
 
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.  Redistributions
in binary form must reproduce the above copyright notice, this list of
conditions and the following disclaimer in the documentation and/or
other materials provided with the distribution.  Neither the name of
the Intel Corporation nor the names of its contributors may be used to
endorse or promote products derived from this software without
specific prior written permission.
 
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE INTEL OR
ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
END_LEGAL */
#include <iostream>
#include <fstream>
#include <time.h>
#include <string>
#include <vector>
#include "pin.H"

ofstream OutFile;

// basic block count
static UINT32 bblcount = 0;

// The running count of instructions is kept here
// make it static to help the compiler optimize docount
static UINT32 inscount1 = 0;
static UINT32 inscount0 = 0;

// This function is called before every block
void docount1(UINT32 c) { inscount1 += c; }

void prog_trace(TRACE trace, VOID* vptr)
{
  for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl))
  {
    bblcount++;
    // Insert a call to docount before every bbl, passing the number of instructions
    BBL_InsertCall(bbl, IPOINT_BEFORE, (AFUNPTR)docount1, IARG_UINT32, BBL_NumIns(bbl), IARG_END);
  }
}

// This function is called for every instruction
void docount0() { inscount0++; }

static UINT32 memReadCount = 0;

void MemRead(VOID *ip, VOID *addr)
{
  ++memReadCount;
}


static UINT32 memWriteCount = 0;

void MemWrite(VOID *ip, VOID *addr)
{
  ++memWriteCount;
}

// Pin calls this function every time a new instruction is encountered
VOID Instruction(INS ins, VOID *v)
{
  UINT32 memOps = INS_MemoryOperandCount(ins);
  
  for (UINT32 memOp = 0; memOp < memOps; ++memOp)
  {
    if (INS_MemoryOperandIsRead(ins, memOp))
    {
      INS_InsertPredicatedCall(
        ins,
        IPOINT_BEFORE,
        (AFUNPTR)MemRead,
        IARG_INST_PTR,
        IARG_MEMORYOP_EA,
        memOp,
        IARG_END);
    }

    if (INS_MemoryOperandIsWritten(ins, memOp))
    {
      INS_InsertPredicatedCall(
        ins,
        IPOINT_BEFORE,
        (AFUNPTR)MemWrite,
        IARG_INST_PTR, 
        IARG_MEMORYOP_EA,
        memOp,
        IARG_END);
    }
  }

  INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)docount0, IARG_END);
}



KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", 
  "inscount.out", "specify output file name");

vector <string> inputs (10);

// This function is called when the application exits
VOID Fini(INT32 code, VOID *v)
{
  time_t rawtime;
  struct tm *timeinfo;
  char buffer [80];

  // get time now
  time(&rawtime);
  timeinfo = localtime (&rawtime);

  strftime (buffer, 80, "%a %b %e %T %Y", timeinfo);

  // Write to a file since cout and cerr maybe closed by the application
  OutFile.setf(ios::showbase);
  OutFile << "// Benchmark Characteristics" << endl;
  OutFile << "// Time: " << buffer << endl;
  OutFile << "// Benchmark: ";
  
  for (vector<string>::iterator it = inputs.begin(); it != inputs.end(); ++it)
  {
    OutFile << *it << " ";
  }
  OutFile << endl;

  OutFile << " " << endl;
  OutFile << "// Program Stats" << endl;
  OutFile << "Basic Blocks: " << bblcount << endl;
  OutFile << "Memory Reads: " << memReadCount << endl;
  OutFile << "Memory Writes: " << memWriteCount << endl;
  OutFile << "Total Instructions: " << inscount1 << endl;
  OutFile.close();
}

/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */

INT32 Usage()
{
    cerr << "Performs basic benchmarking for input programs" << endl;
    cerr << endl << KNOB_BASE::StringKnobSummary() << endl;
    return -1;
}

/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */
/*   argc, argv are the entire command line: pin -t <toolname> -- ...    */
/* ===================================================================== */

int main(int argc, char * argv[])
{
    // Initialize pin
    if (PIN_Init(argc, argv)) return Usage();
    
    int vidx = 0;
    
    
    for (int idx = 0; idx < argc; ++idx)
    {
      if (idx > 5)
      {
        inputs[vidx] = argv[idx];
        ++vidx;
      }
    }
    
    // generate output file
    OutFile.open(KnobOutputFile.Value().c_str());
    
    TRACE_AddInstrumentFunction(prog_trace, 0);
    
    // Register Instruction to be called to instrument instructions
    INS_AddInstrumentFunction(Instruction, 0);

    // Register Fini to be called when the application exits
    PIN_AddFiniFunction(Fini, 0);
    
    // Start the program, never returns
    PIN_StartProgram();
    
    return 0;
}
