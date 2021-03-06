//===-- allplay.cpp -------------------------------------------------------===//
//
// Author: Will Dietz (WD), wdietz2@uiuc.edu
//
//===----------------------------------------------------------------------===//
//
// Collection of experimental utilities on allexe playgrounds
//
//===----------------------------------------------------------------------===//

#include "allvm-analysis/ABCDB.h"
#include "allvm-analysis/GitVersion.h"

#include <allvm/Allexe.h>
#include <allvm/ExitOnError.h>

#include "subcommand-registry.h"

#include <llvm/Support/CommandLine.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/PrettyStackTrace.h>
#include <llvm/Support/Signals.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>

using namespace allvm_analysis;
using namespace allvm;
using namespace llvm;

int main(int argc, const char *argv[]) {
  InitializeAllTargetInfos();
  InitializeAllTargets();
  InitializeAllTargetMCs();
  InitializeAllAsmPrinters();
  InitializeAllAsmParsers();

  sys::PrintStackTraceOnErrorSignal(argv[0]);
  PrettyStackTraceProgram X(argc, argv);
  llvm_shutdown_obj Y; // Call llvm_shutdown() on exit.

  cl::ParseCommandLineOptions(argc, argv, "allplay\n\n");

  ResourcePaths RP = ResourcePaths::get(ALLVM_PREFIX);

  for (auto *SC : cl::getRegisteredSubcommands()) {
    if (*SC) {
      // If no subcommand was provided, we need to explicitly check if this is
      // the top-level subcommand.
      if (SC == &*cl::TopLevelSubCommand)
        break;
      if (auto C = dispatch(SC)) {
        allvm::ExitOnError("allplay: ")(C(RP));
        return 0;
      }
    }
  }

  cl::PrintHelpMessage(false, true);
}
