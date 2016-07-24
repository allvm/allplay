#include <llvm/ADT/StringExtras.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_os_ostream.h>

#include "ImageExecutor.h"
#include "ZipArchive.h"

using namespace allvm;
using namespace llvm;

static cl::opt<std::string> LibNone("libnone", cl::desc("Path of libnone.a"));

static cl::opt<std::string> InputFilename(cl::Positional, cl::Required,
                                          cl::desc("<input allvm file>"));

static cl::list<std::string> InputArgv(cl::ConsumeAfter,
                                       cl::desc("<program arguments>..."));

const StringRef ALLEXE_MAIN = "main.bc";

void nameModule(Module *M, StringRef File, StringRef Entry, uint32_t crc) {
  // Make up a naming scheme for caching
  // TODO: Use more meaningful ID's
  // * Use module hashes like used in ThinLTO?
  // * Reflect what optimizations have/haven't been done
  // * etc.

  // For now, use the crc from the allexe zip to at least recognize
  // a different module with same name.
  std::string crcHex = utohexstr(crc);
  std::string ID = ("allexe:" + crcHex + "-" + Entry).str();
  M->setModuleIdentifier(ID);
}

int main(int argc, const char **argv, const char **envp) {
  // Link in necessary libraries
  InitializeNativeTarget();
  InitializeNativeTargetAsmPrinter();
  InitializeNativeTargetAsmParser();
  cl::ParseCommandLineOptions(argc, argv, "allvm runtime executor");

  LLVMContext context;

  auto exezip = ZipArchive::openForReading(InputFilename);
  if (!exezip) {
    errs() << "Could not open " << InputFilename << ": ";
    errs() << exezip.getError().message() << '\n';
    return 1;
  }

  auto bcFiles = (*exezip)->listFiles();
  if (bcFiles.empty()) {
    errs() << "allexe contained no files!\n";
    return 1;
  }

  auto mainFile = bcFiles.front();
  auto supportFiles = bcFiles.drop_front();
  if (mainFile != ALLEXE_MAIN) {
    errs() << "Could not open " << InputFilename << ": ";
    errs() << "First entry was '" << mainFile << "',";
    errs() << " expected '" << ALLEXE_MAIN << "'\n";
    return 1;
  }

  uint32_t crc;
  auto bitcode = (*exezip)->getEntry(ALLEXE_MAIN, &crc);
  if (!bitcode) {
    errs() << "Could not open " << InputFilename << ": ";
    errs() << "no main.bc present\n";
    return 1;
  }

  SMDiagnostic err;
  auto M = parseIR(bitcode->getMemBufferRef(), err, context);
  if (!M.get()) {
    err.print(argv[0], errs());
    return 1;
  }
  nameModule(M.get(), InputFilename, ALLEXE_MAIN, crc);

  auto executor = make_unique<ImageExecutor>(std::move(M));

  // Add supporting libraries
  for (auto &lib : supportFiles) {
    auto lib_bitcode = (*exezip)->getEntry(lib, &crc);
    if (!lib_bitcode) {
      errs() << "Could not open " << InputFilename << ": ";
      errs() << "Failed to load '" << lib << "'\n";
      return 1;
    }
    auto LibM = parseIR(lib_bitcode->getMemBufferRef(), err, context);
    if (!LibM.get()) {
      err.print(argv[0], errs());
      return 1;
    }
    nameModule(LibM.get(), InputFilename, lib, crc);
    executor->addModule(std::move(LibM));
  }

  // Add name of file as argv[0]
  StringRef ProgName = InputFilename;
  if (sys::path::has_extension(InputFilename))
    ProgName = ProgName.drop_back(sys::path::extension(ProgName).size());
  InputArgv.insert(InputArgv.begin(), ProgName);

  if (LibNone.empty())
    return executor->runBinary(InputArgv, envp);

  return executor->runHostedBinary(InputArgv, envp, LibNone);
}
