/******************************************************************************
 * Copyright (c) 2017 Philipp Schubert.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Philipp Schubert and others
 *****************************************************************************/

#include "phasar/PhasarLLVM/Utils/LLVMIRToSrc.h"

#include "phasar/PhasarLLVM/Utils/LLVMShorthands.h"

#include "llvm/ADT/APInt.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Demangle/Demangle.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/DebugLoc.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Operator.h"
#include "llvm/IR/Value.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_ostream.h"

#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <string>

using namespace psr;

static llvm::DbgVariableIntrinsic *getDbgVarIntrinsic(const llvm::Value *V) {
  if (auto *VAM = llvm::ValueAsMetadata::getIfExists(
          const_cast<llvm::Value *>(V))) { // NOLINT FIXME when LLVM supports it
    if (auto *MDV = llvm::MetadataAsValue::getIfExists(V->getContext(), VAM)) {
      for (auto *U : MDV->users()) {
        if (auto *DBGIntr = llvm::dyn_cast<llvm::DbgVariableIntrinsic>(U)) {
          return DBGIntr;
        }
      }
    }
  } else if (const auto *Arg = llvm::dyn_cast<llvm::Argument>(V)) {
    /* If mem2reg is not activated, formal parameters will be stored in
     * registers at the beginning of function call. Debug info will be linked to
     * those alloca's instead of the arguments itself. */
    for (const auto *User : Arg->users()) {
      if (const auto *Store = llvm::dyn_cast<llvm::StoreInst>(User)) {
        if (Store->getValueOperand() == Arg &&
            llvm::isa<llvm::AllocaInst>(Store->getPointerOperand())) {
          return getDbgVarIntrinsic(Store->getPointerOperand());
        }
      }
    }
  }
  return nullptr;
}

llvm::DILocalVariable *psr::getDILocalVariable(const llvm::Value *V) {
  if (auto *DbgIntr = getDbgVarIntrinsic(V)) {
    if (auto *DDI = llvm::dyn_cast<llvm::DbgDeclareInst>(DbgIntr)) {
      return DDI->getVariable();
    }
    if (auto *DVI = llvm::dyn_cast<llvm::DbgValueInst>(DbgIntr)) {
      return DVI->getVariable();
    }
  }
  return nullptr;
}

static llvm::DIGlobalVariable *getDIGlobalVariable(const llvm::Value *V) {
  if (const auto *GV = llvm::dyn_cast<llvm::GlobalVariable>(V)) {
    if (auto *MN = GV->getMetadata(llvm::LLVMContext::MD_dbg)) {
      if (auto *DIGVExp =
              llvm::dyn_cast<llvm::DIGlobalVariableExpression>(MN)) {
        return DIGVExp->getVariable();
      }
    }
  }
  return nullptr;
}

static llvm::DISubprogram *getDISubprogram(const llvm::Value *V) {
  if (const auto *F = llvm::dyn_cast<llvm::Function>(V)) {
    return F->getSubprogram();
  }
  return nullptr;
}

llvm::DILocation *psr::getDILocation(const llvm::Value *V) {
  // Arguments and Instruction such as AllocaInst
  if (auto *DbgIntr = getDbgVarIntrinsic(V)) {
    if (auto *MN = DbgIntr->getMetadata(llvm::LLVMContext::MD_dbg)) {
      return llvm::dyn_cast<llvm::DILocation>(MN);
    }
  } else if (const auto *I = llvm::dyn_cast<llvm::Instruction>(V)) {
    if (auto *MN = I->getMetadata(llvm::LLVMContext::MD_dbg)) {
      return llvm::dyn_cast<llvm::DILocation>(MN);
    }
  }
  return nullptr;
}

std::string psr::getVarNameFromIR(const llvm::Value *V) {
  if (auto *LocVar = getDILocalVariable(V)) {
    return LocVar->getName().str();
  }
  if (auto *GlobVar = getDIGlobalVariable(V)) {
    return GlobVar->getName().str();
  }
  return "";
}

static llvm::DIType *getVarTypeFromIRImpl(const llvm::Value *V) {
  if (auto *LocVar = getDILocalVariable(V)) {
    return LocVar->getType();
  }
  if (auto *GlobVar = getDIGlobalVariable(V)) {
    return GlobVar->getType();
  }
  if (const auto *Call = llvm::dyn_cast<llvm::CallBase>(V)) {
    if (const auto *Callee = llvm::dyn_cast<llvm::Function>(
            Call->getCalledOperand()->stripPointerCastsAndAliases())) {
      if (auto *DICallee = Callee->getSubprogram()) {
        auto Types = DICallee->getType()->getTypeArray();
        if (Types.size()) {
          return Types[0];
        }
      }
    }
  }
  return nullptr;
}

