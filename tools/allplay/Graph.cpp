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

#include "allvm/BCDB.h"
#include "allvm/ResourcePaths.h"

#include <llvm/ADT/DenseMap.h>
#include <llvm/ADT/DenseSet.h>
#include <llvm/ADT/StringMap.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/Errc.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/ThreadPool.h>
#include <llvm/Support/ToolOutputFile.h>
#include <llvm/Support/raw_ostream.h>

#include <range/v3/all.hpp>

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

Error graph(BCDB &DB, StringRef Prefix, StringRef GraphFilename) {

  auto removePrefix = [Prefix](StringRef S) {
    if (S.startswith(Prefix))
      S = S.drop_front(Prefix.size());
    StringRef NixStorePrefix = "/nix/store/";
    if (S.startswith(NixStorePrefix))
      S = S.drop_front(NixStorePrefix.size());
    return S;
  };

  errs() << "Building AllexeGraph...\n";
  StringMap<size_t> StringIndexMap;
	std::vector<StringRef> Nodes;
  size_t N = 0;
  auto addVertex = [&](StringRef S) {
    assert(!StringIndexMap.count(S));
    assert(N == StringIndexMap.size());

    StringIndexMap[S] = N;
		Nodes.push_back(S);
    ++N;
  };

  for (const auto &A : DB.getAllexes())
    addVertex(removePrefix(A.Filename));

  for (const auto &M : DB.getMods())
    addVertex(removePrefix(M.Filename));

  errs() << "Adding edges...\n";
	using Edge = std::pair<size_t, size_t>;
	std::vector<Edge> Edges;
  for (const auto &A : DB.getAllexes()) {
    for (const auto &M : A.Modules) {
      auto V1 = StringIndexMap[removePrefix(A.Filename)];
      auto V2 = StringIndexMap[removePrefix(M.Filename)];
			Edges.push_back({V1, V2});
    }
  }

  std::error_code EC;
  tool_output_file GraphFile(GraphFilename, EC, sys::fs::OpenFlags::F_Text);
  if (EC)
    return make_error<StringError>("Unable to open file " + GraphFilename, EC);

  errs() << "Writing graph to " << GraphFilename << "\n";
  auto &OS = GraphFile.os();

  OS << "digraph allexes {\n";
  OS << "rankdir=LR;\n";
  OS << "newrank=true;\n";
  OS << "overlap=false;\n";
  OS << "splines=true;\n";
  OS << "compound=true;\n";
  OS << "node [shape=record];\n";

  auto getGroup = [&] (StringRef S) {
    return removePrefix(S).split('/').first;
  };

	Nodes |= ranges::action::sort(std::less<StringRef>{}, getGroup);

	auto Grouped = Nodes | ranges::view::group_by([&getGroup](auto a, auto b){
		return getGroup(a) == getGroup(b);
	});

  size_t GIdx = 0;
	RANGES_FOR (auto G, Grouped) {
    OS << "subgraph cluster_" << GIdx++ << " {\n";
    OS << "labelloc = \"b\";\n";
    OS << "label = \"" << getGroup(*G.begin()) << "\";\n";

		RANGES_FOR(auto GN, G)
      OS << "Node" << StringIndexMap[GN] << " [label=\"" << GN.split('/').second << "\"];\n";
    OS << "}\n";
	}

  for(auto &E: Edges)
    OS << "Node" << E.first << " -> Node" << E.second << "\n";

 // std::sort(Nodes.begin(), Nodes.end(), [&](auto &A, auto &B) {
 //    return getGroup(G[A]) < getGroup(G[B]);
 // });

#if 0
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
#endif

  //for (const auto &V : G.vertices()) {
  //  OS << "A" << V.first << " [label=\"" << V.second << "\"];\n";
  //}

  OS << "}\n";

  GraphFile.keep();

  return Error::success();
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
