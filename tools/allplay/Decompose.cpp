#include "Decompose.h"

#include "boost_progress.h"
#include "subcommand-registry.h"

#include <llvm/ADT/StringExtras.h>
#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Module.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Object/ModuleSymbolTable.h>
#include <llvm/Object/ObjectFile.h>
#include <llvm/Support/Errc.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/ToolOutputFile.h>
#include <llvm/Transforms/IPO.h>
#include <llvm/Transforms/Utils/SplitModule.h>

#include <algorithm>
#include <vector>

using namespace allvm;
using namespace llvm;

namespace {

// TODO: 'decompose' allexe in-place?
cl::SubCommand Decompose("decompose",
                         "Decompose a bitcode file into linkable fragments");
cl::opt<std::string> InputFile(cl::Positional, cl::Required,
                               cl::desc("input bitcode filename"),
                               cl::sub(Decompose));
cl::opt<std::string> OutputDirectory("o", cl::Required,
                                     cl::desc("Path to write fragments"),
                                     cl::sub(Decompose));
cl::opt<bool>
    DumpModules("dump", cl::Optional, cl::init(false),
                cl::desc("Dump modules before writing them, use with caution."),
                cl::sub(Decompose));

bool hasSymbolDefinition(llvm::Module *M) {
  ModuleSymbolTable MST;
  MST.addModule(M);

  for (auto &S : MST.symbols()) {
    auto Flags = MST.getSymbolFlags(S);
    if (Flags & object::SymbolRef::SF_Undefined)
      continue;
    return true;
  }

  return false;
}

const unsigned SplitFactor = 37; // MAGIC

} // end anonymous namespace

Error allvm::decompose(StringRef BCFile, StringRef OutDir, bool Verbose) {

  auto &OS = Verbose ? errs() : nulls();
  OS << "Loading file '" << BCFile << "'...\n";
  LLVMContext C;
  SMDiagnostic Diag;
  auto M = parseIRFile(BCFile, Diag, C);
  if (!M)
    return make_error<StringError>("Unable to open IR file " + BCFile,
                                   errc::invalid_argument);

  if (auto Err = M->materializeAll())
    return Err;

  if (auto EC = sys::fs::create_directories(OutDir))
    return errorCodeToError(EC);

  OS << "Splitting...\n";

  legacy::PassManager PM;
  PM.add(createGlobalOptimizerPass());

  std::vector<std::unique_ptr<Module>> ModQ;
  ModQ.push_back(std::move(M));

  auto SplitWhileUseful = [&](bool PreserveLocals, auto Callback) {
    unsigned CurSplitFactor = SplitFactor;
    while (!ModQ.empty()) {
      auto CurM = std::move(ModQ.back());
      ModQ.pop_back();

      size_t Empty = 0;
      size_t Before = ModQ.size();
      SplitModule(std::move(CurM), CurSplitFactor,
                  [&](std::unique_ptr<Module> MPart) {
                    PM.run(*MPart);
                    if (!hasSymbolDefinition(MPart.get())) {
                      ++Empty;
                      return;
                    }
                    // OS << "Adding module..\n";
                    ModQ.emplace_back(std::move(MPart));
                  },
                  PreserveLocals);

      assert(Empty != CurSplitFactor && "all partitions empty??");
      auto UsefulPartitions = CurSplitFactor - Empty;

      assert(UsefulPartitions = ModQ.size() - Before);
      if (UsefulPartitions == 1) {
        // We tried to split but failed (single partition)
        // so don't requeue, pass to callback
        auto OutM = std::move(ModQ.back());
        ModQ.pop_back();

        Callback(std::move(OutM));
      }

      // TODO: Increase split factor to tease out partitions,
      // but only near the end--early on splitting small amounts
      // is critical
      // ++CurSplitFactor;
    }

    return Error::success();
  };

  std::vector<std::unique_ptr<Module>> SecondQueue;
  auto addToSecondQueue = [&](auto MPart) {
    SecondQueue.emplace_back(std::move(MPart));
  };

  OS << "First pass...\n";
  if (auto E = SplitWhileUseful(true, addToSecondQueue))
    return std::move(E);

  assert(ModQ.empty());
  ModQ.swap(SecondQueue);

  OS << "Partitions: " << ModQ.size() << "\n";
  OS << "Second pass...\n";
  size_t CurModIdx = 0;
  auto writeToDisk = [&](auto OutM) {
    std::string OutName = (OutDir + "/" + utostr(CurModIdx++)).str();

    std::error_code EC;
    tool_output_file Out(OutName, EC, sys::fs::F_None);
    if (EC) {
      assert(false && "Eep bad error path FIXME");
      // return errorCodeToError(EC);
    }

    if (DumpModules)
      OutM->dump(); // not to OS

    WriteBitcodeToFile(OutM.get(), Out.os());

    Out.keep();
  };

  auto E = SplitWhileUseful(false, writeToDisk);
  OS << "Partitions: " << CurModIdx << "\n";
  return std::move(E);
}

namespace {
CommandRegistration
    Unused(&Decompose, [](ResourcePaths &RP LLVM_ATTRIBUTE_UNUSED) -> Error {
      return decompose(InputFile, OutputDirectory, true);
    });
} // end anonymous namespace
