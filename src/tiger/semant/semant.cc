#include "tiger/semant/semant.h"

#include "tiger/absyn/absyn.h"

namespace absyn {

// 发现错误后返回的类型
// #define DEFULT_ERROR_TYPR type::IntTy::Instance()
type::Ty *DEFULT_ERROR_TYPR = type::IntTy::Instance();

// 遵循原则：
// 所有的SemAnalyze返回的Ty都ActualTy()
// 尽可能多找错
// 以type.h中的ActualTy()方式 不同地方创建的RecordTy各不相同
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
  // 这里不用管var到底是Simple、Field还是Subscript
  // 因为Var的SemAnalyze是一个纯虚函数，会找到对应的子类
  return var_->SemAnalyze(venv, tenv, labelcount, errormsg);
}

type::Ty *NilExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  return type::NilTy::Instance();
}

type::Ty *IntExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  return type::IntTy::Instance();
}

type::Ty *StringExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  return type::StringTy::Instance();
}

type::Ty *CallExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                              int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  env::EnvEntry *entry = venv->Look(func_);
  if (entry && typeid(*entry) == typeid(env::FunEntry)) {
    // 找到该函数的定义
    env::FunEntry *fun = dynamic_cast<env::FunEntry *>(entry);
    // const & 减少开销
    const auto &formals = fun->formals_->GetList();
    const auto &res = fun->result_;
    bool flag = true;  // 标记是否有错误
    const auto &args = args_->GetList();
    std::list<type::Ty *> args_ty;
    // 先把表达式都算出来放到args_ty容器中
    for (const auto &x : args) {
      args_ty.push_back(x->SemAnalyze(venv, tenv, labelcount, errormsg));
    }
    // 遍历formals检查类型是否匹配
    auto it1 = formals.begin();
    auto it2 = args_ty.begin();
    for (; it1 != formals.end() && it2 != args_ty.end(); it1++, it2++) {
      if ((*it1)->ActualTy() != (*it2)->ActualTy()) {
        errormsg->Error(pos_, "para type mismatch");
        flag = false;
      }
    }
    if (it1 != formals.end()) {
      errormsg->Error(pos_, "too few params in function %s",
                      func_->Name().data());
      flag = false;
    }

    if (it2 != args_ty.end()) {
      errormsg->Error(pos_, "too many params in function %s",
                      func_->Name().data());
      flag = false;
    }
    // 没有任何错误，返回函数返回值类型
    if (flag) return res->ActualTy();
  } else {
    // ERROR! not found!
    errormsg->Error(pos_, "undefined function %s", func_->Name().data());
  }
  // 有问题默认返回ERROR_TYPE(int)
  return DEFULT_ERROR_TYPR;
}

type::Ty *OpExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                            int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *left_ty =
      left_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  type::Ty *right_ty =
      right_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  switch (oper_) {
    case PLUS_OP:
    case MINUS_OP:
    case DIVIDE_OP:
    case TIMES_OP:
    case AND_OP:
    case OR_OP:
      // 加减乘除 与 & | 操作左右操作数必须是int
      if (typeid(*left_ty) != typeid(type::IntTy)) {
        errormsg->Error(left_->pos_, "integer required");
      }
      if (typeid(*right_ty) != typeid(type::IntTy)) {
        errormsg->Error(right_->pos_, "integer required");
      }
      return type::IntTy::Instance();
      break;
    case ABSYN_OPER_COUNT:
      assert(false);
      break;
    default:
      // 否则就是比较操作，只需要类型相等
      if (!left_ty->IsSameType(right_ty)) {
        errormsg->Error(pos_, "same type required");
        return type::IntTy::Instance();
      }
      break;
  }
  return DEFULT_ERROR_TYPR;
}

type::Ty *RecordExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *ty = tenv->Look(typ_);
  if (!ty) {
    // Check type existence
    errormsg->Error(pos_, "undefined type %s", typ_->Name().data());
  } else {
    ty = ty->ActualTy();
    if (typeid(*ty) != typeid(type::RecordTy)) {
      // Check if is a record
      errormsg->Error(pos_, std::string("not a record type"));
      return ty;
    } else {
      auto args = fields_->GetList();  // Given Efields
      auto formals = dynamic_cast<type::RecordTy *>(ty)
                         ->fields_->GetList();  // declared Fields
      // check the two list
      auto it1 = args.begin();
      auto it2 = formals.begin();
      for (; it1 != args.end() && it2 != formals.end(); it1++, it2++) {
        // 解析表达式exp得到类型ty
        type::Ty *ty =
            (*it1)->exp_->SemAnalyze(venv, tenv, labelcount, errormsg);
        // 比较两者的name 附录要求按顺序一一对应
        if ((*it1)->name_ != (*it2)->name_)
          errormsg->Error((*it1)->exp_->pos_, "field %s doesn't exist",
                          (*it1)->name_->Name().data());
        // 比较两者类型
        if (!ty->IsSameType((*it2)->ty_))
          errormsg->Error((*it1)->exp_->pos_, "unmatched assign exp");
      }
      // 比较数量
      if (it1 != args.end()) errormsg->Error(pos_, "too many Efields");
      if (it2 != formals.end()) errormsg->Error(pos_, "too few Efields");
      // 即使参数都对不上，还是当做对上名字的RecordTy返回
      return dynamic_cast<type::RecordTy *>(ty);
    }
  }
  return DEFULT_ERROR_TYPR;
}

