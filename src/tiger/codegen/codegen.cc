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
  /* TODO: Put your lab5 code here */
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
