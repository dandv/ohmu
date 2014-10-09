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

// Note: approach for handling pressure that exceeds the number of bits we have
// in our "registerSet" bitfields:  Given that we use a bitmask to store the
// remaining valid registers for a given live range, if that set becomes all 0
// we cannot allocate a register for this register and a "spill" must occur.  I
// use "spill" in quotes because we may have more registers than we do bits.
// Once we've completed a pass of register allocation we will have allocated
// registers up to the bitwidth of our registerSet bit set.  Everything that
// couldn't have been allocated will have a 0 in their register field.  We can
// run another pass to allocate these independantly, ignoring the already
// allocated liveRanges.  These passes can continue to allocate registers if
// registers exist or they can allocate spill slots.  After some number of
// finite passes, the allocator can assign fixed slots to the remaining live
// ranges.

// Copying at an instruction:  Sometimes it's necessary to copy arguments (e.g.
// because they're destroyed by the instruction but still live) and results
// (because they are used multiple times).  Sometimes they instruction will need
// additional registers to perform these copies e.g. c = (a - b) + a;  The sub
// will destroy a, but the add needs it.  However, if the register allocator
// allocates a and b to the same registers as the continuing a and c then we
// must destroy the register that contains b.  However the semantics of x86 say
// we must destroy the register contining a.  Given that we can only see two
// registers and we need 3 (storage for a, result (c) and argument b) we must
// allocate a register or spill slot.  This occurs after general register
// allocation and require us knowing which registers are free at point in which
// the instruction executes.

// Live ranges:  Each instruction result (SSA value) can have multiple live
// ranges.  These live ranges can, in general, overlap.  Overlapping must occur
// at a point when a destructive operation consumes a value in the middle of its
// live range.  

// Live range splitting/merging: Using the model that each use corresponds to a live
// range leads to an inefficiency if we use the same argument twice for an
// instruction.  Each use constitutes a 'different' live range even though it
// shouldn't.

// Literal hoisting:  In general, it's not necessary to allocate registers for
// literals.  They can use one

#include "types.h"
#include "interface.h"
#include <vector>

using namespace clang::threadSafety::til;
//using namespace Jagger;

#ifdef _MSC_VER
extern "C" unsigned char _BitScanForward(unsigned long * Index, unsigned long Mask);
#endif

static int lowIndex(unsigned x) {
  unsigned long a;
  _BitScanReverse(&a, (unsigned long)x);
  return (int)a;
}

template <typename Type>
struct RangeAdaptor {
  RangeAdaptor(Type* data, size_t size) : _begin(data), _end(data + size) {}
  RangeAdaptor(Type* begin, Type* end) : _begin(begin), _end(end) {}
  Type* begin() const { return _begin; }
  Type* end() const { return _end; }

 private:
  Type* _begin;
  Type* _end;
};

template <typename Type>
RangeAdaptor<Type> AdaptRange(Type* data, size_t size) {
  return RangeAdaptor<Type>(data, size);
}

template <typename Type>
RangeAdaptor<Type> AdaptRange(Type* begin, Type* end) {
  return RangeAdaptor<Type>(begin, end);
}

struct LiveRange {
  struct Iterator {
    Iterator(EventStream events, size_t index)
        : events(events), index(index), skipUntil(index) {}
    EventRef operator*() const { return events[index]; }
    Iterator& operator++() {
      if (events.codes[index] == HEADER_DOMINATES)
        skipUntil = events.data[index];
      else if (events.codes[index] == HEADER && index <= skipUntil)
        index = events.data[index];
      index--;
      return *this;
    }
    bool operator!=(const Iterator& a) const { return index != a.index; }

   private:
     EventStream events;
    size_t index;
    size_t skipUntil;
  };

  LiveRange(EventStream events, size_t def, size_t use)
      : events(events), def(def), use(use) {}
  Iterator begin() const { return Iterator(events, use - 1); }
  Iterator end() const { return Iterator(EventStream(), def); }

 private:
  EventStream events;
  size_t def;
  size_t use;
};

namespace Jagger {
Opcode countedMarker;
}  // namespace Jagger

//namespace {
struct RegisterAllocator {
  void encode(SCFG* const* cfg, size_t numCFGs);

  static size_t countEvents(SExpr* expr);
  static size_t emitBlockHeader(EventStream events, size_t index, Block* block);
  static size_t emitArgument   (EventStream events, size_t index, Phi* phi);
  static size_t emitEvents     (EventStream events, size_t index, SExpr* expr);
  static size_t emitTerminator (EventStream events, size_t index, Terminator* term, BasicBlock* basicBlock);

  Block* blocks;
  size_t numBlocks;
  EventStream events;
  size_t numEvents;
  std::vector<std::pair<Data, Data>> conflicts;
};
//}  // namespace

