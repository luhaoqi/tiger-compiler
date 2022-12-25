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

} // namespace temp

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

      // out[s] = ∪{n∈succ[s]}_{in[n]}
      for (const auto &succ : node->Succ()->GetList()) {
        for (const auto &temp : in_->Look(succ)->GetList()) {
          out_set.insert(temp);
        }
      }

      // check if in[s] changed
      if (in_set != in_origin) {
        changed = true;
        in_list->assign(in_set);
      }
      // check if out[s] changed
      if (out_set != out_origin) {
        changed = true;
        out_list->assign(out_set);
      }
    }
  }
  // print in out def use
  //  auto c = temp::Map::LayerMap(reg_manager->temp_map_, temp::Map::Name());
  //  printf("output def & out\n");
  //  for (const auto &node : flowgraph_->Nodes()->GetList()) {
  //    auto in_list = in_->Look(node);
  //    auto out_list = out_->Look(node);
  //    printf("instr: %s
  //    \n",((assem::OperInstr*)(node->NodeInfo()))->assem_.c_str());
  //    printf("def: ");
  //    for (const auto &temp : node->NodeInfo()->Def()->GetList()) {
  //      printf("%s ", c->Look(temp)->c_str());
  //    }
  //    printf("      use: ");
  //    for (const auto &temp : node->NodeInfo()->Use()->GetList()) {
  //      printf("%s ", c->Look(temp)->c_str());
  //    }
  //    printf("\nout: ");
  //    for (const auto &temp : out_list->GetList()) {
  //      printf("%s ", c->Look(temp)->c_str());
  //    }
  //    printf("\nin: ");
  //    for (const auto &temp : in_list->GetList()) {
  //      printf("%s ", c->Look(temp)->c_str());
  //    }
  //    printf("\n");
  //    printf("----------\n");
  //  }
}

void LiveGraphFactory::InterfGraph() {
  /* TODO: Put your lab6 code here */
  auto &interf_graph = live_graph_.interf_graph;
  auto &moves = live_graph_.moves;

  // 15个寄存器，除了rsp不参与分配
  // 这些相当于是预着色的结点
  temp::TempList *regs = reg_manager->Registers();
  // 下面找出所有的节点，使用set去重
  std::set<temp::Temp *> temps;
  // 将这些寄存器转化成图中的节点
  // 注意：按照书上的做法，为了减少边，reg之间不互相连边，这些属于pre_colored，到时候直接判断两个预着色的就行
  for (auto &reg : regs->GetList()) {
    temps.insert(reg);
  }

  for (auto &node : flowgraph_->Nodes()->GetList()) {
    // add use[s]
    for (auto &temp : node->NodeInfo()->Use()->GetList()) {
//      if (temp == reg_manager->GetRegister(frame::RegisterName::rcx))
//        printf("use[s]: %s\n",((assem::OperInstr*)(node->NodeInfo()))->assem_.c_str());
      temps.insert(temp);
    }
    // add def[s]
    for (auto &temp : node->NodeInfo()->Def()->GetList()) {
//      if (temp == reg_manager->GetRegister(frame::RegisterName::rcx))
//        printf("use[s]: %s\n",((assem::OperInstr*)(node->NodeInfo()))->assem_.c_str());
      temps.insert(temp);
    }
  }
  // 不管里面的rsp,不用给他分配颜色，后面加颜色的时候也不加rsp，所以也不用管这些node和rsp的冲突关系
  temps.erase(reg_manager->StackPointer());
  // 把temps中的去重过后的节点加入interference graph
  for (auto &temp : temps)
    temp_node_map_->Enter(temp, interf_graph->NewNode(temp));

  // 添加冲突边
  // 设一条non-move-related 指令的 Def 中的 a
  // ，对该指令out[s]中的任意变量b[i]，添加冲突边(a, b[i]) 设一条 move-related
  // 指令为 a<-c，对该指令out[s]中的任意不同于c的变量b[i]，添加冲突边(a, b[i])
  temp::Temp *rsp = reg_manager->StackPointer();
  for (auto &node : flowgraph_->Nodes()->GetList()) {
    auto instr = node->NodeInfo();
    if (typeid(*instr) == typeid(assem::MoveInstr)) {
      // move-related

      // 处理out、def的temp集合，去除rsp
      std::set<temp::Temp *> out, def;
      for (auto &out_temp : out_->Look(node)->GetList()) {
        out.insert(out_temp);
      }
      out.erase(rsp);
      for (auto &def_temp : instr->Def()->GetList()) {
        def.insert(def_temp);
      }
      def.erase(rsp);
      // 对于moveInstr 需要再out中减去use
      auto use_list = instr->Use()->GetList();
      assert(use_list.size() == 1);
      out.erase(use_list.front());

      // 双重循环，添加边 (out-use)<->def
      for (auto &out_temp : out) {
        auto out_node = temp_node_map_->Look(out_temp);
        for (auto &def_temp : def) {
          auto def_node = temp_node_map_->Look(def_temp);
          // 规避自环
          if (out_node == def_node)
            continue;
          interf_graph->AddEdge(def_node, out_node);
          interf_graph->AddEdge(out_node, def_node);
        }
      }

      // 添加传送指令
      auto def_list = instr->Def()->GetList();
      // use、def 都只有一个
      assert(def_list.size() == 1);
      auto &dst = def_list.front();
      auto &src = use_list.front();
      if (dst != rsp && src != rsp) {
        auto dst_node = temp_node_map_->Look(dst);
        auto src_node = temp_node_map_->Look(src);
        if (!moves->Contain(src_node, dst_node)) {
          moves->Append(src_node, dst_node);
        }
      }
    } else {
      // non-move-related
      // 处理out、def的temp集合，去除rsp
      std::set<temp::Temp *> out, def;
      for (auto &out_temp : out_->Look(node)->GetList()) {
        out.insert(out_temp);
      }
      out.erase(rsp);
      for (auto &def_temp : instr->Def()->GetList()) {
        def.insert(def_temp);
      }
      def.erase(rsp);

      // 双重循环，添加边 out<->def
      for (auto &out_temp : out) {
        auto out_node = temp_node_map_->Look(out_temp);
        for (auto &def_temp : def) {
          auto def_node = temp_node_map_->Look(def_temp);
          // 规避自环，其实在reg alloc中的Add edge中也判断了
          if (out_node == def_node)
            continue;
          interf_graph->AddEdge(def_node, out_node);
          interf_graph->AddEdge(out_node, def_node);
        }
      }
    }
  }
}

void LiveGraphFactory::Liveness() {
  // liveness analysis
  LiveMap();
  // build interference graph
  InterfGraph();
}

} // namespace live
