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

  OS << "CREATE CONSTRAINT ON (m:Module) ASSERT m.CRC IS UNIQUE;\n";
  OS << "CREATE INDEX ON :Allexe(Name);\n";
  OS << "CREATE INDEX ON :Allexe(Path);\n";
  OS << "CREATE INDEX ON :Module(Name);\n";
  OS << "CREATE INDEX ON :Module(Path);\n";

  // Create module nodes
  size_t idx = 0;
  for (auto &M: DB.getMods()) {
    OS << "CREATE (:Module {";
    OS << "Name:\"" << basename(M.Filename) << "\", ";
    OS << "Path:\"" << removePrefix(M.Filename) << "\", ";
    OS << "CRC:" << M.ModuleCRC;
    OS  << "})\n";
    if (++idx == 500) {
      idx = 0;
      OS << ";\n";
    }
  }

  if (idx != 0) OS << ";\n\n";

  OS << "CALL db.awaitIndex(\":Module(CRC)\");\n";

  // allexe nodes
  for (auto &A: DB.getAllexes()) {
    OS << "CREATE (:Allexe {";
    OS << "Name:\"" << basename(A.Filename) << "\", ";
    OS << "Path:\"" << removePrefix(A.Filename) << "\"";
    OS  << "})\n";
  }

  OS << ";\n\n";

  // emit allexe -> module relationships
  idx = 0;
  for (auto &A: DB.getAllexes()) {
    OS << "MATCH (a:Allexe {Path:\"" << removePrefix(A.Filename) << "\"})\n";
    size_t i = 0;
    for (auto &M: A.Modules) {
      OS << "\tMATCH (m" << idx << ":Module {CRC:" << M.ModuleCRC << "})\n";
      OS << "\tCREATE UNIQUE (a)-[:CONTAINS {index:" << i++ << "}]->(m" << idx << ")\n";
      ++idx;
    }
    OS << ";\n";
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
