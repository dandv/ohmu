//===- ThreadSafetyTraverse.h ----------------------------------*- C++ --*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines a framework for doing generic traversals and rewriting
// operations over the Thread Safety TIL.
//
// UNDER CONSTRUCTION.  USE AT YOUR OWN RISK.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_ANALYSIS_ANALYSES_THREADSAFETYPRINT_H
#define LLVM_CLANG_ANALYSIS_ANALYSES_THREADSAFETYPRINT_H

#include "ThreadSafetyTIL.h"

#include <ostream>

namespace clang {
namespace threadSafety {
namespace til {

// Pretty printer for TIL expressions
template <typename Self, typename StreamType>
class PrettyPrinter {
protected:
  Self *self() { return reinterpret_cast<Self *>(this); }

  void indent()   { Indent += 2; }
  void unindent() { Indent -= 2; }

  void newline(StreamType &SS) {
    SS << "\n";
    for (unsigned i = 0; i < Indent; ++i)
      SS << " ";
  }

  // TODO: further distinguish between binary operations.
  static const unsigned Prec_Atom = 0;
  static const unsigned Prec_Postfix = 1;
  static const unsigned Prec_Unary = 2;
  static const unsigned Prec_Binary = 3;
  static const unsigned Prec_Other = 4;
  static const unsigned Prec_Decl = 5;
  static const unsigned Prec_MAX = 6;

  // Return the precedence of a given node, for use in pretty printing.
  unsigned precedence(const SExpr *E) {
    switch (E->opcode()) {
      case COP_VarDecl:    return Prec_Atom;
      case COP_Function:   return Prec_Decl;
      case COP_Code:       return Prec_Decl;
      case COP_Field:      return Prec_Decl;
      case COP_Slot:       return Prec_Decl;
      case COP_Record:     return Prec_Atom;
      case COP_ScalarType: return Prec_Atom;

      case COP_Literal:    return Prec_Atom;
      case COP_Variable:   return Prec_Atom;
      case COP_Apply:      return Prec_Postfix;
      case COP_Project:    return Prec_Postfix;

      case COP_Call:       return Prec_Postfix;
      case COP_Alloc:      return Prec_Other;
      case COP_Load:       return Prec_Postfix;
      case COP_Store:      return Prec_Other;
      case COP_ArrayIndex: return Prec_Postfix;
      case COP_ArrayAdd:   return Prec_Postfix;

      case COP_UnaryOp:    return Prec_Unary;
      case COP_BinaryOp:   return Prec_Binary;
      case COP_Cast:       return Prec_Atom;

      case COP_SCFG:       return Prec_Decl;
      case COP_BasicBlock: return Prec_MAX;
      case COP_Phi:        return Prec_Atom;
      case COP_Goto:       return Prec_Atom;
      case COP_Branch:     return Prec_Atom;
      case COP_Return:     return Prec_Other;

      case COP_Future:     return Prec_Atom;
      case COP_Undefined:  return Prec_Atom;
      case COP_Wildcard:   return Prec_Atom;

      case COP_Identifier: return Prec_Atom;
      case COP_Let:        return Prec_Atom;
      case COP_Letrec:     return Prec_Atom;
      case COP_IfThenElse: return Prec_Decl;
    }
    return Prec_MAX;
  }

  void printBlockLabel(StreamType & SS, const BasicBlock *BB, int index) {
    if (!BB) {
      SS << "BB_null";
      return;
    }
    SS << "BB_";
    SS << BB->blockID();
    if (index >= 0) {
      SS << ":";
      SS << index;
    }
  }

  StringRef printableName(StringRef N) {
    if (N.length() > 0)
      return N;
    return StringRef("_x", 2);
  }


