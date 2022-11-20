//
// Created by wzl on 2021/10/12.
//

#ifndef TIGER_COMPILER_X64FRAME_H
#define TIGER_COMPILER_X64FRAME_H

#include "tiger/frame/frame.h"

namespace frame {
class X64RegManager : public RegManager {
 public:
  /* TODO: Put your lab5 code here */
  X64RegManager() : frame::RegManager() {}
  temp::TempList* Registers() {}
  temp::TempList* ArgRegs() {}
  temp::TempList* CallerSaves() {}
  temp::TempList* CalleeSaves() {}
  temp::TempList* ReturnSink() {}
  int WordSize() {}
  temp::Temp* FramePointer() {}
  temp::Temp* StackPointer() {}
  temp::Temp* ReturnValue() {}
};

class InFrameAccess : public Access {
 public:
  int offset;
  explicit InFrameAccess(int offset) : offset(offset) {}
  tree::Exp* ToExp(tree::Exp* framePtr) const override {
    return new tree::MemExp(
        new tree::BinopExp(tree::BinOp::PLUS_OP, framePtr,
                           new tree::ConstExp(offset)));  // mem(+(fp,offset))
  }
};

class InRegAccess : public Access {
 public:
  temp::Temp* reg;
  explicit InRegAccess(temp::Temp* reg) : reg(reg) {}
  tree::Exp* ToExp(tree::Exp* framePtr) const override {
    return new tree::TempExp(reg);
  }
};

}  // namespace frame
#endif  // TIGER_COMPILER_X64FRAME_H
