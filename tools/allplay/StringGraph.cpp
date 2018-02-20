#include "StringGraph.h"

#include <llvm/ADT/SmallVector.h>
#include <llvm/Support/Errc.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/FormatVariadic.h>
#include <llvm/Support/ToolOutputFile.h>
#include <llvm/Support/raw_ostream.h>

#include <range/v3/all.hpp>

using namespace allvm_analysis;
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

  RANGES_FOR(auto N, Nodes | ranges::view::keys) {
    auto attrs = getNodeAttrs(N);
    auto attrStr = attrs.empty() ? std::string() : formatv(" [{0}]", attrs);
    OS << formatv("Node{0}{1};\n", getNodeIndex(N), attrStr);
  }

  RANGES_FOR(auto E, Edges) {
    VertexID Src, Dst;
    llvm::StringRef Attrs;
    std::tie(Src, Dst, Attrs) = E;
    auto AttrStr = Attrs.empty() ? std::string() : formatv(" [{0}]", Attrs);
    OS << formatv("Node{0} -> Node{1};\n", Src, Dst, AttrStr);
  }

  OS << "}\n";

  GraphFile.keep();

  return Error::success();
}

std::string StringGraph::stringify(ArrayRef<StringAttr> attrs) {
  SmallVector<std::string, 4> AttrStrings;

  for (auto &A : attrs)
    AttrStrings.push_back(formatv("{0}=\"{1}\"", A.first, A.second));

  return llvm::join(AttrStrings.begin(), AttrStrings.end(), ";");
}
