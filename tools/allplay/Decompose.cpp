#include "Decompose.h"

#include "boost_progress.h"
#include "subcommand-registry.h"

#include "allvm/SplitModule.h"

#include <llvm/ADT/StringExtras.h>
#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Object/ModuleSymbolTable.h>
#include <llvm/Object/ObjectFile.h>
#include <llvm/Support/Errc.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/TarWriter.h>
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
cl::opt<std::string>
    OutputPath("o", cl::Required,
               cl::desc("Path to write fragments (or filename of tarfile)"),
               cl::sub(Decompose));
cl::opt<bool> WriteTar(
    "write-tar", cl::Optional, cl::init(false),
    cl::desc("Write partitions into tar, instead of loose files in directory"),
    cl::sub(Decompose));
cl::opt<bool>
    DumpModules("dump", cl::Optional, cl::init(false),
                cl::desc("Dump modules before writing them, use with caution."),
                cl::sub(Decompose));
cl::opt<bool> VerifyModules(
    "verify", cl::Optional, cl::init(false),
    cl::desc("Run module verifier on modules before writing to disk."),
    cl::sub(Decompose));
cl::opt<bool>
    LLVMSplitModule("llvm-splitmodule", cl::Optional, cl::init(false),
                    cl::desc("Use LLVM's SplitModule instead of local version"),
                    cl::sub(Decompose));

bool isUsefulByItself(GlobalValue *GV) { return !GV->isDeclaration(); }

bool isNonEmpty(Module *M) {
  for (auto &F : *M)
    if (isUsefulByItself(&F))
      return true;
  for (auto &GV : M->globals())
    if (isUsefulByItself(&GV))
      return true;
  for (auto &GA : M->aliases())
    if (isUsefulByItself(&GA))
      return true;
  for (auto &GIF : M->ifuncs())
    if (isUsefulByItself(&GIF))
      return true;

  return false;
}

auto removeDeadDecls = [](auto I, auto E) {
  while (I != E) {
    auto *GV = &*I++;

    GV->removeDeadConstantUsers();

    if (GV->isDeclaration() && GV->use_empty()) {
      GV->eraseFromParent();
    }
  }
};

void removeDeadGlobalDecls(Module &M) {
  removeDeadDecls(M.begin(), M.end());
  removeDeadDecls(M.global_begin(), M.global_end());
}

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

Error writeBCToDisk(std::unique_ptr<Module> Mod, StringRef Filename) {
  std::error_code EC;
  tool_output_file Out(Filename, EC, sys::fs::F_None);
  if (EC)
    return errorCodeToError(EC);

  if (DumpModules)
    Mod->dump(); // not to OS

  WriteBitcodeToFile(Mod.get(), Out.os());

  Out.keep();
  return Error::success();
}

const unsigned SplitFactor = 11; // MAGIC
const StringRef ModuleIDPrefix = "base";

} // end anonymous namespace

