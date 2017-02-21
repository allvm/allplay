#include "subcommand-registry.h"

#include "allvm/BCDB.h"

#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>

using namespace allvm;
using namespace llvm;

namespace {

cl::SubCommand Uncombine("uncombine",
                         "Uncombine all modules from combined bitcode file");
cl::opt<std::string> InputFilename(cl::Positional, cl::Required,
                                   cl::value_desc("filename"),
                                   cl::desc("combined bitcode"),
                                   cl::sub(Uncombine));
cl::opt<std::string> OutputPrefix("prefix", cl::Required,
                                  cl::desc("prefix to give modules"),
                                  cl::sub(Uncombine));

Error uncombineModule(StringRef Filename, StringRef Prefix) {
  errs() << "Un-combining combined 'Module'...\n";

  auto MB = errorOrToExpected(MemoryBuffer::getFile(Filename));
  if (!MB)
    return MB.takeError();
  auto Mods = getBitcodeModuleList(**MB);
  if (!Mods)
    return Mods.takeError();

  size_t N = 0;
  for (auto &BitcodeMod : *Mods) {
    std::error_code EC;
    std::string ModFilename = (Prefix + Twine(N++)).str();
    raw_fd_ostream OS(ModFilename, EC, sys::fs::OpenFlags::F_None);
    if (EC)
      return make_error<StringError>(
          "cannot open " + ModFilename + ": " + EC.message(), EC);
    OS.write(BitcodeMod.getBuffer().data(), BitcodeMod.getBuffer().size());
  }

  return Error::success();
}

CommandRegistration Unused(&Uncombine, [](ResourcePaths &RP) -> Error {
  return uncombineModule(InputFilename, OutputPrefix);
});

} // end anonymous namespace
