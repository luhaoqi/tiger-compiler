#include "tiger/translate/translate.h"

#include <tiger/absyn/absyn.h>

#include "tiger/env/env.h"
#include "tiger/errormsg/errormsg.h"
#include "tiger/frame/frame.h"
#include "tiger/frame/temp.h"
#include "tiger/frame/x64frame.h"

extern frame::Frags *frags;
extern frame::RegManager *reg_manager;

namespace tr {

Access *Access::AllocLocal(Level *level, bool escape) {
  /* TODO: Put your lab5 code here */
  return new Access(level, level->frame_->allocLocal(escape));
}

class Cx {
 public:
  PatchList trues_;
  PatchList falses_;
  tree::Stm *stm_;

  Cx(PatchList trues, PatchList falses, tree::Stm *stm)
      : trues_(trues), falses_(falses), stm_(stm) {}
};

class Exp {
 public:
  [[nodiscard]] virtual tree::Exp *UnEx() = 0;
  [[nodiscard]] virtual tree::Stm *UnNx() = 0;
  [[nodiscard]] virtual Cx UnCx(err::ErrorMsg *errormsg) = 0;
};

class ExpAndTy {
 public:
  tr::Exp *exp_;
  type::Ty *ty_;

  ExpAndTy(tr::Exp *exp, type::Ty *ty) : exp_(exp), ty_(ty) {}
};

class ExExp : public Exp {
 public:
  tree::Exp *exp_;

  explicit ExExp(tree::Exp *exp) : exp_(exp) {}

  [[nodiscard]] tree::Exp *UnEx() override {
    /* TODO: Put your lab5 code here */
    return exp_;
  }
  [[nodiscard]] tree::Stm *UnNx() override {
    /* TODO: Put your lab5 code here */
    return new tree::ExpStm(exp_);
  }
  // 这个转换只会出现在IfExp和WhileExp的test中
  [[nodiscard]] Cx UnCx(err::ErrorMsg *errormsg) override {
    /* TODO: Put your lab5 code here */
    // if (exp) then exp1 else exp2
    // TODO: 这里生成的label只是个占位符，在外面还是要被替换掉的
    // 这样其实浪费了两个label，不过无所谓了，也可以写一个 new temp::Label
    // 不过这样也会内存泄漏，自己取舍吧
    // 其实有一种方法：外面直接用这里生成的label
    temp::Label *t = temp::LabelFactory::NewLabel();
    temp::Label *f = temp::LabelFactory::NewLabel();

    // 如果exp不等于0，那么跳转到t，否则跳转到f
    tree::CjumpStm *stm =
        new tree::CjumpStm(tree::NE_OP, exp_, new tree::ConstExp(0), t, f);

    auto trues = PatchList({&t});
    auto falses = PatchList({&f});
    return tr::Cx(trues, falses, stm);
  }
};

class NxExp : public Exp {
 public:
  tree::Stm *stm_;

  explicit NxExp(tree::Stm *stm) : stm_(stm) {}

  [[nodiscard]] tree::Exp *UnEx() override {
    /* TODO: Put your lab5 code here */
    // 返回一个EseqExp，执行stm以后的返回值随便选个0
    return new tree::EseqExp(stm_, new tree::ConstExp(0));
  }
  [[nodiscard]] tree::Stm *UnNx() override {
    /* TODO: Put your lab5 code here */
    return stm_;
  }
  [[nodiscard]] Cx UnCx(err::ErrorMsg *errormsg) override {
    /* TODO: Put your lab5 code here */
    printf("Error: Nx(stm) cannot be a test exp(Cx).");
    assert(0);
  }
};

class CxExp : public Exp {
 public:
  Cx cx_;

  CxExp(PatchList trues, PatchList falses, tree::Stm *stm)
      : cx_(trues, falses, stm) {}

  [[nodiscard]] tree::Exp *UnEx() override {
    /* TODO: Put your lab5 code here */
    temp::Temp *r = temp::TempFactory::NewTemp();
    temp::Label *t = temp::LabelFactory::NewLabel();
    temp::Label *f = temp::LabelFactory::NewLabel();
    cx_.trues_.DoPatch(t);
    cx_.falses_.DoPatch(f);
    // finally the code is :
    // move r, 1
    // cx_.stm_
    // f : move r, 0
    // t : r
    auto move_1_to_r =
        new tree::MoveStm(new tree::TempExp(r), new tree::ConstExp(1));
    auto move_0_to_r =
        new tree::MoveStm(new tree::TempExp(r), new tree::ConstExp(0));
    auto label_t_and_temp_r =
        new tree::EseqExp(new tree::LabelStm(t), new tree::TempExp(r));
    auto f_move_t_r = new tree::EseqExp(
        cx_.stm_,
        new tree::EseqExp(new tree::LabelStm(f),
                          new tree::EseqExp(move_0_to_r, label_t_and_temp_r)));
    return new tree::EseqExp(move_1_to_r, f_move_t_r);
  }

