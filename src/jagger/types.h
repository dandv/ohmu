//===- types.h -------------------------------------------------*- C++ --*-===//
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

#pragma once

//#include "x64builder\x64builder.h"


enum {
  VALUE = 0x80, COPY = 0xc0, FIXED = 0x20, KEY = 0x10, PHI = 0x8f,
  ALIAS_FLAGS = VALUE, ALIAS_GENERAL, ALIAS_SSE,
  NOP = 0, USE, MUTED_USE, HEADER, HEADER_DOMINATES,
  INT32, LOAD, STORE, ULOAD, USTORE, GATHER, SCATTER,
  SEXT, ZEXT, FCVT, /* ZEXT/FCVT used for truncation */
  AND, OR, ANDN, ORN, XOR, XNOR, NAND, NOR, NOT,
  SLL, SLR, SAR, ROL, ROR,
  MIN, MAX,
  ADD, SUB, SUBR, ADDN, ADC, SBB, NEG, ABS,
  MUL, MULHI, DIV, MOD, RCP,
  AOS, AOSOA,
  MADD, MSUB, MSUBR, MADDN,
  FMADD, FMSUB, FMSUBR, FMADDN,
  EQ, NEQ, LT, LE, ORD, EQU, NEQU, LTU, LEU, UNORD,
  JUMP, BRANCH, CALL, RET,
  BT, BTS, BTR, BTC,
  CTZ, CLZ, POPCNT, /* other bit ops bmi1/bmi2 */
  SQRT, RSQRT,
  SHUFFLE, BROADCAST, EXTRACT, INSERT,
  MEMSET, MEMCPY,
};

typedef unsigned char Opcode;
typedef unsigned int Data;

struct Event {
  Event() {}
  Event(Opcode code, Data data) : code(code), data(data) {}
#ifdef _M_X64
  Event(Opcode code, size_t data) : code(code), data((Data)data) {
    assert(this->data == data);
  }
#endif
  Opcode code;
  Data data;
};

struct EventRef {
  EventRef(Opcode& code, Data& data) : code(code), data(data) {}
  EventRef& operator=(Event Event) {
    code = Event.code;
    data = Event.data;
    return *this;
  }
  Opcode& code;
  Data& data;
};

struct EventStream {
  EventRef operator [](size_t index) const { return EventRef(codes[index], data[index]); }
  Opcode* codes;
  Data* data;
};

struct Block {
  Block* dominator;
  Block* head;
  size_t firstEvent;
  size_t numEvents;
};

void print(EventStream events, size_t numInstrs);

