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

#ifndef VMILL_BC_LIFTER_H_
#define VMILL_BC_LIFTER_H_

#include <cstdint>
#include <memory>
#include <vector>

namespace llvm {
class LLVMContext;
class Module;
}  // namespace llvm
namespace remill {
class Arch;
}  // namespace remill
namespace vmill {

class DecodedTraceList;

// A single-entry, multiple-exit trace, starting at `pc`.
struct LiftedTrace {
  const uint64_t entry_pc;
  const uint64_t hash;
  llvm::Function * const func;
};

class LifterImpl;

// Lifts machine code instructions into LLVM functions.
class Lifter {
 public:
  ~Lifter(void);

  explicit Lifter(const remill::Arch *arch_,
                  const std::shared_ptr<llvm::LLVMContext> &context);

  // Lift a list of decoded traces into a new LLVM bitcode module, and
  // return the resulting module.
  std::unique_ptr<llvm::Module> Lift(const DecodedTraceList &traces) const;

 protected:
  Lifter(void) = delete;

  std::unique_ptr<LifterImpl> impl;
};

}  // namespace vmill

#endif  // VMILL_BC_LIFTER_H_
