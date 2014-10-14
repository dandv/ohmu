//===- InplaceReducer.h ----------------------------------------*- C++ --*-===//
// Copyright 2014  Google
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//===----------------------------------------------------------------------===//
//
// InplaceReducer implements the reducer interface so that each reduce simply
// returns a pointer to the original term.
//
// It is intended to be used as a basic class for destructive in-place
// transformations.
//
//===----------------------------------------------------------------------===//

#ifndef OHMU_TIL_INPLACEREDUCER_H
#define OHMU_TIL_INPLACEREDUCER_H

#include "clang/Analysis/Analyses/ThreadSafetyTIL.h"
#include "clang/Analysis/Analyses/ThreadSafetyTraverse.h"
#include "clang/Analysis/Analyses/ThreadSafetyPrint.h"

namespace ohmu {

using namespace clang::threadSafety::til;


// Used by SExprReducerMap.  Most terms map to SExpr*.
template <class T> struct InplaceTypeMap { typedef SExpr* Ty; };

// These kinds of SExpr must map to the same kind.
// We define these here b/c template specializations cannot be class members.
template<> struct InplaceTypeMap<VarDecl>     { typedef VarDecl* Ty; };
template<> struct InplaceTypeMap<BasicBlock>  { typedef BasicBlock* Ty; };


/// Defines the TypeMap for traversals that return SExprs.
/// See CopyReducer and InplaceReducer for details.
class InplaceReducerMap {
public:
  // An SExpr reducer rewrites one SExpr to another.
  template <class T> struct TypeMap : public SExprTypeMap<T> { };

  typedef std::nullptr_t NullType;

  static NullType reduceNull() { return nullptr; }
};


/// InplaceReducer implements the reducer interface so that each reduce simply
/// returns a pointer to the original term.
///
/// It is intended to be used as a basic class for destructive in-place
/// transformations.
class InplaceReducer  {
public:
  // Destructively update SExprs by writing back results.
  template <class T>
  T* handleResult(SExpr** Eptr, T* Res) {
    *Eptr = Res;
    return Res;
  }
  BasicBlock* handleResult(BasicBlock** B, BasicBlock* Res) { return Res; }
  VarDecl*    handleResult(VarDecl** VD,   VarDecl *Res)    { return Res; }


  SExpr* reduceWeak(Instruction* I) {
    // In most cases, we can return the rewritten version of I.
    // However, phi node arguments on back-edges have not been rewritten yet;
    // we'll rewrite those when we do the jump.
    if (I->instrID() <= CurrentInstrID)
      return InstructionMap[I->instrID()];
    else
      return I;
  }
  VarDecl*     reduceWeak(VarDecl *E)      { return E; }
  BasicBlock*  reduceWeak(BasicBlock *E)   { return E; }


  VarDecl* reduceVarDecl(VarDecl &Orig, SExpr* E) {
    return &Orig;
  }
  VarDecl* reduceVarDeclLetrec(VarDecl* Nvd, SExpr* D) { return Nvd; }

  SExpr* reduceFunction(Function &Orig, VarDecl *Nvd, SExpr* E0) {
    return &Orig;
  }
  SExpr* reduceSFunction(SFunction &Orig, VarDecl *Nvd, SExpr* E0) {
    return &Orig;
  }
  SExpr* reduceCode(Code &Orig, SExpr* E0, SExpr* E1) {
    return &Orig;
  }
  SExpr* reduceField(Field &Orig, SExpr* E0, SExpr* E1) {
    return &Orig;
  }

  SExpr* reduceLiteral(Literal &Orig) {
    return &Orig;
  }
  template<class T>
  SExpr* reduceLiteralT(LiteralT<T> &Orig) {
    return &Orig;
  }
  SExpr* reduceLiteralPtr(LiteralPtr &Orig) {
    return &Orig;
  }
  SExpr* reduceVariable(Variable &Orig, VarDecl* VD) {
    return &Orig;
  }