  void printSExpr(const SExpr *E, StreamType &SS, unsigned P, bool Sub=true) {
    if (!E) {
      self()->printNull(SS);
      return;
    }
    if (Sub) {
      if (const auto *I = E->asCFGInstruction()) {
        SS << printableName(I->instrName()) << I->instrID();
        return;
      }
    }
    if (self()->precedence(E) > P) {
      // Wrap expr in () if necessary.
      SS << "(";
      self()->printSExpr(E, SS, Prec_MAX);
      SS << ")";
      return;
    }

    switch (E->opcode()) {
#define TIL_OPCODE_DEF(X)                                                  \
    case COP_##X:                                                          \
      self()->print##X(cast<X>(E), SS);                                    \
      return;
#include "ThreadSafetyOps.def"
#undef TIL_OPCODE_DEF
    }
  }

  void printNull(StreamType &SS) {
    SS << "#null";
  }

  void printScalarType(const ScalarType *E, StreamType &SS) {
    SS << E->valueType().getTypeName();
  }

  template<class T>
  void printLiteralT(const LiteralT<T> *E, StreamType &SS) {
    SS << E->value();
  }

  void printLiteralT(const LiteralT<uint8_t> *E, StreamType &SS) {
    SS << "'" << E->value() << "'";
  }

  void printLiteral(const Literal *E, StreamType &SS) {
    ValueType VT = E->valueType();
    switch (VT.Base) {
    case ValueType::BT_Void: {
      SS << "void";
      return;
    }
    case ValueType::BT_Bool: {
      if (E->as<bool>().value())
        SS << "true";
      else
        SS << "false";
      return;
    }
    case ValueType::BT_Int: {
      switch (VT.Size) {
      case ValueType::ST_8:
        if (VT.Signed)
          printLiteralT(&E->as<int8_t>(), SS);
        else
          printLiteralT(&E->as<uint8_t>(), SS);
        return;
      case ValueType::ST_16:
        if (VT.Signed)
          printLiteralT(&E->as<int16_t>(), SS);
        else
          printLiteralT(&E->as<uint16_t>(), SS);
        return;
      case ValueType::ST_32:
        if (VT.Signed)
          printLiteralT(&E->as<int32_t>(), SS);
        else
          printLiteralT(&E->as<uint32_t>(), SS);
        return;
      case ValueType::ST_64:
        if (VT.Signed)
          printLiteralT(&E->as<int64_t>(), SS);
        else
          printLiteralT(&E->as<uint64_t>(), SS);
        return;
      default:
        break;
      }
      break;
    }
    case ValueType::BT_Float: {
      switch (VT.Size) {
      case ValueType::ST_32:
        printLiteralT(&E->as<float>(), SS);
        return;
      case ValueType::ST_64:
        printLiteralT(&E->as<double>(), SS);
        return;
      default:
        break;
      }
      break;
    }
    case ValueType::BT_String: {
      SS << "\"";
      printLiteralT(&E->as<StringRef>(), SS);
      SS << "\"";
      return;
    }
    case ValueType::BT_Pointer: {
      SS << "#ptr";
      return;
    }
    case ValueType::BT_ValueRef: {
      SS << "#vref";
      return;
    }
    }
  }

  void printVariable(const Variable *E, StreamType &SS) {
    if (E->variableDecl()->name().length() > 0) {
      SS << E->variableDecl()->name();
      return;
    }
    SS << "_x";
  }

  void printVarDecl(const VarDecl *E, StreamType &SS) {
    if (E->kind() == VarDecl::VK_SFun)
      SS << "@";
    SS << printableName(E->name());
    switch (E->kind()) {
    case VarDecl::VK_Fun:
      SS << ": ";
      break;
    case VarDecl::VK_SFun:
      return;
    case VarDecl::VK_Let:
    case VarDecl::VK_Letrec:
      SS << " = ";
      break;
    }
    printSExpr(E->definition(), SS, Prec_Decl);
  }

  void printFunction(const Function *E, StreamType &SS, unsigned sugared = 0) {
    switch (sugared) {
      default:
        SS << "\\(";   // Lambda
        break;
      case 1:
        SS << "(";     // Slot declarations
        break;
      case 2:
        SS << ", ";    // Curried functions
        break;
    }
    self()->printVarDecl(E->variableDecl(), SS);

    const SExpr *B = E->body();
    if (B && B->opcode() == COP_Function) {
      self()->printFunction(cast<Function>(B), SS, 2);
    }
    else {
      SS << ") ";
      self()->printSExpr(B, SS, Prec_Decl);
    }
  }

