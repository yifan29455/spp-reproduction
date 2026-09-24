/*
 *    Copyright 2023 The ChampSim Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*! @file
 *  This is an example of the PIN tool that demonstrates some basic PIN APIs
 *  and could serve as the starting point for developing your first PIN tool
 */

#include <atomic>
#include <algorithm>
#include <limits>
#include <iomanip>
#include <map>
#include <fstream>
#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <cstdio>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <utility>

#include "../../inc/trace_instruction.h"
#include "pin.H"

using trace_instr_format_t = input_instr_v2;

/* ================================================================== */
// Global variables
/* ================================================================== */

static std::atomic<UINT64> skippedInstrCount(0);
static std::atomic<UINT64> tracedInstructions(0);
static std::atomic<UINT64> tracedCumulativeInstructions(0);
static std::atomic<bool> tracingActive(false);
static std::atomic<bool> captureThisInstruction(false);
static std::atomic<bool> traceLimitReached(false);
static std::atomic<bool> foundStartSymbol(false);
static std::atomic<bool> foundStopSymbol(false);
static std::atomic<bool> mainImageLoaded(false);
static std::atomic<UINT64> register_values_out_of_range(0);
static std::atomic<UINT64> source_register_set_full(0);
static std::atomic<UINT64> destination_register_set_full(0);
static std::atomic<UINT64> source_memory_set_full(0);
static std::atomic<UINT64> destination_memory_set_full(0);
static std::atomic<UINT64> zero_memory_addresses(0);
static ADDRINT destination_overflow_ip[16] = {};
static UINT32 destination_overflow_reg[16] = {};
static std::map<ADDRINT, UINT64> destination_overflow_by_ip;
using pending_open_t = std::pair<std::string, ADDRINT>;
static std::map<THREADID, pending_open_t> pending_open_by_tid;
using file_version_t = std::array<unsigned long long, 5>;
struct file_audit_t {
  file_version_t version{};
  bool stable = false;
  unsigned int access_bits = 0;
};
static std::map<std::string, file_audit_t> opened_read_files;

std::ofstream outfile;

trace_instr_format_t curr_instr;

/* ===================================================================== */
// Command line switches
/* ===================================================================== */
KNOB<std::string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "champsim.trace", "specify file name for Champsim tracer output");

KNOB<UINT64> KnobSkipInstructions(KNOB_MODE_WRITEONCE, "pintool", "s", "0", "How many instructions to skip before tracing begins");

KNOB<UINT64> KnobTraceInstructions(KNOB_MODE_WRITEONCE, "pintool", "t", "1000000", "How many instructions to trace");

KNOB<std::string> KnobStartSymbol(KNOB_MODE_WRITEONCE, "pintool", "start_symbol", "",
                                  "Symbol name to start tracing (e.g., 'main' or '_Z4testv'). If specified `-s` argument is ignored.");

KNOB<std::string> KnobStopSymbol(KNOB_MODE_WRITEONCE, "pintool", "stop_symbol", "", "Symbol name to stop tracing (optional)");

/* ===================================================================== */
// Utilities
/* ===================================================================== */

/*!
 *  Print out help message.
 */
INT32 Usage()
{
  std::cerr << "This tool creates a register and memory access trace" << std::endl
            << "Specify the output trace file with -o" << std::endl
            << "Specify the number of instructions to skip before tracing with -s" << std::endl
            << "Specify the number of instructions to trace with -t" << std::endl
            << std::endl;

  std::cerr << KNOB_BASE::StringKnobSummary() << std::endl;

  return -1;
}

/* ===================================================================== */
// Analysis routines
/* ===================================================================== */