  SExpr* reduceApply(Apply &Orig, SExpr* E0, SExpr* E1) {
    return &Orig;
  }
  SExpr* reduceSApply(SApply &Orig, SExpr* E0, SExpr* E1) {
    return &Orig;
  }
  SExpr* reduceProject(Project &Orig, SExpr* E0) {
    return &Orig;
  }
  SExpr* reduceCall(Call &Orig, SExpr* E0) {
    return &Orig;
  }
  SExpr* reduceAlloc(Alloc &Orig, SExpr* E0) {
    return &Orig;
  }
  SExpr* reduceLoad(Load &Orig, SExpr* E0) {
    return &Orig;
  }
  SExpr* reduceStore(Store &Orig, SExpr* E0, SExpr* E1) {
    return &Orig;
  }
  SExpr* reduceArrayIndex(ArrayIndex &Orig, SExpr* E0, SExpr* E1) {
    return &Orig;
  }
  SExpr* reduceArrayAdd(ArrayAdd &Orig, SExpr* E0, SExpr* E1) {
    return &Orig;
  }
  SExpr* reduceUnaryOp(UnaryOp &Orig, SExpr* E0) {
    return &Orig;
  }
  SExpr* reduceBinaryOp(BinaryOp &Orig, SExpr* E0, SExpr* E1) {
    return &Orig;
  }
  SExpr* reduceCast(Cast &Orig, SExpr* E0) {
    return &Orig;
  }

  Phi* reducePhiBegin(Phi &Orig) {
    return &Orig;
  }
  void reducePhiArg(Phi &Orig, Phi* Ph, unsigned i, SExpr* E) { }
  Phi* reducePhi(Phi* Ph) { return Ph; }

  SExpr* reduceGoto(Goto &Orig, BasicBlock *B) {
    return &Orig;
  }
  SExpr* reduceBranch(Branch &O, SExpr* C, BasicBlock *B0, BasicBlock *B1) {
    return &O;
  }
  SExpr* reduceReturn(Return &Orig, SExpr* E) {
    return &Orig;
  }


  BasicBlock* reduceBasicBlockBegin(BasicBlock &Orig);
  bool reduceBBArgument(BasicBlock *BB, unsigned i, SExpr* E);
  bool reduceBBInstruction(BasicBlock *BB, unsigned i, SExpr* E);
  void reduceBBTerminator(BasicBlock *BB, SExpr* E) {
    BB->setTerminator(cast<Terminator>(E));
  }
  BasicBlock* reduceBasicBlock(BasicBlock *BB);


  SCFG* reduceSCFGBegin(SCFG &Orig);
  void reduceSCFGBlock(SCFG* Scfg, unsigned i, BasicBlock* B) { }
  SCFG* reduceSCFG(SCFG* Scfg);


  SExpr* reduceUndefined(Undefined &Orig) {
    return &Orig;
  }
  SExpr* reduceWildcard(Wildcard &Orig) {
    return &Orig;
  }

  SExpr* reduceIdentifier(Identifier &Orig) {
    return &Orig;
  }
  SExpr* reduceLet(Let &Orig, VarDecl *Nvd, SExpr* B) {
    return &Orig;
  }
  SExpr* reduceLetrec(Letrec &Orig, VarDecl *Nvd, SExpr* B) {
    return &Orig;
  }
  SExpr* reduceIfThenElse(IfThenElse &Orig, SExpr* C, SExpr* T, SExpr* E) {
    return &Orig;
  }


  InplaceReducer()
    : CurrentCFG(nullptr), CurrentBB(nullptr), CurrentInstrID(0) { }

protected:
  SCFG*        CurrentCFG;
  BasicBlock*  CurrentBB;
  unsigned     CurrentInstrID;
  std::vector<SExpr*>  InstructionMap;
};


}  // end namespace ohmu

#endif  // OHMU_TIL_INPLACEREDUCER_H