#if 0
void InstructionStream::traverse(Instruction* use, Instruction* def) {
  Instruction* skipUntil = use;
  auto defIndex = (int)(def - instrs);
  auto keyIndex = defIndex + def->key;
#if 0
  for (Instruction* i = use - 1; i > def; --i) {
    if (i->opcode.code == Opcode::HEADER) {
      if (i->opcode.flags)
        skipUntil = i + i->arg1;
      else if (i <= skipUntil)
        i = i + i->arg1;
      continue;
    }
    if (!i->opcode.hasResult) continue;
    if (def->key == i->key) continue;
    auto iKeyIndex = (int)(i->getKey() - instrs);
    interactions.push_back(std::make_pair(std::min(keyIndex, iKeyIndex),
                                          std::max(keyIndex, iKeyIndex)));
  }
#else
  for (auto instr : LiveRange(def, use)) {
    if (!instr.opcode.aliasSet) continue;
    if (def->key == instr.key) continue;
    auto iKeyIndex = (int)(instr.getKey() - instrs);
    interactions.push_back(std::make_pair(std::min(keyIndex, iKeyIndex),
                                          std::max(keyIndex, iKeyIndex)));
  }
#endif
}
#endif

void RegisterAllocator::encode(SCFG* const* cfgs, size_t numCFGs) {
  if (!numCFGs) return;

  {
    // Count the blocks.
    size_t numBlocks = 0;
    for (auto cfg : AdaptRange(cfgs, numCFGs))
      numBlocks += cfg->numBlocks();
    this->numBlocks = numBlocks;
  }

  assert(numBlocks);
  blocks = new Block[numBlocks];

  {
    // Initialize blocks and count instructions.
    Block* nextBlock = blocks;
    size_t numEvents = 0;
    for (auto cfg : AdaptRange(cfgs, numCFGs))
      for (auto basicBlock : *cfg) {
      basicBlock->setBackendID(nextBlock);
      nextBlock->dominator = nullptr;
      nextBlock->head = nextBlock;
      size_t size = 0;
      if (BasicBlock* parent = basicBlock->parent()) {
        size++;  // block header
        nextBlock->dominator = (Block*)parent->getBackendID();
        if (basicBlock->PostDominates(*parent) ||
          nextBlock->dominator + 1 == nextBlock)
          nextBlock->head = nextBlock->dominator->head;
      }
      for (auto arg : basicBlock->arguments()) size += countEvents(arg);
      for (auto instr : basicBlock->instructions())
        size += countEvents(instr);
      size += countEvents(basicBlock->terminator());
      nextBlock->firstEvent = numEvents;
      nextBlock->numEvents = size;
      numEvents += size;
      nextBlock++;
      }
    this->numEvents = numEvents;
  }

  assert(numEvents);
  events.codes = new Opcode[numEvents];
  events.data = new Data[numEvents];

  {
    // Emit instructions.
    size_t index = 0;
    Block* block = blocks;
    for (auto cfg : AdaptRange(cfgs, numCFGs))
      for (auto basicBlock : *cfg) {
      assert(block->firstEvent == index);
      index = emitBlockHeader(events, index, block);
      for (auto arg : basicBlock->arguments())
        index = emitArgument(events, index, cast<Phi>(arg));
      for (auto instr : basicBlock->instructions())
        index = emitEvents(events, index, instr);
      index = emitTerminator(events, index, basicBlock->terminator(),
        basicBlock);
      assert(index == block->firstEvent + block->numEvents);
      block++;
      }
  }

  // Verify integrety of copies
  for (size_t i = 0; i < numEvents; ++i) {
    if (events[i].code == USE) {
      size_t def = events[i].data;
      assert(events[def].code & VALUE);
      assert(events[def].code & FIXED || events[def].data == def);
    }
    if (events[i].code >= COPY)
      assert(events[i - 1].code == USE);
  }

  // Link copies.
  for (size_t i = 0; i < numEvents; ++i) {
    if (events[i].code >= COPY) {
      size_t def = events[i - 1].data;
      // max is deterministic in parallel.
      if (!(events[def].code & FIXED))
        events[def].data = std::max(events[def].data, i);
    }
  }

  // Traverse the keys.
  for (size_t i = 0; i < numEvents; ++i) {
    // TOOD: fuse these tests
    if (events[i].code & VALUE && !(events[i].code & FIXED)) {
      size_t key = i;
      do {
        key = events[key].data;
        assert(events[key].code & VALUE);
      } while (!(events[key].code & FIXED) && events[key].data != key);
      events[i].data = (Data)key;
    }
  }

  // Determine last uses.
  for (size_t i = 0; i < numEvents; ++i) {
    if (events[i].code != USE) continue;
    size_t def = events[i].data;
    if (events[def].code & FIXED) {
      events[i].code = MUTED_USE;
      continue;
    }
    for (auto event : LiveRange(events, def, i)) {
      if (event.code == USE && event.data == def) {
        event.code = MUTED_USE;
        // TODO: terminate here if we can.
      }
    }
  }

  // Unlink conflicting copies and mark conflicts.
  for (size_t i = 0; i < numEvents; ++i) {
    if (events[i].code != USE) continue;
    size_t def = events[i].data;
    size_t key = events[def].data;
    for (auto event : LiveRange(events, def, i))
      if (event.code & VALUE) {
        if (!(event.code & FIXED) && event.data == key)
          event.data = &event.code - events.codes;
        size_t other = event.data;
        conflicts.push_back(
            std::make_pair(std::min(key, other), std::max(key, other)));
      }
  }

  // Find the keys.
  std::vector<Data> keys;
  std::vector<Data> fixed;
  for (size_t i = 0; i < numEvents; ++i) {
    if (!(events[i].code & VALUE)) continue;
    if (events[i].code & FIXED)
      fixed.push_back(i);
    else if (events[i].data == i) {
      events[i].code |= KEY;
      events[i].data = 0;
      keys.push_back(i);
    }
  }
  for (auto i : fixed) printf("fixed : %3d [%d]\n", i, events[i].data);
  for (auto i : keys) printf("key : %3d\n", i);

  std::sort(conflicts.begin(), conflicts.end());
  std::vector<std::pair<Data, Data>> uniqued;
  uniqued.push_back(*conflicts.begin());
  for (auto i = conflicts.begin() + 1, e = conflicts.end(); i != e; ++i)
    if (*i != i[-1]) uniqued.push_back(*i);
  for (auto i : uniqued) printf("> %d, %d\n", i.first, i.second);

  for (auto i : uniqued) {
    events[i.first].data++;
    events[i.second].data++;
  }

#if 0
  for (Instruction* i = instrs, *e = instrs + numInstrs; i != e; ++i) {
    if (i->opcode == &globalOpcodes.phi)
      continue;
    if (i->opcode->hasArg0 && !i->arg0Live) traverse(i, i->arg0);
    if (i->opcode->hasArg1 && !i->arg1Live) traverse(i, i->arg1);
  }

  assert(!interactions.empty());
  // We should really just return in this case.

  std::sort(interactions.begin(), interactions.end());
  std::vector<std::pair<int, int>> uniqued;
  uniqued.push_back(*interactions.begin());
  for (auto i = interactions.begin() + 1, e = interactions.end(); i != e; ++i)
    if (*i != i[-1]) uniqued.push_back(*i);
  for (auto i : uniqued) printf("> %d, %d\n", i.first, i.second);
  for (auto i : uniqued) {
    instrs[i.first].pressure++;
    instrs[i.second].pressure++;
  }

  std::vector<std::<int, int>> shared;
  for (Instruction* i = instrs, *e = instrs + numInstrs; i != e; ++i)
    if (i->opcode == &globalOpcodes.add || i->opcode == &globalOpcodes.mul ||
        i->opcode == &globalOpcodes.cmpeq) {
    shared.push_back(std::make_pair());
    }

  std::vector<Instruction*> work;
  for (auto i = instrs, e = instrs + numInstrs; i != e; ++i) work.push_back(i);
  std::sort(work.begin(), work.end(), [](Instruction* a, Instruction* b) {
    return a->pressure < b->pressure || a->pressure == b->pressure && a < b;
  });
  for (auto i : work) printf("%d : %d\n", i - instrs, i->pressure);

  for (auto& i : uniqued)
    if (instrs[i.first].pressure > instrs[i.second].pressure ||
        instrs[i.first].pressure == instrs[i.second].pressure &&
            i.first > i.second) {
      i = std::make_pair(i.second, i.first);
      printf("*");
    } else
      printf(".");
  printf("\n");

  // for (auto i : uniqued) printf("> %d, %d\n", i.first, i.second);
  // printf("\n");
  std::sort(uniqued.begin(), uniqued.end(),
            [&](std::pair<int, int> a, std::pair<int, int> b) {
    return instrs[a.first].pressure < instrs[b.first].pressure ||
           instrs[a.first].pressure == instrs[b.first].pressure &&
               a.first < b.first;
  });
  for (auto i : uniqued) printf("> %d, %d\n", i.first, i.second);

  auto interaction = uniqued.begin();
  for (auto i : work) {
    i->reg = ~i->invalidRegs & -~i->invalidRegs;
    while (interaction != uniqued.end() && instrs + interaction->first == i)
      instrs[interaction++->second].invalidRegs |= i->reg;
  }

#if 1
      // for debugging
      std::vector<std::pair<int, int>> ranges;
  auto point = uniqued.begin();
  for (auto i = uniqued.begin() + 1, e = uniqued.end(); i != e; ++i)
    if (i->first != i[-1].first) {
      ranges.push_back(
          std::make_pair(point - uniqued.begin(), i - uniqued.begin()));
      point = i;
    }
  ranges.push_back(std::make_pair(point - uniqued.begin(), uniqued.size()));
  for (auto i : ranges) printf("[%d, %d) : %d : %d\n", i.first, i.second, uniqued[i.first].first, i.second - i.first);
#endif

  printf("blocks = %d\ninstrs = %d\n", (int)numBlocks, (int)numInstrs);
  for (size_t i = 0; i < numBlocks; i++)
    printf("block%d\n  parent = %d\n  first = %d\n  instrs = %d\n", (int)i,
           blocks[i].dominator ? (int)(blocks[i].dominator - blocks) : -1,
           (int)(blocks[i].instrs - instrs), (int)blocks[i].numInstrs);
#endif

#if 0
  // Patch up all of the jump targets.
  for (auto block : *cfg) {
    if (!block->terminator())
      continue;
    switch (block->terminator()->opcode()) {
    case COP_Goto:
      instrs[block->VX64BlockEnd].arg1 =
        cast<Goto>(block->terminator())->targetBlock()->VX64BlockStart - block->VX64BlockEnd;
      break;
    case COP_Branch:
      instrs[block->VX64BlockEnd].arg1 =
        cast<Branch>(block->terminator())->elseBlock()->VX64BlockStart - block->VX64BlockEnd;
      break;
    }
  }
#endif
}

