#include "StringGraph.h"

#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Errc.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/ToolOutputFile.h>
#include <llvm/Support/raw_ostream.h>

#include <range/v3/all.hpp>

using namespace allvm;
using namespace llvm;

Error StringGraph::writeGraph(StringRef F, StrFn getLabel, StrFn getGroup LLVM_ATTRIBUTE_UNUSED) {
    std::error_code EC;
    tool_output_file GraphFile(F, EC, sys::fs::OpenFlags::F_Text);
    if (EC)
      return make_error<StringError>("Unable to open file " + F, EC);

    errs() << "Writing graph to " << F << "..\n";

    auto &OS = GraphFile.os();

    OS << "digraph G {\n";
    // Graph properties
    OS << "rankdir=LR;\n";
    OS << "newrank=true;\n";
    OS << "overlap=false;\n";
    OS << "splines=true;\n";
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
    RANGES_FOR(auto N, Nodes) {
      OS << "Node" << getNodeIndex(N) << " [label=\"" << getLabel(N) << "\"];\n";
    }

    RANGES_FOR(auto E, Edges) {
      OS << "Node" << E.first << " -> " << "Node" << E.second << ";\n";
    }

    OS << "}\n";

    GraphFile.keep();

    return Error::success();
}

