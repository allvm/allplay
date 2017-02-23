#include "subcommand-registry.h"

#include "allvm/BCDB.h"
#include "allvm/ModuleFlags.h"

#include <llvm/Support/FileSystem.h>
#include <llvm/Support/ToolOutputFile.h>

using namespace llvm;
using namespace allvm;

namespace {

cl::SubCommand Cypher("cypher",
                              "Create cypher queries for allexe data");
cl::opt<std::string> InputDirectory(cl::Positional, cl::Required,
                                    cl::desc("<input directory to scan>"),
                                    cl::sub(Cypher));
cl::opt<std::string>
    Output("o", cl::Required,
               cl::desc("name of file to write Cypher query for allexe data"),
               cl::sub(Cypher));

Error cypher(BCDB &DB, StringRef Prefix, StringRef OutputFilename) {
  std::error_code EC;
  tool_output_file OutFile(OutputFilename, EC, sys::fs::OpenFlags::F_Text);
  if (EC)
    return make_error<StringError>("Unable to open file " + OutputFilename, EC);
  auto &OS = OutFile.os();

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

  auto basename = [](StringRef S) {
    return S.rsplit('/').second;
  };

  // Create module nodes
  // For now, assume index/etc already exists on :Module(Path)
  for (auto &M: DB.getMods()) {
    OS << "MERGE (:Module {";
    OS << "Name:\"" << basename(M.Filename) << "\", ";
    OS << "Path:\"" << removePrefix(M.Filename) << "\", ";
    OS << "CRC:" << M.ModuleCRC;
    OS  << "});\n";
  }

  OS << "\n";

  // allexe nodes
  for (auto &A: DB.getAllexes()) {
    OS << "MERGE (:Allexe {";
    OS << "Name:\"" << basename(A.Filename) << "\", ";
    OS << "Path:\"" << removePrefix(A.Filename) << "\"";
    OS  << "});\n";
  }

  // emit allexe -> module relationships
  for (auto &A: DB.getAllexes()) {
    // OS << "MATCH (a:Allexe {Path:\"" << A.Filename << "\"})\n";
    size_t i = 0;
    for (auto &M: A.Modules) {
      OS << "MATCH (a:Allexe {Path:\"" << removePrefix(A.Filename) << "\"})\n";
      OS << "MATCH (m:Module {CRC:" << M.ModuleCRC << "})\n";
      OS << "MERGE (a)-[:CONTAINS {index:" << i++ << "}]->(m);\n";
      //OS << "\tCREATE UNIQUE (a)-[:CONTAINS]->(:Module {CRC:" << M.ModuleCRC << "})\n";
    }
    //OS << ";\n";
  }

  OutFile.keep();

  return Error::success();
}

CommandRegistration Unused(&Cypher, [](ResourcePaths &RP) -> Error {
  errs() << "Scanning " << InputDirectory << "...\n";

  auto ExpDB = BCDB::loadFromAllexesIn(InputDirectory, RP);
  if (!ExpDB)
    return ExpDB.takeError();
  auto &DB = *ExpDB;

  errs() << "Done! Allexes found: " << DB->allexe_size() << "\n";

  return cypher(*DB, InputDirectory, Output);
});

} // end anonymous namespace