Opcode countedMarker;

size_t RegisterAllocator::countEvents(SExpr* expr) {
  if (expr->getBackendID()) return 0;
  expr->setBackendID(&countedMarker);
  switch (expr->opcode()) {
    case COP_Literal:
      return 2;
    case COP_Variable:
      return countEvents(cast<Variable>(expr)->definition());
    case COP_BinaryOp: {
      int size = countEvents(cast<BinaryOp>(expr)->expr0()) +
                 countEvents(cast<BinaryOp>(expr)->expr1());
      switch (cast<BinaryOp>(expr)->binaryOpcode()) {
      case BOP_Add: return size + 6;
      case BOP_Sub: return size + 7;
      case BOP_Mul: return size + 10;
      case BOP_Eq: return size + 4;
      case BOP_Lt: return size + 4;
      case BOP_Leq: return size + 4;
      }
      assert(false);
      return 0;
    }
    case COP_Phi:
      return 1;
    case COP_Goto:
      return cast<Goto>(expr)->targetBlock()->arguments().size() * 2 + 1;
    case COP_Branch:
      return countEvents(cast<Branch>(expr)->condition()) + 2;
    case COP_Return:
      return countEvents(cast<Return>(expr)->returnValue()) + 3;
    default:
      printf("unknown opcode: %d\n", expr->opcode());
      assert(false);
      return 0;
  }
}

