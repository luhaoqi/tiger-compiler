#include "tiger/liveness/liveness.h"
#include <set>

extern frame::RegManager *reg_manager;

namespace temp {

void TempList::assign(const std::set<Temp *> &s) {
  temp_list_.clear();
  for (const auto &temp : s) {
    temp_list_.push_back(temp);
  }
}

}; // namespace temp

namespace live {

bool MoveList::Contain(INodePtr src, INodePtr dst) {
  return std::any_of(move_list_.cbegin(), move_list_.cend(),
                     [src, dst](std::pair<INodePtr, INodePtr> move) {
                       return move.first == src && move.second == dst;
                     });
}

void MoveList::Delete(INodePtr src, INodePtr dst) {
  assert(src && dst);
  auto move_it = move_list_.begin();
  for (; move_it != move_list_.end(); move_it++) {
    if (move_it->first == src && move_it->second == dst) {
      break;
    }
  }
  move_list_.erase(move_it);
}

MoveList *MoveList::Union(MoveList *list) {
  auto *res = new MoveList();
  for (auto move : move_list_) {
    res->move_list_.push_back(move);
  }
  for (auto move : list->GetList()) {
    if (!res->Contain(move.first, move.second))
      res->move_list_.push_back(move);
  }
  return res;
}

MoveList *MoveList::Intersect(MoveList *list) {
  auto *res = new MoveList();
  for (auto move : list->GetList()) {
    if (Contain(move.first, move.second))
      res->move_list_.push_back(move);
  }
  return res;
}

void LiveGraphFactory::LiveMap() {
  /* TODO: Put your lab6 code here */

  // 初始化 in[] 和 out[]
  for (const auto &node : flowgraph_->Nodes()->GetList()) {
    in_->Enter(node, new temp::TempList());
    out_->Enter(node, new temp::TempList());
  }

  // Liveness analysis
  // until not change
  bool changed = true;
  while (changed) {
    changed = false;
    // in[s] = use[s] ∪ (out[s] – def[s])
    // out[s] = ∪{n∈succ[s]}_{in[n]}

    // TODO: 倒序提高效率, 或者使用别的技术
    for (const auto &node : flowgraph_->Nodes()->GetList()) {
      // get current instr to get cur->Use() & cur->Def()
      auto cur = node->NodeInfo();
      // 下面直接使用std::set 来完成集合操作，包括添加、删除与判断相等
      auto in_list = in_->Look(node);
      auto out_list = out_->Look(node);

      std::set<temp::Temp *> in_set, out_set, in_origin, out_origin;
      // save origin set to check later
      for (const auto &temp : out_list->GetList()) {
        out_origin.insert(temp);
      }
      for (const auto &temp : in_list->GetList()) {
        in_origin.insert(temp);
      }

      // in[s] = out[s]
      in_set = out_origin;
      // delete def[s] from in[s] (in[s] = (out[s] – def[s]) )
      for (const auto &temp : cur->Def()->GetList()) {
        in_set.erase(temp);
      }
      // insert use[s] to in[s]  ( in[s] = use[s] ∪ (out[s] – def[s]) )
      for (const auto &temp : cur->Use()->GetList()) {
        in_set.insert(temp);
      }
      // check if in[s] changed
      if (in_set != in_origin){
        changed = true;
        in_list->assign(in_set);
      }

      // out[s] = ∪{n∈succ[s]}_{in[n]}
      for (const auto &succ : node->Succ()->GetList()){
        for (const auto &temp: in_->Look(succ)->GetList()){
          out_set.insert(temp);
        }
      }
      // check if out[s] changed
      if (out_set != out_origin){
        changed = true;
        out_list->assign(out_set);
      }
    }
  }
}

void LiveGraphFactory::InterfGraph() { /* TODO: Put your lab6 code here */
}

void LiveGraphFactory::Liveness() {
  LiveMap();
  InterfGraph();
}

} // namespace live
