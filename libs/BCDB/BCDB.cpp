//===-- BCDB.cpp ----------------------------------------------------------===//
//
// Author: Will Dietz (WD), wdietz2@uiuc.edu
//
//===----------------------------------------------------------------------===//
//
// Datastructures for storing collections of bitcode.
//
//===----------------------------------------------------------------------===//

#include "ForEachFile.h"

#include "allvm/BCDB.h"
#include "allvm/ModuleFlags.h"
#include "allvm/ResourcePaths.h"

#include <llvm/ADT/StringRef.h>

using namespace allvm;
using namespace llvm;

llvm::Expected<std::unique_ptr<BCDB>>
BCDB::loadFromAllexesIn(StringRef InputDirectory, ResourcePaths &RP) {

  // DenseMap<uint32_t, size_t> ModuleMap;

  auto DB = llvm::make_unique<BCDB>();

  auto addAllexe = [&](auto A, StringRef F) -> llvm::Error {

    AllexeDesc AD;
    AD.Filename = F;
    for (size_t i = 0, e = A->getNumModules(); i != e; ++i) {
      auto crc = A->getModuleCRC(i);

      // not safe, crc32 collisions and whatnot, but works for now..
      if (!DB->ModuleMap.count(crc)) {
        LLVMContext LocalContext;
        auto M = A->getModule(i, LocalContext);
        if (!M)
          return M.takeError();
        if (auto Err = (*M)->materializeMetadata())
          return Err;
        ModuleInfo MI{crc, getALLVMSourceString(M->get())};

        // if (StringRef(MI.Filename).contains("samba")) continue;
        // if (StringRef(MI.Filename).contains("llvm-all")) continue;
        // if (StringRef(MI.Filename).contains("llvm-lld")) continue;

        DB->Infos.push_back(MI);
        DB->ModuleMap.insert({crc, MI});

        // TODO: Add to DB->Modules, but in a way we can find it again
      }
      AD.Modules.push_back(DB->ModuleMap[crc]);
    }

    DB->Allexes.push_back(AD);

    return Error::success();
  };

  if (auto Err = foreach_allexe(InputDirectory, addAllexe, RP))
    return std::move(Err);

  // Allexe --> { list of bc }
  // Allexe --> allexe_handle --> getModules() -> list of modules
  //                          \-> getMerged() -> single module, alltogether'd as
  //                          needed
  // * BC <--> Analysis
  // * Subgraph <--> Analysis ?

  return std::move(DB);
}