size_t RegisterAllocator::emitBlockHeader(EventStream events, size_t index,
                                          Block* block) {
  if (!block->dominator) return index;
  if (block->head != block)
    events[index++] = Event(HEADER_DOMINATES, block->head->firstEvent);
  else
    events[index++] = Event(
        HEADER, block->dominator->firstEvent + block->dominator->numEvents);
  return index;
}

size_t RegisterAllocator::emitArgument(EventStream events, size_t index, Phi* phi) {
  size_t result = index;
  events[index++] = Event(PHI, result);
  phi->setBackendID(&events[result].code);
  return index;
}

static size_t getBackendID(EventStream events, SExpr* expr) {
  return (size_t)((Opcode*)expr->getBackendID() - events.codes);
}

size_t RegisterAllocator::emitEvents(EventStream events, size_t index, SExpr* expr) {
  if (expr->getBackendID() != &countedMarker) return index;
  size_t result = 0;
  switch (expr->opcode()) {
    case COP_Literal: {
      Literal* literal = cast<Literal>(expr);
      switch (literal->valueType().Base) {
        case ValueType::BT_Int:
          result = index;
          events[index++] = Event(VALUE | ALIAS_GENERAL, result);
          events[index++] = Event(INT32, (Data)literal->as<int>().value());
          break;
        default:
          assert(false);
      }
      break;
    }
    case COP_Variable: {
      auto definition = cast<Variable>(expr)->definition();
      index = emitEvents(events, index, definition);
      result = (Opcode*)definition->getBackendID() - events.codes;
      break;
    }
    case COP_BinaryOp: {
      auto binaryOp = cast<BinaryOp>(expr);
      index = emitEvents(events, index, binaryOp->expr0());
      index = emitEvents(events, index, binaryOp->expr1());
      printf("?? %d : %d %d\n", binaryOp->binaryOpcode(),
             binaryOp->expr0()->opcode(),
             binaryOp->expr1()->opcode());
      auto arg0 = getBackendID(events, binaryOp->expr0());
      auto arg1 = getBackendID(events, binaryOp->expr1());
      switch (binaryOp->binaryOpcode()) {
        case BOP_Add:
          // TODO: add cases for types
          // TODO: sort out the lea/add issue
          result = index + 3;
          events[index++] = Event(USE, arg0);
          events[index++] = Event(COPY | ALIAS_GENERAL, result);
          events[index++] = Event(USE, arg1);
          events[index++] = Event(VALUE | ALIAS_GENERAL, result);
          events[index++] = Event(VALUE | ALIAS_FLAGS, result + 1);
          events[index++] = Event(ADD, 0);
          break;
        case BOP_Sub:
          // TODO: add cases for types
          result = index + 2;
          events[index++] = Event(USE, arg0);
          events[index++] = Event(COPY | ALIAS_GENERAL, result - 1);
          events[index++] = Event(VALUE | ALIAS_GENERAL, result);
          events[index++] = Event(USE, arg1);
          events[index++] = Event(COPY | ALIAS_GENERAL, arg1);
          events[index++] = Event(VALUE | ALIAS_FLAGS, result + 3);
          events[index++] = Event(ADD, 0);
          break;
        case BOP_Mul:
          result = index + 7;
          events[index++] = Event(USE, arg0);
          events[index++] = Event(COPY | FIXED | ALIAS_GENERAL, 0);
          events[index++] = Event(USE, arg1);
          events[index++] = Event(COPY | FIXED | ALIAS_GENERAL, 1);
          events[index++] = Event(VALUE | FIXED | ALIAS_GENERAL, 0);
          events[index++] = Event(VALUE | FIXED | ALIAS_GENERAL, 1);
          events[index++] = Event(USE, result - 3);
          events[index++] = Event(COPY | ALIAS_GENERAL, result);
          events[index++] = Event(VALUE | ALIAS_FLAGS, result + 1);
          events[index++] = Event(MUL, 0);
          break;
        case BOP_Eq:
          result = index + 2;
          events[index++] = Event(USE, arg0);
          events[index++] = Event(USE, arg1);
          events[index++] = Event(VALUE | ALIAS_FLAGS, result);
          events[index++] = Event(EQ, 0);
          break;
        case BOP_Lt:
          result = index + 2;
          events[index++] = Event(USE, arg0);
          events[index++] = Event(USE, arg1);
          events[index++] = Event(VALUE | ALIAS_FLAGS, result);
          events[index++] = Event(LT, 0);
          break;
        case BOP_Leq:
          result = index + 2;
          events[index++] = Event(USE, arg0);
          events[index++] = Event(USE, arg1);
          events[index++] = Event(VALUE | ALIAS_FLAGS, result);
          events[index++] = Event(LE, 0);
          break;
        default:
          assert(false);
      }
      break;
    }
    default:
      printf("unknown opcode: %d\n", expr->opcode());
      assert(false);
      return 0;
  }
  expr->setBackendID(&events[result].code);
  return index;
}

