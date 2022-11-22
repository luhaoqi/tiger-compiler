#include "tiger/frame/x64frame.h"

extern frame::RegManager* reg_manager;

namespace frame {
/* TODO: Put your lab5 code here */
// InFrameAccess,InRegAccess,X64Frame的声明都放到x64frame.h
// 这样才有代码提示

X64Frame::X64Frame(temp::Label* name, const std::list<bool>& formals)
    : Frame(name) {
  // 根据传入的形参的逃逸属性allocLocal
  for (auto escape : formals) {
    formals_.push_back(allocLocal(escape));
  }
  // TODO: view_shift
}

// create a new frame
Frame* FrameFatory::NewFrame(temp::Label* label,
                             const std::list<bool>& formals) {
  return new X64Frame(label, formals);
}

// alloc local variable in frame
// @escape : if the var is escape
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

// X64RegManager
X64RegManager::X64RegManager() : RegManager() {
  // https://learn.microsoft.com/en-us/windows-hardware/drivers/debugger/x64-architecture
  std::string reg_names[16] = {"%rax", "%rbx", "%rcx", "%rdx", "%rsi", "%rdi",
                               "%rbp", "%rsp", "%r8",  "%r9",  "%r10", "%r11",
                               "%r12", "%r13", "%r14", "%r15"};
  std::vector<std::string*> names;
  // 生成 x86-64 的16个寄存器
  for (int i = 0; i < 16; i++) {
    temp::Temp* reg = temp::TempFactory::NewTemp();
    regs_.push_back(reg);
    temp_map_->Enter(reg, new std::string(reg_names[i]));
  }
}

/**
 * Get general-purpose registers except RSI  (应该是rsp？)
 * NOTE: returned temp list should be in the order of calling convention
 * @return general-purpose registers
 */
[[nodiscard]] temp::TempList* X64RegManager::Registers() {
  auto temp_list = new temp::TempList();
  for (size_t i = 0; i < 16; i++) {
    // 跳过 %rsp
    // rsp作为stack pointer不参与寄存器分配
    if (i == RegisterName::rsp) {
      continue;
    }
    temp_list->Append(GetRegister(i));
  }
  return temp_list;
}
/**
 * Get registers which can be used to hold arguments
 * NOTE: returned temp list must be in the order of calling convention
 * @return argument registers
 */
[[nodiscard]] temp::TempList* X64RegManager::ArgRegs() {
  // 参数寄存器:%rdi,%rsi,%rdx,%rcx,%r8,%r9
  auto temp_list = new temp::TempList();
  temp_list->Append(GetRegister(RegisterName::rdi));  //%rdi
  temp_list->Append(GetRegister(RegisterName::rsi));  //%rsi
  temp_list->Append(GetRegister(RegisterName::rdx));  //%rdx
  temp_list->Append(GetRegister(RegisterName::rcx));  //%rcx
  temp_list->Append(GetRegister(RegisterName::r8));   //%r8
  temp_list->Append(GetRegister(RegisterName::r9));   //%r9
  return temp_list;
}

/**
 * Get caller-saved registers
 * NOTE: returned registers must be in the order of calling convention
 * @return caller-saved registers
 */
[[nodiscard]] temp::TempList* X64RegManager::CallerSaves() {
  auto temp_list = new temp::TempList();
  temp_list->Append(GetRegister(RegisterName::rax));  //%rax
  temp_list->Append(GetRegister(RegisterName::rcx));  //%rcx
  temp_list->Append(GetRegister(RegisterName::rdx));  //%rdx
  temp_list->Append(GetRegister(RegisterName::rsi));  //%rsi
  temp_list->Append(GetRegister(RegisterName::rdi));  //%rdi
  temp_list->Append(GetRegister(RegisterName::r8));   //%r8
  temp_list->Append(GetRegister(RegisterName::r9));   //%r9
  temp_list->Append(GetRegister(RegisterName::r10));  //%r10
  temp_list->Append(GetRegister(RegisterName::r11));  //%r11
}

/**
 * Get callee-saved registers
 * NOTE: returned registers must be in the order of calling convention
 * @return callee-saved registers
 */
[[nodiscard]] temp::TempList* X64RegManager::CalleeSaves() {
  auto temp_list = new temp::TempList();
  temp_list->Append(GetRegister(RegisterName::rbx));  //%rbx
  temp_list->Append(GetRegister(RegisterName::rbp));  //%rbp
  temp_list->Append(GetRegister(RegisterName::r12));  //%r12
  temp_list->Append(GetRegister(RegisterName::r13));  //%r13
  temp_list->Append(GetRegister(RegisterName::r14));  //%r14
  temp_list->Append(GetRegister(RegisterName::r15));  //%r15
  // TODO:这里%rsp存疑
  return temp_list;
}

/**
 * Get return-sink registers
 * @return return-sink registers
 */
[[nodiscard]] temp::TempList* X64RegManager::ReturnSink() {
  // TODO: 不清楚这是什么
}

/**
 * Get word size
 */
int X64RegManager::WordSize() { return 8; }

[[nodiscard]] temp::Temp* X64RegManager::FramePointer() {
  return GetRegister(RegisterName::rbp);
}

[[nodiscard]] temp::Temp* X64RegManager::StackPointer() {
  return GetRegister(RegisterName::rsp);
}

[[nodiscard]] temp::Temp* X64RegManager::ReturnValue() {
  return GetRegister(RegisterName::rax);
}

}  // namespace frame