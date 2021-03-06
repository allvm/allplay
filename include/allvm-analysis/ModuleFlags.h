#ifndef ALLVM_ANALYSIS_MODULEFLAGS_H
#define ALLVM_ANALYSIS_MODULEFLAGS_H

#include <llvm/ADT/StringRef.h>
#include <llvm/IR/Metadata.h>
#include <llvm/IR/Module.h>

#include <vector>

namespace allvm_analysis {

static const llvm::StringRef MF_ALLVM_SOURCE = "ALLVM Source";
static const llvm::StringRef MF_WLLVM_SOURCE = "WLLVM Source";

inline void setModuleFlag(llvm::Module *M, llvm::StringRef Key,
                          llvm::StringRef Source) {
  llvm::LLVMContext &C = M->getContext();
  M->addModuleFlag(llvm::Module::AppendUnique, Key,
                   llvm::MDNode::get(C, llvm::MDString::get(C, Source)));
}

inline void setWLLVMSource(llvm::Module *M, llvm::StringRef Source) {
  setModuleFlag(M, MF_WLLVM_SOURCE, Source);
}

inline void setALLVMSource(llvm::Module *M, llvm::StringRef Source) {
  setModuleFlag(M, MF_ALLVM_SOURCE, Source);
}

inline llvm::StringRef getWLLVMSource(llvm::Module *M) {
  using namespace llvm;
  auto *Meta = M->getModuleFlag(MF_WLLVM_SOURCE);
  if (!Meta)
    return {};
  auto *MD = cast<MDNode>(Meta);

  assert(MD->getNumOperands() == 1);
  return cast<MDString>(*MD->op_begin())->getString();
}

inline std::vector<llvm::StringRef> getALLVMSources(llvm::Module *M) {
  using namespace llvm;
  auto *Meta = M->getModuleFlag(MF_ALLVM_SOURCE);
  if (!Meta)
    return {};
  auto *MD = cast<MDNode>(Meta);

  std::vector<StringRef> Sources;
  for (auto I = MD->op_begin(), E = MD->op_end(); I != E; ++I)
    Sources.push_back(cast<MDString>(*I)->getString());
  return Sources;
}

inline std::string getALLVMSourceString(llvm::Module *M) {
  auto Sources = getALLVMSources(M);
  assert(!Sources.empty());
  if (Sources.size() == 1)
    return Sources.front();
  return llvm::join(Sources.begin(), Sources.end(), ",");
}

inline llvm::StringRef getALLVMSourceString(llvm::Function *F) {
  return getALLVMSourceString(F->getParent());
}

} // end namespace allvm_analysis

#endif // ALLVM_ANALYSIS_MODULEFLAGS_H