  void printCode(const Code *E, StreamType &SS) {
    SS << ": ";
    self()->printSExpr(E->returnType(), SS, Prec_Decl-1);
    SS << " -> ";
    self()->printSExpr(E->body(), SS, Prec_Decl);
  }

  void printField(const Field *E, StreamType &SS) {
    SS << ": ";
    self()->printSExpr(E->range(), SS, Prec_Decl-1);
    SS << " = ";
    self()->printSExpr(E->body(), SS, Prec_Decl);
  }

  void printSlot(const Slot *E, StreamType &SS) {
    SS << E->name();
    if (auto *F = dyn_cast<Function>(E->definition())) {
      printFunction(F, SS, 1);
    }
    else if (auto *C = dyn_cast<Code>(E->definition())) {
      SS << "()";
      printCode(C, SS);
    }
    else if (auto *F = dyn_cast<Field>(E->definition())) {
      printField(F, SS);
    }
    else {
      SS << " = ";
      self()->printSExpr(E->definition(), SS, Prec_Decl);
    }
    SS << ";";
  }

  void printRecord(const Record *E, StreamType &SS) {
    SS << "struct {";
    self()->indent();
    for (auto &S : E->slots()) {
      self()->newline(SS);
      self()->printSlot(S.get(), SS);
    }
    self()->unindent();
    self()->newline(SS);
    SS << "}";
  }

  void printApply(const Apply *E, StreamType &SS, bool sugared = false) {
    const SExpr *F = E->fun();

    if (E->isSelfApplication()) {
      self()->printSExpr(F, SS, Prec_Postfix);
      if (E->isDelegation()) {
        SS << "@(";
        self()->printSExpr(E->arg(), SS, Prec_MAX);
        SS << ")";
      }
      else if (Verbose)
        SS << "@()";
      return;
    }

    const Apply *FA = dyn_cast<Apply>(F);
    if (FA && !FA->isSelfApplication()) {
      printApply(cast<Apply>(F), SS, true);
      SS << ", ";
    } else {
      self()->printSExpr(F, SS, Prec_Postfix);
      SS << "(";
    }
    self()->printSExpr(E->arg(), SS, Prec_MAX);
    if (!sugared)
      SS << ")";
  }

  void printProject(const Project *E, StreamType &SS) {
    if (CStyle) {
      // Omit the 'this->'
      if (const Apply *SAP = dyn_cast<Apply>(E->record())) {
        if (auto *V = dyn_cast<Variable>(SAP->fun())) {
          if (V->variableDecl()->kind() == VarDecl::VK_SFun &&
              !SAP->isDelegation()) {
            SS << E->slotName();
            return;
          }
        }
      }
      if (isa<Wildcard>(E->record())) {
        // handle existentials
        SS << "&";
        //SS << E->clangDecl()->getQualifiedNameAsString();
        return;
      }
    }
    self()->printSExpr(E->record(), SS, Prec_Postfix);
    if (CStyle && E->isArrow()) {
      SS << "->";
    }
    else {
      SS << ".";
    }
    SS << E->slotName();
  }

  void printCall(const Call *E, StreamType &SS) {
    const SExpr *T = E->target();
    if (T->opcode() == COP_Apply) {
      self()->printApply(cast<Apply>(T), SS, true);
      SS << ")";
      if (Verbose)
        SS << "()";
    }
    else {
      self()->printSExpr(T, SS, Prec_Postfix);
      SS << "()";
    }
  }

  void printAlloc(const Alloc *E, StreamType &SS) {
    SS << "new ";
    self()->printSExpr(E->initializer(), SS, Prec_Other-1);
  }

  void printLoad(const Load *E, StreamType &SS) {
    self()->printSExpr(E->pointer(), SS, Prec_Postfix);
    if (!CStyle)
      SS << "^";
  }