static const llvm::GEPOperator *getStructGep(const llvm::Value *V) {
  if (const auto *Gep = llvm::dyn_cast<llvm::GEPOperator>(V)) {
    if (Gep->getNumIndices() != 2) {
      return nullptr;
    }
    const auto *FirstIdx =
        llvm::dyn_cast<llvm::ConstantInt>(Gep->indices().begin()->get());
    if (!FirstIdx || FirstIdx->getZExtValue() != 0) {
      return nullptr;
    }

    const auto *SecondIdx = llvm::dyn_cast<llvm::ConstantInt>(
        std::next(Gep->indices().begin())->get());
    if (!SecondIdx) {
      return nullptr;
    }
    return Gep;
  }

  return nullptr;
}

#define LOG(...)                                                               \
  if (Log)                                                                     \
  llvm::errs() << __VA_ARGS__

static std::pair<const llvm::Value *, size_t>
getOffsetAndBase(const llvm::Value *V, bool Log) {
  const auto *Base = V->stripPointerCastsAndAliases();
  uint64_t Offset = 0;
  if (const auto *Gep = getStructGep(Base)) {
    // Look for gep ptr, 0, N; where N is a constant

    LOG("> Gep " << *Gep << '\n');

    const auto *SecondIdx =
        llvm::cast<llvm::ConstantInt>(std::next(Gep->indices().begin())->get());

    Offset = SecondIdx->getZExtValue();
    Base = Gep->getPointerOperand();

    LOG("> Gep is well-formed; idx: " << Offset << '\n');
  }
  return {Base, Offset};
}

static llvm::DIType *getStructElementType(llvm::DIType *BaseTy, size_t Offset,
                                          bool Log) {
  const auto *DerivedTy = llvm::dyn_cast<llvm::DIDerivedType>(BaseTy);
  auto *StructTy = DerivedTy ? DerivedTy->getBaseType() : BaseTy;

  if (Offset == 0 && DerivedTy) {
    if (Log) {
      llvm::errs() << "> Return StructTy ";
      if (StructTy) {
        llvm::errs() << *StructTy;
      }

      llvm::errs() << '\n';
    }
    return StructTy;
  }

  LOG("> Field-access at offset " << Offset << '\n');

  if (const auto *CompositeTy =
          llvm::dyn_cast<llvm::DICompositeType>(StructTy)) {
    if (Offset > CompositeTy->getElements().size()) {
      LOG("> Out-of-bounds (" << Offset << " > "
                              << CompositeTy->getElements().size() << '\n');

      return nullptr;
    }
    auto Elems = CompositeTy->getElements();

    LOG("> Accessing array at [" << Offset << "] for " << *CompositeTy << '\n');

    if (auto *ElemTy = llvm::dyn_cast<llvm::DIType>(Elems[Offset])) {
      LOG("> Return ElemTy\n");
      return ElemTy;
    }
  }
  return nullptr;
}

static llvm::DIType *getVarTypeFromIRRec(const llvm::Value *V, size_t Depth,
                                         bool Log) {
  static constexpr size_t DepthLimit = 10;

  V = V->stripPointerCastsAndAliases();

  LOG("[getVarTypeFromIRRec]: " << llvmIRToString(V) << " // " << Depth
                                << '\n');

  if (auto *VarTy = getVarTypeFromIRImpl(V)) {
    LOG("> Return VarTy " << *VarTy << '\n');
    return VarTy;
  }

  auto InternalGetOffsetAndBase =
      [Log](const llvm::Value *V) -> std::pair<const llvm::Value *, size_t> {
    if (const auto *Load = llvm::dyn_cast<llvm::LoadInst>(V)) {
      return getOffsetAndBase(Load->getPointerOperand(), Log);
    }
    if (const auto *Gep = llvm::dyn_cast<llvm::GEPOperator>(V)) {
      return getOffsetAndBase(Gep->getPointerOperand(), Log);
    }
    return {};
  };

  auto [Base, Offset] = InternalGetOffsetAndBase(V);
  if (!Base) {
    return nullptr;
  }

  // TODO: Get rid of the recursion
  if (Depth >= DepthLimit) {
    llvm::errs() << "Reach depth-limit\n";
    return nullptr;
  }
  auto *BaseTy = getVarTypeFromIRRec(Base, Depth + 1, Log);
  if (!BaseTy) {
    return nullptr;
  }
  return getStructElementType(BaseTy, Offset, Log);
}
#undef LOG
llvm::DIType *psr::getVarTypeFromIR(const llvm::Value *V) {
  return getVarTypeFromIRRec(V, 0, /*getMetaDataID(V) == "629303"*/ false);
}

