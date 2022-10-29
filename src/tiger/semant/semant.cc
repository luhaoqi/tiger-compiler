#include "tiger/semant/semant.h"

#include "tiger/absyn/absyn.h"

namespace absyn {

// 发现错误后返回的类型
// #define DEFULT_ERROR_TYPR type::IntTy::Instance()
type::Ty *DEFULT_ERROR_TYPR = type::IntTy::Instance();

// 所有的SemAnalyze返回的Ty都ActualTy()

void AbsynTree::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                           err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  root_->SemAnalyze(venv, tenv, 0, errormsg);
}

type::Ty *SimpleVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  env::EnvEntry *entry = venv->Look(sym_);
  if (entry && typeid(*entry) == typeid(env::VarEntry)) {
    return (static_cast<env::VarEntry *>(entry))->ty_->ActualTy();
  } else {
    errormsg->Error(pos_, "undefined variable %s", sym_->Name().data());
    return DEFULT_ERROR_TYPR;
  }
}

type::Ty *FieldVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *type = var_->SemAnalyze(venv, tenv, labelcount, errormsg);
  type = type->ActualTy();  // TODO: test remove
  if (typeid(*type) != typeid(type::RecordTy)) {
    // not a record type, error!
    errormsg->Error(pos_, "not a record type");
  } else {
    // check if var.sym is declared
    auto fieldList = (dynamic_cast<type::RecordTy *>(type))->fields_->GetList();
    for (const auto &x : fieldList)
      if (x->name_ == sym_) {
        return x->ty_->ActualTy();
      }
    // not found.
    errormsg->Error(pos_, "field %s doesn't exist", sym_->Name().data());
  }
  return DEFULT_ERROR_TYPR;
}

type::Ty *SubscriptVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   int labelcount,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *type = var_->SemAnalyze(venv, tenv, labelcount, errormsg);
  type = type->ActualTy();  // TODO: test remove
  type::Ty *expTy = subscript_->SemAnalyze(venv, tenv, labelcount, errormsg);
  expTy = expTy->ActualTy();  // TODO: test remove
  if (typeid(*expTy) != typeid(type::IntTy)) {
    // check if subscript is int
    errormsg->Error(pos_, "subscript must be integer");
  } else {
    if (typeid(*type) != typeid(type::ArrayTy)) {
      // check if var is an array
      errormsg->Error(pos_, "array type required");
    } else {
      // all is right, return the type of array
      return (dynamic_cast<type::ArrayTy *>(type))->ty_->ActualTy();
    }
  }
  // 这里的处理是如果两个都错就返回两个错误信息;
  return DEFULT_ERROR_TYPR;
}

type::Ty *VarExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
}

type::Ty *NilExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
}

type::Ty *IntExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
}

type::Ty *StringExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
}

type::Ty *CallExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                              int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
}

type::Ty *OpExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                            int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
}

type::Ty *RecordExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
}

type::Ty *SeqExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
}

type::Ty *AssignExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
}

type::Ty *IfExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                            int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
}

type::Ty *WhileExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
}

type::Ty *ForExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
}

type::Ty *BreakExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
}

type::Ty *LetExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
}

type::Ty *ArrayExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
}

type::Ty *VoidExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                              int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
}

void FunctionDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
}

void VarDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv, int labelcount,
                        err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
}

void TypeDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv, int labelcount,
                         err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
}

type::Ty *NameTy::SemAnalyze(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  // such as:
  // type a = array of int
  // type b = a   -> b is a NameTy
  type::Ty *ty = tenv->Look(name_);
  if (!ty) {
    // doesn't find name_ in typeEnvironment, ERROR!
    errormsg->Error(pos_, "undefined type %s", name_->Name().data());
  } else {
    return ty;
  }
  return DEFULT_ERROR_TYPR;
}

type::Ty *RecordTy::SemAnalyze(env::TEnvPtr tenv,
                               err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  // type a = {id:int, name:string}
  // 代码框架提供，错误提示也在里面了,错误的话就返回缺少那个的RecordTy
  // 极端情况下甚至会造成别的错误识别不出来，但问题不大，有错误编译就不会过
  return new type::RecordTy(record_->MakeFieldList(tenv, errormsg));
}

type::Ty *ArrayTy::SemAnalyze(env::TEnvPtr tenv,
                              err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  // type a = array of int  / type a = array of b
  // array_ = int / b
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

namespace sem {

void ProgSem::SemAnalyze() {
  FillBaseVEnv();
  FillBaseTEnv();
  absyn_tree_->SemAnalyze(venv_.get(), tenv_.get(), errormsg_.get());
}
}  // namespace sem