#if 0
namespace Jagger {

struct Block;
struct Instruction;
struct Use;

// EQ_OQ(EQ) 0H Equal(ordered, non - signaling) False False True False No
// UNORD_Q(UNORD) 3H Unordered(non - signaling) False False False True No
// NEQ_UQ(NEQ) 4H Not - equal(unordered, nonsignaling) True True False True No
// ORD_Q(ORD) 7H Ordered(non - signaling) True True True False No
// EQ_UQ 8H Equal(unordered, non - signaling) False False True True No
// FALSE_OQ(FALSE) BH False(ordered, non - signaling) False False False False No
// NEQ_OQ CH Not - equal(ordered, non - signaling) True True False False No
// TRUE_UQ(TRUE) FH True(unordered, non - signaling) True True True True No
// LT_OQ 11H Less - than(ordered, nonsignaling) False True False False No
// LE_OQ 12H Less - than - or - equal(ordered, nonsignaling) False True True False No
// NLT_UQ 15H Not - less - than(unordered, nonsignaling) True False True True No
// NLE_UQ 16H Not - less - than - or - equal(unordered, nonsignaling) True False False True No
// NGE_UQ 19H Not - greater - than - or - equal(unordered, nonsignaling) False True False True No
// NGT_UQ 1AH Not - greater - than(unordered, nonsignaling) False True True True No
// GE_OQ 1DH Greater - than - or - equal(ordered, nonsignaling) True False True False No
// GT_OQ 1EH Greater - than(ordered, nonsignaling) True False False False No

// Instruction families

struct LOGICFamily {
  bool val00;
  bool val01;
  bool val10;
  bool val11;
};

struct ADDFamily {
  bool neg_result;
  bool neg_arg1;
  // round mode
};

struct MADFamily {
  bool neg_result;
  bool neg_arg1;
  bool fused;
  // round mode
};

struct COPYFamily {
  bool src_in_mem;
  bool dst_in_mem;
  bool unaligned;
};

struct CVTFamily { // sext, zext, fcvt
  bool signaling;
  // round mode
};

struct ShiftFamily {
  bool left;
  bool rotate;
  bool arithmetic;
};

struct CMPFamily {
  bool lt;
  bool eq;
  bool gt;
  bool unord;
};

struct Opcode {
  enum Code {
    // KEYED COPY, COPY, FORCED COPY, VALUE, FORCED VALUE, KEYED VALUE, USE
    NOP, PHI, HEADER, TIE, COPY, SOURCE, SINK,
    VALUE, LOAD, STORE, ULOAD, USTORE, GATHER, SCATTER,
    SEXT, ZEXT, FCVT, /* ZEXT/FCVT used for truncation */
    AND, OR, ANDN, ORN, XOR, XNOR, NAND, NOR, NOT,
    SLL, SLR, SAR, ROL, ROR,
    MIN, MAX,
    ADD, SUB, SUBR, ADDN, ADC, SBB, NEG, ABS,
    MUL, MULHI, DIV, MOD, RCP,
    AOS, AOSOA,
    MADD, MSUB, MSUBR, MADDN,
    FMADD, FMSUB, FMSUBR, FMADDN,
    EQ, NEQ, LT, LE, ORD, EQU, NEQU, LTU, LEU, UNORD,
    JUMP, BRANCH, CALL, RET,
    BT, BTS, BTR, BTC,
    CTZ, CLZ, POPCNT, /* other bit ops bmi1/bmi2 */
    SQRT, RSQRT,
    SHUFFLE, BROADCAST, EXTRACT, INSERT,
    MEMSET, MEMCPY,
  };
  struct AliasSet {
    enum Enum {
      NONE, GENERAL, FLAGS, SIMD, X87, MEMORY
    };
  };
  struct Flags {
    enum Enum {
      NO_FLAGS = 0,
      DOMINATING_HEADER = 1,
      CONFLICT_PROXY = 1,
      PHI_PROXY = 1,
    };
  };
  struct Type {
    enum {
      BOOLEAN, UNSIGNED, SIGNED, FLOAT
    };
  };
  //   
  // CRC, AES,
  // MOVEMASK,
  // RDTSC
  // ATOMICS, EXCHAGE WITH MEMORY, CMPEXCH WITH MEMORY

  Opcode() {}
  Opcode(Code code, AliasSet::Enum aliasSet, Flags::Enum flags = Flags::NO_FLAGS, unsigned fixedReg = 0) {
    *(unsigned*)this = 0;
    this->code = code;
    this->aliasSet = aliasSet;
    //this->doCopyArg0 = doCopyArg0;
    //this->doCopyArg1 = doCopyArg1;
    this->flags = flags;
    this->fixedReg = fixedReg;
  }
  Opcode& setFlags(Flags::Enum flags) { this->flags = flags; return *this; }
  bool operator<(const Opcode& a) const { return *(int*)this < *(int*)this; }
  bool operator==(const Opcode& a) const { return *(int*)this == *(int*)this; }
  bool operator!=(const Opcode& a) const { return *(int*)this != *(int*)this; }

  unsigned code : 8;

  unsigned aliasSet : 3;
  //unsigned doCopyArg0 : 1;
  //unsigned doCopyArg1 : 1;
  unsigned flags : 3; // header kind, rounding mode, fp-cmp ordering

