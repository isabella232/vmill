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

#ifndef VMILL_ARCH_DECODER_H_
#define VMILL_ARCH_DECODER_H_

#include <functional>
#include <list>
#include <map>

#include "remill/Arch/Instruction.h"

namespace vmill {

class AddressSpace;

using InstructionMap = std::map<uint64_t, remill::Instruction>;

// Hash of the bytes of the machine code in the trace.
struct TraceId {
 public:
  uint64_t hash1;
  uint64_t hash2;

  inline bool operator==(const TraceId &that) const {
    return hash1 == that.hash1 && hash2 == that.hash2;
  }
};

struct DecodedTrace {
  uint64_t pc;  // Entry PC of the trace.
  TraceId id;  //
  InstructionMap instructions;
};

// Starting from `start_pc`, read executable bytes out of a memory region
// using `byte_reader`, and returns a mapping of decoded instruction program
// counters to the decoded instructions themselves.
std::list<DecodedTrace> DecodeTraces(AddressSpace &addr_space,
                                     uint64_t start_pc);

}  // namespace vmill

#endif  // VMILL_ARCH_DECODER_H_
