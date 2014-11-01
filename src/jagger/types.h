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

enum {
  VALUE = 0x80, PHI = 0x90, COPY = 0xa0, PHI_COPY = 0xb0,
  FLAGS_REGS = 0x01, GP_REGS = 0x02, SSE_REGS = 0x03, X87_REGS = 0x04,
  IS_FIXED = 0x40, USE_FIXED = VALUE | IS_FIXED,
  VALUE_MASK = 0xf0, REGS_MASK = 0x07,
  USE_EFLAGS = USE_FIXED | FLAGS_REGS,
  USE_EAX = USE_FIXED | GP_REGS,
  USE_EDX = USE_FIXED | GP_REGS | (1 << 3),
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
  EventRef(Opcode& code, Data& data, size_t index)
    : code(code), data(data) {}//, index(index) {}
  EventRef& operator=(Event Event) {
    code = Event.code;
    data = Event.data;
    return *this;
  }
  Opcode& code;
  Data& data;
  //size_t index;
  //EventRef prior() const {
  //  return EventRef((&code)[-1], (&data)[-1], index - 1);
  //}
};

struct EventStream {
  EventRef operator[](size_t index) const {
    return EventRef(codes[index], data[index], index);
  }
  Opcode* codes;
  Data* data;
};

struct Block {
  Block* dominator;
  Block* head;
  size_t firstEvent;
  size_t numEvents;
};

struct Work {
  explicit Work(Data index) : index(index) {}
  bool operator <(const Work& a) const { return count < a.count; }
  Data count;
  Data index;
};

struct Sidecar {
  Sidecar() : preferred(0), invalid(0) {}
  Data preferred;
  Data invalid;
};

void print_stream(EventStream events, size_t numInstrs);
void print_asm(EventStream events, size_t numInstrs);
void make_asm(EventStream events, size_t numEvents);

#if 0
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
#endif