  [[nodiscard]] tree::Stm *UnNx() override {
    /* TODO: Put your lab5 code here */
    temp::Label *label = temp::LabelFactory::NewLabel();
    cx_.trues_.DoPatch(label);
    cx_.falses_.DoPatch(label);
    // 执行完cx.stm_ , 直接跳到下一行，赋一个label标签
    return new tree::SeqStm(cx_.stm_, new tree::LabelStm(label));
  }
  [[nodiscard]] Cx UnCx(err::ErrorMsg *errormsg) override {
    /* TODO: Put your lab5 code here */
    return cx_;
  }
};

ProgTr::ProgTr(std::unique_ptr<absyn::AbsynTree> absyn_tree,
               std::unique_ptr<err::ErrorMsg> errormsg)
    : absyn_tree_(std::move(absyn_tree)),
      errormsg_(std::move(errormsg)),
      tenv_(std::make_unique<env::TEnv>()),
      venv_(std::make_unique<env::VEnv>()) {
  // total goal: 还需要初始化 main_level
  // 1. 初始化main_label 使用NamedLabel自定义label
  temp::Label *main_label = temp::LabelFactory::NamedLabel("__tigermain__");
  // 2. 初始化main_frame
  frame::Frame *main_frame = frame::FrameFactory::NewFrame(main_label, {});
  // 3. 初始化main_level_
  main_level_ = std::make_unique<Level>(main_frame, nullptr);
}

// 下面的Translate中的label指的是如果遇到break应该跳到哪个label

void ProgTr::Translate() {
  /* TODO: Put your lab5 code here */
  // 定义见env.cc
  // 初始化venv，这里是将运行时函数加入venv
  FillBaseVEnv();
  // 初始化tenv，将int、string加入tenv
  FillBaseTEnv();
  auto main_ExpTy = absyn_tree_->Translate(
      venv_.get(), tenv_.get(), main_level_.get(),
      temp::LabelFactory::NamedLabel("__tigermain__"), errormsg_.get());
  // save code
  frags->PushBack(
      new frame::ProcFrag(main_ExpTy->exp_->UnNx(), main_level_->frame_));
}

}  // namespace tr

namespace absyn {

// 一些重复用到的辅助函数

// 计算静态链 当前的level是current，目标level是target
tree::Exp *StaticLink(tr::Level *current, tr::Level *target) {
  tree::Exp *FP = new tree::TempExp(reg_manager->FramePointer());
  while (current != target) {
    assert(current && current->parent_);
    FP = current->frame_->formals_.front()->ToExp(FP);
    current = current->parent_;
  }
  return FP;
}

tr::Exp *Translate_SimpleVar(tr::Access *access, tr::Level *level) {
  // calculate corresponding FP by static links
  // current = level, target = access->lecel_
  tree::Exp *FP = StaticLink(level, access->level_);
  // MEM(+(TEMP fp, CONST offset));
  return new tr::ExExp(access->access_->ToExp(FP));
}

// 保存函数定义的代码
void saveFunctionFrag(tr::Exp *body, tr::Level *level) {
  // procEntryExit1(view shift) + move(RV, body)

  // move(RV, body)
  tree::Stm *stm = new tree::MoveStm(
      new tree::TempExp(reg_manager->ReturnValue()), body->UnEx());

  // view shift
  stm = frame::FrameFactory::ProcEntryExit1(level->frame_, stm);
  // save code
  frags->PushBack(new frame::ProcFrag(stm, level->frame_));
}

tr::ExpAndTy *AbsynTree::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return root_->Translate(venv, tenv, level, label, errormsg);
}

tr::ExpAndTy *SimpleVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  auto entry = venv->Look(sym_);
  env::VarEntry *var_entry = dynamic_cast<env::VarEntry *>(entry);
  assert(var_entry);
  auto exp = Translate_SimpleVar(var_entry->access_, level);
  return new tr::ExpAndTy(exp, var_entry->ty_->ActualTy());
}

