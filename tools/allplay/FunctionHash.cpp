#include "subcommand-registry.h"

#include "StringGraph.h"

#include "allvm/BCDB.h"
#include "allvm/ModuleFlags.h"

#include <llvm/ADT/DenseMap.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/Errc.h>
#include <llvm/Support/Format.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Transforms/Utils/FunctionComparator.h>

#include <range/v3/all.hpp>

#include <algorithm>
#include <functional>
#include <numeric>

using namespace llvm;
using namespace allvm;

namespace {

cl::SubCommand FunctionHashes("functionhashes",
                              "Analyze basic hashes of functions");
cl::opt<std::string> InputDirectory(cl::Positional, cl::Required,
                                    cl::desc("<input directory to scan>"),
                                    cl::sub(FunctionHashes));
cl::opt<std::string>
    WriteGraph("write-graph", cl::Optional,
               cl::desc("name of file to write graph, does nothing if empty"),
               cl::init(""), cl::sub(FunctionHashes));
cl::opt<bool>
    PrintFunctions("print-functions", cl::Optional, cl::init(true),
                   cl::desc("Print functions grouped by hash (default=true)"),
                   cl::sub(FunctionHashes));

cl::opt<unsigned> GraphThreshold(
    "graph-threshold", cl::Optional, cl::init(2000),
    cl::desc("Threshold for including in graph, by insts-per-fn"),
    cl::sub(FunctionHashes));

using FunctionHash = FunctionComparator::FunctionHash;

struct FuncDesc {
  const ModuleInfo *Mod;
  std::string FuncName;
  size_t Insts;
  std::string Source;
  FunctionHash H;
};

template <typename T> auto countInsts(const T *V) {
  return std::accumulate(
      V->begin(), V->end(), size_t{0},
      [](auto N, auto &child) { return N + countInsts(&child); });
}
template <> auto countInsts(const BasicBlock *B) { return B->size(); }
// auto countInsts(const FuncDesc &FD) { return FD.Insts; }

// Meant for containers, so named differently.
template <typename T> auto countAllInsts(const T &Vs) {
  return std::accumulate(Vs.begin(), Vs.end(), size_t{0},
                         [](auto N, auto &V) { return N + countInsts(V); });
}

auto instCount = [](auto Fns) {
  return ranges::accumulate(Fns | ranges::view::transform(&FuncDesc::Insts),
                            size_t{0});
};

auto group_ptr_by_hash() {
  return ranges::view::group_by([](auto *A, auto *B) { return A->H == B->H; });
}

auto group_by_hash() {
  return ranges::view::group_by([](auto &A, auto &B) { return A.H == B.H; });
}

auto to_ptr() {
  return ranges::view::transform([](auto &F) { return &F; });
}

auto filter_small_ranges(ssize_t n) {
  return ranges::view::remove_if(
      [=](auto A) { return ranges::distance(A) <= n; });
}

auto filter_by_inst_count(ssize_t n) {
  return ranges::view::remove_if([=](auto A) {
    return (ssize_t(instCount(A)) / ranges::distance(A)) < n;
  });
}

auto sort_by_range_size() {
  return ranges::action::sort(std::greater<ssize_t>(),
                              [](auto &A) { return ranges::distance(A); });
}

auto to_vec_sort_uniq() {
  return ranges::to_vector | ranges::action::sort | ranges::action::unique;
}

Error functionHash(BCDB &DB) {

  errs() << "Materializing and computing function hashes...\n";
  size_t totalInsts = 0;
  std::vector<FuncDesc> Functions;

  for (auto &MI : DB.getMods()) {
    SMDiagnostic SM;
    LLVMContext C;
    auto M = llvm::parseIRFile(MI.Filename, SM, C);
    if (!M)
      return make_error<StringError>(
          "Unable to open module file " + MI.Filename, errc::invalid_argument);
    if (auto Err = M->materializeAll())
      return Err;
    for (auto &F : *M) {
      if (F.isDeclaration())
        continue;
      auto H = FunctionComparator::functionHash(F);

      // errs() << "Hash for '" << F.getName() << "': " << H << "\n";
      Functions.push_back(
          FuncDesc{&MI, F.getName(), countInsts(&F), MI.Filename, H});
    }

    totalInsts += countInsts(M.get());
  }

  errs() << "Hashes computed, grouping...\n";

  // Sort by hash
  Functions |= ranges::action::sort(std::less<FunctionHash>{}, &FuncDesc::H);

  if (PrintFunctions) {
    auto Groups =
        Functions
        // Indirection so copies are cheaper (only useful because we concretize
        // to vector for sorting)
        | to_ptr()
        // Group by hash
        | group_ptr_by_hash()
        // Remove singleton groups
        | filter_small_ranges(1)
        // Sort by group size, largest first.
        | ranges::to_vector | sort_by_range_size();

    size_t redundantInstsMaybe = 0;
    RANGES_FOR(auto G, Groups) {
      errs() << "-----------\n";
      auto numFns = ranges::distance(G);
      errs() << "Function Group, count: " << numFns << "\n";

      auto numInsts = instCount(G);
      errs() << "Insts: " << numInsts << "\n";
      assert(numFns > 0);
      errs() << "InstsPerFn: " << numInsts / size_t(numFns) << "\n";
      auto numInstsSkipFirst = instCount(G | ranges::view::drop_exactly(1));
      redundantInstsMaybe += numInstsSkipFirst;
      RANGES_FOR(auto F, G) {
        errs() << F->Source << ": " << F->FuncName << "\n";
      }
    }

    errs() << "Total instructions in filtered DB: " << totalInsts << "\n";
    errs() << "Possibly redundant instructions: " << redundantInstsMaybe
           << "\n";
    errs() << "Ratio: "
           << format("%.4g", double(redundantInstsMaybe) / double(totalInsts))
           << "\n";
  }

  if (!WriteGraph.empty()) {
    errs() << "Writing FunctionHash Graph...\n";

    StringGraph Graph;

    auto SharedFunctions =
        Functions | group_by_hash() | filter_small_ranges(1) |
        filter_by_inst_count(GraphThreshold) | ranges::view::join;

    auto ModHashPairs =
        SharedFunctions | ranges::view::transform([](const auto &FD) {
          return std::pair<StringRef, FunctionHash>{FD.Source, FD.H};
        }) |
        to_vec_sort_uniq();

    auto Mods = ModHashPairs | ranges::view::keys | to_vec_sort_uniq();
    auto Hashes = ModHashPairs | ranges::view::transform([](const auto &A) {
                    return Twine(A.second).str();
                  }) |
                  to_vec_sort_uniq();

    auto getModLabel = [](StringRef S) { return S.rsplit('/').second; };
    RANGES_FOR(auto &M, Mods) {
      Graph.addVertex(M, {{"label", getModLabel(M)},
                          {"style", "filled"},
                          {"fillcolor", "cyan"}});
    }
    RANGES_FOR(auto &H, Hashes) {
      // Graph.addVertex(H,{{"label","(hash)"}});
      Graph.addVertexWithLabel(H, "hash");
    }
    RANGES_FOR(auto &MH, ModHashPairs) {
      Graph.addEdge(MH.first, Twine(MH.second).str());
    }

#if 0
    auto NamedFunctions = SharedFunctions | ranges::view::transform([](auto &F) {
                            return std::pair<const FuncDesc *, std::string>{
                                &F, F.FuncName + "\\n" + F.Source};
                          }) |
                          ranges::to_vector;
    auto Hashes = SharedFunctions | ranges::view::transform(&FuncDesc::H) |
                  ranges::to_vector | ranges::action::sort |
                  ranges::action::unique;
    auto StrHashes =
        Hashes |
        ranges::view::transform([](auto H) { return Twine(H).str(); }) |
        ranges::to_vector;

    RANGES_FOR(const auto &H, StrHashes) { Graph.addVertex(H); }

    RANGES_FOR(const auto &NF, NamedFunctions) {
     // Graph.addVertex(NF.second);

     // Graph.addEdge(NF.first->Source, NF.second);
     // Graph.addEdge(NF.second, Twine(NF.first->H).str());
     //  Graph.addEdge(NF.first->Source, Twine(NF.first->H).str());

    }
#endif

    return Graph.writeGraph(WriteGraph);

    // Module -> (Source: Function)
  }

  return Error::success();
}

CommandRegistration Unused(&FunctionHashes, [](ResourcePaths &RP) -> Error {
  errs() << "Loading allexe's from " << InputDirectory << "...\n";
  auto ExpDB = BCDB::loadFromAllexesIn(InputDirectory, RP);
  if (!ExpDB)
    return ExpDB.takeError();
  auto &DB = *ExpDB;
  errs() << "Done! Allexes found: " << DB->allexe_size() << "\n";

  return functionHash(*DB);
});

} // end anonymous namespace
