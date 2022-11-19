#include "tiger/escape/escape.h"

#include "tiger/absyn/absyn.h"

namespace esc {
void EscFinder::FindEscape() { absyn_tree_->Traverse(env_.get()); }
}  // namespace esc

namespace absyn {

void AbsynTree::Traverse(esc::EscEnvPtr env) {
  /* TODO: Put your lab5 code here */
  /* depth from 0 */
  root_->Traverse(env, 1);
}

void SimpleVar::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  // 看到变量，查看是否在更深层，是的话说明是逃逸变量
  esc::EscapeEntry* t = env->Look(sym_);
  if (depth > t->depth_) *(t->escape_) = true;
  return;
}

void FieldVar::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  // 类似 a.x 遍历a 只关注这个var本身，x只是个symbol
  var_->Traverse(env, depth);
  return;
}

void SubscriptVar::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  // a[x] 同时检查 a 和 x
  var_->Traverse(env, depth);
  subscript_->Traverse(env, depth);
  return;
}

void VarExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  var_->Traverse(env, depth);
  return;
}

void NilExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  return;
}

void IntExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  return;
}

void StringExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  return;
}

void CallExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  // e.g. f(x,y,z,w) check x,y,z,w
  auto list = args_->GetList();
  for (auto ele : list) ele->Traverse(env, depth);
  return;
}

void OpExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  left_->Traverse(env, depth);
  right_->Traverse(env, depth);
  return;
}

void RecordExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  // node{val = x, next = nxt}  check x & nxt
  auto list = fields_->GetList();
  for (auto ele : list) ele->exp_->Traverse(env, depth);
  return;
}

void SeqExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  // check each exp
  auto list = seq_->GetList();
  for (auto ele : list) ele->Traverse(env, depth);
  return;
}

void AssignExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  // a:=b check a&b
  var_->Traverse(env, depth);
  exp_->Traverse(env, depth);
  return;
}

void IfExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  // if (x > 2) then a:=2 else b:=c
  // check both exp
  test_->Traverse(env, depth);
  then_->Traverse(env, depth);
  if (elsee_) elsee_->Traverse(env, depth);
  return;
}

void WhileExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  // while (x > 2) do a:=a+1
  test_->Traverse(env, depth);
  body_->Traverse(env, depth);
  return;
}

void ForExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  // for 需要增加一个变量声明
  // for i:=1 to n do a:=a+1
  // 这个escape是ForExp自己的一个成员变量
  escape_ = false;
  env->Enter(var_, new esc::EscapeEntry(depth, &(escape_)));
  lo_->Traverse(env, depth);
  hi_->Traverse(env, depth);
  body_->Traverse(env, depth);
  return;
}

void BreakExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  return;
}

void LetExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  auto list = decs_->GetList();
  for (auto ele : list) ele->Traverse(env, depth);
  body_->Traverse(env, depth);
  return;
}

void ArrayExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  size_->Traverse(env, depth);
  init_->Traverse(env, depth);
  return;
}

void VoidExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  return;
}

void FunctionDec::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  // FunDec 唯一可能dep++
  auto fuclist = functions_->GetList();
  for (auto fuc : fuclist) {
    // 同层的函数是互相看不到局部变量的 所以用BeginScope和EndScope区分可见度
    env->BeginScope();
    auto paramlist = fuc->params_->GetList();
    // function merge(a:list, b:list):list
    for (auto param : paramlist) {
      // a:list 代表的是一个Field，也自带escape属性，专门表示函数形参的逃逸属性
      param->escape_ = false;
      env->Enter(param->name_,
                 new esc::EscapeEntry(depth + 1, &param->escape_));
    };
    fuc->body_->Traverse(env, depth + 1);
    env->EndScope();
  };
  return;
}

void VarDec::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  // 除了函数声明 dep都不会变
  // VarDec 的任务就是为env加入新的内容
  // VarDec 中也有escape成员变量表示这个Var的逃逸属性
  escape_ = false;
  env->Enter(var_, new esc::EscapeEntry(depth, &escape_));
  init_->Traverse(env, depth);
  return;
}

void TypeDec::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  return;
}

}  // namespace absyn