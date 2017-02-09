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

cl::SubCommand Combine("combine",
                       "Combine all modules into single bitcode file");
cl::opt<std::string> InputDirectory(cl::Positional, cl::Required,
                                    cl::value_desc("directory"),
                                    cl::desc("input directory to scan"),
                                    cl::sub(Combine));
cl::opt<std::string> OutputFilename("o", cl::Required,
                                    cl::desc("name of module to write"),
                                    cl::sub(Combine));

Error saveCombinedModule(BCDB &DB, StringRef Filename) {
  errs() << "Creating COMBINED 'Module'..\n";

  SmallVector<char, 0> Buffer;
  // Apparently this makes things work, even on the binary-cat path,
  // even when we don't use the writer again.
  // Probably does some headers or so?
  BitcodeWriter Writer(Buffer);

  // Binary cat, like llvm-cat does (optionally)
  for (auto MI : DB.getMods()) {
    auto MB = errorOrToExpected(MemoryBuffer::getFile(MI.Filename));
    if (!MB)
      return MB.takeError();
    auto Mods = getBitcodeModuleList(**MB);
    if (!Mods)
      return Mods.takeError();

    for (auto &BitcodeMod : *Mods)
      Buffer.insert(Buffer.end(), BitcodeMod.getBuffer().begin(),
                    BitcodeMod.getBuffer().end());
  }

  std::error_code EC;
  raw_fd_ostream OS(Filename, EC, sys::fs::OpenFlags::F_None);
  if (EC) {
    return make_error<StringError>(
        "cannot open " + Filename + ": " + EC.message(), EC);
  }

  OS.write(Buffer.data(), Buffer.size());

  return Error::success();
}

CommandRegistration Unused(&Combine, [](ResourcePaths &RP) -> Error {
  errs() << "Loading allexe's from " << InputDirectory << "...\n";
  auto ExpDB = BCDB::loadFromAllexesIn(InputDirectory, RP);
  if (!ExpDB)
    return ExpDB.takeError();
  auto &DB = *ExpDB;
  errs() << "Done! Allexes found: " << DB->allexe_size() << "\n";

  return saveCombinedModule(*DB, OutputFilename);
});

} // end anonymous namespace
