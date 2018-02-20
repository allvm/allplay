#include "subcommand-registry.h"

#include "allvm-analysis/BCDB.h"

#include <llvm/ADT/DenseSet.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/Errc.h>
#include <llvm/Support/Format.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/raw_ostream.h>

using namespace allvm_analysis;
using namespace allvm;
using namespace llvm;

namespace {

cl::SubCommand FindUses("finduses", "Search modules for uses of function");

cl::opt<std::string> InputDirectory(cl::Positional, cl::Required,
                                    cl::desc("<input directory to scan>"),
                                    cl::sub(FindUses));
cl::opt<std::string> FuncName(cl::Positional, cl::Required,
                              cl::desc("<name of function>"),
                              cl::sub(FindUses));

Error findUses(BCDB &DB, llvm::StringRef Symbol) {

  errs() << "Finding uses of '" << Symbol << "' in BCDB...\n";

  errs() << "Err, looking for users of function with that name\n";

  DenseSet<decltype(ModuleInfo::ModuleCRC)> ModulesWithReference;

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

      errs() << MI.Filename << "\n";

      ModulesWithReference.insert(MI.ModuleCRC);
    }
  }

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

CommandRegistration Unused(&FindUses, [](ResourcePaths &RP) -> Error {
  errs() << "Loading allexe's from " << InputDirectory << "...\n";
  auto ExpDB = BCDB::loadFromAllexesIn(InputDirectory, RP);
  if (!ExpDB)
    return ExpDB.takeError();
  auto &DB = *ExpDB;
  errs() << "Done! Allexes found: " << DB->allexe_size() << "\n";

  return findUses(*DB, FuncName);
});

} // end anonymous namespace
