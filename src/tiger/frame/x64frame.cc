#include "tiger/frame/x64frame.h"

extern frame::RegManager* reg_manager;

namespace frame {
/* TODO: Put your lab5 code here */
// InFrameAccess,InRegAccess,X64Frame的声明都放到x64frame.h
// 这样才有代码提示

Frame* FrameFatory::NewFrame(temp::Label* label,
                             const std::list<bool>& formals) {
  return new X64Frame(label, formals);
}

frame::Access* X64Frame::allocLocal(bool escape) {
  frame::Access* local;
  if (escape) {
    // 如果是逃逸变量，申请内存空间，栈帧向低地址方向移动一个wordsize
    offset -= reg_manager->WordSize();
    local = new InFrameAccess(offset);
  } else {
    // 如果不是逃逸变量，直接使用寄存器
    local = new InRegAccess(temp::TempFactory::NewTemp());
  }
  return local;
}

}  // namespace frame