// the index for this block in the target's phis
// TODO: make this not a search;
static size_t getPhiIndex(BasicBlock* basicBlock, BasicBlock* targetBlock) {
  auto& predecessors = targetBlock->predecessors();
  for (size_t i = 0, e = predecessors.size(); i != e; ++i)
    if (predecessors[i] == basicBlock) return i;
  return 0;
}

size_t RegisterAllocator::emitTerminator(EventStream events,
                                         size_t index, Terminator* term,
                                         BasicBlock* basicBlock) {
  size_t result = 0;
  switch (term->opcode()) {
    case COP_Goto: {
      auto jump = cast<Goto>(term);
      auto targetBasicBlock = jump->targetBlock();
      auto phiIndex = getPhiIndex(basicBlock, targetBasicBlock);
      auto& arguments = targetBasicBlock->arguments();

      auto numArguments = (int)arguments.size();
      //auto block = (Block*)basicBlock->getBackendID();
      auto targetBlock = (Block*)targetBasicBlock->getBackendID();
      auto targetPhiIndex = targetBlock->firstEvent + 1;

      // This loop should emit nothing! TODO: validate and remove
      // for (auto arg : arguments)
      //  nextEvent = emitEvents(nextEvent, cast<Phi>(arg)->values()[phiIndex]);
      for (auto arg : arguments) {
        size_t arg0 = getBackendID(events, cast<Phi>(arg)->values()[phiIndex]);
        events[index++] = Event(USE, arg0);
        events[index++] = Event(COPY | ALIAS_GENERAL, targetPhiIndex++);
      }
      events[result = index++] = Event(JUMP, 0);
      break;
    }
    case COP_Branch: {
      auto branch = cast<Branch>(term);
      auto condition = branch->condition();
      index = emitEvents(events, index, condition);
      events[index++] = Event(USE, (Opcode*)condition->getBackendID() - events.codes);
      events[result = index++] = Event(BRANCH, 0);
      break;
    }
    case COP_Return: {
      auto ret = cast<Return>(term);
      auto value = ret->returnValue();
      index = emitEvents(events, index, value);
      events[index++] = Event(USE, (Opcode*)value->getBackendID() - events.codes);
      events[index++] = Event(COPY | FIXED | ALIAS_GENERAL, 0);
      events[result = index++] = Event(RET, 0);
      break;
    }
  }
  term->setBackendID(&events[result].code);
  return index;
}

#if 0

#if 0
namespace {
struct Pool {
  Pool() : data((char*)malloc(0x100)), capacity(0x100), size(0) {}

  template <typename T>
  T* allocate(size_t quantity) {
    assert(!(sizeof(T) % 4) && __alignof(T) <= 4);
    size_t offset = size;
    if (quantity) {
      size += sizeof(T) * quantity;
      while (size > capacity) data = (char*)realloc(data, capacity *= 2);
    }
    return (T*)(data + offset);
  }

  size_t getSize() const { return size; }
  int getOffset() const { assert((int)size == size); return (int)size; }
  char* getBase() const { return data; }

 private:
  char* data;
  size_t capacity;
  size_t size;
};
#endif

struct InstructionStream {
#if 0
  int getNewInstr() const { return pool.getOffset(); }
  int getLastInstr() const { return getNewInstr() - sizeof(Instruction); }
  int getNewBlock() const { return pool.getOffset(); }
  int getLastBlock() const { return getNewBlock() - sizeof(Block); }
#endif

  int countBytes(SCFG* cfg) const;
  int countInstrs(SExpr* expr) const;
  int countInstrs(BasicBlock* block) const;

#if 0
  void emitBlock(BasicBlock* block);
  //void emitBlockLink(BasicBlock* block);
  void emitEchos(Phi* phi);
  void emitPhi(Phi* phi);
  void encode(SCFG* cfg);

  int emitExpression(SExpr* expr);
  void emitTerminator(BasicBlock* basicBlock);

  void emitLiteral(Literal* literal);
  void emitBinaryOp(BinaryOp* binaryOp);
  void emitJump(BasicBlock* basicBlock, Goto* jump);
  void emitBranch(BasicBlock* basicBlock, Branch* branch);

  void printWalk(Instruction* instr, Instruction* target);
  void printWalks();
#endif

#if 0
  void computePressure(Event* event);
  unsigned computeValidRegs(Event* event, Event*& source);
  void markInvalidRegs(Event* event);
  void propegateCopies(Event* source, unsigned reg);
  void untwistPairs(Event* event);
#endif

private:
  //Block* currentBlock;
  //Pool pool;
  //void* root;
  void* root;
};
} // namespace

