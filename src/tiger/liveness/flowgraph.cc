#include "tiger/liveness/flowgraph.h"

namespace fg {

void FlowGraphFactory::AssemFlowGraph() {
  /* TODO: Put your lab6 code here */
  // 构造控制流图，每条instr对应一个node
  // 注意jmp需要跳转(不能连接下一条), 有条件跳转连两条边
  // label_map_ 需要把labelInstr与对应的node记录下来

  FNodePtr prev = nullptr;
  for (const auto &instr : instr_list_->GetList()) {
    FNodePtr cur = flowgraph_->NewNode(instr);
    if (prev)
      flowgraph_->AddEdge(prev, cur);
    if (typeid(instr) == typeid(assem::LabelInstr *)) {
      auto p = dynamic_cast<assem::LabelInstr *>(instr);
      label_map_->Enter(p->label_, cur);
    }
    prev = cur;
    if (typeid(instr) == typeid(assem::OperInstr *)) {
      auto p = dynamic_cast<assem::OperInstr *>(instr);
      if (p->assem_.find("jmp") == 0) {
        // if instr is unconditional jump
        prev = nullptr;
      }
    }
  }

  // handle the unconditional & conditional jump
  for (const auto &node : flowgraph_->Nodes()->GetList()) {
    auto instr = node->NodeInfo();
    if (typeid(instr) == typeid(assem::OperInstr *)) {
      auto p = dynamic_cast<assem::OperInstr *>(instr);
      if (p->jumps_) {
        if (p->jumps_->labels_) {
          auto labels = *(p->jumps_->labels_);
          for (const auto &label : labels) {
            FNodePtr jump_node = label_map_->Look(label);
            if (jump_node) {
              flowgraph_->AddEdge(node, jump_node);
            }
          }
        }
      }
    }
  }
}

} // namespace fg

namespace assem {

temp::TempList *LabelInstr::Def() const {
  /* TODO: Put your lab6 code here */
  return new temp::TempList();
}

temp::TempList *MoveInstr::Def() const {
  /* TODO: Put your lab6 code here */
  return dst_ ? dst_ : new temp::TempList();
}

temp::TempList *OperInstr::Def() const {
  /* TODO: Put your lab6 code here */
  return dst_ ? dst_ : new temp::TempList();
}

temp::TempList *LabelInstr::Use() const {
  /* TODO: Put your lab6 code here */
  return new temp::TempList();
}

temp::TempList *MoveInstr::Use() const {
  /* TODO: Put your lab6 code here */
  return src_ ? src_ : new temp::TempList();
}

temp::TempList *OperInstr::Use() const {
  /* TODO: Put your lab6 code here */
  return src_ ? src_ : new temp::TempList();
}
} // namespace assem
