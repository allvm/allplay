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

  auto Groups = Functions
      // Indirection so copies are cheaper (only needed because we concretize to vector for sorting)
      | ranges::view::transform([](auto &F){ return &F; })
      // Group by hash
      | ranges::view::group_by([](auto *A, auto *B) { return A->H == B->H; })
      // Remove singleton groups
      | ranges::view::remove_if([](auto A) { return ranges::distance(A) == 1; })
      // Sort by group size, largest first.
      | ranges::to_vector
      | ranges::action::sort(std::greater<size_t>(), [](auto &A) { return ranges::distance(A); });

  RANGES_FOR(auto G, Groups) {
    errs() << "Group!\n";
    errs() << ranges::distance(G) << "\n";
    RANGES_FOR(auto F, G) {
      errs() << "Function: " << F->FuncName << "\n";
    }
  }

#if 0
  std::vector<FnVec> fnCollisions;

  for (auto &KV : hashToFns) {
    fnCollisions.push_back(KV.getSecond());
  }

  // sort by number of functions sharing a hash
  // (most frequent first)
  std::sort(fnCollisions.begin(), fnCollisions.end(),
            [](auto &A, auto &B) { return A.size() > B.size(); });

  size_t redundantInstsMaybe = 0;
  size_t redundantInstsLarger = 0;
  for (auto &fns : fnCollisions) {
    if (fns.size() == 1)
      continue;
    errs() << "----------\n";
    errs() << "Functions: " << fns.size() << "\n";

    // DenseSet<Module *> mods;
    // for (auto *F : fns)
    //   mods.insert(F->getParent());
    // errs() << "Modules: " << mods.size() << "\n";

    auto numInsts = countAllInsts(fns);

    errs() << "Insts: " << numInsts << "\n";
    auto instsPerFn = numInsts / fns.size();
    errs() << "InstsPerFn: " << instsPerFn << "\n";

    // This isn't apparently always true, but usually is
    // assert(instsPerFn * fns.size() == numInsts);

    redundantInstsMaybe += numInsts - instsPerFn;
    if (instsPerFn >= 20)
      redundantInstsLarger += numInsts - instsPerFn;

    for (auto &F : fns)
      errs() << F.Source << ": " << F.FuncName << "\n";

    // if this is non-trivial function, print it for my curiosity :)
    // if (instsPerFn > 50) {
    //   fns[0]->dump();
    //}

    // if (fns.size() > 50)
    // fns[0]->dump();
    // fns[1]->dump();
    // for (auto *F : fns) {
    //    F->dump();
    //}
  }

  errs() << "Total instructions in filtered DB: " << totalInsts << "\n";
  errs() << "Possibly redundant instructions: " << redundantInstsMaybe << "\n";
  errs() << "Ratio: "
         << format("%.4g", double(redundantInstsMaybe) / double(totalInsts))
         << "\n";
  errs() << "Possibly redundant instructions (in functions with >=20 insts): "
         << redundantInstsLarger << "\n";
  errs() << "Ratio (>=20): "
         << format("%.4g", double(redundantInstsLarger) / double(totalInsts))
         << "\n";
#endif

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
