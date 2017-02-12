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
#include <llvm/XRay/Graph.h>

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

using VertexAttr = llvm::StringRef; // ?
using EdgeAttr = int;
using VertexID = size_t;
using AGraph = llvm::xray::Graph<VertexAttr, EdgeAttr, VertexID>;

Error graph(BCDB &DB, StringRef Prefix, StringRef GraphFilename) {

  AGraph G;

  errs() << "Building AllexeGraph...\n";
  StringMap<size_t> StringIndexMap;
  size_t N = 0;
  auto addVertex = [&](StringRef S) {
    assert(!StringIndexMap.count(S));
    assert(N == StringIndexMap.size());

    if (S.startswith(Prefix))
      S = S.drop_front(Prefix.size());
    StringIndexMap[S] = N;
    G[N] = S;
    ++N;
  };

  for (const auto &A : DB.getAllexes())
    addVertex(A.Filename);

  for (const auto &M : DB.getMods())
    addVertex(M.Filename);

  errs() << "Adding edges..\n";
  for (const auto &A : DB.getAllexes()) {
    for (const auto &M : A.Modules) {
      auto V1 = StringIndexMap[A.Filename];
      auto V2 = StringIndexMap[M.Filename];
      G[{V1, V2}] = EdgeAttr{0};
    }
  }

  std::error_code EC;
  tool_output_file GraphFile(GraphFilename, EC, sys::fs::OpenFlags::F_Text);
  if (EC)
    return make_error<StringError>("Unable to open file " + GraphFilename, EC);

  errs() << "Writing graph to " << GraphFilename << "\n";
  auto &OS = GraphFile.os();

  // Ty to llvm-xray code for this...
  OS << "digraph allexes {\n";
  OS << "rankdir=LR;\n";
  OS << "newrank = true;\n";
  OS << "node [shape=record];\n";

  errs() << "edges...\n";
  for (const auto &E : G.edges()) {
    OS << "A" << E.first.first << " -> "
       << "A" << E.first.second << ";\n";
  }
  errs() << "nodes...\n";

  for (const auto &V : G.vertices()) {
    OS << "A" << V.first << " [label=\"" << V.second << "\"];\n";
  }

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
