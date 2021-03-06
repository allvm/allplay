//===-- Graph.cpp ---------------------------------------------------------===//
//
// Author: Will Dietz (WD), wdietz2@uiuc.edu
//
//===----------------------------------------------------------------------===//
//
// Misc Graph construction and analysis
//
//===----------------------------------------------------------------------===//

#include "subcommand-registry.h"

#include "StringGraph.h"

#include "allvm-analysis/ABCDB.h"

#include <allvm/ResourcePaths.h>

#include <llvm/Support/raw_ostream.h>

using namespace allvm_analysis;
using namespace allvm;
using namespace llvm;

namespace {

cl::SubCommand Graph("graph", "produce bcdb graph (NOT IMPLEMENTED YET)");
cl::opt<std::string> InputDirectory(cl::Positional, cl::Required,
                                    cl::desc("<input directory to scan>"),
                                    cl::sub(Graph));
cl::opt<std::string> OutputFilename("o", cl::Required,
                                    cl::desc("name of file to write graph"),
                                    cl::sub(Graph));
cl::opt<bool> UseClusters("cluster", cl::Optional,
                          cl::desc("Emit nodes in clusters"), cl::sub(Graph));

Error graph(ABCDB &DB, StringRef Prefix, StringRef GraphFilename) {

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

  errs() << "Building AllexeGraph...\n";

  StringGraph G;

  auto getLabel = [&](StringRef S) {
    return removePrefix(S).split('/').second;
  };
  auto addVertex = [&](StringRef S) {
    S = removePrefix(S);
    G.addVertexWithLabel(S, getLabel(S));
  };
  for (const auto &A : DB.getAllexes())
    addVertex(A.Filename);

  for (const auto &M : DB.getMods())
    addVertex(M.Filename);

  errs() << "Adding edges...\n";
  for (const auto &A : DB.getAllexes()) {
    for (const auto &M : A.Modules) {
      G.addEdge(removePrefix(A.Filename), removePrefix(M.Filename));
    }
  }

  return G.writeGraph(GraphFilename);
}

CommandRegistration Unused(&Graph, [](ResourcePaths &RP) -> Error {
  errs() << "Loading allexe's from " << InputDirectory << "...\n";
  auto ExpDB = ABCDB::loadFromAllexesIn(InputDirectory, RP);
  if (!ExpDB)
    return ExpDB.takeError();
  auto &DB = *ExpDB;
  errs() << "Done! Allexes found: " << DB->allexe_size() << "\n";

  return graph(*DB, InputDirectory, OutputFilename);
});

} // end anonymous namespace