  void printStore(const Store *E, StreamType &SS) {
    self()->printSExpr(E->destination(), SS, Prec_Other-1);
    SS << " := ";
    self()->printSExpr(E->source(), SS, Prec_Other-1);
  }

  void printArrayIndex(const ArrayIndex *E, StreamType &SS) {
    self()->printSExpr(E->array(), SS, Prec_Postfix);
    SS << "[";
    self()->printSExpr(E->index(), SS, Prec_MAX);
    SS << "]";
  }

  void printArrayAdd(const ArrayAdd *E, StreamType &SS) {
    self()->printSExpr(E->array(), SS, Prec_Postfix);
    SS << " + ";
    self()->printSExpr(E->index(), SS, Prec_Atom);
  }

  void printUnaryOp(const UnaryOp *E, StreamType &SS) {
    SS << getUnaryOpcodeString(E->unaryOpcode());
    self()->printSExpr(E->expr(), SS, Prec_Unary);
  }

  void printBinaryOp(const BinaryOp *E, StreamType &SS) {
    self()->printSExpr(E->expr0(), SS, Prec_Binary-1);
    SS << " " << getBinaryOpcodeString(E->binaryOpcode()) << " ";
    self()->printSExpr(E->expr1(), SS, Prec_Binary-1);
  }

  void printCast(const Cast *E, StreamType &SS) {
    if (!CStyle) {
      SS << "cast.";
      SS << getCastOpcodeString(E->castOpcode());
      SS << "(";
      self()->printSExpr(E->expr(), SS, Prec_Unary);
      SS << ")";
      return;
    }
    self()->printSExpr(E->expr(), SS, Prec_Unary);
  }

  void printSCFG(const SCFG *E, StreamType &SS) {
    SS << "CFG {";
    self()->indent();
    bool First = true;
    for (auto &B : E->blocks()) {
      self()->newline(SS);
      if (!First)
        self()->newline(SS);
      First = false;
      printBasicBlock(B.get(), SS);
    }
    self()->unindent();
    self()->newline(SS);
    SS << "}";
  }


  void printBBInstr(const Instruction *E, StreamType &SS) {
    if (!E) {
      if (Verbose) {
        self()->newline(SS);
        SS << "null;\n";
      }
      return;
    }
    self()->newline(SS);
    if (E->opcode() != COP_Store) {
      SS << "let " << printableName(E->instrName()) << E->instrID();
      if (Verbose) {
        SS << ": " << E->valueType().getTypeName();
      }
      SS << " = ";
    }
    self()->printSExpr(E, SS, Prec_MAX, false);
    SS << ";";
  }

  void printBasicBlock(const BasicBlock *E, StreamType &SS) {
    printBlockLabel(SS, E, -1);
    SS << ": // ";

    SS << "preds={";
    bool First = true;
    for (auto &B : E->predecessors()) {
      if (!First)
        SS << ", ";
      printBlockLabel(SS, B.get(), -1);
      First = false;
    }
    SS << "}";

    SS << " dom=";
    printBlockLabel(SS, E->parent(), -1);
    SS << " post=";
    printBlockLabel(SS, E->postDominator(), -1);

    self()->indent();

    for (auto *A : E->arguments()) {
      printBBInstr(A, SS);
    }
    for (auto *I : E->instructions()) {
      printBBInstr(I, SS);
    }
    auto *T = E->terminator();
    if (T) {
      self()->newline(SS);
      self()->printSExpr(T, SS, Prec_MAX, false);
      SS << ";";
    }
    self()->unindent();
  }

  void printPhi(const Phi *E, StreamType &SS) {
    SS << "phi(";
    if (E->status() == Phi::PH_SingleVal)
      self()->printSExpr(E->values()[0].get(), SS, Prec_MAX);
    else {
      unsigned i = 0;
      for (auto &V : E->values()) {
        if (i++ > 0)
          SS << ", ";
        self()->printSExpr(V.get(), SS, Prec_MAX);
      }
    }
    SS << ")";
  }

  void printGoto(const Goto *E, StreamType &SS) {
    SS << "goto ";
    printBlockLabel(SS, E->targetBlock(), E->phiIndex());
  }

