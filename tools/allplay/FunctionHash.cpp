#include "subcommand-registry.h"

#include "StringGraph.h"
#include "boost_progress.h"

#include "allvm/BCDB.h"
#include "allvm/ModuleFlags.h"

#include <llvm/ADT/DenseMap.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/Errc.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Format.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/ToolOutputFile.h>
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
    PrintFunctions("print-functions", cl::Optional, cl::init(false),
                   cl::desc("Print functions grouped by hash (default=false)"),
                   cl::sub(FunctionHashes));

cl::opt<unsigned> GraphThreshold(
    "graph-threshold", cl::Optional, cl::init(2000),
    cl::desc("Threshold for including in graph, by insts-per-fn"),
    cl::sub(FunctionHashes));
cl::opt<unsigned>
    MinFontSize("min-font-size", cl::Optional, cl::init(12),
                cl::desc("Minimum (starting) font size for nodes"),
                cl::sub(FunctionHashes));
cl::opt<bool> UseLogSize(
    "use-log-size", cl::Optional, cl::init(true),
    cl::desc("Size graph nodes by log(count) instead of linear count"),
    cl::sub(FunctionHashes));

cl::opt<std::string> WriteCSV("write-csv", cl::Optional, cl::init(""),
                              cl::sub(FunctionHashes));
cl::opt<bool> UseBCScanner("bc-scanner", cl::Optional, cl::init(false),
                           cl::desc("Use BC scanner instead of allexe scanner"),
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

auto group_by_module() {
  return ranges::view::group_by(
      [](auto &A, auto &B) { return A.Source == B.Source; });
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

  boost::progress_display progress(DB.getMods().size());
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
    ++progress;
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
      auto numInstsSkipFirst = instCount(G | ranges::view::tail);
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

    auto SharedFunctions = Functions | group_by_hash() |
                           filter_small_ranges(1) |
                           filter_by_inst_count(GraphThreshold) |
                           ranges::view::join | ranges::to_vector;

    auto ModHashPairs =
        SharedFunctions | ranges::view::transform([](const auto &FD) {
          return std::pair<StringRef, FunctionHash>{FD.Source, FD.H};
        }) |
        to_vec_sort_uniq();

    auto ModGroups =
        SharedFunctions | ranges::to_vector |
        ranges::action::sort(std::less<StringRef>(), &FuncDesc::Source);
    auto HashGroups =
        SharedFunctions | ranges::to_vector |
        ranges::action::sort(std::less<FunctionHash>(), &FuncDesc::H);

    auto getModLabel = [](StringRef S) { return S.rsplit('/').second; };
    RANGES_FOR(auto M, ModGroups | group_by_module()) {
      auto Count = ranges::distance(M);
      // Don't bother with fractional font sizes
      auto SizeAddend =
          static_cast<size_t>(UseLogSize ? std::log(Count) : Count);
      auto Source = M.begin()->Source;
      Graph.addVertex(Source,
                      {{"label", getModLabel(Source)},
                       {"style", "filled"},
                       {"fontsize", Twine(MinFontSize + SizeAddend).str()},
                       {"fillcolor", "cyan"}});
    }

    RANGES_FOR(auto H, HashGroups | group_by_hash()) {
      auto Count = ranges::distance(H);
      auto SizeAddend =
          static_cast<size_t>(UseLogSize ? std::log(Count) : Count);
      auto CountStr = Twine(Count).str();
      auto Size = Twine(MinFontSize + SizeAddend).str();
      auto HStr = Twine(H.begin()->H).str();
      Graph.addVertex(
          HStr, {{"label", CountStr}, {"fontsize", Size}, {"shape", "circle"}});
    }

    RANGES_FOR(auto &MH, ModHashPairs) {
      Graph.addEdge(MH.first, Twine(MH.second).str());
    }

    return Graph.writeGraph(WriteGraph);
  }

  if (!WriteCSV.empty()) {
    std::error_code EC;
    tool_output_file CSVFile(WriteCSV, EC, sys::fs::OpenFlags::F_Text);
    if (EC)
      return make_error<StringError>("Unable to open file " + WriteCSV, EC);
    errs() << "Writing CSV data to " << WriteCSV << "...\n";

    auto &OS = CSVFile.os();

    OS << "Source,FuncName,Insts,Hash\n";
    RANGES_FOR(const auto &Row, Functions) {
      OS << Row.Source << "," << Row.FuncName << "," << Row.Insts << ","
         << "H" << Row.H << "\n";
    }

    CSVFile.keep();
    errs() << "Done!\n";
  }

  return Error::success();
}

CommandRegistration Unused(&FunctionHashes, [](ResourcePaths &RP) -> Error {
  errs() << "Scanning " << InputDirectory << "...\n";

  auto ExpDB = UseBCScanner ? BCDB::loadFromBitcodeIn(InputDirectory, RP)
                            : BCDB::loadFromAllexesIn(InputDirectory, RP);
  if (!ExpDB)
    return ExpDB.takeError();
  auto &DB = *ExpDB;

  errs() << "Done! Allexes found: " << DB->allexe_size() << "\n";
  errs() << "Done! Modules found: " << DB->getMods().size() << "\n";

  return functionHash(*DB);
});

} // end anonymous namespace