int InstructionStream::countBytes(SCFG* cfg) const {
  int numBlocks = 0;
  int numInstrs = 0;
  for (auto block : *cfg) {
    numBlocks++;
    numInstrs += countInstrs(block);
  }
  printf("blocks = %d\ninstrs = %d\n", numBlocks, numInstrs);
  return numBlocks * (int)sizeof(Block) + numInstrs * (int)sizeof(Instruction);
}

int InstructionStream::countInstrs(BasicBlock* block) const {
  int x = 0;
  // count the number of phi instructions.  Because phi instructions are binary
  // we need n-1 of them to make a tree of phi/tie instructions with n leaves.
  x += block->arguments().size() * (block->predecessors().size() - 1);
  for (auto instr : block->instructions()) x += countInstrs(instr);
  auto terminator = block->terminator();
  if (!terminator)
    x++;  // ret instruction special case
  else
    x += countInstrs(terminator);
  return x;
}

int InstructionStream::countInstrs(SExpr* expr) const {
  if (expr->id()) return 0;
  expr->setId(1);  // all ids < 4 are special
  switch (expr->opcode()) {
    case COP_Literal:
      return 1;
    case COP_Variable:
      return countInstrs(cast<Variable>(expr)->definition());
    case COP_BinaryOp:
      return countInstrs(cast<BinaryOp>(expr)->expr0()) +
             countInstrs(cast<BinaryOp>(expr)->expr1()) + 1;
    case COP_Goto:
      return cast<Goto>(expr)->targetBlock()->arguments().size() + 1;
    case COP_Branch:
      return countInstrs(cast<Branch>(expr)->condition()) + 1;
    default:
      assert(false);
      return 0;
  }
}

void InstructionStream::emitBlock(BasicBlock* block) {
  int blockID = block->blockID();
  assert(blockID == (int)blocks.size());
  BasicBlock* parent = block->DominatorNode.Parent;
  while (parent && block->PostDominates(*parent)) // TODO: || parent is previous block
    parent = parent->DominatorNode.Parent;
  blocks.push_back(Block(parent ? parent->blockID() - blockID : 0, getNewID()));
  currentBlock = &blocks.back();
  for (auto arg : block->arguments())
    emitPhi(cast<clang::threadSafety::til::Phi>(arg->definition()));
  for (auto instr : block->instructions())
    emitExpression(instr);
  emitTerminator(block);
  currentBlock->lastInstr = getLastID();
}

#if 0
void InstructionStream::emitBlockLink(BasicBlock* block) {
  if (!block->DominatorNode.Parent)
    return;
  int targetOffset = block->DominatorNode.Parent->VX64BlockEnd - getNewID();
  // In this case the previous block is immediately prior and we don't need to
  // walk or skip.
  if (targetOffset == -1)
    return;
  if (block->PostDominates(*block->DominatorNode.Parent))
    events.push_back(Link().initWalkBack(targetOffset));
  else
    events.push_back(Link().initSkipBack(targetOffset));
}
#endif

void InstructionStream::emitPhi(Phi* phi) {
  int id = getNewID();
  if (phi->values().size() == 1) {
    // TODO: eliminate this case in the middle-end
    phi->setId((phi->values()[0])->id());
    return;
  }
  assert(phi->values().size() == 2);
  instrs.push_back(Instruction(
      currentBlock,
      &OpCodes::phi,
      cast<Variable>(phi->values()[0])->id() - id,
      cast<Variable>(phi->values()[1])->id() - id));
  phi->setId(id);
}

void InstructionStream::emitLiteral(Literal* literal) {
  switch (literal->valueType().Base) {
  case ValueType::BT_Int:
    instrs.push_back(Instruction(
      currentBlock,
      &OpCodes::intValue,
      literal->as<int>().value()));
    break;
  default:
    assert(false);
  }
}

void InstructionStream::emitBinaryOp(BinaryOp* binaryOp) {
  int expr0ID = emitExpression(binaryOp->expr0());
  int expr1ID = emitExpression(binaryOp->expr1());
  const OpCode* opcode = &OpCodes::nop;
  switch (binaryOp->binaryOpcode()) {
  case BOP_Add: opcode = &OpCodes::add; break;
  case BOP_Mul: opcode = &OpCodes::mul; break;
  case BOP_Eq : opcode = &OpCodes::cmpeq; break;
  case BOP_Lt : opcode = &OpCodes::cmplt; break;
  case BOP_Leq: opcode = &OpCodes::cmple; break;
  default:
    assert(false);
  }
  int site = getNewID();
  instrs.push_back(Instruction(currentBlock, opcode, expr0ID - site, expr1ID - site));
}

int InstructionStream::emitExpression(SExpr* expr) {
  if (expr->id())
    return expr->id();
  switch (expr->opcode()) {
  case COP_Literal: emitLiteral(cast<Literal>(expr)); break;
  case COP_Variable: emitExpression(cast<Variable>(expr)->definition()); break;
  case COP_BinaryOp: emitBinaryOp(cast<BinaryOp>(expr)); break;
  }
  expr->setId(getLastID());
  return expr->id();
}

