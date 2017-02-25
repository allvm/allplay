#include "Decompose.h"

#include "boost_progress.h"
#include "subcommand-registry.h"

#include "allvm/BCDB.h"

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
#include <llvm/Support/ThreadPool.h>
#include <llvm/Support/ToolOutputFile.h>
#include <llvm/Transforms/IPO.h>
#include <llvm/Transforms/Utils/SplitModule.h>

#include <algorithm>
#include <vector>

using namespace allvm;
using namespace llvm;

namespace {

cl::SubCommand
    DecomposeAllexes("decompose-allexes",
                     "Find allexes and decompose bitcode. Other things maybe");
cl::opt<std::string> InputDirectory(cl::Positional, cl::Required,
                                    cl::desc("<input directory to scan>"),
                                    cl::sub(DecomposeAllexes));
cl::opt<unsigned> Threads("j", cl::Optional, cl::init(0),
                          cl::desc("Number of threads, 0 to auto-detect"));

Error decomposeAllexes(BCDB &DB) {
  StringRef OutBase = "bits";
  unsigned NThreads = Threads;
  if (NThreads == 0)
    NThreads = std::thread::hardware_concurrency();
  ThreadPool TP(NThreads);

  // exit on error instead of propagating errors
  // out of the thread pool safely
  ExitOnError ExitOnErr("allplay decompose-allexes: ");

  errs() << "Decomposing " << DB.getMods().size() << " modules,";
  errs() << " using " << NThreads << " threads...\n";

  size_t I = 0;
  for (auto &MI : DB.getMods()) {
    std::string dir = (OutBase + "/" + utostr(I++)).str();
    TP.async(
        [&ExitOnErr](auto Filename, auto OutDir) {
          ExitOnErr(decompose(Filename, OutDir, false));
        },
        MI.Filename, dir);
  }

  errs() << "Waiting for threads to complete...\n";
  TP.wait();

  return Error::success();
}

CommandRegistration Unused(&DecomposeAllexes, [](ResourcePaths &RP) -> Error {
  errs() << "Loading allexe's from " << InputDirectory << "...\n";
  auto ExpDB = BCDB::loadFromAllexesIn(InputDirectory, RP);
  if (!ExpDB)
    return ExpDB.takeError();
  auto &DB = *ExpDB;
  errs() << "Done! Allexes found: " << DB->allexe_size() << "\n";
  return decomposeAllexes(*DB);
});
} // end anonymous namespace
