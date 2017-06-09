#include "subcommand-registry.h"

// Preserve insert order
#define CPPTOML_USE_MAP
#include "cpptoml.h"

#include "allvm/BCDB.h"

#include <llvm/ADT/DenseSet.h>
#include <llvm/IR/CallSite.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/Errc.h>
#include <llvm/Support/Format.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/raw_ostream.h>

using namespace allvm;
using namespace llvm;

namespace {

cl::SubCommand
    FindDirectUses("finddirectuses",
                   "Search modules for uses of function, direct calls only");

cl::opt<std::string> InputDirectory(cl::Positional, cl::Required,
                                    cl::desc("<input directory to scan>"),
                                    cl::sub(FindDirectUses));
cl::opt<std::string> FuncName(cl::Positional, cl::Required,
                              cl::desc("<name of function>"),
                              cl::sub(FindDirectUses));

std::string toStr(const llvm::Value *V) {
  std::string S;
  raw_string_ostream OS(S);
  OS << *V;
  return S;
}

Error findUses(BCDB &DB, llvm::StringRef Symbol) {

  errs() << "Finding uses of '" << Symbol << "' in BCDB...\n";

  errs() << "Err, looking for users of function with that name\n";

  DenseSet<decltype(ModuleInfo::ModuleCRC)> ModulesWithReference;

  auto root = cpptoml::make_table();
  for (auto MI : DB.getMods()) {
    SMDiagnostic SM;
    LLVMContext C;
    auto M = llvm::parseIRFile(MI.Filename, SM, C);
    if (!M)
      return make_error<StringError>(
          "Unable to open module file " + MI.Filename, errc::invalid_argument);

    if (auto Err = M->materializeAll())
      return Err;

    if (auto *F = M->getFunction(Symbol)) {
      assert(F->isDeclaration());
      assert(F->hasNUsesOrMore(1));

      assert(!F->hasAddressTaken());

      auto call_table = cpptoml::make_table();
      for (auto U : F->users()) {
        auto *I = dyn_cast<Instruction>(U);
        if (!I) {
          errs() << "\tNon-Instruction use found! Use: " << *U << "\n";
        } else {
          CallSite CS(I);
          assert(CS && "Non-callsite instruction?");

          auto *ContainingF = I->getFunction();
          auto CFName = ContainingF->getName();
          if (!call_table->contains(CFName))
            call_table->insert(CFName, cpptoml::make_array());
          call_table->get_array(CFName)->push_back(toStr(I));
        }
      }

      ModulesWithReference.insert(MI.ModuleCRC);
      root->insert(MI.Filename, call_table);
    }
  }

  errs() << "\n-------------------\n";
  std::stringstream ss;
  ss << *root;
  outs() << ss.str() << "\n";
  outs().flush();

  errs() << "\n-------------------\n";
  errs() << "Allexes containing matched module:\n";
  size_t MatchingAllexes = 0;
  for (auto &A : DB.getAllexes()) {
    auto containsRef = std::any_of(
        A.Modules.begin(), A.Modules.end(), [&ModulesWithReference](auto &M) {
          return ModulesWithReference.count(M.ModuleCRC);
        });

    if (containsRef) {
      ++MatchingAllexes;
      errs() << A.Filename << "\n";
    }
  }

  // little helper
  auto printPercent = [](auto A, auto B) {
    assert(A <= B);
    errs() << "Percent: " << format("%.4g", 100. * double(A) / double(B))
           << "\n";
  };

  errs() << "Number of modules in DB: " << DB.getMods().size() << "\n";
  errs() << "Modules with reference to '" << Symbol
         << "': " << ModulesWithReference.size() << "\n";
  printPercent(ModulesWithReference.size(), DB.getMods().size());

  errs() << "Allexes in DB: " << DB.getAllexes().size() << "\n";
  errs() << "Allexes with reference: " << MatchingAllexes << "\n";
  printPercent(MatchingAllexes, DB.getAllexes().size());

  return Error::success();
}

CommandRegistration Unused(&FindDirectUses, [](ResourcePaths &RP) -> Error {
  errs() << "Loading allexe's from " << InputDirectory << "...\n";
  auto ExpDB = BCDB::loadFromAllexesIn(InputDirectory, RP);
  if (!ExpDB)
    return ExpDB.takeError();
  auto &DB = *ExpDB;
  errs() << "Done! Allexes found: " << DB->allexe_size() << "\n";

  return findUses(*DB, FuncName);
});

} // end anonymous namespace
