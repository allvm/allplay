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
#include <llvm/ADT/StringSet.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/Errc.h>
#include <llvm/Support/ThreadPool.h>
#include <llvm/Support/raw_ostream.h>

// boost
//#pragma clang diagnostic push
//#pragma clang diagnostic ignored "-Weverything"
//#include <boost/graph/adjacency_list.hpp>
//#include <boost/graph/graph_traits.hpp>
//#include <boost/graph/topological_sort.hpp>
//#pragma clang diagnostic pop

using namespace allvm;
using namespace llvm;

namespace {

// using Graph = boost::adjacency_list<>;
// using vertex_t = Graph::vertex_descriptor;
// using edge_t = Graph::edge_descriptor;

struct AllexeVertex {};

cl::SubCommand Graph("graph", "produce bcdb graph (NOT IMPLEMENTED YET)");
cl::opt<std::string> InputDirectory(cl::Positional, cl::Required,
                                    cl::desc("<input directory to scan>"),
                                    cl::sub(Graph));
cl::opt<std::string> OutputFilename("o", cl::Required,
                                    cl::desc("name of file to write graph"),
                                    cl::sub(Graph));

CommandRegistration Unused(&Graph, [](ResourcePaths &) -> Error {
  return make_error<StringError>("'graph' subcommand not implemented yet!",
                                 errc::function_not_supported);
});

} // end anonymous namespace
