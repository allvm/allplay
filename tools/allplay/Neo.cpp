#include "subcommand-registry.h"

#include "boost_progress.h"

#include "allvm-analysis/ABCDB.h"
#include "allvm-analysis/ModuleFlags.h"

#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/Errc.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/ToolOutputFile.h>
#include <llvm/Transforms/Utils/FunctionComparator.h>

#include <algorithm>
#include <functional>
#include <numeric>

using namespace allvm_analysis;
using namespace allvm;
using namespace llvm;

namespace {

cl::SubCommand NeoCSV("neocsv", "Create CSV files for importing into neo4j");
cl::opt<std::string> InputDirectory(cl::Positional, cl::Required,
                                    cl::desc("<input directory to scan>"),
                                    cl::sub(NeoCSV));
cl::opt<std::string> ModOut("modules", cl::init("modules.csv"),
                            cl::desc("name of file to write module node data"),
                            cl::sub(NeoCSV));
cl::opt<std::string>
    AllOut("allexes", cl::init("allexes.csv"),
           cl::desc("name of file to write allexe module node data"),
           cl::sub(NeoCSV));
cl::opt<std::string>
    FuncOut("funcs", cl::init("funcs.csv"),
            cl::desc("name of file to write function node data"),
            cl::sub(NeoCSV));
cl::opt<std::string>
    ModGlobalsOut("modglobals", cl::init("modglobals.csv"),
                  cl::desc("name of file to write contains function rel data"),
                  cl::sub(NeoCSV));
cl::opt<std::string>
    ContainsOut("contains", cl::init("contains.csv"),
                cl::desc("name of file to write contains mod rel data"),
                cl::sub(NeoCSV));
cl::opt<std::string>
    AliasOut("aliases", cl::init("aliases.csv"),
             cl::desc("name of file to write aliases node data"),
             cl::sub(NeoCSV));
cl::opt<std::string>
    GlobalOut("globals", cl::init("globals.csv"),
              cl::desc("name of file to write globals node data"),
              cl::sub(NeoCSV));

template <typename T> auto countInsts(const T *V) {
  return std::accumulate(
      V->begin(), V->end(), size_t{0},
      [](auto N, auto &child) { return N + countInsts(&child); });
}
template <> auto countInsts(const BasicBlock *B) { return B->size(); }

Expected<std::unique_ptr<tool_output_file>> openFile(StringRef Filename) {
  std::error_code EC;
  auto F = llvm::make_unique<tool_output_file>(Filename, EC,
                                               sys::fs::OpenFlags::F_Text);
  if (EC)
    return make_error<StringError>("Unable to open file " + Filename, EC);
  return std::move(F);
}

std::unique_ptr<tool_output_file> openFile(StringRef Filename, Error &E) {
  auto F = openFile(Filename);
  if (!F) {
    E = joinErrors(std::move(E), F.takeError());
    return nullptr;
  }
  return std::move(*F);
}

Error neo(ABCDB &DB, StringRef Prefix) {
  Error E = Error::success();
  auto ModOutFile = openFile(ModOut, E);
  auto FuncOutFile = openFile(FuncOut, E);
  auto AllOutFile = openFile(AllOut, E);
  auto ContainsOutFile = openFile(ContainsOut, E);
  auto GlobalOutFile = openFile(GlobalOut, E);
  auto ModGlobalsOutFile = openFile(ModGlobalsOut, E);
  auto AliasOutFile = openFile(AliasOut, E);
  if (E)
    return E;

  auto &ModS = ModOutFile->os();
  auto &FuncS = FuncOutFile->os();
  auto &AllS = AllOutFile->os();
  auto &ContainS = ContainsOutFile->os();
  auto &GlobalS = GlobalOutFile->os();
  auto &ModGlobalS = ModGlobalsOutFile->os();
  auto &AliasS = AliasOutFile->os();

  // TODO: canonicalize all paths into nix store
  auto removePrefix = [Prefix](StringRef S) {
    if (S.startswith(Prefix))
      S = S.drop_front(Prefix.size());
    StringRef NixStorePrefix = "/nix/store/";
    if (S.startswith(NixStorePrefix))
      S = S.drop_front(NixStorePrefix.size());
    if (S.startswith("/"))
      S = S.drop_front(1);
    return S;
  };

  auto basename = [](StringRef S) { return S.rsplit('/').second; };

  boost::progress_display mod_progress(DB.getMods().size());

  // Create module nodes
  ModS << "CRC:ID(Module),Name,Path\n";
  FuncS << ":ID(Global),Name,Insts:int,Hash:long,:LABEL\n";
  GlobalS << ":ID(Global),Name,:LABEL\n"; // XXX: Add more info
  AliasS << ":ID(Global),Name,Aliasee\n"; // XXX: Add info
  ModGlobalS << ":START_ID(Module),:END_ID(Global),:TYPE\n";
  size_t GlobalID = 0;
  for (auto &MI : DB.getMods()) {
    ModS << MI.ModuleCRC << "," << basename(MI.Filename) << ","
         << removePrefix(MI.Filename) << "\n";

    SMDiagnostic SM;
    LLVMContext C;
    auto M = llvm::parseIRFile(MI.Filename, SM, C);
    if (!M)
      return make_error<StringError>(
          "Unable to open module file " + MI.Filename, errc::invalid_argument);
    if (auto Err = M->materializeAll())
      return Err;

    for (auto &F : *M) {
      FuncS << GlobalID << "," << F.getName() << ",";
      if (F.isDeclaration()) {
        FuncS << "0,0,Declaration\n";
      } else {
        auto H = FunctionComparator::functionHash(F);
        FuncS << countInsts(&F) << "," << H << ",Definition\n";
      }

      // Edge property redundant with node label, but oh well
      auto ModFuncRel = F.isDeclaration() ? "DECLARES" : "DEFINES";
      ModGlobalS << MI.ModuleCRC << "," << GlobalID << "," << ModFuncRel
                 << "\n";

      ++GlobalID;
    }

    for (auto &G : M->globals()) {
      auto ModRel = G.isDeclaration() ? "DECLARES" : "DEFINES";
      auto Label = G.isDeclaration() ? "Declaration" : "Definition";
      GlobalS << GlobalID << "," << G.getName() << "," << Label << "\n";
      ModGlobalS << MI.ModuleCRC << "," << GlobalID << "," << ModRel << "\n";

      ++GlobalID;
    };

    for (auto &A : M->aliases()) {
      AliasS << GlobalID << "," << A.getName() << ","
             << A.getAliasee()->getName() << "\n";

      ModGlobalS << MI.ModuleCRC << "," << GlobalID << ","
                 << "DEFINES\n";

      ++GlobalID;
    }

    ++mod_progress;
  }

  // allexe nodes
  AllS << "ID:ID(Allexe),Name,Path\n";
  for (size_t idx = 0; idx < DB.allexe_size(); ++idx) {
    auto &A = DB.getAllexes()[idx];
    AllS << idx << "," << basename(A.Filename) << ","
         << removePrefix(A.Filename) << "\n";
  }

  // emit allexe -> module relationships
  ContainS << ":START_ID(Allexe),Index,:END_ID(Module)\n";
  for (size_t idx = 0; idx < DB.allexe_size(); ++idx) {
    auto &A = DB.getAllexes()[idx];
    for (size_t i = 0; i < A.Modules.size(); ++i) {
      auto &M = A.Modules[i];
      ContainS << idx << "," << i << "," << M.ModuleCRC << "\n";
    }
  }

  ModOutFile->keep();
  ModGlobalsOutFile->keep();
  AliasOutFile->keep();
  FuncOutFile->keep();
  GlobalOutFile->keep();
  AllOutFile->keep();
  ContainsOutFile->keep();

  errs() << "Import using 'neo4j-admin' command, something like:\n";
  errs() << "\n";

  errs() << "sudo NEO4J_CONF=/var/lib/neo4j/conf \\\n";
  errs() << "neo4j-admin import\\\n";
  errs() << "\t--mode=csv \\\n";
  errs() << "\t--id-type=INTEGER \\\n";
  errs() << "\t--nodes:Module=" << ModOut << " \\\n";
  errs() << "\t--nodes:Function=" << FuncOut << " \\\n";
  errs() << "\t--nodes:Global=" << GlobalOut << " \\\n";
  errs() << "\t--nodes:Alias=" << AliasOut << " \\\n";
  errs() << "\t--nodes:Allexe=" << AllOut << " \\\n";
  errs() << "\t--relationships:CONTAINS=" << ContainsOut << " \\\n";
  errs() << "\t--relationships=" << ModGlobalsOut << " \n";

  errs() << "\n";
  errs() << "Be sure to stop the database and remove it beforehand...\n";

  return Error::success();
}

CommandRegistration Unused(&NeoCSV, [](ResourcePaths &RP) -> Error {
  errs() << "Scanning " << InputDirectory << "...\n";

  auto ExpDB = ABCDB::loadFromAllexesIn(InputDirectory, RP);
  if (!ExpDB)
    return ExpDB.takeError();
  auto &DB = *ExpDB;

  errs() << "Done! Allexes found: " << DB->allexe_size() << "\n";

  return neo(*DB, InputDirectory);
});

} // end anonymous namespace
