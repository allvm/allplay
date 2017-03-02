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

#include <llvm/ADT/DenseSet.h>
#include <llvm/ADT/StringRef.h>

//#include <llvm/Support/SourceMgr.h>
//#include <llvm/IRReader/IRReader.h>

//#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/Support/FileSystem.h>

// UniqueID doesn't work in DenseSet
#include <unordered_set>

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

template <> struct llvm::DenseMapInfo<llvm::sys::fs::UniqueID> {
  using PairDMI = llvm::DenseMapInfo<std::pair<uint64_t, uint64_t>>;
  using UniqueID = llvm::sys::fs::UniqueID;
  static inline UniqueID getEmptyKey() {
    auto Pair = PairDMI::getEmptyKey();
    return {Pair.first, Pair.second};
  }
  static inline UniqueID getTombstoneKey() {
    auto Pair = PairDMI::getTombstoneKey();
    return {Pair.first, Pair.second};
  }
  static unsigned getHashValue(const UniqueID &Val) {
    auto P = std::make_pair(Val.getDevice(), Val.getFile());
    return PairDMI::getHashValue(P);
  }
  static inline bool isEqual(const UniqueID &LHS, const UniqueID &RHS) {
    return LHS == RHS;
  }
};

llvm::Expected<std::unique_ptr<BCDB>>
BCDB::loadFromBitcodeIn(StringRef InputDirectory, ResourcePaths &) {
  using namespace llvm::sys::fs;
  auto DB = llvm::make_unique<BCDB>();

  // std::unordered_set<UniqueID> BCIDs;
  DenseSet<UniqueID> BCIDs;

  auto addIfBC = [&](StringRef Path) -> Error {
    file_magic magic;
    if (auto EC = identify_magic(Path, magic)) {
      errs() << "Error reading magic: " << Path << "\n";
      return Error::success();
    }
    if (magic == file_magic::bitcode) {
      UniqueID ID;
      if (auto EC = getUniqueID(Path, ID)) {
        errs() << "Error getting unique id: " << Path << "\n";
        return Error::success();
      }
      if (BCIDs.insert(ID).second) {
        DB->Infos.push_back({0, Path});
      }
    }

    return Error::success();
  };

  if (auto Err = foreach_file_in_directory(InputDirectory, addIfBC))
    return std::move(Err);

  return std::move(DB);
}