std::string psr::getFunctionNameFromIR(const llvm::Value *V) {
  // We can return unmangled function names w/o checking debug info
  if (const auto *F = llvm::dyn_cast<llvm::Function>(V)) {
    return F->getName().str();
  }
  if (const auto *Arg = llvm::dyn_cast<llvm::Argument>(V)) {
    return Arg->getParent()->getName().str();
  }
  if (const auto *I = llvm::dyn_cast<llvm::Instruction>(V)) {
    return I->getFunction()->getName().str();
  }
  return "";
}

std::string psr::getFilePathFromIR(const llvm::Value *V) {
  if (const auto *DIF = getDIFileFromIR(V)) {
    return getFilePathFromIR(DIF);
  }
  /* As a fallback solution, we will return 'source_filename' info from
   * module. However, it is not guaranteed to contain the absoult path, and it
   * will return 'llvm-link' for linked modules. */
  if (const auto *F = llvm::dyn_cast<llvm::Function>(V)) {
    return F->getParent()->getSourceFileName();
  }
  if (const auto *Arg = llvm::dyn_cast<llvm::Argument>(V)) {
    return Arg->getParent()->getParent()->getSourceFileName();
  }
  if (const auto *I = llvm::dyn_cast<llvm::Instruction>(V)) {
    return I->getFunction()->getParent()->getSourceFileName();
  }

  return {};
}

std::string psr::getFilePathFromIR(const llvm::DIFile *DIF) {
  auto FileName = DIF->getFilename();
  auto DirName = DIF->getDirectory();

  if (FileName.empty()) {
    return {};
  }

  // try to concatenate file path and dir to get absolute path
  if (!DirName.empty() &&
      !llvm::sys::path::has_root_directory(DIF->getFilename())) {
    llvm::SmallString<256> Buf;
    llvm::sys::path::append(Buf, DirName, FileName);

    return Buf.str().str();
  }

  return FileName.str();
}

const llvm::DIFile *psr::getDIFileFromIR(const llvm::Value *V) {
  if (const auto *GO = llvm::dyn_cast<llvm::GlobalObject>(V)) {
    if (auto *MN = GO->getMetadata(llvm::LLVMContext::MD_dbg)) {
      if (auto *Subpr = llvm::dyn_cast<llvm::DISubprogram>(MN)) {
        return Subpr->getFile();
      }
      if (auto *GVExpr = llvm::dyn_cast<llvm::DIGlobalVariableExpression>(MN)) {
        return GVExpr->getVariable()->getFile();
      }
    }
  } else if (const auto *Arg = llvm::dyn_cast<llvm::Argument>(V)) {
    if (auto *LocVar = getDILocalVariable(Arg)) {
      return LocVar->getFile();
    }
  } else if (const auto *I = llvm::dyn_cast<llvm::Instruction>(V)) {
    if (I->isUsedByMetadata()) {
      if (auto *LocVar = getDILocalVariable(I)) {
        return LocVar->getFile();
      }
    } else if (I->getMetadata(llvm::LLVMContext::MD_dbg)) {
      return I->getDebugLoc()->getFile();
    }
  }
  return nullptr;
}

std::string psr::getDirectoryFromIR(const llvm::Value *V) {
  // Argument and Instruction
  if (auto *DILoc = getDILocation(V)) {
    return DILoc->getDirectory().str();
  }
  if (auto *DISubpr = getDISubprogram(V)) { // Function
    return DISubpr->getDirectory().str();
  }
  if (auto *DIGV = getDIGlobalVariable(V)) { // Globals
    return DIGV->getDirectory().str();
  }
  return "";
}

unsigned int psr::getLineFromIR(const llvm::Value *V) {
  // Argument and Instruction
  if (auto *DILoc = getDILocation(V)) {
    return DILoc->getLine();
  }
  if (auto *DISubpr = getDISubprogram(V)) { // Function
    return DISubpr->getLine();
  }
  if (auto *DIGV = getDIGlobalVariable(V)) { // Globals
    return DIGV->getLine();
  }
  return 0;
}

unsigned int psr::getColumnFromIR(const llvm::Value *V) {
  // Globals and Function have no column info
  if (auto *DILoc = getDILocation(V)) {
    return DILoc->getColumn();
  }
  return 0;
}

std::pair<unsigned, unsigned> psr::getLineAndColFromIR(const llvm::Value *V) {
  // Argument and Instruction
  if (auto *DILoc = getDILocation(V)) {
    return {DILoc->getLine(), DILoc->getColumn()};
  }
  if (auto *DISubpr = getDISubprogram(V)) { // Function
    return {DISubpr->getLine(), 0};
  }
  if (auto *DIGV = getDIGlobalVariable(V)) { // Globals
    return {DIGV->getLine(), 0};
  }
  return {0, 0};
}