void InstructionStream::emitTerminator(BasicBlock* basicBlock) {
  auto expr = basicBlock->terminator();
  if (!expr) {
    // Presently Ohmu IR doesn't have/use ret instructions.
    // We should figure out how to differentiate functions that return values
    // and those that don't.
    //assert(!instrs.empty() && instrs.back().opcode->hasResult);
    //assert(basicBlock->instructions().size() != 0);
    instrs.push_back(Instruction(currentBlock, &OpCodes::ret, -1));
    return;
  }
  switch (expr->opcode()) {
  case COP_Goto: emitJump(basicBlock, cast<Goto>(expr)); break;
  case COP_Branch: emitBranch(basicBlock, cast<Branch>(expr)); break;
  }
}

// the index for this block in the target's phis
static size_t getPhiIndex(BasicBlock* basicBlock, BasicBlock* targetBlock) {
  auto& predecessors = targetBlock->predecessors();
  for (size_t i = 0, e = predecessors.size(); i != e; ++i)
    if (predecessors[i] == basicBlock)
      return i;
  return 0;
}

void InstructionStream::emitJump(BasicBlock* basicBlock, Goto* jump) {
  auto targetBlock = jump->targetBlock();
  size_t phiIndex = getPhiIndex(basicBlock, targetBlock);
  auto& arguments = targetBlock->arguments();
  for (auto arg : arguments) {
    SExpr* expr = cast<Phi>(arg->definition())->values()[phiIndex];
    int argid = emitExpression(expr);
    int echoid = getNewID();
    instrs.push_back(Instruction(currentBlock, &OpCodes::echo, argid - echoid));
    expr->setId(echoid);
  }
  instrs.push_back(Instruction(currentBlock, &OpCodes::jump));
}

void InstructionStream::emitBranch(BasicBlock* basicBlock, Branch* branch) {
  // There should be no critical edges.
  emitExpression(branch->condition());
  instrs.push_back(Instruction(currentBlock, &OpCodes::branch,
                               branch->condition()->id() - getNewID()));
}

void InstructionStream::encode(SCFG* cfg) {
  for (auto block : *cfg)
    emitBlock(block);
  instrs.push_back(Instruction(currentBlock, &OpCodes::nop));
  for (auto block : *cfg) {
    Block* b = new Block();
    // TODO:b.init();
    emitBlock(block);
  }
  // Patch up all of the jump targets.
  for (auto block : *cfg) {
    if (!block->terminator())
      continue;
    switch (block->terminator()->opcode()) {
    case COP_Goto:
      instrs[block->VX64BlockEnd].arg1 =
          cast<Goto>(block->terminator())->targetBlock()->VX64BlockStart - block->VX64BlockEnd;
      break;
    case COP_Branch:
      instrs[block->VX64BlockEnd].arg1 =
          cast<Branch>(block->terminator())->elseBlock()->VX64BlockStart - block->VX64BlockEnd;
      break;
    }
  }
#if 0
  // Compute pressure generated by the pairs.
  for (auto& event : instrs) {
    if (event.kind != Object::USE)
      continue;
    computePressure(&event);
  }
  std::vector<Event*> ptrs;
  for (auto& event : instrs)
    if (event.kind == Object::USE)
      ptrs.push_back(&event);
  std::sort(ptrs.begin(), ptrs.end(), [](Event *a, Event *b) {
    a = a + a->use.offsetToValue;
    b = b + b->use.offsetToValue;
    return a->liveRange.pressure < b->liveRange.pressure;
  });
#endif
#if 0
  for (auto ptr : ptrs) {
    Event* source = nullptr;
    auto registerSet = computeValidRegs(ptr, source);
    unsigned sourceCopySet = source->value.copySet;
    unsigned destCopySet = ptr->value.copySet;
// FIXME if we have c = a - b, a must survive, and the total number of allocated
// registers for c, b, and a is 2 then we have no place to squirl away a during
// computation of c so we need to add an artifical use of a so that we get
// another register allocated as a place to store it.
#if 1
    // printf(">> %x %x %x", registerSet, sourceCopySet, destCopySet);
    if (registerSet & sourceCopySet & destCopySet) {
      registerSet &= sourceCopySet & destCopySet;
      //printf(" <SD>");
    }
    else if (registerSet & destCopySet) {
      registerSet &= destCopySet;
      //printf(" <D>");
    }
    else if (registerSet & sourceCopySet) {
      registerSet &= sourceCopySet;
      //printf(" <S>");
    }
#endif
    unsigned reg = registerSet & -registerSet;
    //printf(" %x : %x\n", registerSet, reg);
    ptr->liveRange.reg = reg;
    markInvalidRegs(ptr);
    propegateCopies(source, reg);
  }
#endif
}

#if 0
void InstructionStream::computePressure(Event* event) {
  Event* walkTo = event;
  for (Event* i = event - 1, *e = event + event->use.offsetToValue; i != e; --i)
    switch (i->kind) {
    case Object::HOLE: break;
    case Object::WALK_BACK:
      walkTo = std::min(walkTo, i + i->link.offsetToTarget);
      break;
    case Object::SKIP_BACK:
      if (i + i->link.offsetToTarget < walkTo)
        i += i->link.offsetToTarget;
      break;
    case Object::USE:
      // TODO: this will double count some values
      if (i < walkTo && i + i->use.offsetToValue == e)
        return;
      break;
    case Object::JUMP:
    case Object::BRANCH: break;
    default:
      assert(i->value.isValue());
      assert((i + i->value.offsetToRep)->value.isValue());
      (i + i->value.offsetToRep)->value.pressure++;
  }
}
#endif