void ResetCurrentInstruction(VOID* ip)
{
  captureThisInstruction = false;
  if (!KnobStartSymbol.Value().empty()) {
    if (tracingActive) {
      if (tracedCumulativeInstructions >= KnobTraceInstructions.Value()) {
        traceLimitReached = true;
        tracingActive = false;
        std::cout << "Reached instruction limit: " << KnobTraceInstructions.Value() << ". Stopping trace." << std::endl;
        PIN_ExitApplication(0);
      }
      ++tracedInstructions;
      ++tracedCumulativeInstructions;
      captureThisInstruction = true;
    } else {
      ++skippedInstrCount;
    }
  } else if (skippedInstrCount < KnobSkipInstructions.Value()) {
    ++skippedInstrCount;
  } else {
    if (tracedCumulativeInstructions >= KnobTraceInstructions.Value()) {
      traceLimitReached = true;
      tracingActive = false;
      std::cout << "Reached instruction limit: " << KnobTraceInstructions.Value() << ". Stopping trace." << std::endl;
      PIN_ExitApplication(0);
    }
    tracingActive = true;
    ++tracedInstructions;
    ++tracedCumulativeInstructions;
    captureThisInstruction = true;
  }
  curr_instr = {};
  curr_instr.ip = (unsigned long long int)ip;
}

BOOL ShouldWrite()
{
  return captureThisInstruction.load();
}

void WriteCurrentInstruction()
{
  typename decltype(outfile)::char_type buf[sizeof(trace_instr_format_t)];
  std::memcpy(buf, &curr_instr, sizeof(trace_instr_format_t));
  outfile.write(buf, sizeof(trace_instr_format_t));
  if (!outfile) {
    std::cerr << "TRACE_ERROR write failed" << std::endl;
    PIN_ExitProcess(2);
  }
}

void BranchOrNot(UINT32 taken)
{
  if (!captureThisInstruction)
    return;
  curr_instr.is_branch = 1;
  curr_instr.branch_taken = taken;
}

void WriteRegisterToSet(unsigned char* begin, unsigned char* end, UINT32 reg, std::atomic<UINT64>* full_counter)
{
  if (!captureThisInstruction)
    return;
  if (reg == 0)
    return; // Zero is the trace format's empty-register sentinel.
  if (reg > std::numeric_limits<unsigned char>::max()) {
    ++register_values_out_of_range;
    return;
  }

  auto set_end = std::find(begin, end, 0);
  auto found = std::find(begin, set_end, static_cast<unsigned char>(reg));
  if (found != set_end)
    return;
  if (set_end == end) {
    const auto index = full_counter->fetch_add(1);
    if (full_counter == &destination_register_set_full) {
      ++destination_overflow_by_ip[static_cast<ADDRINT>(curr_instr.ip)];
    }
    if (full_counter == &destination_register_set_full && index < 16) {
      destination_overflow_ip[index] = static_cast<ADDRINT>(curr_instr.ip);
      destination_overflow_reg[index] = reg;
    }
    return;
  }
  *set_end = static_cast<unsigned char>(reg);
}

void WriteMemoryToSet(unsigned long long* begin, unsigned long long* end, ADDRINT address, std::atomic<UINT64>* full_counter)
{
  if (!captureThisInstruction)
    return;
  if (address == 0) {
    ++zero_memory_addresses; // Zero is the trace format's empty-address sentinel.
    return;
  }

  auto set_end = std::find(begin, end, 0ULL);
  auto found = std::find(begin, set_end, static_cast<unsigned long long>(address));
  if (found != set_end)
    return;
  if (set_end == end) {
    ++(*full_counter);
    return;
  }
  *set_end = static_cast<unsigned long long>(address);
}

VOID StartTracing(VOID* ip)
{
  if (!tracingActive) {
    tracingActive = true;
    tracedInstructions = 0;
    std::cout << "[PIN] Started tracing at IP: 0x" << std::hex << ip << std::dec << std::endl;
  }
}

VOID StopTracing(VOID* ip)
{
  if (tracingActive) {
    tracingActive = false;
    std::cout << "[PIN] Stopped tracing at IP: 0x" << std::hex << ip << std::dec << " after " << tracedInstructions << " instructions" << std::endl;
  }
}

