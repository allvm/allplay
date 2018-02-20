//===- SplitModule.h - Split a module into partitions -----------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines the function allvm_analysis::SplitModule, which splits a
// module into multiple linkable partitions. It can be used to implement
// parallel code generation for link-time optimization.
//
//===----------------------------------------------------------------------===//

#ifndef ALLVM_ANALYSIS_TRANSFORMS_UTILS_SPLITMODULE_H
#define ALLVM_ANALYSIS_TRANSFORMS_UTILS_SPLITMODULE_H

#include "llvm/ADT/STLExtras.h"
#include <memory>

namespace llvm {

class Module;
class StringRef;

} // end namespace llvm

namespace allvm_analysis {
/// Splits the module M into N linkable partitions. The function ModuleCallback
/// is called N times passing each individual partition as the MPart argument.
///
/// FIXME: This function does not deal with the somewhat subtle symbol
/// visibility issues around module splitting, including (but not limited to):
///
/// - Internal symbols should not collide with symbols defined outside the
///   module.
/// - Internal symbols defined in module-level inline asm should be visible to
///   each partition.
void SplitModule(std::unique_ptr<llvm::Module> M, unsigned N,
                 llvm::function_ref<void(std::unique_ptr<llvm::Module> MPart)>
                     ModuleCallback,
                 bool PreserveLocals = false);

} // end namespace allvm_analysis

#endif // ALLVM_ANALYSIS_TRANSFORMS_UTILS_SPLITMODULE_H
