#include "allvm-analysis/BCDB.h"

#include <llvm/Support/OnDiskHashTable.h>

using namespace allvm_analysis;
using namespace allvm;
using namespace llvm;

llvm::Error BCDB::writeToDisk(StringRef Path) {

  errs() << "Path: " << Path << "\n";

  // Key
  // Value
  // Rule: Key -> Value
  // Task: running instance of a rule

  return Error::success();
}