#if 0
unsigned InstructionStream::computeValidRegs(Event* event, Event*& source) {
  unsigned validSet = ~event->liveRange.invalidRegs;
  Event* walkTo = event;
  for (Event* i = event - 1, *e = event + event->liveRange.offsetToOrigin; i != e; --i)
    switch (i->kind) {
    case Object::WALK_BACK:
      walkTo = std::min(walkTo, i + i->link.offsetToTarget);
      break;
    case Object::SKIP_BACK:
      if (i + i->link.offsetToTarget < walkTo)
        i += i->link.offsetToTarget;
      break;
    case Object::LIVE_RANGE:
      if (i < walkTo && i + i->liveRange.offsetToOrigin == e) {
        source = i;
        return validSet;
      }
      validSet &= ~(i->liveRange.reg | i->value.copySet | i->liveRange.invalidRegs);
    }
  source = event + event->liveRange.offsetToOrigin;
  return validSet;
}
#endif

#if 0
void InstructionStream::markInvalidRegs(Event* event) {
  unsigned reg = event->liveRange.reg;
  Event* walkTo = event;
  for (Event* i = event - 1, *e = event + event->liveRange.offsetToOrigin; i != e; --i)
    switch (i->kind) {
    case Object::WALK_BACK:
      walkTo = std::min(walkTo, i + i->link.offsetToTarget);
      break;
    case Object::SKIP_BACK:
      if (i + i->link.offsetToTarget < walkTo)
        i += i->link.offsetToTarget;
      break;
    case Object::LIVE_RANGE:
      if (i < walkTo && i + i->liveRange.offsetToOrigin == e)
        return;
      i->liveRange.invalidRegs |= reg;
    }
}
#endif

#if 0
void InstructionStream::propegateCopies(Event* source, unsigned reg) {
  if (source->kind == Object::PHI) {
    int key = source->phiLink.key;
    do {
      propegateCopies(source + source->phiLink.offsetToTarget, reg);
      --source;
    } while (source->kind == Object::PHI && source->phiLink.key == key);
    return;
  }
  assert(source->object.isValue());
  source->value.copySet |= reg;
}
#endif

#if 0
void InstructionStream::untwistPairs(Event* event) {
  if (event->kind != Object::ADD)
    return;
  unsigned copySet = event->instruction.copySet;
  unsigned reg0 = event[-2].liveRange.reg;
  unsigned reg1 = event[-1].liveRange.reg;
  if (!(reg0 & copySet) && (reg1 & copySet))
    std::swap(event[-2].liveRange.reg, event[-1].liveRange.reg);
}
#endif

void InstructionStream::printWalk(Instruction* instr, Instruction* target) {
  Instruction* first = instrs.data() + instr->block->firstInstr;
}

void InstructionStream::printWalks() {
  //return;
  for (size_t i = 0, e = instrs.size(); i != e; ++i) {
    Instruction* instr = &instrs[i];
    if (!instr->opcode->hasArg0)
      continue;
    Instruction* first = instrs.data() + instr->block->firstInstr;
    //printf("first = %d\n", first - instrs.data());
    if (instr->opcode->hasArg0) {
      Instruction* target = instr + instr->arg0;
      printf("? %d => %d\n", i, i + instr->arg0);
      for (;;) {
        for (; instr != target && instr >= first; instr--)
          printf("%d->", instr - instrs.data());
        if (instr == target)
          break;
        //break;
        const Block* block = instr->block;
        assert(instrs.data() + block->firstInstr == instr + 1);
        if (!block->parent)
          break;
        //printf("block : %d parent : %d current : %d next : %d target : %d\n",
        //  block - blocks.data(), );
        instr += (block + block->parent)->lastInstr - block->firstInstr - 1;
      }
      printf("%d\n", instr - instrs.data());
      instr = &instrs[i];
    }
#if 0
    if (instr->opcode->hasArg1) {
      Instruction* target = instr + instr->arg1;
      printf("? %d => %d\n", i, i + instr->arg1);
      for (;;) {
        for (; instr != target && instr >= first; instr--)
          printf("%d->", instr - instrs.data());
        if (instr == target)
          break;
        //break;
        const Block* block = instr->block;
        assert(instrs.data() + block->firstInstr == instr + 1);
        if (!block->parent)
          break;
        //printf("block : %d parent : %d current : %d next : %d target : %d\n",
        //  block - blocks.data(), );
        instr += (block + block->parent)->lastInstr - block->firstInstr - 1;
      }
      printf("%d\n", instr - instrs.data());
    }
#endif
  }
}

// FIXME put me in a header
//extern void emitASM(X64Builder& builder, Event* events, size_t numEvents);
#endif

void encode(SCFG* cfg, char* output) {
  RegisterAllocator allocator;
  allocator.encode(&cfg, 1);

  print(allocator.events, allocator.numEvents);
  //stream.printWalks();
  //X64Builder builder;
  //emitASM(builder, InstructionStream.events.data(), InstructionStream.events.size());
}
