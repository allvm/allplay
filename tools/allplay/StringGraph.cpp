#include "StringGraph.h"

#include <llvm/ADT/SmallVector.h>
#include <llvm/Support/Errc.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/FormatVariadic.h>
#include <llvm/Support/ToolOutputFile.h>
#include <llvm/Support/raw_ostream.h>

#include <range/v3/all.hpp>

using namespace allvm;
using namespace llvm;

Error StringGraph::writeGraph(StringRef F) {
  std::error_code EC;
  tool_output_file GraphFile(F, EC, sys::fs::OpenFlags::F_Text);
  if (EC)
    return make_error<StringError>("Unable to open file " + F, EC);

  errs() << "Writing graph to " << F << "...\n";

  auto &OS = GraphFile.os();

  OS << "digraph G {\n";
  // Graph properties
  OS << "rankdir=LR;\n";
  OS << "newrank=true;\n";
  OS << "overlap=false;\n";
  OS << "outputorder=edgesfirst;\n"; // Don't hide nodes
  // OS << "splines=true;\n";
  OS << "compound=true;\n";
  OS << "node [shape=record];\n";

  // auto Grouped = Nodes | ranges::view::group_by(GrpBy);

  // size_t GIdx = 0;
  // RANGES_FOR(auto Grp, Grouped) {
  //   if (useClusters) {
  // if (UseClusters) {
  //   OS << "subgraph cluster_" << GIdx++ << " {\n";
  //   OS << "labelloc = \"b\";\n";
  //   OS << "label = \"" << getGroup(*Grp.begin()) << "\";\n";
  //   }

  // }
  RANGES_FOR(auto N, Nodes | ranges::view::keys) {
    OS << formatv("Node {0} [{1}];\n", getNodeIndex(N), getNodeAttrs(N));
  }

  RANGES_FOR(auto E, Edges) {
    VertexID Src, Dst;
    llvm::StringRef Attrs;
    std::tie(Src, Dst, Attrs) = E;
    OS << formatv("Node {0} -> Node {1} [{2}];\n", Src, Dst, Attrs);
  }

  OS << "}\n";

  GraphFile.keep();

  return Error::success();
}

void StringGraph::addVertex(llvm::StringRef S, ArrayRef<StringAttr> attrs) {
  SmallVector<std::string, 4> AttrStrings;

  for (auto &A : attrs)
    AttrStrings.push_back(
        (StringRef(A.first) + "=\"" + StringRef(A.second) + "\"").str());

  return addVertex(S, llvm::join(AttrStrings.begin(), AttrStrings.end(), ";"));
}
