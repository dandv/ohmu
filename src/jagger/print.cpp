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

void print(EventStream events, size_t numInstrs) {
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