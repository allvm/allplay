#include "subcommand-registry.h"

#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/StringExtras.h>
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

std::string toPaddedDec(uint64_t N, unsigned W) {
  // Get fixed-width decimal string for the number
  auto S = utostr(N);
  assert(S.size() <= W);
  while (S.size() < W)
    S = "0" + S;
  return S;
}

Error uncombineModule(StringRef Filename, StringRef Prefix) {
  errs() << "Un-combining combined 'Module'...\n";

  auto MB = errorOrToExpected(MemoryBuffer::getFile(Filename));
  if (!MB)
    return MB.takeError();
  auto Mods = getBitcodeModuleList(**MB);
  if (!Mods)
    return Mods.takeError();

  SmallVector<char, 0> Header;
  BitcodeWriter Writer(Header);

  size_t N = 0;
  auto MaxWidth = utostr(Mods->size()).size(); // lol, n-1
  for (auto &BitcodeMod : *Mods) {
    std::error_code EC;
    auto S = toPaddedDec(N++, MaxWidth);
    std::string ModFilename = (Prefix + S + ".bc").str();
    raw_fd_ostream OS(ModFilename, EC, sys::fs::OpenFlags::F_None);
    if (EC)
      return make_error<StringError>(
          "cannot open " + ModFilename + ": " + EC.message(), EC);

    OS << Header << BitcodeMod.getBuffer();
  }

  return Error::success();
}

CommandRegistration Unused(&Uncombine, [](ResourcePaths &RP) -> Error {
  return uncombineModule(InputFilename, OutputPrefix);
});

} // end anonymous namespace
