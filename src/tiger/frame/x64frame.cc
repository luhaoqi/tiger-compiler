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
}

void X64Frame::setViewShift(const std::list<bool>& escapes) {
  tree::Exp *src, *dst;
  tree::Stm* stm;

  // 参数寄存器: %rdi,%rsi,%rdx,%rcx,%r8,%r9
  const auto& reg_list = reg_manager->ArgRegs()->GetList();
  auto it = reg_list.begin();

  int i = 0;  // 第几个超过Maxargs的参数
  const int ws = reg_manager->WordSize();
  temp::Temp* FP = reg_manager->FramePointer();

  // 执行view shift，把所有参数都转移到该函数的虚寄存器中
  // 参数以%rdi,%rsi,%rdx,%rcx,%r8,%r9的顺序排列
  // move t1,%rdi; move t2 %rsi ...
  for (frame::Access* access : formals_) {
    // dst is decided by InRegAccess or InFrameAccess, just invoke ToExp
    dst = access->ToExp(new tree::TempExp(FP));

    // src is decided by if it's first 6 args
    if (it != reg_list.end()) {
      src = new tree::TempExp(*it);
      it++;
    } else {
      // 帧指针现在指向return address
      // 因此栈上的第i个参数的offset是i*wordsize(向上走)
      // MEM( +(FP, i * wordsize) )
      // |  ...  |  arg 8  |  arg 7  |  return address  |  saved rbp (FP) |  ...
      i++;
      src = new tree::MemExp(new tree::BinopExp(tree::BinOp::PLUS_OP,
                                                new tree::TempExp(FP),
                                                new tree::ConstExp(i * ws)));
    }
    // move dst, src
    stm = new tree::MoveStm(dst, src);

    // view_shift保存执行view shift所需要的所有语句，把新的move加入到后面
    view_shift = view_shift ? new tree::SeqStm(view_shift, stm) : stm;
  }
}

// create a new frame
Frame* FrameFactory::NewFrame(temp::Label* label,
                              const std::list<bool>& formals) {
  frame::X64Frame* f = new X64Frame(label, formals);
  f->setViewShift(formals);
  return f;
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

// view shift
tree::Stm* FrameFactory::ProcEntryExit1(Frame* f, tree::Stm* stm) {
  // 直接把生成frame时候的shift view 语句粘贴到stm前面
  return new tree::SeqStm(f->view_shift, stm);
}

// 添加下沉指令(Sink instr), 表示一些寄存器是出口活跃的
assem::InstrList* FrameFactory::ProcEntryExit2(assem::InstrList* body) {
  body->Append(new assem::OperInstr("", new temp::TempList(),
                                    reg_manager->ReturnSink(), nullptr));
  return body;
}

assem::Proc* FrameFactory::ProcEntryExit3(frame::Frame* f,
                                          assem::InstrList* body) {
  static char instr[1024];

  // 原来需要把rbp压入栈，并把rsp赋给rbp作为FP
  // 现在记录一个framesize，FP = SP + framesize
  // 显然framesize = -offset (因为offset是负数，每次申请-wordsize)
  // 需要注意的是如果这个frame内部有call并且参数超过6个(或者说参数传递寄存器的个数)
  // 就需要额外再大一些,为size_for_more_args - offset

  int argRegs = reg_manager->ArgRegs()->GetList().size();
  int size_for_more_args =
      std::max(f->maxArgs - argRegs, 0) * reg_manager->WordSize();

  std::string prolog;
  // .set xx_framesize, $size
  sprintf(instr, ".set %s_framesize, %d\n", f->name_->Name().c_str(),
          -f->offset);
  prolog = std::string(instr);
  // xx:
  sprintf(instr, "%s:\n", f->name_->Name().c_str());
  prolog.append(std::string(instr));
  // subq $size, %rsp
  sprintf(instr, "subq $%d, %%rsp\n", size_for_more_args - f->offset);
  prolog.append(std::string(instr));

  // addq $size, %rsp
  sprintf(instr, "addq $%d, %%rsp\n", size_for_more_args - f->offset);
  std::string epilog = std::string(instr);
  // retq
  epilog.append(std::string("retq\n"));
  return new assem::Proc(prolog, body, epilog);
}

tree::Exp* FrameFactory::externalCall(temp::Label* name, tree::ExpList* args) {
  return new tree::CallExp(new tree::NameExp(name), args);
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
  // 出口活跃寄存器 第9章程序设计部分
  // TODO: 也许可以加上Callee-saved regs?
  temp::TempList* temp_list = CalleeSaves();
  temp_list->Append(StackPointer());  //%rsp
  temp_list->Append(ReturnValue());   //%rax
  return temp_list;
}

/**
 * Get operative registers
 * @return operative registers
 */
[[nodiscard]] temp::TempList* X64RegManager::OperateRegs() {
  // binop操作相关
  // 主要是imul和idiv
  // rax和rdx
  auto temp_list = new temp::TempList();
  temp_list->Append(GetRegister(RegisterName::rax));  //%rax
  temp_list->Append(GetRegister(RegisterName::rdx));  //%rdx
  return temp_list;
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
