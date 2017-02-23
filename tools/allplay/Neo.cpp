#include "subcommand-registry.h"

#include "allvm/BCDB.h"
#include "allvm/ModuleFlags.h"

#include <llvm/Support/FileSystem.h>
#include <llvm/Support/ToolOutputFile.h>

using namespace llvm;
using namespace allvm;

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
    ContainsOut("contains", cl::init("contains.csv"),
                cl::desc("name of file to write contains module node data"),
                cl::sub(NeoCSV));

Error neo(BCDB &DB, StringRef Prefix) {
  std::error_code EC;
  tool_output_file ModOutFile(ModOut, EC, sys::fs::OpenFlags::F_Text);
  if (EC)
    return make_error<StringError>("Unable to open file " + ModOut, EC);
  auto &ModS = ModOutFile.os();
  tool_output_file AllOutFile(AllOut, EC, sys::fs::OpenFlags::F_Text);
  if (EC)
    return make_error<StringError>("Unable to open file " + AllOut, EC);
  auto &AllS = AllOutFile.os();
  tool_output_file ContainsOutFile(ContainsOut, EC, sys::fs::OpenFlags::F_Text);
  if (EC)
    return make_error<StringError>("Unable to open file " + ContainsOut, EC);
  auto &ContainS = ContainsOutFile.os();

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

  // Create module nodes
  ModS << "CRC:ID(Module),Name,Path\n";
  for (auto &M : DB.getMods()) {
    ModS << M.ModuleCRC << "," << basename(M.Filename) << ","
         << removePrefix(M.Filename) << "\n";
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

  ModOutFile.keep();
  AllOutFile.keep();
  ContainsOutFile.keep();

  errs() << "Import using 'neo4j-admin' command, something like:\n";
  errs() << "\n";

  errs() << "sudo NEO4J_CONF=/var/lib/neo4j/conf \\\n";
  errs() << "neo4j-admin import\\\n";
  errs() << "\t--mode=csv \\\n";
  errs() << "\t--id-type=INTEGER \\\n";
  errs() << "\t--nodes:Module=" << ModOut << " \\\n";
  errs() << "\t--nodes:Allexe=" << AllOut << " \\\n";
  errs() << "\t--relationships:CONTAINS=" << ContainsOut << " \n";

  errs() << "\n";
  errs() << "Be sure to stop the database and remove it beforehand...\n";

  return Error::success();
}

CommandRegistration Unused(&NeoCSV, [](ResourcePaths &RP) -> Error {
  errs() << "Scanning " << InputDirectory << "...\n";

  auto ExpDB = BCDB::loadFromAllexesIn(InputDirectory, RP);
  if (!ExpDB)
    return ExpDB.takeError();
  auto &DB = *ExpDB;

  errs() << "Done! Allexes found: " << DB->allexe_size() << "\n";

  return neo(*DB, InputDirectory);
});

} // end anonymous namespace
