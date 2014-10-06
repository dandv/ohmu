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

static char OpcodeNames[][16] = {
  "NOP", "PHI", "HEADER", "TIE", "COPY", "SOURCE", "SINK",
  "VALUE", "LOAD", "STORE", "ULOAD", "USTORE", "GATHER", "SCATTER",
  "SEXT", "ZEXT", "FCVT", /* ZEXT/FCVT used for truncation */
  "AND", "OR", "ANDN", "ORN", "XOR", "XNOR", "NAND", "NOR", "NOT", "TEST",
  "SLL", "SLR", "SAR", "ROL", "ROR",
  "MIN", "MAX",
  "ADD", "SUB", "SUBR", "ADDN", "ADC", "SBB", "NEG", "ABS",
  "MUL", "MULHI", "DIV", "MOD", "RCP",
  "AOS", "AOSOA"
  "MADD", "MSUB", "MSUBR", "MADDN",
  "FMADD", "FMSUB", "FMSUBR", "FMADDN",
  "EQ",  "NEQ",  "LT",  "LE",  "ORD",  "EQU",  "NEQU",  "LTU",  "LEU",  "UNORD",
  "JUMP", "BRANCH", "CALL", "RET",
  "BT", "BTS", "BTR", "BTC",
  "CTZ", "CLZ", "POPCNT", /* other bit ops bmi1/bmi2 */
  "SQRT", "RSQRT",
  "SHUFFLE", "BROADCAST", "EXTRACT", "INSERT",
  "MEMSET", "MEMCPY",
};


namespace Jagger {
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