//===- event.cpp -----------------------------------------------*- C++ --*-===//
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

#include "types.h"
#include <stdio.h>
#include <map>

static char OpcodeNames[][20] = {
  "NOP", "USE", "MUTED_USE", "HEADER", "HEADER_DOMINATES",
  "INT32", "LOAD", "STORE", "ULOAD", "USTORE", "GATHER", "SCATTER",
  "SEXT", "ZEXT", "FCVT",
  "AND", "OR", "ANDN", "ORN", "XOR", "XNOR", "NAND", "NOR", "NOT",
  "SLL", "SLR", "SAR", "ROL", "ROR",
  "MIN", "MAX",
  "ADD", "SUB", "SUBR", "ADDN", "ADC", "SBB", "NEG", "ABS",
  "MUL", "MULHI", "DIV", "MOD", "RCP",
  "AOS", "AOSOA",
  "MADD", "MSUB", "MSUBR", "MADDN",
  "FMADD", "FMSUB", "FMSUBR", "FMADDN",
  "EQ", "NEQ", "LT", "LE", "ORD", "EQU", "NEQU", "LTU", "LEU", "UNORD",
  "JUMP", "BRANCH", "CALL", "RET",
  "BT", "BTS", "BTR", "BTC",
  "CTZ", "CLZ", "POPCNT",
  "SQRT", "RSQRT",
  "SHUFFLE", "BROADCAST", "EXTRACT", "INSERT",
  "MEMSET", "MEMCPY",
};

void print_stream(EventStream events, size_t numInstrs) {
  for (size_t i = 0; i < numInstrs; ++i) {
    auto code = events[i].code;
    printf("%3d > ", i);
    if (code <= MEMCPY)
      printf("%s", OpcodeNames[code]);
    else {
      if ((code & VALUE_MASK) == VALUE) printf("VALUE");
      if ((code & VALUE_MASK) == PHI) printf("PHI");
      if ((code & VALUE_MASK) == COPY) printf("COPY");
      if ((code & VALUE_MASK) == PHI_COPY) printf("PHI_COPY");
    }
    printf(" : %d\n", events[i].data);
  }
}

void print_asm(EventStream events, size_t numInstrs) {
  for (size_t i = 0; i < numInstrs; ++i) {
    auto code = events[i].code;
    if (code & VALUE) {
      if (!(code & 0x20)) continue;  //< is not a copy
      auto dst = events[i].data;
      if ((code & VALUE_MASK) == PHI_COPY) dst = events[dst].data;
      auto src = events[events[i - 1].data].data;
      if (src != dst) printf("copy %02x %02x\n", dst, src);
      continue;
    }
    if (code <= HEADER_DOMINATES)
      continue;
    switch (code) {
    case INT32:
      printf("mov %02x '%d'\n", events[i - 1].data, events[i].data);
      break;
    case ADD: {
      auto dst = events[i - 2].data;
      if (events[i - 2].code == MUTED_USE)
        dst = events[dst].data;
      auto src = events[events[i - 4].data].data;
      printf("add %02x %02x\n", dst, src);
      break;
    }
    case SUB:
      printf("sub %02x %02x\n", events[events[i - 3].data].data,
        events[events[i - 2].data].data);
      break;
    case MUL: {
      auto dst = events[events[i - 9].data].data;
      auto src = events[events[i - 8].data].data;
      if (src == 1 || dst == 2) std::swap(src, dst);
      if (dst != 1) printf("copy 01 %02x\n", dst);
      if (src != 2) printf("copy 02 %02x\n", src);
      printf("mul 01 02\n");
      if (events[i - 2].data != 1) printf("copy %02x 01\n", events[i - 2].data);
      break;
    }
    case EQ:
      printf("cmp %02x %02x\n", events[events[i - 3].data].data,
             events[events[i - 2].data].data);
      break;
    case JUMP:
      printf("JUMP ???\n");
      break;
    case BRANCH:
      printf("JE ???\n");
      break;
    case RET:
      if (events[events[i - 2].data].data != 1)
        printf("copy 01 %0x2", events[events[i - 2].data].data);
      break;
    default: printf("unknown op! %02x\n", code);
    }
  }
}

#if 0
namespace Jagger {
void print(Instrs instrs, size_t numInstrs);
void Instruction::print(const Instruction* base) {
#if 0
  std::map<Opcode, const OpcodeInfo*> exmap;
  exmap.insert(std::make_pair(Opcodes::add, &Opcodes::add));
  auto query = exmap.find(this->opcode);
  auto opcode = query == exmap.end() ? &Opcodes::unknown : query->second;
  printf("%3d %-10s", this - base, opcode->name);
  if (opcode == ) printf(" jump to %d", arg1 - base);
  if (opcode->hasResult)
    printf(" |%3d| {%04x (%04x) : %04x} [%2d]", key - base, ~invalidRegs,
           preferredRegs, reg, pressure);
  if (opcode->hasArg0) {
    printf(" (%d", arg0 - base);
    if (!arg0Live)
      printf("*");
    if (opcode->hasArg1) {
      printf(", %d", arg1 - base);
      if (!arg1Live)
        printf("*");
    }
    printf(")");
  }
  if (opcode->isIntLiteral)
    printf(" %-3d ", arg0);
  printf("\n");
#endif
  printf("%3d > %s", this - base, OpcodeNames[this->opcode.code]);
  if (this->arg0) printf(" : %d", this->getArg0() - base);
  if (this->arg1) printf(", %d", this->getArg1() - base);
  printf("\n");
}

void print(Instruction* instrs, size_t numInstrs) {
  for (auto i = instrs, e = instrs + numInstrs; i != e; ++i)
    i->print(instrs);
}
}
#endif