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

using FunctionHash = FunctionComparator::FunctionHash;

struct FuncDesc {
  ModuleInfo *Mod;
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

Error functionHash(BCDB &DB) {

  errs() << "Materializing and computing function hashes...\n";
  size_t totalInsts = 0;
  std::vector<FuncDesc> Functions;

  for (auto MI : DB.getMods()) {
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
      Functions.push_back(FuncDesc{&MI, F.getName(), countInsts(&F), MI.Filename, H});
    }

    totalInsts += countInsts(M.get());
  }

  errs() << "Hashes computed, grouping...\n";

  // Sort by hash
  Functions |= ranges::action::sort(std::less<FunctionHash>{}, &FuncDesc::H);

  auto Groups = Functions
      // Indirection so copies are cheaper (only useful because we concretize to vector for sorting)
      | ranges::view::transform([](auto &F){ return &F; })
      // Group by hash
      | ranges::view::group_by([](auto *A, auto *B) { return A->H == B->H; })
      // Remove singleton groups
      | ranges::view::remove_if([](auto A) { return ranges::distance(A) == 1; })
      // Sort by group size, largest first.
      | ranges::to_vector
      | ranges::action::sort(std::greater<size_t>(), [](auto &A) { return ranges::distance(A); });

  size_t redundantInstsMaybe = 0;
  RANGES_FOR(auto G, Groups) {
    errs() << "-----------\n";
    auto numFns = ranges::distance(G);
    errs() << "Function Group, count: " << numFns << "\n";

    auto instCount = [](auto Fns) { return ranges::accumulate(Fns | ranges::view::transform(&FuncDesc::Insts), size_t{0}); };

    auto numInsts = instCount(G);
    errs() << "Insts: " << numInsts << "\n";
    assert(numFns > 0);
    errs() << "InstsPerFn: " << numInsts/size_t(numFns) << "\n";
    auto numInstsSkipFirst = instCount(G|ranges::view::drop(1));
    redundantInstsMaybe += numInstsSkipFirst;
    RANGES_FOR(auto F, G) {
      errs() << F->Source << ": " << F->FuncName << "\n";
    }
  }

  errs() << "Total instructions in filtered DB: " << totalInsts << "\n";
  errs() << "Possibly redundant instructions: " << redundantInstsMaybe << "\n";
  errs() << "Ratio: "
         << format("%.4g", double(redundantInstsMaybe) / double(totalInsts))
         << "\n";

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