// Instrument image loads to find symbols
VOID ImageLoad(IMG img, VOID* v)
{
  std::cerr << "TRACE_IMAGE path=" << std::quoted(std::string{IMG_Name(img)})
            << " low=0x" << std::hex << IMG_LowAddress(img)
            << " high=0x" << IMG_HighAddress(img) << std::dec << std::endl;

  // Look for start symbol
  if (!KnobStartSymbol.Value().empty() && !foundStartSymbol.load()) {
    RTN rtn = RTN_FindByName(img, KnobStartSymbol.Value().c_str());

    if (RTN_Valid(rtn)) {
      foundStartSymbol.store(true);
      RTN_Open(rtn);
      // Insert call at function entry
      RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)StartTracing, IARG_INST_PTR, IARG_END);
      RTN_Close(rtn);

      std::cout << "Found start symbol '" << KnobStartSymbol.Value() << "' in image " << IMG_Name(img) << std::endl;
    }
  }

  // Look for stop symbol
  if (!KnobStopSymbol.Value().empty() && !foundStopSymbol.load()) {
    RTN rtn = RTN_FindByName(img, KnobStopSymbol.Value().c_str());

    if (RTN_Valid(rtn)) {
      foundStopSymbol.store(true);
      RTN_Open(rtn);
      // Insert call at function entry (or use IPOINT_AFTER for function exit)
      RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)StopTracing, IARG_INST_PTR, IARG_END);
      RTN_Close(rtn);

      std::cout << "Found stop symbol '" << KnobStopSymbol.Value() << "' in image " << IMG_Name(img) << std::endl;
    }
  }

  if (IMG_IsMainExecutable(img)) {
    bool exit = false;
    if (!KnobStartSymbol.Value().empty() && !foundStartSymbol.load()) {
      std::cerr << "[PIN] ERROR: Trace start symbol '" << KnobStartSymbol.Value() << "' not found!" << std::endl;
      exit = true;
    }
    if (!KnobStopSymbol.Value().empty() && !foundStopSymbol.load()) {
      std::cerr << "[PIN] ERROR: Trace stop symbol '" << KnobStopSymbol.Value() << "' not found!" << std::endl;
      exit = true;
    }
    if (exit) {
      PIN_ExitApplication(1);
    }
  }
}

/* ===================================================================== */
// Instrumentation callbacks
/* ===================================================================== */

// Is called for every instruction and instruments reads and writes
VOID Instruction(INS ins, VOID* v)
{
  // begin each instruction with this function
  INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)ResetCurrentInstruction, IARG_INST_PTR, IARG_END);

  // instrument branch instructions
  if (INS_IsBranch(ins))
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)BranchOrNot, IARG_BRANCH_TAKEN, IARG_END);

  // instrument register reads
  UINT32 readRegCount = INS_MaxNumRRegs(ins);
  for (UINT32 i = 0; i < readRegCount; i++) {
    UINT32 regNum = INS_RegR(ins, i);
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)WriteRegisterToSet, IARG_PTR, curr_instr.source_registers, IARG_PTR,
                   curr_instr.source_registers + NUM_INSTR_SOURCES, IARG_UINT32, regNum, IARG_PTR, &source_register_set_full, IARG_END);
  }

  // instrument register writes
  UINT32 writeRegCount = INS_MaxNumWRegs(ins);
  for (UINT32 i = 0; i < writeRegCount; i++) {
    UINT32 regNum = INS_RegW(ins, i);
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)WriteRegisterToSet, IARG_PTR, curr_instr.destination_registers, IARG_PTR,
                   curr_instr.destination_registers + NUM_INSTR_DESTINATIONS_V2, IARG_UINT32, regNum, IARG_PTR, &destination_register_set_full, IARG_END);
  }

  // instrument memory reads and writes
  UINT32 memOperands = INS_MemoryOperandCount(ins);

  // Iterate over each memory operand of the instruction.
  for (UINT32 memOp = 0; memOp < memOperands; memOp++) {
    if (INS_MemoryOperandIsRead(ins, memOp))
      INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)WriteMemoryToSet, IARG_PTR, curr_instr.source_memory, IARG_PTR,
                     curr_instr.source_memory + NUM_INSTR_SOURCES, IARG_MEMORYOP_EA, memOp, IARG_PTR, &source_memory_set_full, IARG_END);
    if (INS_MemoryOperandIsWritten(ins, memOp))
      INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)WriteMemoryToSet, IARG_PTR, curr_instr.destination_memory, IARG_PTR,
                     curr_instr.destination_memory + NUM_INSTR_DESTINATIONS, IARG_MEMORYOP_EA, memOp, IARG_PTR, &destination_memory_set_full, IARG_END);
  }

  // finalize each instruction with this function
  INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)ShouldWrite, IARG_END);
  INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)WriteCurrentInstruction, IARG_END);
}

