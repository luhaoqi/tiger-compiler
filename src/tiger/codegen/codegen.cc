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
}

void LabelStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
}

void JumpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
}

void CjumpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
}

void MoveStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
}

void ExpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
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