  //unsigned hasFixedReg : 1;
  unsigned fixedReg : 8;
  //unsigned type : 2;

  unsigned _ : 5;
  unsigned isArg0NotLastUse : 1;
  unsigned isArg1NotLastUse : 1;
  unsigned isUnused : 1;
};

//struct OpcodeInfo {
//  char name[16];
//  Opcode opcode;
//  unsigned hasFixedRegArg0 : 1;
//  unsigned hasFixedRegArg1 : 1;
//  unsigned aliasSetArg0 : 3;
//  unsigned aliasSetArg1 : 3;
//  unsigned fixedRegArg0 : 8;
//  unsigned fixedRegArg1 : 8;
//};

struct Instruction {
  Instruction* init(Opcode opcode) { return init(opcode, this); }
  Instruction* init(Opcode opcode, Instruction* arg0) {
    return init(opcode, arg0, this);
  }
  Instruction* init(Opcode opcode, Instruction* arg0, Instruction* arg1) {
    return init(opcode, arg0, arg1, this);
  }
  Instruction* init(Opcode opcode, Instruction* arg0, Instruction* arg1,
                    Instruction* key) {
    this->opcode = opcode;
    this->key = key - this;
    this->arg0 = arg0 - this;
    this->arg1 = arg1 - this;
    order = 0;
    return this;
  }

  Instruction* getKey() { return this + key; }
  Instruction* getArg0() { return this + arg0; }
  Instruction* getArg1() { return this + arg1; }
  const Instruction* getKey() const { return this + key; }
  const Instruction* getArg0() const { return this + arg0; }
  const Instruction* getArg1() const { return this + arg1; }
  const Instruction* updateKey() {
    if (key) key = this[key].updateKey() - this;
    return getKey();
  }
  const Instruction* updateKey(const Instruction* newKey) {
    key = (key ? this[key].updateKey(newKey) : newKey) - this;
    return getKey();
  }

  void print(const Instruction* base);

  Opcode opcode;
  int key;
  int arg0; // also literal
  int arg1; // jump target
  int order;
};

extern Instruction countedMarker;

struct Block {
  //bool dominates(const Block& block) const {
  //  return dominatorID <= block.dominatorID && block.dominatorID < dominatorID + dominatorSize;
  //}
  //bool postDominates(const Block& block) const {
  //  return postDominatorID <= block.postDominatorID && block.postDominatorID < postDominatorID + postDominatorSize;
  //}

  Block* dominator;
  Block* head;
  //int dominatorID;
  //int dominatorSize;
  //Block* postDominator;
  //int postDominatorID;
  //int postDominatorSize;
  //size_t local_size;
  //size_t total_size;



  //Block* parent;
  Instruction* instrs;
  size_t firstInstr;
  size_t numInstrs;
  //size_t numPhis;
  //size_t numEchos;
  //Block** Preds;
  //Block** Succs;
  //size_t numPreds;
  //size_t numSuccs;
};

//struct Procedure {
//  Block* getFirstBlock() const {
//    return (Block*)((char*)this + firstBlockOffset);
//  }
//  Block* getLastBlock() const {
//    return (Block*)((char*)this + lastBlockOffset);
//  }
//  Procedure& setFirstBlock(Block* instr) {
//    firstBlockOffset = (int)((char*)instr - (char*)this);
//    return *this;
//  }
//  Procedure& setLastBlock(Block* instr) {
//    lastBlockOffset = (int)((char*)instr - (char*)this);
//    return *this;
//  }
//
//  int firstBlockOffset;
//  int lastBlockOffset;
//};

#if 0
namespace Opcodes {
  extern const OpcodeInfo header;
  //extern const OpcodeInfo headerDominates;
  extern const OpcodeInfo nop;
  extern const OpcodeInfo jump;
  extern const OpcodeInfo branch;
  extern const OpcodeInfo intValue;
  //extern const OpcodeInfo arg0;
  extern const OpcodeInfo call;
  extern const OpcodeInfo ret;
  extern const OpcodeInfo copy;
  extern const OpcodeInfo phi;
  extern const OpcodeInfo add;
  extern const OpcodeInfo mul;
  extern const OpcodeInfo eq;
  extern const OpcodeInfo lt;
  extern const OpcodeInfo le;
  extern const OpcodeInfo unknown;
};
#endif

void print(Instruction* instrs, size_t numInstrs);

#if 0
union Event;
struct Object {
  enum Kind {
    HOLE,
    WALK_BACK,
    SKIP_BACK,
    USE,
    JUMP,
    BRANCH,
    INT_LITERAL,
    PHI,
    NOP,
    ADD,
    MUL,
    CMP_EQ,
    CMP_LT,
    CMP_LE,
  };

