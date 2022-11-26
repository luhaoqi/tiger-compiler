#include "tiger/codegen/codegen.h"

#include <cassert>
#include <sstream>

extern frame::RegManager *reg_manager;

namespace {

constexpr int maxlen = 1024;

}  // namespace

namespace cg {

void CodeGen::Codegen() {
  /* TODO: Put your lab5 code here */
  // set frame size label
  fs_ = frame_->name_->Name() + "_framesize";

  // create new instruction list
  assem::InstrList *instr_list = new assem::InstrList();

  // save callee save register
  temp::TempList *callee_saved_regs = reg_manager->CalleeSaves();
  std::string movq_assem("movq `s0, `d0");
  std::vector<std::pair<temp::Temp *, temp::Temp *>> store_pair;
  for (auto reg : callee_saved_regs->GetList()) {
    auto storeReg = temp::TempFactory::NewTemp();
    instr_list->Append(new assem::MoveInstr(
        movq_assem, new temp::TempList({storeReg}), new temp::TempList({reg})));
    store_pair.emplace_back(reg, storeReg);
  }

  // 遍历trace的stmList 进行Munch
  const auto &trace_stmList = traces_->GetStmList()->GetList();
  for (auto stm : trace_stmList) {
    stm->Munch(*instr_list, fs_);
  }

  // restore callee saved register
  for (auto pair : store_pair) {
    temp::Temp *reg = pair.first;
    temp::Temp *storeReg = pair.second;
    instr_list->Append(new assem::MoveInstr(
        movq_assem, new temp::TempList({reg}), new temp::TempList({storeReg})));
  }

  // ProcEntryExit2: add sink instrutions
  auto new_instr_list = frame::FrameFactory::ProcEntryExit2(instr_list);
  assem_instr_ = std::make_unique<AssemInstr>(new_instr_list);
}

void AssemInstr::Print(FILE *out, temp::Map *map) const {
  for (auto instr : instr_list_->GetList()) instr->Print(out, map);
  fprintf(out, "\n");
}
}  // namespace cg

namespace tree {
/* TODO: Put your lab5 code here */

void SeqStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  left_->Munch(instr_list, fs);
  right_->Munch(instr_list, fs);
}

void LabelStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  instr_list.Append(
      new assem::LabelInstr(temp::LabelFactory::LabelString(label_), label_));
}

void JumpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  instr_list.Append(new assem::OperInstr("jmp `j0", nullptr, nullptr,
                                         new assem::Targets(jumps_)));
}

void CjumpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  auto left_temp = left_->Munch(instr_list, fs);
  auto right_temp = right_->Munch(instr_list, fs);
  std::string opstr;
  switch (op_) {
      // 因为在tree的namespace不用加tree::
    case EQ_OP:
      opstr = "je `j0";
      break;
    case NE_OP:
      opstr = "jne `j0";
      break;
    case LT_OP:
      opstr = "jl `j0";
      break;
    case LE_OP:
      opstr = "jle `j0";
      break;
    case GE_OP:
      opstr = "jge `j0";
      break;
    case GT_OP:
      opstr = "jg `j0";
      break;
    // 以下几个是无符号比较，不过在translate中没有，看Relop中有就写上了
    case ULT_OP:
      opstr = "jb `j0";
      break;
    case ULE_OP:
      opstr = "jbe `j0";
      break;
    case UGE_OP:
      opstr = "jae `j0";
      break;
    case UGT_OP:
      opstr = "ja `j0";
      break;
    default:
      assert(false);
      break;
  }
  // 这边需要注意cmpq的比较顺序和cjumpstm的相反
  instr_list.Append(new assem::OperInstr(
      "cmpq `s1, `s0", nullptr, new temp::TempList({left_temp, right_temp}),
      nullptr));
  instr_list.Append(new assem::OperInstr(
      opstr, nullptr, nullptr,
      new assem::Targets(
          new std::vector<temp::Label *>({true_label_, false_label_}))));
}

void MoveStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  // p.s. 如果摸鱼MemExp可以只写move `s0, (`s1)这一种情况
  // 下面写的过程中需要注意,类似move `s0, (`s1)后面那个也是source list
  // 因为不是写到`s1,而是写到(`s1)，没有被写的寄存器（临时变量）
  if (typeid(*dst_) == typeid(MemExp)) {
    // 如果destination是MemExp
    // TODO: 根据BinOp优化
    auto dst_mem_exp = dynamic_cast<MemExp *>(dst_);
    assert(dst_mem_exp);
    auto src_tmp = src_->Munch(instr_list, fs);
    auto dst_tmp = dst_mem_exp->exp_->Munch(instr_list, fs);
    instr_list.Append(
        new assem::OperInstr("movq `s0, (`s1)", nullptr,
                             new temp::TempList({src_tmp, dst_tmp}), nullptr));

  } else if (typeid(*dst_) == typeid(TempExp)) {
    // 如果destination是TempExp
    // MOVE(TEMP(i), e2)
    auto dst_exp = dynamic_cast<TempExp *>(dst_);
    assert(dst_exp);
    auto src_temp = src_->Munch(instr_list, fs);
    instr_list.Append(new assem::MoveInstr("movq `s0 `d0",
                                           new temp::TempList({dst_exp->temp_}),
                                           new temp::TempList({src_temp})));
  } else
    assert(0);
}

void ExpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  exp_->Munch(instr_list, fs);
}

temp::Temp *BinopExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  // 在这里要尤其注意操作的dst和src，用于活跃分析
  /* TODO: Put your lab5 code here */
  temp::Temp *left = left_->Munch(instr_list, fs);
  temp::Temp *right = right_->Munch(instr_list, fs);
  temp::Temp *res = temp::TempFactory::NewTemp();
  // reference: https://blog.csdn.net/Chauncyxu/article/details/121890457
  // 注意看清楚乘法和除法的细节,用到rax和rdx
  switch (op_) {
    case PLUS_OP:
      // 先把left移到res 因为 addq a, b <=> b=a+b
      instr_list.Append(new assem::MoveInstr("movq `s0, `d0",
                                             new temp::TempList({res}),
                                             new temp::TempList({left})));
      // res是隐藏的(implicit)src
      // 考虑res=res+right 左边是dst右边是src
      // 计算完了以后res里存的是加起来的和
      // 这里的src和dst在后面用来做活跃分析
      instr_list.Append(
          new assem::OperInstr("addq `s0, `d0", new temp::TempList({res}),
                               new temp::TempList({right, res}), nullptr));
      break;
    case MINUS_OP:
      // 减法与加法基本一致
      instr_list.Append(new assem::MoveInstr("movq `s0, `d0",
                                             new temp::TempList({res}),
                                             new temp::TempList({left})));
      instr_list.Append(
          new assem::OperInstr("subq `s0, `d0", new temp::TempList({res}),
                               new temp::TempList({right, res}), nullptr));
      break;
    case MUL_OP:
      // R[%rdx]:R[%rax]<-S×R[%rax] 有符号乘法
      // 将left移动到%rax里，这里returnValue指代%rax
      instr_list.Append(new assem::MoveInstr(
          "movq `s0, `d0", new temp::TempList({reg_manager->ReturnValue()}),
          new temp::TempList({left})));

      // 将right作为imulq的参数，它会与%rax做乘法，并将结果存放在%rax和%rdx中
      // 等价于 R[%rdx]:R[%rax] = S * R[%rax]
      // src: S, rax; dst:rax,rdx
      instr_list.Append(new assem::OperInstr(
          "imulq `s0", reg_manager->OperateRegs(),
          new temp::TempList({right, reg_manager->ReturnValue()}), nullptr));

      // 最后把%rax移动到reg中,这里只取64位，多的就算溢出了
      instr_list.Append(new assem::MoveInstr(
          "movq `s0, `d0", new temp::TempList({res}),
          new temp::TempList({reg_manager->ReturnValue()})));
      break;
    case DIV_OP:
      // cqto
      // R[%rdx]:R[%rax]←符号扩展(R[%rax])
      // idivq S
      // R[%rdx]←R[%rdx]:R[%rax] mod S
      // R[%rax]←R[%rdx]:R[%rax] ÷ S

      // 移动left到%rax 同乘法
      instr_list.Append(new assem::MoveInstr(
          "movq `s0, `d0", new temp::TempList({reg_manager->ReturnValue()}),
          new temp::TempList({left})));

      // cqto进行扩展，直接把rax的符号位复制到rdx
      // dst:rax,rdx src:rax
      instr_list.Append(new assem::OperInstr(
          "cqto", reg_manager->OperateRegs(),
          new temp::TempList({reg_manager->ReturnValue()}), nullptr));

      // 将右值作为idivq的参数，它是除数与%rax相除，并将商和余数存放在%rax和%rdx中
      // 这里被修改的reg是rax和rdx，被读取的是right
      instr_list.Append(
          new assem::OperInstr("idivq `s0", reg_manager->OperateRegs(),
                               new temp::TempList({right}), nullptr));

      // movq %rax, res
      instr_list.Append(new assem::MoveInstr(
          "movq `s0, `d0", new temp::TempList({res}),
          new temp::TempList({reg_manager->ReturnValue()})));
      break;
    default:
      assert(0);
      break;
  }
}

temp::Temp *MemExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
}

temp::Temp *TempExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
}

temp::Temp *EseqExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
}

temp::Temp *NameExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
}

temp::Temp *ConstExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
}

temp::Temp *CallExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
}

temp::TempList *ExpList::MunchArgs(assem::InstrList &instr_list,
                                   std::string_view fs) {
  /* TODO: Put your lab5 code here */
}

}  // namespace tree
