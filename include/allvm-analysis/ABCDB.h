//===-- ABCDB.h -----------------------------------------------------------===//
//
// Author: Will Dietz (WD), wdietz2@uiuc.edu
//
//===----------------------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#ifndef ALLVM_ANALYSIS_ABCDB_H
#define ALLVM_ANALYSIS_ABCDB_H

#include <allvm/Allexe.h>

#include <llvm/ADT/DenseMap.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/Error.h>

#include <vector>

namespace allvm_analysis {

struct ModuleInfo {
  uint32_t ModuleCRC;
  std::string Filename;
};

class ABCDB {
public:
  static llvm::Expected<std::unique_ptr<ABCDB>>
  loadFromAllexesIn(llvm::StringRef InputDirectory, allvm::ResourcePaths &RP);
  static llvm::Expected<std::unique_ptr<ABCDB>>
  loadFromBitcodeIn(llvm::StringRef InputDirectory, allvm::ResourcePaths &RP);

  auto begin() const { return Modules.begin(); }
  auto end() const { return Modules.end(); }
  auto size() const { return Modules.size(); }

  auto allexe_begin() const { return Allexes.begin(); }
  auto allexe_end() const { return Allexes.end(); }
  auto allexe_size() const { return Allexes.size(); }
  auto &getAllexes() const { return Allexes; }

  auto &getMods() const { return Infos; }

  // ABCDB() { C.setDiscardValueNames(true); }

  llvm::Error writeToDisk(llvm::StringRef Path);

private:
  // llvm::Error addModulesFrom(const Allexe &A);

  // llvm::LLVMContext C;

  // TODO: remove this
  std::vector<llvm::Module *> Modules;

  // TODO: Rework this class entirely O:)

  llvm::DenseMap<uint32_t, ModuleInfo> ModuleMap;
  std::vector<ModuleInfo> Infos;
  struct AllexeDesc {
    std::string Filename;
    llvm::SmallVector<ModuleInfo, 1> Modules;
  };
  std::vector<AllexeDesc> Allexes;
};

} // end namespace allvm_analysis

#endif // ALLVM_ANALYSIS_ABCDB_H
