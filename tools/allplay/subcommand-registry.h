#ifndef SUBCOMMAND_REGISTRY_H
#define SUBCOMMAND_REGISTRY_H

#include "allvm/ResourcePaths.h"

#include <llvm/Support/CommandLine.h>
#include <llvm/Support/Error.h>

namespace allvm {

// XXX: Taken from llvm-xray's source!

// Use |CommandRegistration| as a global initialiser that registers a function
// and associates it with |SC|. This requires that a command has not been
// registered to a given |SC|.

struct CommandRegistration {
  CommandRegistration(llvm::cl::SubCommand *SC,
                      std::function<llvm::Error(ResourcePaths &)> Command);
};

// Requires that |SC| is not null and has an associated function to it.
std::function<llvm::Error(ResourcePaths &)> dispatch(llvm::cl::SubCommand *SC);

} // end namespace allvm

#endif // SUBCOMMAND_REGISTRY_H
