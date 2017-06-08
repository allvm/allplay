#include "subcommand-registry.h"

#include "boost_progress.h"

// Preserve insert order
#define CPPTOML_USE_MAP
#include "cpptoml.h"

#include "allvm/BCDB.h"

#include <llvm/ADT/DenseSet.h>
#include <llvm/IR/CallSite.h>
#include <llvm/IR/InlineAsm.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/Errc.h>
#include <llvm/Support/Format.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/raw_ostream.h>

using namespace allvm;
using namespace llvm;

namespace {

cl::SubCommand AsmScan("asmscan",
                       "Search modules for module-level and inline asm");

cl::opt<std::string> InputDirectory(cl::Positional, cl::Required,
                                    cl::desc("<input directory to scan>"),
                                    cl::sub(AsmScan));
cl::opt<bool> UseBCScanner("bc-scanner", cl::Optional, cl::init(false),
                           cl::desc("Use BC scanner instead of allexe scanner"),
                           cl::sub(AsmScan));

Error asmScan(BCDB &DB) {

  errs() << "Starting Asm Scan...\n";

  // TODO: Would it be useful to store asm strings for aggregate analysis?
  // TODO: Count occurrences of inline asm?
  DenseSet<decltype(ModuleInfo::ModuleCRC)> ModulesWithModuleAsm;
  DenseSet<decltype(ModuleInfo::ModuleCRC)> ModulesWithInlineAsm;

  auto root = cpptoml::make_table();

  boost::progress_display mod_progress(DB.getMods().size(), llvm::errs());
  for (auto MI : DB.getMods()) {
    SMDiagnostic SM;
    LLVMContext C;
    auto M = llvm::parseIRFile(MI.Filename, SM, C);
    if (!M)
      return make_error<StringError>(
          "Unable to open module file " + MI.Filename, errc::invalid_argument);

    if (auto Err = M->materializeAll())
      return Err;

    auto &Asm = M->getModuleInlineAsm();
    auto mod_table = cpptoml::make_table();
    if (!Asm.empty()) {
      mod_table->insert("module-level", Asm);
      ModulesWithModuleAsm.insert(MI.ModuleCRC);
    }

    auto inline_table = cpptoml::make_table();
    for (auto &F : *M) {
      auto inst_array = cpptoml::make_array();
      for (auto &B : F) {
        for (auto &I : B) {
          CallSite CS(&I);
          if (!CS)
            continue;

          if (auto *IA = dyn_cast<InlineAsm>(CS.getCalledValue())) {
            // Found inline asm!
            inst_array->push_back(IA->getAsmString() + " ---- " +
                                  IA->getConstraintString());

            ModulesWithInlineAsm.insert(MI.ModuleCRC);
          }
        }
      }
      if (inst_array->begin() != inst_array->end())
        inline_table->insert(F.getName(), inst_array);
    }
    if (!inline_table->empty())
      mod_table->insert("inline", inline_table);
    if (!mod_table->empty()) {
      root->insert(MI.Filename, mod_table);
    }
    ++mod_progress;
  }

  errs() << "Asm scan complete: \n";
  std::stringstream ss;
  ss << *root;
  outs() << ss.str() << "\n";
  outs().flush();

  errs() << "\n-------------------\n";
  errs() << "Allexes containing some form of asm:\n";
  errs() << "(ModuleLevelAsm?,InlineAsm?,AllexePath)\n";
  size_t ModAllexes = 0;
  size_t InlineAllexes = 0;
  for (auto &A : DB.getAllexes()) {

    auto hasModuleInSet = [](auto &Allexe, auto &Set) {
      return std::any_of(Allexe.Modules.begin(), Allexe.Modules.end(),
                         [&Set](auto &M) { return Set.count(M.ModuleCRC); });
    };

    bool modAsm = hasModuleInSet(A, ModulesWithModuleAsm);
    bool inlineAsm = hasModuleInSet(A, ModulesWithInlineAsm);

    if (modAsm)
      ++ModAllexes;
    if (inlineAsm)
      ++InlineAllexes;
    if (modAsm || inlineAsm)
      errs() << modAsm << "," << inlineAsm << "," << A.Filename << "\n";
  }

  // little helper
  auto printPercent = [](auto A, auto B) {
    assert(A <= B);
    errs() << "Percent: " << format("%.4g", 100. * double(A) / double(B))
           << "\n";
  };

  errs() << "Number of modules in DB: " << DB.getMods().size() << "\n";
  errs() << "Modules with mod-level asm: " << ModulesWithModuleAsm.size()
         << "\n";
  printPercent(ModulesWithModuleAsm.size(), DB.getMods().size());
  errs() << "Modules with inline asm: " << ModulesWithInlineAsm.size() << "\n";
  printPercent(ModulesWithInlineAsm.size(), DB.getMods().size());

  errs() << "Allexes in DB: " << DB.getAllexes().size() << "\n";
  errs() << "Allexes with mod-level asm: " << ModAllexes << "\n";
  printPercent(ModAllexes, DB.getAllexes().size());
  errs() << "Allexes with inline asm: " << InlineAllexes << "\n";
  printPercent(InlineAllexes, DB.getAllexes().size());

  return Error::success();
}

CommandRegistration Unused(&AsmScan, [](ResourcePaths &RP) -> Error {
  errs() << "Scanning " << InputDirectory << "...\n";

  auto ExpDB = UseBCScanner ? BCDB::loadFromBitcodeIn(InputDirectory, RP)
                            : BCDB::loadFromAllexesIn(InputDirectory, RP);
  if (!ExpDB)
    return ExpDB.takeError();
  auto &DB = *ExpDB;

  errs() << "Done! Allexes found: " << DB->allexe_size() << "\n";
  errs() << "Done! Modules found: " << DB->getMods().size() << "\n";

  return asmScan(*DB);
});

} // end anonymous namespace
