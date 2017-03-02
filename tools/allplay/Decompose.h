#ifndef ALLPLAY_DECOMPOSE_H
#define ALLPLAY_DECOMPOSE_H

#include <llvm/Support/Error.h>

namespace allvm {

llvm::Error decompose(llvm::StringRef BCFile, llvm::StringRef OutDir,
                      unsigned NumPartitions = 0, // 0 means 'auto'
                      bool Verbose = false);

} // end namespace allvm

#endif // ALLPLAY_DECOMPOSE_H