/*!
 * Print out analysis results.
 * This function is called when the application exits.
 * @param[in]   code            exit code of the application
 * @param[in]   v               value specified by the tool in the
 *                              PIN_AddFiniFunction function call
 */
VOID SyscallEntry(THREADID tid, CONTEXT* ctx, SYSCALL_STANDARD std, VOID*)
{
  const ADDRINT number = PIN_GetSyscallNumber(ctx, std);
  if (number != SYS_open && number != SYS_openat) {
    pending_open_by_tid.erase(tid);
    return;
  }

  const bool is_open = number == SYS_open;
  const ADDRINT path_address = PIN_GetSyscallArgument(ctx, std, is_open ? 0 : 1);
  const ADDRINT flags = PIN_GetSyscallArgument(ctx, std, is_open ? 1 : 2);
  char path[4096] = {};
  const std::size_t copied = PIN_SafeCopy(path, reinterpret_cast<const VOID*>(path_address), sizeof(path) - 1);
  if (copied == 0) {
    pending_open_by_tid.erase(tid);
    return;
  }
  path[copied] = '\0';
  pending_open_by_tid[tid] = {std::string(path), flags};
}

VOID SyscallExit(THREADID tid, CONTEXT* ctx, SYSCALL_STANDARD std, VOID*)
{
  const auto pending = pending_open_by_tid.find(tid);
  if (pending == pending_open_by_tid.end())
    return;

  const long fd = static_cast<long>(PIN_GetSyscallReturn(ctx, std));
  const ADDRINT flags = pending->second.second;
  const ADDRINT access_mode = flags & O_ACCMODE;
  if (fd >= 0 && access_mode != O_WRONLY) {
    char proc_path[64] = {};
    std::snprintf(proc_path, sizeof(proc_path), "/proc/self/fd/%ld", fd);
    char resolved[4096] = {};
    const ssize_t length = ::readlink(proc_path, resolved, sizeof(resolved) - 1);
    std::string path_for_identity;
    if (length >= 0) {
      resolved[length] = '\0';
      path_for_identity = resolved;
    } else {
      path_for_identity = pending->second.first;
    }

    struct stat metadata {};
    file_version_t version{};
    const bool stat_ok = ::fstat(static_cast<int>(fd), &metadata) == 0;
    if (stat_ok) {
      version = {
          static_cast<unsigned long long>(metadata.st_dev),
          static_cast<unsigned long long>(metadata.st_ino),
          static_cast<unsigned long long>(metadata.st_size),
          static_cast<unsigned long long>(metadata.st_mtim.tv_sec * 1000000000LL + metadata.st_mtim.tv_nsec),
          static_cast<unsigned long long>(metadata.st_ctim.tv_sec * 1000000000LL + metadata.st_ctim.tv_nsec)};
    }
    unsigned int access_bits = access_mode == O_RDONLY ? 1U : 2U;
    if (flags & O_CREAT) access_bits |= 4U;
    if (flags & O_TRUNC) access_bits |= 8U;
    if (flags & O_APPEND) access_bits |= 16U;
    auto insertion = opened_read_files.emplace(path_for_identity, file_audit_t{version, stat_ok, access_bits});
    if (!insertion.second) {
      auto& previous = insertion.first->second;
      if (!stat_ok || previous.version != version)
        previous.stable = false;
      previous.access_bits |= access_bits;
    }
  }
  pending_open_by_tid.erase(pending);
}
VOID Fini(INT32 code, VOID* v) {
  outfile.close();
  if (outfile.fail()) { std::cerr << "TRACE_ERROR close failed" << std::endl; PIN_ExitProcess(2); }
  const UINT64 written = std::min(tracedCumulativeInstructions.load(), KnobTraceInstructions.Value());
  std::cerr << "TRACE_SUMMARY skipped=" << skippedInstrCount.load() << " records=" << written
            << " record_bytes=" << sizeof(trace_instr_format_t) << " trace_format=2 header_bytes=" << TRACE_V2_HEADER.size()
            << " register_values_out_of_range=" << register_values_out_of_range.load()
            << " source_register_set_full=" << source_register_set_full.load()
            << " destination_register_set_full=" << destination_register_set_full.load()
            << " source_memory_set_full=" << source_memory_set_full.load()
            << " destination_memory_set_full=" << destination_memory_set_full.load()
            << " zero_memory_addresses=" << zero_memory_addresses.load()
            << " termination=" << (traceLimitReached.load() ? "limit" : "natural")
            << " app_exit=" << code << std::endl;
  const auto samples = std::min<UINT64>(destination_register_set_full.load(), 16);
  for (UINT64 i = 0; i < samples; ++i)
    std::cerr << "TRACE_REGISTER_OVERFLOW_SAMPLE kind=destination ip=0x" << std::hex << destination_overflow_ip[i] << std::dec
              << " reg=" << destination_overflow_reg[i] << std::endl;
  for (const auto& entry : destination_overflow_by_ip)
    std::cerr << "TRACE_REGISTER_OVERFLOW_PC ip=0x" << std::hex << entry.first << std::dec << " count=" << entry.second << std::endl;
  for (const auto& entry : opened_read_files) {
    const auto& audit = entry.second;
    const auto& version = audit.version;
    std::cerr << "TRACE_FILE path=" << std::quoted(entry.first)
              << " dev=" << version[0] << " ino=" << version[1] << " size=" << version[2]
              << " mtime_ns=" << version[3] << " ctime_ns=" << version[4]
              << " stable=" << (audit.stable ? 1 : 0)
              << " access_bits=" << audit.access_bits << std::endl;
  }
}

