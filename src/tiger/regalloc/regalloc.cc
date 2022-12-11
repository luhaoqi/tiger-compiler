#include "tiger/regalloc/regalloc.h"

#include "tiger/output/logger.h"

extern frame::RegManager *reg_manager;

namespace ra {
/* TODO: Put your lab6 code here */
void RegAllocator::RegAlloc() {

  // 一直循环Main直到没有spilledNode
  while (true) {
    live_graph = AnalyzeLiveness(instr_list_);
    Build();
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
    moveList[src]->insert(std::make_pair(src, dst));
    moveList[dst]->insert(std::make_pair(src, dst));

    worklistMoves.insert(std::make_pair(src, dst));
  }

  for (auto &u : live_graph.interf_graph->Nodes()->GetList()) {
    // init pre_colored
    if (reg_manager->temp_map_->Look(u->NodeInfo()) != nullptr)
      pre_colored.insert(u);

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
  return pre_colored.find(t) != pre_colored.end();
}

} // namespace ra