std::string psr::getSrcCodeFromIR(const llvm::Value *V, bool Trim) {
  if (auto Loc = getDebugLocation(V)) {
    return getSrcCodeFromIR(*Loc, Trim);
  }
  return {};
}

std::string psr::getSrcCodeFromIR(DebugLocation Loc, bool Trim) {
  if (Loc.Line == 0) {
    return {};
  }
  auto Path = getFilePathFromIR(Loc.File);

  if (llvm::sys::fs::exists(Path) && !llvm::sys::fs::is_directory(Path)) {
    std::ifstream Ifs(Path, std::ios::binary);
    if (Ifs.is_open()) {
      Ifs.seekg(std::ios::beg);
      std::string SrcLine;
      for (unsigned int I = 0; I < Loc.Line - 1; ++I) {
        Ifs.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
      }
      std::getline(Ifs, SrcLine);
      return Trim ? llvm::StringRef(SrcLine).trim().str() : SrcLine;
    }
  }
  return {};
}

std::string psr::getModuleIDFromIR(const llvm::Value *V) {
  if (const auto *GO = llvm::dyn_cast<llvm::GlobalObject>(V)) {
    return GO->getParent()->getModuleIdentifier();
  }
  if (const auto *Arg = llvm::dyn_cast<llvm::Argument>(V)) {
    return Arg->getParent()->getParent()->getModuleIdentifier();
  }
  if (const auto *I = llvm::dyn_cast<llvm::Instruction>(V)) {
    return I->getFunction()->getParent()->getModuleIdentifier();
  }
  return "";
}

bool SourceCodeInfo::empty() const noexcept { return SourceCodeLine.empty(); }

bool SourceCodeInfo::operator==(const SourceCodeInfo &Other) const noexcept {
  // don't compare the SourceCodeFunctionName. It is directly derivable from
  // line, column and filename
  return Line == Other.Line && Column == Other.Column &&
         SourceCodeLine == Other.SourceCodeLine &&
         SourceCodeFilename == Other.SourceCodeFilename;
}

bool SourceCodeInfo::equivalentWith(const SourceCodeInfo &Other) const {
  // Here, we need to compare the SourceCodeFunctionName, because we don't
  // compare the complete SourceCodeFilename
  if (Line != Other.Line || Column != Other.Column ||
      SourceCodeLine != Other.SourceCodeLine ||
      SourceCodeFunctionName != Other.SourceCodeFunctionName) {
    return false;
  }

  auto Pos = SourceCodeFilename.find_last_of(
      std::filesystem::path::preferred_separator);
  if (Pos == std::string::npos) {
    Pos = 0;
  }

  return llvm::StringRef(Other.SourceCodeFilename)
      .endswith(llvm::StringRef(SourceCodeFilename)
                    .slice(Pos + 1, llvm::StringRef::npos));
}

void psr::from_json(const nlohmann::json &J, SourceCodeInfo &Info) {
  J.at("sourceCodeLine").get_to(Info.SourceCodeLine);
  J.at("sourceCodeFileName").get_to(Info.SourceCodeFilename);
  if (auto Fn = J.find("sourceCode"); Fn != J.end()) {
    Fn->get_to(Info.SourceCodeFunctionName);
  }
  J.at("line").get_to(Info.Line);
  J.at("column").get_to(Info.Column);
}
void psr::to_json(nlohmann::json &J, const SourceCodeInfo &Info) {
  J = nlohmann::json{
      {"sourceCodeLine", Info.SourceCodeLine},
      {"sourceCodeFileName", Info.SourceCodeFilename},
      {"sourceCodeFunctionName", Info.SourceCodeFunctionName},
      {"line", Info.Line},
      {"column", Info.Column},
  };
}

SourceCodeInfo psr::getSrcCodeInfoFromIR(const llvm::Value *V) {
  return SourceCodeInfo{
      getSrcCodeFromIR(V),
      getFilePathFromIR(V),
      llvm::demangle(getFunctionNameFromIR(V)),
      getLineFromIR(V),
      getColumnFromIR(V),
  };
}

std::optional<DebugLocation> psr::getDebugLocation(const llvm::Value *V) {
  // Argument and Instruction
  if (auto *DILoc = getDILocation(V)) {
    return DebugLocation{DILoc->getLine(), DILoc->getColumn(),
                         DILoc->getFile()};
  }
  if (auto *DISubpr = getDISubprogram(V)) { // Function
    return DebugLocation{DISubpr->getLine(), 0, DISubpr->getFile()};
  }
  if (auto *DIGV = getDIGlobalVariable(V)) { // Globals
    return DebugLocation{DIGV->getLine(), 0, DIGV->getFile()};
  }

  return std::nullopt;
}
