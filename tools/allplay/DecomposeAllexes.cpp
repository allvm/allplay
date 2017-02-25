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

Error decomposeAllexes(BCDB &DB) {
  StringRef OutDir = "bits";
  boost::progress_display progress(DB.getMods().size());
  size_t I = 0;
  for (auto &MI : DB.getMods()) {
    std::string dir = (OutDir + "/" + utostr(I++)).str();
    if (auto Err = decompose(MI.Filename, dir, false))
      return Err;
    ++progress;
  }

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
