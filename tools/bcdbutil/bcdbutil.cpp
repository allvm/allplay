//===-- bcdbutil.cpp ------------------------------------------------------===//
//
// Author: Will Dietz (WD), wdietz2@uiuc.edu
//
//===----------------------------------------------------------------------===//
//
// Utilities for creating and analyzing BCDB's.
//
//===----------------------------------------------------------------------===//

#include "allvm/Allexe.h"
#include "allvm/BCDB.h"
#include "allvm/GitVersion.h"
#include "allvm/ResourceAnchor.h"

#include "subcommand-registry.h"

#include <llvm/Support/CommandLine.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/PrettyStackTrace.h>
#include <llvm/Support/Signals.h>
#include <llvm/Support/raw_ostream.h>

using namespace allvm;
using namespace llvm;

int main(int argc, const char *argv[]) {
  sys::PrintStackTraceOnErrorSignal(argv[0]);
  PrettyStackTraceProgram X(argc, argv);
  llvm_shutdown_obj Y; // Call llvm_shutdown() on exit.

  cl::ParseCommandLineOptions(argc, argv, "BCDB Utilities\n\n");

  ResourcePaths RP = ResourcePaths::getAnchored(argv[0]);

  for (auto *SC : cl::getRegisteredSubcommands()) {
    // Skip special top-level subcommand
    if (SC == &*cl::TopLevelSubCommand)
      continue;
    if (*SC)
      if (auto C = dispatch(SC)) {
        ExitOnError("bcdbutil: ")(C(RP));
        return 0;
      }
  }

  cl::PrintHelpMessage(false, true);
}
