#ifndef ALLVM_ASMINFO_H
#define ALLVM_ASMINFO_H

#include <llvm/Support/YAMLParser.h>
#include <llvm/Support/YAMLTraits.h>

#include <string>

namespace allvm {

struct AsmEntry {
  std::string Function;
  std::vector<std::string> Instructions;
};

struct AsmInfo {
  llvm::Optional<std::vector<AsmEntry>> Inline;
  llvm::Optional<std::string> Module;
  std::string Path;
};

} // end namespace allvm

namespace llvm {
namespace yaml {

template <> struct MappingTraits<allvm::AsmInfo> {
  static void mapping(IO &io, allvm::AsmInfo &info) {
    io.mapRequired("path", info.Path);
    io.mapOptional("inline", info.Inline);
    io.mapOptional("module", info.Module);
  }
};

template <> struct MappingTraits<allvm::AsmEntry> {
  static void mapping(IO &io, allvm::AsmEntry &ae) {
    io.mapRequired("function", ae.Function);
    io.mapRequired("instructions", ae.Instructions);
  }
};

} // end namespace yaml
} // end namespace llvm

LLVM_YAML_IS_SEQUENCE_VECTOR(allvm::AsmEntry)
LLVM_YAML_IS_SEQUENCE_VECTOR(allvm::AsmInfo)
LLVM_YAML_IS_SEQUENCE_VECTOR(std::string)

#endif // ALLVM_ASMINFO_H
