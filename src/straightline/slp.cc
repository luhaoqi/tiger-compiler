#include "straightline/slp.h"

#include <iostream>

namespace A {

// for class Stm:

int A::CompoundStm::MaxArgs() const {
  // TODO: put your code here (lab1).
  return std::max(stm1->MaxArgs(), stm2->MaxArgs());
}

Table *A::CompoundStm::Interp(Table *t) const {
  // TODO: put your code here (lab1).
  Table *tmp = stm1->Interp(t);
  return stm2->Interp(tmp);
}

int A::AssignStm::MaxArgs() const {
  // TODO: put your code here (lab1).
  return exp->MaxArgs();
}

Table *A::AssignStm::Interp(Table *t) const {
  // TODO: put your code here (lab1).
  IntAndTable *it = exp->Interp(t);
  Table *tt = it->t->Update(id, it->i);
  // delete it->t;
  return tt;
}

int A::PrintStm::MaxArgs() const {
  // TODO: put your code here (lab1).
  return std::max(exps->NumExps(), exps->MaxArgs());
}

Table *A::PrintStm::Interp(Table *t) const {
  // TODO: put your code here (lab1).
  IntAndTable *it = exps->Interp(t);
  return it->t;
}

// for class Exp:

int A::IdExp::MaxArgs() const { return 0; }

IntAndTable *A::IdExp::Interp(Table *t) const {
  return new IntAndTable(t->Lookup(id), t);
}

int A::NumExp::MaxArgs() const { return 0; }

IntAndTable *A::NumExp::Interp(Table *t) const {
  return new IntAndTable(num, t);
}

int A::OpExp::MaxArgs() const { return 0; }

IntAndTable *A::OpExp::Interp(Table *t) const {
  int s;
  IntAndTable *tl = left->Interp(t), *tr = right->Interp(tl->t);
  int l = tl->i, r = tr->i;

  switch (oper) {
  case PLUS:
    s = l + r;
    break;
  case MINUS:
    s = l - r;
    break;
  case TIMES:
    s = l * r;
    break;
  case DIV:
    s = l / r;
    break;
  default:
    break;
  }
  return new IntAndTable(s, t);
}

int A::EseqExp::MaxArgs() const { return stm->MaxArgs(); }

IntAndTable *A::EseqExp::Interp(Table *t) const {
  return exp->Interp(stm->Interp(t));
}

// for class ExpList:

int A::PairExpList::MaxArgs() const {
  return std::max(exp->MaxArgs(), tail->MaxArgs());
}

int A::PairExpList::NumExps() const { return 1 + tail->NumExps(); }

IntAndTable *A::PairExpList::Interp(Table *t) const {
  IntAndTable *it = exp->Interp(t);
  std::cout << it->i << " ";
  return tail->Interp(it->t);
}

int A::LastExpList::MaxArgs() const { return exp->MaxArgs(); }

int A::LastExpList::NumExps() const { return 1; }

IntAndTable *A::LastExpList::Interp(Table *t) const {
  IntAndTable *it = exp->Interp(t);
  std::cout << it->i << "\n";
  return it;
}

int Table::Lookup(const std::string &key) const {
  if (id == key) {
    return value;
  } else if (tail != nullptr) {
    return tail->Lookup(key);
  } else {
    assert(false);
  }
}

Table *Table::Update(const std::string &key, int val) const {
  return new Table(key, val, this);
}
} // namespace A