VOID ThreadStart(THREADID tid, CONTEXT*, INT32, VOID*) {
  // A shared instruction buffer is safe only when the application has one thread.
  if (tid != 0) {
    std::cerr << "TRACE_ERROR additional application thread tid=" << tid << std::endl;
    PIN_ExitProcess(3);
  }
}

/*!
 * The main procedure of the tool.
 * This function is called when the application image is loaded but not yet started.
 * @param[in]   argc            total number of elements in the argv array
 * @param[in]   argv            array of command line arguments,
 *                              including pin -t <toolname> -- ...
 */
int main(int argc, char* argv[])
{
  static_assert(sizeof(trace_instr_format_t) == 72, "trace record ABI mismatch");
  PIN_InitSymbols();

  // Initialize PIN library. Print help message if -h(elp) is specified
  // in the command line or the command line is invalid
  if (PIN_Init(argc, argv))
    return Usage();

  outfile.open(KnobOutputFile.Value().c_str(), std::ios_base::binary | std::ios_base::trunc);
  if (!outfile) {
    std::cout << "Couldn't open output trace file. Exiting." << std::endl;
    exit(1);
  }

  outfile.write(reinterpret_cast<const char*>(TRACE_V2_HEADER.data()),
                static_cast<std::streamsize>(TRACE_V2_HEADER.size()));
  if (!outfile) { std::cerr << "TRACE_ERROR header write failed" << std::endl; PIN_ExitProcess(2); }

  // Register function to perform symbol lookup
  IMG_AddInstrumentFunction(ImageLoad, 0);

  // Register function to be called to instrument instructions
  INS_AddInstrumentFunction(Instruction, 0);
  PIN_AddThreadStartFunction(ThreadStart, 0);

  PIN_AddSyscallEntryFunction(SyscallEntry, 0);
  PIN_AddSyscallExitFunction(SyscallExit, 0);

  // Register function to be called when the application exits
  PIN_AddFiniFunction(Fini, 0);

  // Start the program, never returns
  PIN_StartProgram();

  return 0;
}