  bool isValue() const { return kind >= INT_LITERAL && kind <= CMP_LE; }
  Object& initObject(Kind kind) { this->kind = kind; return *this; }
  Object& initHole() { return initObject(HOLE); }
  const Event& asEvent() const { return *(const Event*)this; }
  Event& asEvent() { return *(Event*)this; }
  void print(int index);
  void emit(Instr* buffer);
  Kind kind;
};

struct Link : Object {
  Link& initWalkBack(int offsetToTarget) { initObject(WALK_BACK); this->offsetToTarget = offsetToTarget; return *this; }
  Link& initSkipBack(int offsetToTarget) { initObject(SKIP_BACK); this->offsetToTarget = offsetToTarget; return *this; }
  void print(int index);
  int offsetToTarget;
};

struct Use : Object {
  Use& init(int offsetToValue) { initObject(USE); this->offsetToValue = offsetToValue; return *this; }
  void print(int index);
  int offsetToValue; // value
};

struct Jump : Object {
  Jump& initJump(int jumpTarget) { initObject(JUMP); this->jumpTarget = jumpTarget; return *this; }
  Jump& initJumpcc(int jumpTarget) { initObject(BRANCH); this->jumpTarget = jumpTarget; return *this; }
  void print(int index);
  void emit(X64Builder& builder);
  int jumpTarget;
};

struct Value : Object {
  Value& initValue(Kind kind) { initObject(kind); invalidRegs = 0; pressure = 0; reg = 0; offsetToRep = 0; return *this; }
  void updateRep(int index, int rep);
  void print(int index);
  int pressure; // reuse me?
  unsigned invalidRegs;
  unsigned reg;
  int offsetToRep;
};

struct IntLiteral : Value {
  IntLiteral& init(int value) { initValue(INT_LITERAL); this->value = value; return *this; }
  void print(int index);
  void emit(X64Builder& builder);
  int value;
};

struct Instruction : Value {
  Instruction& initPhi() { initValue(PHI); return *this; }
  Instruction& initAdd() { initValue(ADD); return *this; }
  Instruction& initMul() { initValue(MUL); return *this; }
  Instruction& initEq() { initValue(CMP_EQ); return *this; }
  Instruction& initLt() { initValue(CMP_LT); return *this; }
  Instruction& initLe() { initValue(CMP_LE); return *this; }
  void print(int index);
  void emit(X64Builder& builder);
};

union Event {
  Event(Object object) { this->object = object; }
  Event(Value value) { this->value = value; }
  Event(Link link) { this->link = link; }
  Event(Use use) { this->use = use; }
  Event(Jump jump) { this->jump = jump; }
  Event(IntLiteral intLiteral) { this->intLiteral = intLiteral; }
  Event(Instruction instruction) { this->instruction = instruction; }
  void print(int index);
  Object::Kind kind;
  Object object;
  Link link;
  Use use;
  Jump jump;
  Value value;
  IntLiteral intLiteral;
  Instruction instruction;
};

void print(Event* events, size_t numEvents);
#endif
} // namespace Jagger
#endif