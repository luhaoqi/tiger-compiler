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
  frame::Frame *main_frame = frame::FrameFatory::NewFrame(main_label, {});
  // 3. 初始化main_level_
  main_level_ = std::make_unique<Level>(main_frame, nullptr);
}

void ProgTr::Translate() {
  /* TODO: Put your lab5 code here */
  // 定义见env.cc
  // 初始化venv，这里是将运行时函数加入venv
  FillBaseVEnv();
  // 初始化tenv，将int、string加入tenv
  FillBaseTEnv();
  absyn_tree_->Translate(venv_.get(), tenv_.get(), main_level_.get(),
                         temp::LabelFactory::NamedLabel("__tigermain__"),
                         errormsg_.get());
  // TODO: 保存最后的汇编代码
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
}

tr::ExpAndTy *SubscriptVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                      tr::Level *level, temp::Label *label,
                                      err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
}

tr::ExpAndTy *VarExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
}

tr::ExpAndTy *NilExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
}

tr::ExpAndTy *IntExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
}

tr::ExpAndTy *StringExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
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
}

type::Ty *NameTy::Translate(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
}

type::Ty *RecordTy::Translate(env::TEnvPtr tenv,
                              err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
}

type::Ty *ArrayTy::Translate(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
}

}  // namespace absyn
