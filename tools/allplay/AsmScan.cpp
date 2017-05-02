#include "subcommand-registry.h"

#include "allvm/BCDB.h"

#include <llvm/ADT/DenseSet.h>
#include <llvm/IR/CallSite.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/Errc.h>
#include <llvm/Support/Format.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/YAMLParser.h>
#include <llvm/Support/YAMLTraits.h>

using namespace allvm;
using namespace llvm;

using llvm::yaml::MappingTraits;
using llvm::yaml::IO;

struct AsmEntry {
  std::string Function;
  std::vector<std::string> Instructions;
};

struct AsmInfo {
  Optional<std::vector<AsmEntry>> Inline;
  Optional<std::string> Module;
  std::string Path;
};

template <>
  struct MappingTraits<AsmInfo> {
    static void mapping(IO &io, AsmInfo &info) {
      io.mapRequired("path", info.Path);
      io.mapOptional("inline", info.Inline);
      io.mapOptional("module", info.Module);
    };
  };

template <>
  struct MappingTraits<AsmEntry> {
    static void mapping(IO &io, AsmEntry &ae) {
      io.mapRequired("function", ae.Function);
      io.mapRequired("instructions", ae.Instructions);
    };
  };

LLVM_YAML_IS_SEQUENCE_VECTOR(AsmEntry);
LLVM_YAML_IS_SEQUENCE_VECTOR(AsmInfo);
LLVM_YAML_IS_SEQUENCE_VECTOR(std::string);

namespace {

cl::SubCommand AsmScan("asmscan",
                       "Search modules for module-level and inline asm");

cl::opt<std::string> InputDirectory(cl::Positional, cl::Required,
                                    cl::desc("<input directory to scan>"),
                                    cl::sub(AsmScan));

Error asmScan(BCDB &DB) {

  errs() << "Starting Asm Scan...\n";

  // TODO: Would it be useful to store asm strings for aggregate analysis?
  // TODO: Count occurrences of inline asm?
  DenseSet<decltype(ModuleInfo::ModuleCRC)> ModulesWithModuleAsm;
  DenseSet<decltype(ModuleInfo::ModuleCRC)> ModulesWithInlineAsm;

  std::vector<AsmInfo> Infos;

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
    AsmInfo AI;
    AI.Path = MI.Filename;
    if (!Asm.empty()) {
      AI.Module = Asm;
      ModulesWithModuleAsm.insert(MI.ModuleCRC);
    }

    std::string InstStr;
    raw_string_ostream OS(InstStr);
    for (auto &F : *M) {
      for (auto &B : F) {
        AsmEntry AE;
        AE.Function = F.getName();
        for (auto &I : B) {
          CallSite CS(&I);
          if (!CS)
            continue;

          if (auto *IA = dyn_cast<InlineAsm>(CS.getCalledValue())) {
            // Found inline asm!
            InstStr.clear();
            OS << I;
            AE.Instructions.push_back(InstStr);
            ModulesWithInlineAsm.insert(MI.ModuleCRC);
          }
        }
        if (!AE.Instructions.empty()) {
          if (!AI.Inline) AI.Inline = std::vector<AsmEntry>();
          AI.Inline->push_back(AE);
        }

      }
    }
    if (AI.Module.hasValue() || AI.Inline.hasValue())
      Infos.push_back(AI);
  }

  llvm::yaml::Output yout(outs());
  yout << Infos;

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
  errs() << "Loading allexe's from " << InputDirectory << "...\n";
  auto ExpDB = BCDB::loadFromAllexesIn(InputDirectory, RP);
  if (!ExpDB)
    return ExpDB.takeError();
  auto &DB = *ExpDB;
  errs() << "Done! Allexes found: " << DB->allexe_size() << "\n";

  return asmScan(*DB);
});

} // end anonymous namespace
