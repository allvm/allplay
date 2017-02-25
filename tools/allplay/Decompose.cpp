#include "subcommand-registry.h"

#include <llvm/ADT/StringExtras.h>
#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Module.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Object/ModuleSymbolTable.h>
#include <llvm/Support/Errc.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/ToolOutputFile.h>
#include <llvm/Transforms/IPO.h>
#include <llvm/Transforms/Utils/SplitModule.h>
#include <llvm/Object/ObjectFile.h>

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
cl::opt<bool> DumpModules("dump", cl::Optional, cl::init(false),
    cl::desc("Dump modules before writing them, use with caution."),
    cl::sub(Decompose));

bool hasSymbolDefinition(llvm::Module *M) {
  ModuleSymbolTable MST;
  MST.addModule(M);

  for (auto &S: MST.symbols()) {
    auto Flags = MST.getSymbolFlags(S);
    if (Flags & object::SymbolRef::SF_Undefined)
      continue;
    return true;
  }

  return false;
}

Error decompose(StringRef BCFile, StringRef OutDir) {
  errs() << "Loading file '" << BCFile << "'...\n";
  LLVMContext C;
  SMDiagnostic Diag;
  auto M = parseIRFile(BCFile, Diag, C);
  if (!M)
    return make_error<StringError>("Unable to open IR file " + BCFile,
                                   errc::invalid_argument);

  if (auto Err = M->materializeAll())
    return Err;

  errs() << "Splitting...\n";
  // XXX: This is pretty kludgy-- we want something more direct than
  // using SplitModule and whatnot.  But it's a start.
  unsigned NumOutputs = 1000; // M->global_objects());
  std::vector<std::unique_ptr<Module>> Parts;
  SplitModule(std::move(M), NumOutputs,
              [&](std::unique_ptr<Module> MPart) {
                Parts.emplace_back(std::move(MPart));
              },
              false /* PreserveLocals */);

  errs() << "Post-processing split modules...\n";
  legacy::PassManager PM;
  PM.add(createGlobalOptimizerPass());
  for (auto &MPart : Parts) {
    PM.run(*MPart);
  }

  Parts.erase(std::remove_if(Parts.begin(), Parts.end(),[](auto &MPart) {
          return !hasSymbolDefinition(MPart.get());
        }), Parts.end());
  errs() << "Partitions: " << Parts.size() << "\n";

  errs() << "Writing modules to directory '" << OutDir << "'...\n";
  if (auto EC = sys::fs::create_directories(OutDir))
    return errorCodeToError(EC);

  unsigned I = 0;
  for (auto &MPart : Parts) {
    if (DumpModules)
      errs() << "\nModule " << utostr(I) << ":\n";
    std::error_code EC;
    std::string OutName = (OutDir + "/" + utostr(I++)).str();
    tool_output_file Out(OutName, EC, sys::fs::F_None);
    if (EC)
      return errorCodeToError(EC);

    if (DumpModules)
      MPart->dump();

    WriteBitcodeToFile(MPart.get(), Out.os());

    Out.keep();
  }

  errs() << "Done!\n";
  return Error::success();
}

CommandRegistration
    Unused(&Decompose, [](ResourcePaths &RP LLVM_ATTRIBUTE_UNUSED) -> Error {
      return decompose(InputFile, OutputDirectory);
    });

} // end anonymous namespace
