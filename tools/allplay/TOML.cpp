#include "subcommand-registry.h"

// Preserve insert order
#define CPPTOML_USE_MAP
#include "cpptoml.h"

#include "allvm/BCDB.h"

using namespace allvm;
using namespace llvm;

namespace {

cl::SubCommand Toml("toml", "Print allexe contents as toml");

cl::opt<std::string> InputDirectory(cl::Positional, cl::Required,
                                    cl::desc("<input directory to scan>"),
                                    cl::sub(Toml));

Error toml(BCDB &DB) {

  auto root = cpptoml::make_table();

  for (auto &A : DB.getAllexes()) {
    auto modules = cpptoml::make_array();
    for (auto &M : A.Modules)
      modules->push_back(M.Filename);
    root->insert(A.Filename, modules);
  }

  std::stringstream ss;
  ss << *root;
  outs() << ss.str() << "\n";
  outs().flush();

  return Error::success();
}

CommandRegistration Unused(&Toml, [](ResourcePaths &RP) -> Error {
  errs() << "Loading allexe's from " << InputDirectory << "...\n";
  auto ExpDB = BCDB::loadFromAllexesIn(InputDirectory, RP);
  if (!ExpDB)
    return ExpDB.takeError();
  auto &DB = *ExpDB;
  errs() << "Done! Allexes found: " << DB->allexe_size() << "\n";

  return toml(*DB);
});

} // end anonymous namespace
