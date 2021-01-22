/*
 * Copyright (c) 2017 Trail of Bits, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <gflags/gflags.h>
#include <glog/logging.h>

#include <algorithm>
#include <limits>
#include <set>
#include <string>
#include <utility>

#include "remill/Arch/Arch.h"
#include "remill/Arch/Instruction.h"

#include "vmill/Arch/Decoder.h"
#include "vmill/Program/AddressSpace.h"
#include "vmill/Util/Hash.h"

DECLARE_bool(verbose);

namespace vmill {
namespace {

// Read instruction bytes using `byte_reader`.
static std::string ReadInstructionBytes(
    const remill::Arch *arch, AddressSpace &addr_space, uint64_t pc) {

  std::string instr_bytes;
  const auto max_num_bytes = arch->MaxInstructionSize();
  instr_bytes.reserve(max_num_bytes);
  for (uint64_t i = 0; i < max_num_bytes; ++i) {
    uint8_t byte = 0;
    auto byte_pc = pc + i;
    if (!addr_space.TryReadExecutable(static_cast<PC>(byte_pc), &byte)) {
      LOG(WARNING)
          << "Stopping decode at non-executable byte "
          << std::hex << byte_pc << std::dec;
      break;
    }
    instr_bytes.push_back(static_cast<char>(byte));
  }

  return instr_bytes;
}

using DecoderWorkList = std::set<uint64_t>;

// Enqueue control flow targets for processing. We only follow directly
// reachable control-flow targets in this list.
static void AddSuccessorsToWorkList(const remill::Instruction &inst,
                                    DecoderWorkList &work_list) {
  switch (inst.category) {
    case remill::Instruction::kCategoryInvalid:
    case remill::Instruction::kCategoryError:
    case remill::Instruction::kCategoryIndirectJump:
    case remill::Instruction::kCategoryFunctionReturn:
    case remill::Instruction::kCategoryAsyncHyperCall:
      break;

    case remill::Instruction::kCategoryIndirectFunctionCall:
    case remill::Instruction::kCategoryDirectFunctionCall:
      work_list.insert(inst.branch_not_taken_pc);
      break;

    case remill::Instruction::kCategoryNormal:
    case remill::Instruction::kCategoryNoOp:
      work_list.insert(inst.next_pc);
      break;

    case remill::Instruction::kCategoryConditionalAsyncHyperCall:
      work_list.insert(inst.branch_not_taken_pc);
      break;

    case remill::Instruction::kCategoryDirectJump:
      work_list.insert(inst.branch_taken_pc);
      break;

    case remill::Instruction::kCategoryConditionalBranch:
      work_list.insert(inst.branch_taken_pc);
      work_list.insert(inst.next_pc);
      break;
  }
}

// Enqueue control flow targets that will potentially represent future traces.
static void AddSuccessorsToTraceList(const remill::Instruction &inst,
                                     DecoderWorkList &work_list) {
  switch (inst.category) {
    case remill::Instruction::kCategoryDirectFunctionCall:
      if (inst.branch_taken_pc != inst.branch_not_taken_pc) {
        work_list.insert(inst.branch_taken_pc);
      }
      break;

    default:
      break;
  }
}

// The 'version' of this trace is a hash of the instruction bytes.
static TraceId HashTraceInstructions(const DecodedTrace &trace) {
  const auto &insts = trace.instructions;
  TraceHashBaseType min_pc = 1;
  TraceHashBaseType max_pc = 1;

  if (!trace.instructions.empty()) {
    min_pc = static_cast<TraceHashBaseType>(
        trace.instructions.begin()->first);

    max_pc = static_cast<TraceHashBaseType>(
        trace.instructions.rbegin()->first);
  }

  Hasher<TraceHashBaseType> hash2(min_pc * max_pc * insts.size());
  for (const auto &entry : insts) {
    hash2.Update(entry.second.bytes.data(), entry.second.bytes.size());
  }

  return {trace.pc, static_cast<TraceHash>(hash2.Digest())};
}

bool VerifyTraces(const DecodedTraceList &traces) {
  bool out = true;
  for (auto &trace : traces) {
    if (!trace.instructions.count(trace.pc)) {
      DLOG(WARNING) << "Trace at "
                    << std::hex << static_cast<uint64_t>(trace.pc) << std::dec
                    << " does not contain instruction at its begin addr!";
      out &= false;
    }
  }
  return out;
}

}  // namespace

// Starting from `start_pc`, read executable bytes out of a memory region
// using `byte_reader`, and returns a mapping of decoded instruction program
// counters to the decoded instructions themselves.
DecodedTraceList DecodeTraces(const remill::Arch *arch,
                              AddressSpace &addr_space, PC start_pc) {

  DecodedTraceList traces;
  DecoderWorkList trace_list;
  DecoderWorkList work_list;

  DLOG_IF(INFO, FLAGS_verbose)
      << "Recursively decoding machine code, beginning at "
      << std::hex << static_cast<uint64_t>(start_pc);

  trace_list.insert(static_cast<uint64_t>(start_pc));

  while (!trace_list.empty()) {
    auto trace_it = trace_list.begin();
    const auto trace_pc_uint = *trace_it;
    const auto trace_pc = static_cast<PC>(trace_pc_uint);
    trace_list.erase(trace_it);

    if (addr_space.IsMarkedTraceHead(trace_pc)) {
      continue;
    }

    addr_space.MarkAsTraceHead(trace_pc);
    CHECK(work_list.empty());
    work_list.insert(trace_pc_uint);

    DecodedTrace trace;
    trace.pc = static_cast<PC>(trace_pc_uint);
    trace.code_version = addr_space.ComputeCodeVersion(trace_pc);

    while (!work_list.empty()) {
      auto entry_it = work_list.begin();
      const auto pc = *entry_it;
      work_list.erase(entry_it);

      if (trace.instructions.count(static_cast<PC>(pc))) {
        continue;
      }

      remill::Instruction inst;
      auto inst_bytes = ReadInstructionBytes(arch, addr_space, pc);
      //LOG_IF(INFO, inst_bytes.size() == 0) << "0 bytes at: " << std::hex << static_cast<uint64_t>(pc) << std::dec;
      auto decode_successful = arch->DecodeInstruction(
          pc, inst_bytes, inst);

      //LOG(INFO) << "Adding inst at " << std::hex << static_cast<uint64_t>(pc) << std::dec << std::endl;
      trace.instructions[static_cast<PC>(pc)] = inst;

      if (!decode_successful) {
        LOG(WARNING)
            << "Cannot decode instruction at " << std::hex << pc << std::dec
            << ": " << inst.Serialize();
        continue;
      } else {
        AddSuccessorsToWorkList(inst, work_list);
        AddSuccessorsToTraceList(inst, trace_list);
      }
    }

    trace.id = HashTraceInstructions(trace);

    DLOG_IF(INFO, FLAGS_verbose)
        << "Decoded " << trace.instructions.size()
        << " instructions starting from "
        << std::hex << static_cast<uint64_t>(trace.pc) << std::dec;

    traces.push_back(std::move(trace));
  }

  DCHECK(VerifyTraces(traces));
  return traces;
}

}  // namespace vmill