tr::ExpAndTy *FieldVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  tr::ExpAndTy *var_ExpTy = var_->Translate(venv, tenv, level, label, errormsg);
  type::RecordTy *ty = dynamic_cast<type::RecordTy *>(var_ExpTy->ty_);
  assert(ty);
  auto fieldList = (ty)->fields_->GetList();
  int i = 0;
  // 注意需要用UnEx() 将tr::Exp 转为 tree::Exp 然后再用ExExp转换过来
  for (const auto &x : fieldList) {
    if (x->name_ == sym_) {
      // MEM( +(exp, wordsize * num) )
      tree::Exp *exp = new tree::MemExp(
          new tree::BinopExp(tree::BinOp::PLUS_OP, var_ExpTy->exp_->UnEx(),
                             new tree::ConstExp(i * reg_manager->WordSize())));
      tr::ExExp *e = new tr::ExExp(exp);
      return new tr::ExpAndTy(e, ty->ActualTy());
    }
    i++;
  }
  errormsg->Error(pos_, "field %s doesn't exist", sym_->Name().data());
  assert(false);
}

tr::ExpAndTy *SubscriptVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                      tr::Level *level, temp::Label *label,
                                      err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  tr::ExpAndTy *var_ExpTy = var_->Translate(venv, tenv, level, label, errormsg);
  type::ArrayTy *var_ty = dynamic_cast<type::ArrayTy *>(var_ExpTy->ty_);
  assert(var_ty);
  tr::ExpAndTy *sub_ExpTy =
      subscript_->Translate(venv, tenv, level, label, errormsg);

  // MEM( +(exp, wordsize * num) )
  tree::Exp *mul_exp =
      new tree::BinopExp(tree::BinOp::MUL_OP, sub_ExpTy->exp_->UnEx(),
                         new tree::ConstExp(reg_manager->WordSize()));
  tree::Exp *exp = new tree::MemExp(new tree::BinopExp(
      tree::BinOp::PLUS_OP, var_ExpTy->exp_->UnEx(), mul_exp));
  tr::ExExp *e = new tr::ExExp(exp);
  return new tr::ExpAndTy(e, var_ty->ty_->ActualTy());
}

tr::ExpAndTy *VarExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return var_->Translate(venv, tenv, level, label, errormsg);
}

tr::ExpAndTy *NilExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)),
                          type::NilTy::Instance());
}

tr::ExpAndTy *IntExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(val_)),
                          type::IntTy::Instance());
}

tr::ExpAndTy *StringExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  // 定义该字符串的label
  temp::Label *str_label = temp::LabelFactory::NewLabel();
  // 把字符串存到片段中
  frags->PushBack(new frame::StringFrag(str_label, str_));
  return new tr::ExpAndTy(new tr::ExExp(new tree::NameExp(str_label)),
                          type::StringTy::Instance());
}

tr::ExpAndTy *CallExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                 tr::Level *level, temp::Label *label,
                                 err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
}

tr::ExpAndTy *OpExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                               tr::Level *level, temp::Label *label,
                               err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
}

tr::ExpAndTy *RecordExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
}

tr::ExpAndTy *SeqExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
}

tr::ExpAndTy *AssignExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
}

tr::ExpAndTy *IfExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                               tr::Level *level, temp::Label *label,
                               err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
}

tr::ExpAndTy *WhileExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
}

tr::ExpAndTy *ForExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
}

tr::ExpAndTy *BreakExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
}

tr::ExpAndTy *LetExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
}

tr::ExpAndTy *ArrayExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
}

tr::ExpAndTy *VoidExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                 tr::Level *level, temp::Label *label,
                                 err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
}

