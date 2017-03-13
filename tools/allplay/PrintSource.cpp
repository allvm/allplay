#include "subcommand-registry.h"

#include "allvm/ModuleFlags.h"

#include <llvm/IR/LLVMContext.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/Errc.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/raw_ostream.h>

using namespace allvm;
using namespace llvm;

namespace {

cl::SubCommand PrintSource("printsource",
                           "Print Source information from module flags");

cl::opt<std::string> InputFilename(cl::Positional, cl::Required,
                                   cl::desc("<input bitcode file>"),
                                   cl::sub(PrintSource));
cl::opt<bool> OnlyPrintSource("only-print-source", cl::Optional,
                              cl::init(false),
                              cl::desc("Only print the source (nothing else)"),
                              cl::sub(PrintSource));

CommandRegistration
    Unused(&PrintSource, [](ResourcePaths &RP LLVM_ATTRIBUTE_UNUSED) -> Error {
      if (!OnlyPrintSource)
        errs() << "Loading file '" << InputFilename << "'...\n";
      LLVMContext C;
      SMDiagnostic Diag;
      auto M = parseIRFile(InputFilename, Diag, C);
      if (!M)
        return make_error<StringError>(
            "Unable to open IR file " + InputFilename, errc::invalid_argument);

      if (auto Err = M->materializeMetadata())
        return Err;

      auto WSrc = getWLLVMSource(M.get());
      if (WSrc.empty()) {
        return make_error<StringError>(
            "Module did not contain WLLVM Source module flag, or invalid",
            errc::invalid_argument);
      }

      if (!OnlyPrintSource)
        outs() << "WLLVM Source: ";
      outs() << WSrc << "\n";

      return Error::success();
    });

} // end anonymous namespace
