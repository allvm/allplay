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

#include "allvm/BCDB.h"
#include "allvm/ResourcePaths.h"

#include <llvm/Support/raw_ostream.h>


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


Error graph(BCDB &DB, StringRef Prefix, StringRef GraphFilename) {

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

  for (const auto &A : DB.getAllexes())
    G.addVertex(removePrefix(A.Filename));

  for (const auto &M : DB.getMods())
    G.addVertex(removePrefix(M.Filename));

  errs() << "Adding edges...\n";
  for (const auto &A : DB.getAllexes()) {
    for (const auto &M : A.Modules) {
      G.addEdge(removePrefix(A.Filename), removePrefix(M.Filename));
    }
  }

  return G.writeGraph(GraphFilename);

#if 0

  auto getGroup = [&](StringRef S) { return removePrefix(S).split('/').first; };

  auto Grouped = G.nodes() | ranges::view::group_by([&getGroup](auto a, auto b) {
                   return getGroup(a) == getGroup(b);
                 });

  size_t GIdx = 0;
  RANGES_FOR(auto Grp, Grouped) {
    if (UseClusters) {
      OS << "subgraph cluster_" << GIdx++ << " {\n";
      OS << "labelloc = \"b\";\n";
      OS << "label = \"" << getGroup(*Grp.begin()) << "\";\n";
    }

    RANGES_FOR(auto GN, Grp)
    OS << "Node" << G.getNodeIndex(GN) << " [label=\"" << GN.split('/').second
       << "\"];\n";
    if (UseClusters)
      OS << "}\n";
  }

  for (auto &E : G.edges())
    OS << "Node" << E.first << " -> Node" << E.second << "\n";

// std::sort(Nodes.begin(), Nodes.end(), [&](auto &A, auto &B) {
//    return getGroup(G[A]) < getGroup(G[B]);
// });

  for (const auto &A : DB.getAllexes()) {
    OS << "subgraph cluster_" << AIdx << " {\n";
    OS << "labelloc = \"b\";\n";
    OS << "label = \"" << L << "\";\n";
    OS << "}\n";
  }


  errs() << "edges...\n";
  for (const auto &E : G.edges()) {
    OS << "A" << E.first.first << " -> "
       << "A" << E.first.second << ";\n";
  }
  errs() << "nodes...\n";

  // for (const auto &V : G.vertices()) {
  //  OS << "A" << V.first << " [label=\"" << V.second << "\"];\n";
  //}

  OS << "}\n";

  GraphFile.keep();

  return Error::success();
#endif
}

CommandRegistration Unused(&Graph, [](ResourcePaths &RP) -> Error {
  errs() << "Loading allexe's from " << InputDirectory << "...\n";
  auto ExpDB = BCDB::loadFromAllexesIn(InputDirectory, RP);
  if (!ExpDB)
    return ExpDB.takeError();
  auto &DB = *ExpDB;
  errs() << "Done! Allexes found: " << DB->allexe_size() << "\n";

  return graph(*DB, InputDirectory, OutputFilename);
});

} // end anonymous namespace
