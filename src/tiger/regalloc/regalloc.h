#ifndef TIGER_REGALLOC_REGALLOC_H_
#define TIGER_REGALLOC_REGALLOC_H_

#include "tiger/codegen/assem.h"
#include "tiger/codegen/codegen.h"
#include "tiger/frame/frame.h"
#include "tiger/frame/temp.h"
#include "tiger/liveness/liveness.h"
#include "tiger/regalloc/color.h"
#include "tiger/util/graph.h"

#include <map>
#include <set>
#include <stack>

namespace ra {
using InodePtrPair = std::pair<live::INodePtr, live::INodePtr>;
using INodeSet = std::set<live::INodePtr>;
using INodeSetPtr = INodeSet *;
using MoveSet = std::set<InodePtrPair>;
using MoveSetPtr = MoveSet *;
enum NodeType{
  PRE_COLORED,
  INITIAL,
  SIMPLIFY_WORK_LIST,
  FREEZE_WORK_LIST,
  SPILL_WORK_LIST,
  SPILLED_NODE,
  COALESCED_NOED,
  COLORED_NODE,
  SELECTED_NODE
};
enum MoveType{
  COALESCED_MOVE,
  CONSTRAINED_MOVE,
  FROZEN_MOVE,
  WORK_LIST_MOVE,
  ACTIVE_MOVE,
};

class Result {
public:
  temp::Map *coloring_;
  assem::InstrList *il_;

  Result() : coloring_(nullptr), il_(nullptr) {}
  Result(temp::Map *coloring, assem::InstrList *il)
      : coloring_(coloring), il_(il) {}
  Result(const Result &result) = delete;
  Result(Result &&result) = delete;
  Result &operator=(const Result &result) = delete;
  Result &operator=(Result &&result) = delete;
  ~Result() = default;
};

class RegAllocator {
  /* TODO: Put your lab6 code here */
public:
  RegAllocator(frame::Frame *frame, std::unique_ptr<cg::AssemInstr> traces)
      : frame_(frame), instr_list_(traces->GetInstrList()),
        result_(std::make_unique<ra::Result>()) {
    // 初始化，将传入的frame和instr_list 存起来，构建新的result_
    // 下面的InodeSet和MoveSet都不是指针，都会自动初始化一个空的
  }
  void RegAlloc();

  std::unique_ptr<ra::Result> TransferResult() { return std::move(result_); };

private:
  frame::Frame *frame_;                // corresponding frame
  assem::InstrList *instr_list_;       // assem instruction list
  std::unique_ptr<ra::Result> result_; // Temp 与 Register 的对应关系（map)
  int K{15};

  // 过程中记录的临时变量
  live::LiveGraph live_graph{};

  // 节点集合，每个节点属于且仅属于其中一个
  // initial 其实可以直接从live_graph的节点中取出
  INodeSet pre_colored;
  INodeSet simplifyWorklist; // 低度数、传送无关的节点
  INodeSet freezeWorklist;   // 低度数、传送有关的节点
  INodeSet spillWorklist;    // 高度数的节点
  INodeSet spilledNodes;     // 实际溢出的节点
  INodeSet coalescedNodes;   // 已合并的节点
  INodeSet coloredNodes;     // 已成功着色的节点
  std::list<live::INodePtr> selectStack;      // 被简化的节点

  // move指令集合，每个move指令属于且仅属于其中一个
  MoveSet coalescedMoves;   // 已经合并的move指令
  MoveSet constrainedMoves; // 冲突的不能合并的move指令
  MoveSet frozenMoves;      // 放弃合并的move指令
  MoveSet worklistMoves;    // 有可能合并的move指令
  MoveSet activeMoves;      // 还没有做好合并准备的move指令

  // 其他数据结构
  std::map<live::INodePtr, MoveSetPtr> moveList; // 每个节点传送相关的指令
  std::map<live::INodePtr, live::INodePtr> alias; // 已合并的节点的代表元
  std::map<live::INodePtr, int> degrees;          // 节点的度数
  std::set<InodePtrPair> adjSet; // 冲突边集合
  std::map<live::INodePtr, INodeSetPtr> adjList;               // 冲突邻接表
  std::map<temp::Temp *, std::string> color_; // 算法为节点选择的颜色

  std::map<live::INodePtr, NodeType> node_type;
  std::map<InodePtrPair, MoveType> move_type;

  static live::LiveGraph AnalyzeLiveness(assem::InstrList *instr_list_);
  void Clear();
  void Build();
  void AddEdge(live::INode *u, live::INode *v); // 添加边到adjSet与adjList
  bool is_Pre_colored(live::INodePtr t) const;

  void MakeWorklist();
  bool MoveRelated(live::INode *node);
  INodeSetPtr Adjacent(live::INodePtr node);

  void DecrementDegree(live::INode *node);
  void EnableMoves(live::INode *node);
  // 并查集
  live::INodePtr GetAlias(live::INodePtr node) const;
  // 尝试将该节点从传送有关转为传送无关
  void AddWorkList(live::INode *node);
  // George条件 对所有节点，要么是低度数与另一个节点要么已经冲突
  // 修改第一个形参为节点，自己for循环取adjacent
  bool OK(const live::INodePtr &v,const live::INodePtr &r);
  // Briggs条件，高度数节点不超过K
  // 这里修改形参为两个节点方便for循环选择adjacent
  bool Conservative(const live::INodePtr &u,const live::INodePtr &v);
  void Combine(live::INode *u, live::INode *v);

  void Simplify();
  void Coalesce();
  void Freeze();
  void SelectSpill();
  void AssignColors();
  void RewriteProgram();
  void SimplifyInstrList(); // 简化move指令
};

} // namespace ra

#endif