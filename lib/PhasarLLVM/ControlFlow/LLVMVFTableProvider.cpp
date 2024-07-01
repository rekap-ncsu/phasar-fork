#include "phasar/PhasarLLVM/ControlFlow/LLVMVFTableProvider.h"

#include "phasar/PhasarLLVM/DB/LLVMProjectIRDB.h"
#include "phasar/PhasarLLVM/TypeHierarchy/DIBasedTypeHierarchy.h"
#include "phasar/PhasarLLVM/TypeHierarchy/LLVMVFTable.h"
#include "phasar/PhasarLLVM/Utils/LLVMIRToSrc.h"
#include "phasar/Utils/Logger.h"

#include "llvm/BinaryFormat/Dwarf.h"
#include "llvm/Demangle/Demangle.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Casting.h"

using namespace psr;

static std::string getTypeName(const llvm::DIType *DITy) {
  if (const auto *CompTy = llvm::dyn_cast<llvm::DICompositeType>(DITy)) {
    auto Ident = CompTy->getIdentifier();
    return Ident.empty() ? llvm::demangle(CompTy->getName().str())
                         : llvm::demangle(Ident.str());
  }
  return llvm::demangle(DITy->getName().str());
}

static std::vector<const llvm::Function *> getVirtualFunctions(
    const llvm::StringMap<const llvm::GlobalVariable *> &ClearNameTVMap,
    const llvm::DIType *Type) {
  auto ClearName = getTypeName(Type);

  if (ClearName.substr(0, 18) == "typeinfo name for ") {
    ClearName = ClearName.substr(18, ClearName.size() - 1);
  }

  auto It = ClearNameTVMap.find(ClearName);

  if (It != ClearNameTVMap.end()) {
    if (!It->second->hasInitializer()) {
      PHASAR_LOG_LEVEL_CAT(DEBUG, "DIBasedTypeHierarchy",
                           ClearName << " does not have initializer");
      return {};
    }
    if (const auto *I = llvm::dyn_cast<llvm::ConstantStruct>(
            It->second->getInitializer())) {
      return LLVMVFTable::getVFVectorFromIRVTable(*I);
    }
  }
  return {};
}

LLVMVFTableProvider::LLVMVFTableProvider(const llvm::Module &Mod) {
  llvm::StringMap<const llvm::GlobalVariable *> ClearNameTVMap;

  for (const auto &Glob : Mod.globals()) {
    if (DIBasedTypeHierarchy::isVTable(Glob.getName())) {
      auto Demang = llvm::demangle(Glob.getName().str());
      auto ClearName = DIBasedTypeHierarchy::removeVTablePrefix(Demang);
      ClearNameTVMap.try_emplace(ClearName, &Glob);
    }
  }

  llvm::DebugInfoFinder DIF;
  DIF.processModule(Mod);
  for (const auto *Ty : DIF.types()) {
    if (const auto *DerivedTy = llvm::dyn_cast<llvm::DIDerivedType>(Ty)) {
      if (const auto *BaseTy = DerivedTy->getBaseType()) {
        if (const auto *CompTy =
                llvm::dyn_cast<llvm::DICompositeType>(BaseTy)) {
          if (CompTy->getTag() == llvm::dwarf::DW_TAG_class_type ||
              CompTy->getTag() == llvm::dwarf::DW_TAG_structure_type) {
            TypeVFTMap.try_emplace(CompTy,
                                   getVirtualFunctions(ClearNameTVMap, CompTy));
          }
        }
      }
    }
  }
}

LLVMVFTableProvider::LLVMVFTableProvider(const LLVMProjectIRDB &IRDB)
    : LLVMVFTableProvider(*IRDB.getModule()) {}

bool LLVMVFTableProvider::hasVFTable(const llvm::DIType *Type) const {
  return TypeVFTMap.count(Type);
}

const LLVMVFTable *
LLVMVFTableProvider::getVFTableOrNull(const llvm::DIType *Type) const {
  auto It = TypeVFTMap.find(Type);
  return It != TypeVFTMap.end() ? &It->second : nullptr;
}
