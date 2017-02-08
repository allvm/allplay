//===-- ForEachFile.h -----------------------------------------------------===//
//
// Author: Will Dietz (WD), wdietz2@uiuc.edu
//
//===----------------------------------------------------------------------===//
//
// Call function for each file found in a directory tree.
//
//===----------------------------------------------------------------------===//

#ifndef ALLVM_FOREACH_FILE_H
#define ALLVM_FOREACH_FILE_H

#include "allvm/Allexe.h"
#include "allvm/ResourcePaths.h"

#include <llvm/ADT/SmallString.h>
#include <llvm/ADT/Twine.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>

namespace allvm {

typedef std::function<llvm::Error(llvm::StringRef)> PathCallbackT;

static inline llvm::Error foreach_file_in_directory(const llvm::Twine &Path,
                                                    PathCallbackT F,
                                                    bool SkipEmpty = true,
                                                    bool RegularOnly = true) {
  llvm::SmallString<128> PathNative;
  llvm::sys::path::native(Path, PathNative);

  std::error_code EC;
  llvm::sys::fs::file_status status;
  llvm::sys::fs::recursive_directory_iterator File(PathNative, EC), FileEnd;
  if (File != FileEnd) {
    for (; File != FileEnd && !EC; File.increment(EC)) {
      EC = File->status(status);
      if (EC)
        break;
      if (SkipEmpty && (status.getSize() == 0))
        continue;
      if (RegularOnly &&
          (status.type() != llvm::sys::fs::file_type::regular_file))
        continue;
      llvm::Error Err = F(File->path());
      if (Err)
        return Err;
    }
    assert((File == FileEnd) == (!EC));
  }
  return llvm::errorCodeToError(EC);
}

static inline PathCallbackT AllexeCallback(
    std::function<llvm::Error(std::unique_ptr<Allexe>, llvm::StringRef File)>
        Callback,
    ResourcePaths &RP) {
  return [=](llvm::StringRef File) -> llvm::Error {
    auto MaybeAllexe = Allexe::openForReading(File, RP);
    if (MaybeAllexe) {
      if (auto Err = Callback(std::move(*MaybeAllexe), File))
        return Err;
    } else
      llvm::consumeError(MaybeAllexe.takeError());
    return llvm::Error::success();
  };
}

static auto foreach_allexe = std::bind(
    foreach_file_in_directory, std::placeholders::_1,
    std::bind(AllexeCallback, std::placeholders::_2, std::placeholders::_3),
    true, true);

// TODO: Add similar for WLLVMFile, but in separate header

} // end namespace allvm

#endif // ALLVM_FOREACH_FILE_H
