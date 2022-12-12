#include "tiger/regalloc/regalloc.h"

#include "tiger/frame/temp.h"
#include "tiger/output/logger.h"

extern frame::RegManager *reg_manager;

namespace ra {
/* TODO: Put your lab6 code here */

const char *RegAllocator::getNodeName(live::INodePtr node) {
  return c->Look(node->NodeInfo())->c_str();
}

void RegAllocator::RegAlloc() {
  K = (int)reg_manager->Registers()->GetList().size();
  assert(K == 15);
  // added_temps 在过程中不会被清空
  added_temps.clear();
  // 存下所有可以用的颜色（也就是寄存器的名字）
  for (auto &x : reg_manager->Registers()->GetList()) {
    std::string *s = reg_manager->temp_map_->Look(x);
    reg_colors.push_back(*s);
  }
  assert(reg_colors.size() == 15);
  // 一直循环Main直到没有spilledNode
  int loop_num = 0;
  while (true) {
    c = temp::Map::LayerMap(reg_manager->temp_map_, temp::Map::Name());
    TigerLog("loop: %d\n", ++loop_num);
    live_graph = AnalyzeLiveness(instr_list_);
    Build();
    MakeWorklist();
    do {
      // 在执行后面操作的时候还可能到前面来，所以每个操作每次只执行一个node
      if (!simplifyWorklist.empty()) {
        Simplify();
      } else if (!worklistMoves.empty()) {
        Coalesce();
      } else if (!freezeWorklist.empty()) {
        Freeze();
      } else if (!spillWorklist.empty()) {
        SelectSpill();
      }
    } while (!simplifyWorklist.empty() || !worklistMoves.empty() ||
             !freezeWorklist.empty() || !spillWorklist.empty());

    TigerLog("AssignColors();\n");
    AssignColors();

    if (spilledNodes.empty()) {
      // 将着色转入result
      result_->coloring_ = temp::Map::Empty();
      for (const auto &t : color_) {
        result_->coloring_->Enter(t.first, new std::string(t.second));
      }
      // 寄存器不需要放，因为在output.cc中color上面还叠了temp_map_
      // 但是下面简化指令的时候可能找到寄存器，所以需要加进去
      // color_中有除了rsp的所有
      result_->coloring_->Enter(reg_manager->StackPointer(),
                                new std::string("%rsp"));

      SimplifyInstrList();
      result_->il_ = instr_list_;

      // 跳出循环
      break;
    } else {
      RewriteProgram();
    }
  }
}
live::LiveGraph RegAllocator::AnalyzeLiveness(assem::InstrList *instr_list_) {
  fg::FlowGraphFactory flow_graph_fac(instr_list_);
  // build control flow graph
  flow_graph_fac.AssemFlowGraph();

  // get control flow graph
  fg::FGraphPtr flow_graph = flow_graph_fac.GetFlowGraph();

  live::LiveGraphFactory live_graph_fac(flow_graph);
  // liveness analysis & // build interference graph
  live_graph_fac.Liveness();

  return live_graph_fac.GetLiveGraph();
}

void RegAllocator::Clear() {
  pre_colored.clear();

  simplifyWorklist.clear();
  freezeWorklist.clear();
  spillWorklist.clear();
  coalescedNodes.clear();
  coloredNodes.clear();
  selectStack.clear();
  spilledNodes.clear();

  coalescedMoves.clear();
  constrainedMoves.clear();
  frozenMoves.clear();
  worklistMoves.clear();
  activeMoves.clear();

  moveList.clear();
  alias.clear();
  degrees.clear();
  adjSet.clear();
  adjList.clear();
  color_.clear();

  node_type.clear();
  move_type.clear();
}

void RegAllocator::Build() {
  // 这里也做一些初始化
  Clear();
  // 初始化moveList,degree,adjList
  for (auto &node : live_graph.interf_graph->Nodes()->GetList()) {
    moveList[node] = new MoveSet();
    degrees[node] = 0;
    adjList[node] = new INodeSet();
  }

  // 构造color_,预着色
  for (auto t : reg_manager->Registers()->GetList()) {
    auto name = reg_manager->temp_map_->Look(t);
    assert(name);
    color_.insert(std::make_pair(t, *name));
  }

  // 冲突图在AnalyzeLiveness中已经构建完毕
  // Build主要是用来添加Move指令到对应的MoveList
  const auto &org_move_list = live_graph.moves->GetList();

  // 构造moveList，添加到src和dst对应的moveList
  for (auto &move : org_move_list) {
    auto src = move.first;
    auto dst = move.second;
    InodePtrPair t = std::make_pair(src, dst);
    moveList[src]->insert(t);
    moveList[dst]->insert(t);

    worklistMoves.insert(t);
    move_type[t] = WORK_LIST_MOVE;
  }

  for (auto &u : live_graph.interf_graph->Nodes()->GetList()) {
    // init pre_colored
    if (reg_manager->temp_map_->Look(u->NodeInfo()) != nullptr) {
      pre_colored.insert(u);
      node_type[u] = PRE_COLORED;
    }

    // add edge
    for (auto v : u->Adj()->GetList()) {
      AddEdge(u, v);
    }
  }
}

void RegAllocator::AddEdge(live::INode *u, live::INode *v) {
  if (u == v)
    return;

  auto it = adjSet.find(std::make_pair(u, v));
  if (it == adjSet.end()) {
    adjSet.insert(std::make_pair(u, v));
    adjSet.insert(std::make_pair(v, u));

    // 不用给预着色寄存器添加度数，因为默认是无穷大
    if (!is_Pre_colored(u)) {
      adjList[u]->insert(v);
      degrees[u]++;
    }
    if (!is_Pre_colored(v)) {
      adjList[v]->insert(u);
      degrees[v]++;
    }
  }
}

bool RegAllocator::is_Pre_colored(live::INodePtr t) const {
  // 这边调用pre_colored 或者 node_type 都行，甚至不需要pre_colored了
  return pre_colored.find(t) != pre_colored.end();
}

void RegAllocator::MakeWorklist() {
  for (auto node : live_graph.interf_graph->Nodes()->GetList()) {

    if (is_Pre_colored(node)) {
      continue; // 跳过预着色
    }
    int d = degrees[node];
    if (d >= K) {
      spillWorklist.insert(node);
      node_type[node] = SPILL_WORK_LIST;
    } else if (MoveRelated(node)) {
      freezeWorklist.insert(node);
      node_type[node] = FREEZE_WORK_LIST;
    } else {
      simplifyWorklist.insert(node);
      node_type[node] = SIMPLIFY_WORK_LIST;
    }
  }
}

INodeSetPtr RegAllocator::Adjacent(live::INodePtr node) {
  //  这里也是类似的修改adjlist的定义，动态删除因此后面就不用做集合操作了
  return adjList[node];
}

bool RegAllocator::MoveRelated(live::INode *node) {
  // NodeMove(n) != Ø
  // 这里针对书上的做一些修改， moveList等于书上的NodeMoves
  // 即moveList只保存activeMoves和worklistMoves
  return !moveList[node]->empty();

  // 如果想要不删除moveList
  //  for (const auto &x:*moveList[node]){
  //    if (move_type[x] == ACTIVE_MOVE || move_type[x] == COALESCED_MOVE)
  //      return true;
  //  }
  //  return false;
}

void RegAllocator::EnableMoves(live::INode *node) {
  // 这里的修改是从active_move -> work_list_move
  // 因此不用修改moveList[]
  auto node_moves = moveList[node];
  for (auto move : *node_moves) {
    if (move_type[move] == ACTIVE_MOVE) {
      // activeMoves <- activeMoves \ m
      activeMoves.erase(move);
      // worklistMoves <- worklistMoves ∪ m
      worklistMoves.insert(move);
      move_type[move] = WORK_LIST_MOVE;
    }
  }
}

void RegAllocator::DecrementDegree(live::INode *node) {
  if (is_Pre_colored(node)) {
    return;
  }
  // 这里多做个assert检查 可以写成degree[node]--;
  auto it = degrees.find(node);
  assert(it != degrees.end());
  int d = it->second;
  it->second--;

  if (d == K) {
    // EnableMoves({m} ∪ Adjacent(m))
    EnableMoves(node);
    for (const auto &x : *adjList[node]) {
      auto type = node_type[x];
      if (type == SELECTED_NODE || type == COALESCED_NOED)
        continue;
      EnableMoves(x);
    }
    spillWorklist.erase(node);
    if (MoveRelated(node)) {
      freezeWorklist.insert(node);
      node_type[node] = FREEZE_WORK_LIST;
    } else {
      simplifyWorklist.insert(node);
      node_type[node] = SIMPLIFY_WORK_LIST;
    }
  }
}

void RegAllocator::Simplify() {
  auto it = simplifyWorklist.begin();
  auto node = *it; // 随便选，选begin
  // move node from simplifyWorklist to selectedStack
  simplifyWorklist.erase(it);
  selectStack.push_back(node);
  node_type[node] = SELECTED_NODE;
  // get adjacent of node
  //  INodeSetPtr adj = Adjacent(node);
  // remove node and update graph
  for (auto adj_node : *adjList[node]) {
    auto type = node_type[adj_node];
    if (type == SELECTED_NODE || type == COALESCED_NOED)
      continue;
    DecrementDegree(adj_node);
  }
  TigerLog("Simplify: %s\n", c->Look(node->NodeInfo())->c_str());
}

live::INodePtr RegAllocator::GetAlias(live::INodePtr node) const {
  if (coalescedNodes.find(node) != coalescedNodes.end()) {
    auto it = alias.find(node);
    assert(it != alias.end());
    return GetAlias(it->second);
  }
  return node;
}

void RegAllocator::AddWorkList(live::INode *node) {
  // 尝试将该节点从传送有关转为传送无关
  if (!is_Pre_colored(node) && !MoveRelated(node) && degrees[node] < K) {
    assert(freezeWorklist.find(node) != freezeWorklist.end());
    freezeWorklist.erase(node);
    simplifyWorklist.insert(node);
    node_type[node] = SIMPLIFY_WORK_LIST;
  }
}

bool RegAllocator::OK(const live::INodePtr &v, const live::INodePtr &u) {
  TigerLog("OK: v:%s u:%s\n", getNodeName(v), getNodeName(u));
  for (const auto &t : *adjList[v]) {
    auto type = node_type[t];
    if (type == SELECTED_NODE || type == COALESCED_NOED)
      continue;
    TigerLog("check v's adj(t) %s, degree[t]=%d (t,u)=%d\n", getNodeName(t),
             degrees[t], adjSet.find(std::make_pair(t, u)) != adjSet.end());
    // true：t is pre_colored || degree[t] < K || (t, u) belongs adjSet
    if (!(is_Pre_colored(t) || degrees[t] < K ||
          adjSet.find(std::make_pair(t, u)) != adjSet.end()))
      return false;
  }
  TigerLog("OK: true\n");
  // 每一个都符合才是true
  return true;
}

bool RegAllocator::Conservative(const live::INodePtr &u,
                                const live::INodePtr &v) {
  INodeSet s; // 为了防止两个adjacent有交集，需要使用set
  for (const auto &node : *adjList[u]) {
    auto type = node_type[node];
    if (type == SELECTED_NODE || type == COALESCED_NOED)
      continue;
    // 本来应该把pre_colored 的节点degree设为无穷大的
    if (is_Pre_colored(node) || degrees[node] >= K)
      s.insert(node);
  }
  for (const auto &node : *adjList[v]) {
    auto type = node_type[node];
    if (type == SELECTED_NODE || type == COALESCED_NOED)
      continue;
    if (is_Pre_colored(node) || degrees[node] >= K)
      s.insert(node);
  }
  TigerLog("Conservative u=%s, v=%s\n", getNodeName(u), getNodeName(v));
  for (auto &x : s) {
    TigerLog("%s ", getNodeName(x));
  }
  TigerLog(" %d\n", (int)s.size());
  return (int)s.size() < K;
}

void RegAllocator::Combine(live::INode *u, live::INode *v) {
  // v -> u
  // v是move_related 不可能在simplifyWorkList
  auto type = node_type[v];
  assert(type == FREEZE_WORK_LIST || type == SPILL_WORK_LIST);
  if (type == FREEZE_WORK_LIST) {
    freezeWorklist.erase(v);
  } else if (type == SPILL_WORK_LIST) {
    spillWorklist.erase(v);
  }
  coalescedNodes.insert(v);
  node_type[v] = COALESCED_NOED;

  alias[v] = u;

  // 额外的check，可删
  auto it1 = moveList.find(u);
  auto it2 = moveList.find(v);
  assert(it1 != moveList.end());
  assert(it2 != moveList.end());

  // 合并moveList[v] 到 moveList[u]
  for (const auto &move : *moveList[v]) {
    moveList[u]->insert(move);
  }

  EnableMoves(v);

  for (const auto &node : *adjList[v]) {
    auto type_ = node_type[node];
    if (type_ == SELECTED_NODE || type_ == COALESCED_NOED)
      continue;
    AddEdge(node, u);
    DecrementDegree(node);
  }

  if (degrees[u] >= K && node_type[u] == FREEZE_WORK_LIST) {
    freezeWorklist.erase(u);
    spillWorklist.insert(u);
    node_type[u] = SPILL_WORK_LIST;
  }
}

void RegAllocator::Coalesce() {
  auto move = *worklistMoves.begin();
  auto x = move.first;  // src
  auto y = move.second; // dest

  live::INodePtr u = nullptr, v = nullptr;
  // 交换顺序，最后是v合并到u，不可能两个都是pre_colored
  if (is_Pre_colored(y)) {
    u = GetAlias(y);
    v = GetAlias(x);
  } else {
    u = GetAlias(x);
    v = GetAlias(y);
  }
  // 从workListMoves中删除，记得从moveList中删除
  worklistMoves.erase(move);
  moveList[u]->erase(move);
  moveList[v]->erase(move);
  // 开始判断
  if (u == v) {
    // 如果src == dst，那么很简单地合并
    // 这种情况可能发生在 a,b,c互相move
    coalescedMoves.insert(move);
    move_type[move] = COALESCED_MOVE;
    AddWorkList(u);
    TigerLog("Coalesce u==v %s\n", c->Look(u->NodeInfo())->c_str());
  } else if (is_Pre_colored(v) ||
             adjSet.find(std::make_pair(u, v)) != adjSet.end()) {
    // u,v都是预着色或者有冲突边，加入冲突边集合
    constrainedMoves.insert(move);
    move_type[move] = CONSTRAINED_MOVE;
    // 在上面把move语句从workListMoves中删除后moveList[]得到更新
    // 可以尝试把u,v 转换为传送无关节点，之后可以简化
    AddWorkList(u);
    AddWorkList(v);
    TigerLog("Coalesce u,v are interfered %s %s\n",
             c->Look(u->NodeInfo())->c_str(), c->Look(v->NodeInfo())->c_str());
  } else if (
      // 符合 George条件
      // 对v的任意邻居，要么已经和u冲突，要么是低度数
      // 此处只对u是预着色的实寄存器采用这种方法判断
      (is_Pre_colored(u) && OK(v, u)) ||
      // 符合 Briggs 条件
      // uv合并后不会变成高度数节点
      // 此处对两个非预着色临时temp的使用该条件判断
      (!is_Pre_colored(u) && Conservative(u, v))) {
    coalescedMoves.insert(move);
    move_type[move] = COALESCED_MOVE;
    Combine(u, v);
    AddWorkList(u);
    TigerLog("Coalesce u,v combine %s %s\n", c->Look(u->NodeInfo())->c_str(),
             c->Look(v->NodeInfo())->c_str());
  } else {
    // 无法合并，加入activeMoves
    // 等之后再通过EnableMoves恢复到worklistMoves中
    activeMoves.insert(move);
    move_type[move] = ACTIVE_MOVE;
    // 因为是active_move 所以需要加回来
    moveList[u]->insert(move);
    moveList[v]->insert(move);
    TigerLog("Coalesce u,v can't combine %s %s\n",
             c->Look(u->NodeInfo())->c_str(), c->Look(v->NodeInfo())->c_str());
  }
}

void RegAllocator::FreezeMoves(live::INode *u) {
  // 过程中可能会删除moveList[u] 中的move，需要一个tmp
  MoveSet tmp = *moveList[u];
  for (auto move : tmp) {
    auto x = move.first;  // src
    auto y = move.second; // dest

    live::INode *v;
    if (GetAlias(y) == GetAlias(u)) {
      v = GetAlias(x);
    } else {
      v = GetAlias(y);
    }
    // 之所以freeze是因为没有workListMoves来尝试合并了
    //    printf("type = %d\n", move_type[move]);
    assert(move_type[move] == ACTIVE_MOVE);
    activeMoves.erase(move);
    frozenMoves.insert(move);
    move_type[move] = FROZEN_MOVE;
    moveList[u]->erase(move);
    moveList[v]->erase(move);

    if ((!is_Pre_colored(v)) && moveList[v]->empty() && degrees[v] < K) {
      assert(node_type[v] == FREEZE_WORK_LIST);
      freezeWorklist.erase(v);
      simplifyWorklist.insert(v);
      node_type[v] = SIMPLIFY_WORK_LIST;
    }
  }
  assert(moveList[u]->empty());
}

void RegAllocator::Freeze() {
  // 随便找第一个节点冻结
  auto node = *freezeWorklist.begin();
  freezeWorklist.erase(node);
  simplifyWorklist.insert(node);
  node_type[node] = SIMPLIFY_WORK_LIST;
  FreezeMoves(node);
  TigerLog("Freeze %s\n", c->Look(node->NodeInfo())->c_str());
}

void RegAllocator::SelectSpill() {
  live::INode *m = nullptr;
  // 找度数最大的点溢出
  // 还可以用计算权重的方法
  int max = 0;
  for (const auto &node : spillWorklist) {
    if (added_temps.find(node->NodeInfo()) != added_temps.end()) {
      continue;
    }
    int d = degrees[node];
    if (d > max) {
      max = d;
      m = node;
    }
  }
  // 如果没有就随便找一个
  // 前面尽量先跳过因为溢出产生的新节点了
  if (!m) {
    m = *spillWorklist.begin();
  }
  spillWorklist.erase(m);
  simplifyWorklist.insert(m);
  node_type[m] = SIMPLIFY_WORK_LIST;
  FreezeMoves(m);
  TigerLog("SelectSpill %s\n", c->Look(m->NodeInfo())->c_str());
}

void RegAllocator::AssignColors() {
  TigerLog("num of stack %d\n", selectStack.size());
  while (!selectStack.empty()) {
    auto node = selectStack.back();
    TigerLog("now node is %s \n", (c->Look(node->NodeInfo())->c_str()));
    selectStack.pop_back();

    // 可以着的色
    std::set<std::string> colors;
    for (auto &it : reg_colors) {
      colors.insert(it);
    }
    assert(colors.size() == 15);

    // 去掉冲突节点的颜色
    for (auto adj_node : *adjList[node]) {
      //      auto type = node_type[adj_node];
      //      if (type == SELECTED_NODE || type == COALESCED_NOED)
      //        continue;
      TigerLog("now adj is %s \n", (c->Look(adj_node->NodeInfo())->c_str()));

      auto root = GetAlias(adj_node);
      auto type = node_type[root];
      if (type == COLORED_NODE || type == PRE_COLORED) {
        // 邻居有颜色，需要删除该颜色
        auto it = color_.find(root->NodeInfo());
        assert(it != color_.end());
        colors.erase(it->second);
      }
    }

    if (colors.empty()) {
      TigerLog("color is empty. spill\n");
      // 如果没有颜色可以用了，那么它必须被溢出
      spilledNodes.insert(node);
      node_type[node] = SPILLED_NODE;
    } else {
      coloredNodes.insert(node);
      node_type[node] = COLORED_NODE;
      // 随机选一个颜色，比如begin
      color_.insert(std::make_pair(node->NodeInfo(), *colors.begin()));
      TigerLog("choose color: %s\n", (*colors.begin()).c_str());
    }
  }

  // 把合并的节点也加入map
  // 这只在没有spilled的时候有意义，否则需要重新build
  if (spilledNodes.empty()) {
    for (auto &node : coalescedNodes) {
      auto root = GetAlias(node);
      auto it = color_.find(root->NodeInfo());
      assert(it != color_.end());
      color_.insert(std::make_pair(node->NodeInfo(), it->second));
    }
  }
}

void RegAllocator::RewriteProgram() {
  int ws = reg_manager->WordSize();
  std::string label = frame_->GetLabel();
  temp::Temp *rsp = reg_manager->StackPointer();

  const auto &old_instr_list = instr_list_->GetList();

  // 用来存放上一次更新的汇编指令序列，第一次它就是旧指令序列
  std::list<assem::Instr *> prev_instr_list;
  for (auto instr : old_instr_list) {
    prev_instr_list.push_back(instr);
  }

  // 遍历所有溢出的结点
  for (const auto &node : spilledNodes) {
    auto spilledTemp = node->NodeInfo();
    // 在帧中开辟新的空间
    // 需要注意的是，生成汇编还在后面，因为这里跟着改framesize没问题
    frame_->offset -= ws;
    std::list<assem::Instr *> cur_instr_list;
    for (auto instr : prev_instr_list) {
      auto use_regs = instr->Use(); // src
      auto def_regs = instr->Def(); // dst

      // 如果溢出的寄存器被这条语句使用了，那么需要再其之前插入一条取数指令
      if (use_regs->Contain(spilledTemp)) {
        auto newTemp = temp::TempFactory::NewTemp();

        // movq (fun_framesize-offset)(src), dst
        std::string assem = "movq (" + label + "_framesize-" +
                            std::to_string(std::abs(frame_->offset)) +
                            ")(`s0), `d0";

        // src:newTem  dst:p%rsp
        auto pre_instr =
            new assem::OperInstr(assem, new temp::TempList({newTemp}),
                                 new temp::TempList({rsp}), nullptr);
        cur_instr_list.push_back(pre_instr);

        // 将ues中原来的spilledNode替换为新的temp
        use_regs->replaceTemp(spilledTemp, newTemp);
        // 新的temp需要记下来，之后尽量不溢出
        added_temps.insert(newTemp);
      }
      cur_instr_list.push_back(instr);
      if (def_regs->Contain(spilledTemp)) {
        auto newTemp = temp::TempFactory::NewTemp();

        // movq src, (fun_framesize-offset)(dst)
        std::string assem = "movq `s0, (" + label + "_framesize-" +
                            std::to_string(std::abs(frame_->offset)) + ")(`d0)";

        // src: %rsp dst:newTemp
        assem::Instr *back_instr =
            new assem::OperInstr(assem, new temp::TempList({rsp}),
                                 new temp::TempList({newTemp}), nullptr);

        cur_instr_list.push_back(back_instr);

        // 将def中原来的spilledNode替换为新的temp
        def_regs->replaceTemp(spilledTemp, newTemp);
        // 新的temp需要记下来，之后尽量不溢出
        added_temps.insert(newTemp);
      }
    }
    // 保存这一轮的指令列表
    prev_instr_list = cur_instr_list;
  }

  auto newInstrList = new assem::InstrList();
  for (auto instr : prev_instr_list) {
    newInstrList->Append(instr);
  }
  delete instr_list_;
  instr_list_ = newInstrList;
}

void RegAllocator::SimplifyInstrList() {
  auto *ret = new assem::InstrList();
  for (auto instr : instr_list_->GetList()) {
    if (typeid(*instr) == typeid(assem::MoveInstr)) {
      temp::Temp *src = *(instr->Use()->GetList().begin());
      temp::Temp *dst = *(instr->Def()->GetList().begin());

      auto src_ptr = result_->coloring_->Look(src);
      auto dst_ptr = result_->coloring_->Look(dst);
      if (src_ptr == dst_ptr ||
          (src_ptr && dst_ptr && (*src_ptr == *dst_ptr))) {
        continue;
      }
    }
    ret->Append(instr);
  }
  delete instr_list_;
  instr_list_ = ret;
}

} // namespace ra

namespace temp {
void TempList::replaceTemp(temp::Temp *old_temp, temp::Temp *new_temp) {
  for (auto &iter : temp_list_)
    if (iter == old_temp) {
      iter = new_temp;
    }
}

bool TempList::Contain(temp::Temp *temp) const {
  for (auto t : temp_list_) {
    if (t == temp) {
      return true;
    }
  }
  return false;
}

} // namespace temp