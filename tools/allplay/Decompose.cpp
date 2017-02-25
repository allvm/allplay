#include "subcommand-registry.h"

#include <llvm/ADT/StringExtras.h>
#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Module.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/Errc.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/ToolOutputFile.h>
#include <llvm/Transforms/Utils/SplitModule.h>

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

// This snippet taken from ThinLTOBitcodeWriter, repurposed:
// Even modules that don't export could have initializers..
bool exportsSymbol(llvm::Module *M) {
  bool ExportsSymbols = false;
  auto AddGlobal = [&](GlobalValue &GV) {
    if (GV.isDeclaration() || GV.getName().startswith("llvm.") ||
        !GV.hasExternalLinkage())
      return;
    ExportsSymbols = true;
  };

  // TODO: Early exit would be nice :P
  for (auto &F : *M)
    AddGlobal(F);
  for (auto &GV : M->globals())
    AddGlobal(GV);
  for (auto &GA : M->aliases())
    AddGlobal(GA);
  for (auto &IF : M->ifuncs())
    AddGlobal(IF);

  return ExportsSymbols;
}

Error decompose(StringRef BCFile, StringRef OutDir) {
  LLVMContext C;
  SMDiagnostic Diag;
  auto M = parseIRFile(BCFile, Diag, C);
  if (!M)
    return make_error<StringError>("Unable to open IR file " + BCFile,
                                   errc::invalid_argument);

  if (auto Err = M->materializeAll())
    return Err;

  // XXX: This is pretty kludgy-- we want something more direct than
  // using SplitModule and whatnot.  But it's a start.
  unsigned NumOutputs = 1000; // M->global_objects());
  std::vector<std::unique_ptr<Module>> Parts;
  SplitModule(std::move(M), NumOutputs,
              [&](std::unique_ptr<Module> MPart) {
                Parts.emplace_back(std::move(MPart));
              },
              false /* PreserveLocals */);

  // TODO: Filter out useless modules
  // TODO: global-opt?

  size_t ModsWithoutExports = 0;
  for (auto &MPart : Parts) {
    if (!exportsSymbol(MPart.get()))
      ++ModsWithoutExports;
  }
  errs() << "ModsWithoutExports: " << ModsWithoutExports << "\n";

  if (auto EC = sys::fs::create_directories(OutDir))
    return errorCodeToError(EC);

  unsigned I = 0;
  for (auto &MPart : Parts) {
    std::error_code EC;
    std::string OutName = (OutDir + "/" + utostr(I++)).str();
    tool_output_file Out(OutName, EC, sys::fs::F_None);
    if (EC)
      return errorCodeToError(EC);

    WriteBitcodeToFile(MPart.get(), Out.os());

    Out.keep();
  }

  return Error::success();
}

CommandRegistration
    Unused(&Decompose, [](ResourcePaths &RP LLVM_ATTRIBUTE_UNUSED) -> Error {
      return decompose(InputFile, OutputDirectory);
    });

} // end anonymous namespace