  void printBranch(const Branch *E, StreamType &SS) {
    SS << "branch (";
    self()->printSExpr(E->condition(), SS, Prec_MAX);
    SS << ") ";
    printBlockLabel(SS, E->thenBlock(), -1);
    SS << " ";
    printBlockLabel(SS, E->elseBlock(), -1);
  }

  void printReturn(const Return *E, StreamType &SS) {
    SS << "return ";
    self()->printSExpr(E->returnValue(), SS, Prec_Other);
  }

  void printIdentifier(const Identifier *E, StreamType &SS) {
    SS << "$";
    SS << E->name();
  }

  void printLet(const Let *E, StreamType &SS, bool Nested=false) {
    if (!Nested) {
      SS << "{";
      self()->indent();
    }

    self()->newline(SS);
    SS << "let ";
    printVarDecl(E->variableDecl(), SS);
    SS << ";";

    if (auto *L = dyn_cast<Let>(E->body())) {
      printLet(L, SS, true);
    }
    else if (auto *Lr = dyn_cast<Letrec>(E->body())) {
      printLetrec(Lr, SS, true);
    }
    else {
      self()->newline(SS);
      printSExpr(E->body(), SS, Prec_Decl);
      SS << ";";
    }

    if (!Nested) {
      self()->unindent();
      self()->newline(SS);
      SS << "}";
    }
  }

  void printLetrec(const Letrec *E, StreamType &SS, bool Nested=false) {
    if (!Nested) {
      SS << "{";
      self()->indent();
    }

    self()->newline(SS);
    SS << "letrec ";
    printVarDecl(E->variableDecl(), SS);
    SS << ";";

    if (auto *L = dyn_cast<Let>(E->body())) {
      printLet(L, SS, true);
    }
    else if (auto *Lr = dyn_cast<Letrec>(E->body())) {
      printLetrec(Lr, SS, true);
    }
    else {
      self()->newline(SS);
      printSExpr(E->body(), SS, Prec_Decl);
      SS << ";";
    }

    if (!Nested) {
      self()->unindent();
      self()->newline(SS);
      SS << "}";
    }
  }

  void printIfThenElse(const IfThenElse *E, StreamType &SS) {
    if (CStyle) {
      printSExpr(E->condition(), SS, Prec_Unary);
      SS << " ? ";
      printSExpr(E->thenExpr(), SS, Prec_Unary);
      SS << " : ";
      printSExpr(E->elseExpr(), SS, Prec_Unary);
      return;
    }
    SS << "if (";
    printSExpr(E->condition(), SS, Prec_MAX);
    SS << ") then ";
    printSExpr(E->thenExpr(), SS, Prec_Other);
    SS << " else ";
    printSExpr(E->elseExpr(), SS, Prec_Other);
  }

  void printFuture(const Future *E, StreamType &SS) {
    if (E->maybeGetResult()) {
      SS << "#f(";
      self()->printSExpr(E->maybeGetResult(), SS, Prec_MAX);
      SS << ")";
    }
    else
      SS << "#future";
  }

  void printUndefined(const Undefined *E, StreamType &SS) {
    SS << "#undefined";
  }

  void printWildcard(const Wildcard *E, StreamType &SS) {
    SS << "*";
  }


public:
  PrettyPrinter(bool V = false, bool CS = true)
     : Verbose(V), CStyle(CS), Indent(0)
  {}

  static void print(const SExpr *E, StreamType &SS, bool Sub=false) {
    Self printer;
    printer.printSExpr(E, SS, Prec_MAX, Sub);
  }

private:
  bool Verbose;  // Print out additional information
  bool CStyle;   // Print exprs in C-like syntax.
  unsigned Indent;
};


class StdPrinter : public PrettyPrinter<StdPrinter, std::ostream> { };

class TILDebugPrinter : public PrettyPrinter<TILDebugPrinter, std::ostream> {
public:
  TILDebugPrinter() : PrettyPrinter(true, false) { }
};


}  // end namespace til
}  // end namespace threadSafety
}  // end namespace clang


#endif

