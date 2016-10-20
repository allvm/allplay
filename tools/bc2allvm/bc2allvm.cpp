//===-- bc2allvm.cpp ------------------------------------------------------===//
//
// Author: Will Dietz (WD), wdietz2@uiuc.edu
//
//===----------------------------------------------------------------------===//
//
// Create an allexe file from a single bc file.
//
//===----------------------------------------------------------------------===//

#include "ALLVMContextAnchor.h"
#include "ALLVMVersion.h"
#include "Allexe.h"

#include <llvm/ADT/SmallString.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/PrettyStackTrace.h>
#include <llvm/Support/Signals.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/ToolOutputFile.h>
#include <llvm/Support/raw_ostream.h>

using namespace allvm;
using namespace llvm;

namespace {
cl::opt<std::string> MainFile(cl::Positional, cl::Required,
                              cl::desc("<main LLVM bitcode file>"));
cl::list<std::string> InputFiles(cl::Positional, cl::ZeroOrMore,
                                 cl::desc("<input LLVM bitcode file>..."));

cl::opt<std::string> OutputFilename("o", cl::desc("Override output filename"),
                                    cl::value_desc("filename"));
cl::opt<bool> ForceOutput("f", cl::desc("Replace output allexe if it exists"),
                          cl::init(false));

ExitOnError ExitOnErr;

} // end anonymous namespace

int main(int argc, const char **argv) {
  sys::PrintStackTraceOnErrorSignal(argv[0]);
  PrettyStackTraceProgram X(argc, argv);
  llvm_shutdown_obj Y; // Call llvm_shutdown() on exit.

  ALLVMContext AC = ALLVMContext::getAnchored(argv[0]);

  cl::ParseCommandLineOptions(argc, argv);
  ExitOnErr.setBanner(std::string(argv[0]) + ": ");

  // Figure out where we're writing the output
  if (OutputFilename.empty()) {
    StringRef Input = MainFile;
    if (Input != "-") {
      SmallString<64> Output{Input};
      sys::path::replace_extension(Output, "allexe");
      OutputFilename = Output.str();
    }
  }
  if (OutputFilename.empty()) {
    errs() << "No output filename given!\n";
    return 1;
  }
  if (!ForceOutput && llvm::sys::fs::exists(OutputFilename)) {
    errs() << "Output file exists. Use -f flag to force overwrite.\n";
    return 1;
  }

  {
    // Try to open the output file first
    auto Output = ExitOnErr(Allexe::open(OutputFilename, AC, ForceOutput));

    ExitOnErr(Output->addModule(MainFile, ALLEXE_MAIN));

    for (const auto &it : InputFiles) {
      ExitOnErr(Output->addModule(it));
    }

    // TODO: Add (on by default?) feature for checking that
    // the resulting allexe is sane/reasonable/not-obviously-invalid.
    // Allexe::sanityCheck() ?
    // (Allexe destructor writes the file)
  }

  return 0;
}