tr::Exp *FunctionDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  // 同样不做type-checking中做过的内容
  const auto &funList = this->functions_->GetList();

  for (auto &fun : funList) {
    type::TyList *formalTyList = fun->params_->MakeFormalTyList(tenv, errormsg);
    type::Ty *resultTy;
    if (fun->result_) {
      // 查找返回值类型
      resultTy = tenv->Look(fun->result_);
    } else {
      // 是过程不是函数
      resultTy = type::VoidTy::Instance();
    }
    // create label
    // TODO: 重要！
    // 这里为了汇编中的label易读使用函数名字
    // 之后需要改成别的以应对重名函数
    temp::Label *fun_label = temp::LabelFactory::NamedLabel(fun->name_->Name());
    // create  escape list to create level
    std::list<bool> escapes;
    for (auto &field : fun->params_->GetList()) {
      escapes.push_back(field->escape_);
    }
    // create level
    tr::Level *fun_level = tr::Level::NewLevel(level, fun_label, escapes);

    venv->Enter(fun->name_, new env::FunEntry(fun_level, fun_label,
                                              formalTyList, resultTy));
  }

  // 二次扫描
  // 详细思路看TypeDec
  for (const auto &fun : funList) {
    type::TyList *formalTyList = fun->params_->MakeFormalTyList(tenv, errormsg);
    env::FunEntry *entry =
        dynamic_cast<env::FunEntry *>(venv->Look(fun->name_));
    assert(entry != nullptr);

    venv->BeginScope();
    tenv->BeginScope();

    // 执行body前把funDec中的临时变量加进去
    const auto &fieldList = fun->params_->GetList();
    const auto &tyList = formalTyList->GetList();
    const auto &formalList = entry->level_->frame_->formals_;

    assert(fieldList.size() == tyList.size());
    assert(fieldList.size() == formalList.size() - 1);

    auto it1 = fieldList.begin();
    auto it2 = tyList.begin();
    auto it3 = formalList.begin();
    it3++;

    // 加入venv 注意用新的VarEntry的构造函数
    for (; it1 != fieldList.end(); ++it1, ++it2, ++it3) {
      venv->Enter((*it1)->name_,
                  new env::VarEntry(new tr::Access(entry->level_, *it3), *it2));
    }

    // 执行body
    tr::ExpAndTy *body_ExpTy =
        fun->body_->Translate(venv, tenv, entry->level_, label, errormsg);

    // 结束
    venv->EndScope();
    tenv->EndScope();

    // TODO: 存储函数代码
    saveFunctionFrag(body_ExpTy->exp_, entry->level_);
  }
  return new tr::ExExp(new tree::ConstExp(0));
  // 函数没有类似typedec的死循环
}

tr::Exp *VarDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                           tr::Level *level, temp::Label *label,
                           err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  tr::ExpAndTy *init_info =
      init_->Translate(venv, tenv, level, label, errormsg);
  type::Ty *init_ty = init_info->ty_->ActualTy();
  // 这里经过type checking应该没问题
  if (typ_ != nullptr) {
    // record 可能 init 为 nil
    // 直接用声明的类型
    init_ty == tenv->Look(typ_)->ActualTy();
  }
  // allocLocal for Access
  tr::Access *access = tr::Access::AllocLocal(level, escape_);
  // add variable to venv
  venv->Enter(var_, new env::VarEntry(access, init_ty));

  // get var
  tr::Exp *var_exp = Translate_SimpleVar(access, level);

  // move(var, init)
  return new tr::NxExp(
      new tree::MoveStm(var_exp->UnEx(), init_info->exp_->UnEx()));
}

tr::Exp *TypeDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                            tr::Level *level, temp::Label *label,
                            err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  // 虽然主程序去掉了lab4的type-checking，但这里默认都是对的不再做重复的类型检查(重名、循环)
  // 这里直接复制semant.cc 中对应的插入语句
  auto nameTyList = types_->GetList();
  for (auto &nameTy : nameTyList) {
    tenv->Enter(nameTy->name_, new type::NameTy(nameTy->name_, nullptr));
  }
  for (const auto &nameTy : nameTyList) {
    auto ty = tenv->Look(nameTy->name_);
    if (ty) {
      auto NameTy_ = dynamic_cast<type::NameTy *>(ty);
      assert(NameTy_);  // 不能为nullptr
      NameTy_->ty_ = nameTy->ty_->Translate(tenv, errormsg);
    }
  }
  // 随便返回一个，为了和VarDec保持一致
  return new tr::ExExp(new tree::ConstExp(0));
}

type::Ty *NameTy::Translate(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  // 这里直接返回Look的结果也行，反正ActualTy一样
  return new type::NameTy(name_, tenv->Look(name_));
}

type::Ty *RecordTy::Translate(env::TEnvPtr tenv,
                              err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return new type::RecordTy(record_->MakeFieldList(tenv, errormsg));
}

type::Ty *ArrayTy::Translate(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  type::Ty *ty = tenv->Look(array_);
  if (!ty) {
    // Check type existence
    errormsg->Error(pos_, "undefined type %s", array_->Name().data());
  } else {
    return new type::ArrayTy(ty);
  }
  // 如果错误就返回VoidTy,之后就基本不会再匹配上了
  return type::VoidTy::Instance();
}

}  // namespace absyn