Error allvm::decompose(
    std::unique_ptr<llvm::Module> M,
    function_ref<Error(std::unique_ptr<Module> MPart, StringRef Path)>
        ModuleCallback,
    bool Verbose) {

  auto &OS = Verbose ? errs() : nulls();

  OS << "Splitting...\n";

  std::vector<std::unique_ptr<Module>> ModQ;

  // Stash original module identifier,
  // set to predictable value so partitioning
  // isn't reliant on the ModuleIdentifier
  // (which is often a filename)
  std::string OrigModID = M->getModuleIdentifier();
  M->setModuleIdentifier(ModuleIDPrefix);
  ModQ.push_back(std::move(M));

  auto SplitWhileUseful = [&](bool PreserveLocals, auto Callback) {
    while (!ModQ.empty()) {
      auto CurM = std::move(ModQ.back());
      ModQ.pop_back();

      size_t Empty = 0;
      size_t Count = 0;
      size_t Before = ModQ.size();
      auto SplitFn = LLVMSplitModule ? llvm::SplitModule : allvm::SplitModule;
      SplitFn(std::move(CurM), SplitFactor,
              [&](std::unique_ptr<Module> MPart) {
                removeDeadGlobalDecls(*MPart);
                if (VerifyModules)
                  verifyModule(*MPart);
                if (!isNonEmpty(MPart.get()) &&
                    !hasSymbolDefinition(MPart.get())) {
                  ++Empty;
                  return;
                }
                MPart->setModuleIdentifier(MPart->getModuleIdentifier() + "_" +
                                           utostr(Count++));
                ModQ.emplace_back(std::move(MPart));
              },
              PreserveLocals);
      assert(Count && "all partitions empty?!");
      assert((ModQ.size() - Before == Count) && "module queue count mismatch");
      assert((Count + Empty == SplitFactor) &&
             "callback not called splitfactor times");

      if (Count == 1) {
        // We tried to split but failed (single partition)
        // so don't requeue, pass to callback
        auto OutM = std::move(ModQ.back());
        ModQ.pop_back();

        Callback(std::move(OutM));
      }
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
    StringRef Suffix = OutM->getModuleIdentifier();
    assert(Suffix.startswith(ModuleIDPrefix));
    Suffix.consume_front(ModuleIDPrefix);

    OutM->setModuleIdentifier((OrigModID + Suffix).str());

    std::string OutName = utostr(CurModIdx++);
    if (auto E = ModuleCallback(std::move(OutM), OutName)) {
      errs() << "Error writing to disk? :(\n";
      assert(0 && "Bad error path, FIXME!");
    }
  };

  // This extenalizes globals first,  meaning:
  // Decomposed output can be linked together BUT
  // resulting module will have all externals which is likely
  // to cause breakage if attempting to link with other bitcode.
  auto E = SplitWhileUseful(false, writeToDisk);
  OS << "Partitions: " << CurModIdx << "\n";
  return std::move(E);
}

Error allvm::decompose_into_dir(StringRef BCFile, StringRef OutDir,
                                bool Verbose) {
  LLVMContext C;
  SMDiagnostic Diag;
  auto M = parseIRFile(BCFile, Diag, C);
  if (!M)
    return make_error<StringError>("Unable to open IR file " + BCFile,
        errc::invalid_argument);

  if (auto Err = M->materializeAll())
    return Err;
  return decompose_into_dir(std::move(M), OutDir, Verbose);
}

Error allvm::decompose_into_dir(std::unique_ptr<llvm::Module> M, StringRef OutDir,
                                bool Verbose) {
  if (auto EC = sys::fs::create_directories(OutDir))
    return errorCodeToError(EC);

  return decompose(std::move(M),
                   [&OutDir](auto OutM, StringRef Filename) {
                     std::string Path = (OutDir + "/" + Filename).str();
                     return writeBCToDisk(std::move(OutM), Path);
                   },
                   Verbose);
}


Error allvm::decompose_into_tar(StringRef BCFile, StringRef OutDir,
                                bool Verbose) {
  LLVMContext C;
  SMDiagnostic Diag;
  auto M = parseIRFile(BCFile, Diag, C);
  if (!M)
    return make_error<StringError>("Unable to open IR file " + BCFile,
        errc::invalid_argument);

  if (auto Err = M->materializeAll())
    return Err;
  return decompose_into_tar(std::move(M), OutDir, Verbose);
}

Error allvm::decompose_into_tar(std::unique_ptr<llvm::Module> M, StringRef TarFile,
                                bool Verbose) {
  StringRef BasePath = "bits"; // TODO: something useful for this?
  auto TW = TarWriter::create(TarFile, BasePath);
  if (!TW)
    return TW.takeError();

  return decompose(std::move(M),
                   [&TW](auto OutM, StringRef Filename) {
                     SmallVector<char, 0> Buffer;
                     raw_svector_ostream OS(Buffer);

                     WriteBitcodeToFile(OutM.get(), OS);

                     (*TW)->append(Filename,
                                   StringRef(Buffer.data(), Buffer.size()));

                     return Error::success();
                   },
                   Verbose);
}

namespace {
CommandRegistration
    Unused(&Decompose, [](ResourcePaths &RP LLVM_ATTRIBUTE_UNUSED) -> Error {
      if (WriteTar)
        return decompose_into_tar(InputFile, OutputPath, true);
      else
        return decompose_into_dir(InputFile, OutputPath, true);
    });
} // end anonymous namespace
