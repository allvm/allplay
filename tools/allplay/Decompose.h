#ifndef ALLPLAY_DECOMPOSE_H
#define ALLPLAY_DECOMPOSE_H

#include <llvm/ADT/STLExtras.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/Error.h>

namespace allvm {

llvm::Error
decompose(std::unique_ptr<llvm::Module> M,
          llvm::function_ref<llvm::Error(std::unique_ptr<llvm::Module> MPart,
                                         llvm::StringRef Path)>
              ModuleCallback,
          bool Verbose = false);

llvm::Error decompose_into_dir(llvm::StringRef BCFile, llvm::StringRef OutDir,
                               bool Verbose = false);
llvm::Error decompose_into_dir(std::unique_ptr<llvm::Module> M, llvm::StringRef OutDir,
                               bool Verbose = false);
llvm::Error decompose_into_tar(llvm::StringRef BCFile, llvm::StringRef TarFile,
                               bool Verbose = false);
llvm::Error decompose_into_tar(std::unique_ptr<llvm::Module> M, llvm::StringRef TarFile,
                               bool Verbose = false);

} // end namespace allvm

#endif // ALLPLAY_DECOMPOSE_H