type::Ty *SeqExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  const auto &expList = seq_->GetList();

  type::Ty *ty = type::VoidTy::Instance();
  for (const auto &x : expList) {
    ty = x->SemAnalyze(venv, tenv, labelcount, errormsg);
  }

  return ty;
}

type::Ty *AssignExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *var_ty = var_->SemAnalyze(venv, tenv, labelcount, errormsg);
  type::Ty *exp_ty = exp_->SemAnalyze(venv, tenv, labelcount, errormsg);
  // for中的变量不能赋值
  if (typeid(*var_) == typeid(SimpleVar)) {
    SimpleVar *svar = static_cast<SimpleVar *>(var_);
    // 转成simplevar后去查找entry
    env::EnvEntry *entry = venv->Look(svar->sym_);
    if (entry->readonly_) {
      errormsg->Error(svar->pos_, "loop variable can't be assigned");
    }
  }
  // 比较左右类型
  if (!var_ty->IsSameType(exp_ty)) {
    errormsg->Error(pos_, "unmatched assign exp");
  }
  // 无值表达式
  return type::VoidTy::Instance();
}

type::Ty *IfExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                            int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  // 取出test,then,else的type
  type::Ty *test_ty = test_->SemAnalyze(venv, tenv, labelcount, errormsg);
  type::Ty *then_ty = then_->SemAnalyze(venv, tenv, labelcount, errormsg);
  type::Ty *else_ty = nullptr;
  // test必须是int
  if (typeid(*test_ty) != typeid(type::IntTy)) {
    errormsg->Error(test_->pos_, "test of IfExp must be integer");
  }
  if (!elsee_) {
    // if-then
    if (typeid(*then_ty) != typeid(type::VoidTy)) {
      errormsg->Error(then_->pos_, "if-then exp's body must produce no value");
    }
    return type::VoidTy::Instance();
  } else {
    // if-then-else
    else_ty = elsee_->SemAnalyze(venv, tenv, labelcount, errormsg);
    if (!then_ty->IsSameType(else_ty)) {
      errormsg->Error(then_->pos_, "then exp and else exp type mismatch");
    }
    // 如果错误猜测返回的是then后面的类型，否则类型相同
    return then_ty->ActualTy();
  }
}

type::Ty *WhileExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *test_ty = test_->SemAnalyze(venv, tenv, labelcount, errormsg);
  type::Ty *body_ty = nullptr;
  // test必须是int
  if (typeid(*test_ty) != typeid(type::IntTy)) {
    errormsg->Error(test_->pos_, "while test must have int value");
  }
  // body必须是无值
  if (body_) {
    // body后执行，层数+1
    body_ty = body_->SemAnalyze(venv, tenv, labelcount + 1, errormsg);
    // 循环体必须无值
    if (typeid(*body_ty) != typeid(type::VoidTy)) {
      errormsg->Error(body_->pos_, "while body must produce no value");
    }
  }
  // while语句是无值表达式
  return type::VoidTy::Instance();
}

type::Ty *ForExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *lo_ty = lo_->SemAnalyze(venv, tenv, labelcount, errormsg);
  type::Ty *hi_ty = hi_->SemAnalyze(venv, tenv, labelcount, errormsg);

  type::Ty *body_ty = nullptr;

  // 判断上下界是否是int
  if (typeid(*lo_ty) != typeid(type::IntTy)) {
    errormsg->Error(lo_->pos_, "for exp's range type is not integer");
  }
  if (typeid(*hi_ty) != typeid(type::IntTy)) {
    errormsg->Error(hi_->pos_, "for exp's range type is not integer");
  }

  if (body_) {
    // 把循环变量加入value environment 并且设置readonly属性不可修改
    // 之后在解析body 循环层数+1 退出后回收循环变量
    venv->BeginScope();
    venv->Enter(var_, new env::VarEntry(type::IntTy::Instance(), true));
    body_ty = body_->SemAnalyze(venv, tenv, labelcount + 1, errormsg);
    venv->EndScope();
    // 循环体必须无值
    if (typeid(*body_ty) != typeid(type::VoidTy)) {
      errormsg->Error(body_->pos_, "for exp's body must produce no value");
    }
  }
  // 整个for语句无值
  return type::VoidTy::Instance();
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
