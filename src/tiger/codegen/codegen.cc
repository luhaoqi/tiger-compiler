#include "tiger/codegen/codegen.h"

#include <cassert>
#include <sstream>

extern frame::RegManager *reg_manager;

namespace {

constexpr int maxlen = 1024;

}  // namespace

namespace cg {

void CodeGen::Codegen() {
  /* TODO: Put your lab5 code here */
  // set frame size label
  fs_ = frame_->name_->Name() + "_framesize";

  // create new instruction list
  assem::InstrList *instr_list = new assem::InstrList();

  // save callee save register
  temp::TempList *callee_saved_regs = reg_manager->CalleeSaves();
  std::string movq_assem("movq `s0, `d0");
  std::vector<std::pair<temp::Temp *, temp::Temp *>> store_pair;
  for (auto reg : callee_saved_regs->GetList()) {
    auto storeReg = temp::TempFactory::NewTemp();
    instr_list->Append(new assem::MoveInstr(
        movq_assem, new temp::TempList({storeReg}), new temp::TempList({reg})));
    store_pair.emplace_back(reg, storeReg);
  }

  // 遍历trace的stmList 进行Munch
  const auto &trace_stmList = traces_->GetStmList()->GetList();
  for (auto stm : trace_stmList) {
    stm->Munch(*instr_list, fs_);
  }

  // restore callee saved register
  for (auto pair : store_pair) {
    temp::Temp *reg = pair.first;
    temp::Temp *storeReg = pair.second;
    instr_list->Append(new assem::MoveInstr(
        movq_assem, new temp::TempList({reg}), new temp::TempList({storeReg})));
  }

  // ProcEntryExit2: add sink instrutions
  auto new_instr_list = frame::FrameFactory::ProcEntryExit2(instr_list);
  assem_instr_ = std::make_unique<AssemInstr>(new_instr_list);
}

void AssemInstr::Print(FILE *out, temp::Map *map) const {
  for (auto instr : instr_list_->GetList()) instr->Print(out, map);
  fprintf(out, "\n");
}
}  // namespace cg

// 这里fs就是framesize，作用就是在TempExp中进行替换
namespace tree {
/* TODO: Put your lab5 code here */

void SeqStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  left_->Munch(instr_list, fs);
  right_->Munch(instr_list, fs);
}

void LabelStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  instr_list.Append(
      new assem::LabelInstr(temp::LabelFactory::LabelString(label_), label_));
}

void JumpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  instr_list.Append(new assem::OperInstr("jmp `j0", nullptr, nullptr,
                                         new assem::Targets(jumps_)));
}

void CjumpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  auto left_temp = left_->Munch(instr_list, fs);
  auto right_temp = right_->Munch(instr_list, fs);
  std::string opstr;
  switch (op_) {
      // 因为在tree的namespace不用加tree::
    case EQ_OP:
      opstr = "je `j0";
      break;
    case NE_OP:
      opstr = "jne `j0";
      break;
    case LT_OP:
      opstr = "jl `j0";
      break;
    case LE_OP:
      opstr = "jle `j0";
      break;
    case GE_OP:
      opstr = "jge `j0";
      break;
    case GT_OP:
      opstr = "jg `j0";
      break;
    // 以下几个是无符号比较，不过在translate中没有，看Relop中有就写上了
    case ULT_OP:
      opstr = "jb `j0";
      break;
    case ULE_OP:
      opstr = "jbe `j0";
      break;
    case UGE_OP:
      opstr = "jae `j0";
      break;
    case UGT_OP:
      opstr = "ja `j0";
      break;
    default:
      assert(false);
      break;
  }
  // 这边需要注意cmpq的比较顺序和cjumpstm的相反
  instr_list.Append(new assem::OperInstr(
      "cmpq `s1, `s0", nullptr, new temp::TempList({left_temp, right_temp}),
      nullptr));
  // 注意这里true放在前面表示满足条件就跳转到true的位置
  instr_list.Append(new assem::OperInstr(
      opstr, nullptr, nullptr,
      new assem::Targets(
          new std::vector<temp::Label *>({true_label_, false_label_}))));
}

void MoveStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  // p.s. 如果摸鱼MemExp可以只写move `s0, (`s1)这一种情况
  // 下面写的过程中需要注意,类似move `s0, (`s1)后面那个也是source list
  // 因为不是写到`s1,而是写到(`s1)，没有被写的寄存器（临时变量）
  if (typeid(*dst_) == typeid(MemExp)) {
    // 如果destination是MemExp
    // TODO: 根据BinOp优化
    auto dst_mem_exp = dynamic_cast<MemExp *>(dst_);
    assert(dst_mem_exp);

    // 如果MEM内部是加法运算，可以优化指令，而且这种情况很常见
    if (typeid(dst_mem_exp->exp_) == typeid(BinopExp *)) {
      auto binop_exp = dynamic_cast<BinopExp *>(dst_mem_exp->exp_);
      assert(binop_exp);
      // 只支持对加法优化，其实减法也是一样的操作，只是减法的情况比较少
      if (binop_exp->op_ == PLUS_OP) {
        // 取出左右操作数
        auto left_exp = binop_exp->left_;
        auto right_exp = binop_exp->right_;
        int x;
        tree::Exp *e1 = nullptr;

        if (typeid(right_exp) == typeid(ConstExp *)) {
          // MOVE(MEM(+(e1, CONST(x)), e2)
          auto const_exp = dynamic_cast<ConstExp *>(right_exp);
          x = const_exp->consti_;
          e1 = left_exp;
        } else if (typeid(left_exp) == typeid(ConstExp *)) {
          // MOVE(MEM(+(CONST(x), e1), e2)
          auto const_exp = dynamic_cast<ConstExp *>(left_exp);
          x = const_exp->consti_;
          e1 = right_exp;
        }

        if (e1) {
          // 只要e1不是nullptr，那么就说明有一个是ConstExp，可以优化指令
          // e1表示的是另外一个需要Munch的表达式
          // 此时dst是 x(e1) src是src_->Munch(...)
          // 需要注意Instr中的dst是空，因为没有寄存器被写入
          auto e1_reg = e1->Munch(instr_list, fs);
          auto src_reg = src_->Munch(instr_list, fs);

          // movq src, x(e1)
          instr_list.Append(new assem::OperInstr(
              "movq `s0, " + std::to_string(x) + "(`s1)", nullptr,
              new temp::TempList({src_reg, e1_reg}), nullptr));
          // 这种情况结束就可以退出
          return;
        }
      }
    }

    auto src_tmp = src_->Munch(instr_list, fs);
    auto dst_tmp = dst_mem_exp->exp_->Munch(instr_list, fs);
    instr_list.Append(
        new assem::OperInstr("movq `s0, (`s1)", nullptr,
                             new temp::TempList({src_tmp, dst_tmp}), nullptr));

  } else if (typeid(*dst_) == typeid(TempExp)) {
    // 如果destination是TempExp
    // MOVE(TEMP(i), e2)
    auto dst_exp = dynamic_cast<TempExp *>(dst_);
    assert(dst_exp);
    auto src_temp = src_->Munch(instr_list, fs);
    instr_list.Append(new assem::MoveInstr("movq `s0, `d0",
                                           new temp::TempList({dst_exp->temp_}),
                                           new temp::TempList({src_temp})));
  } else
    assert(0);
}

void ExpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  exp_->Munch(instr_list, fs);
}

temp::Temp *BinopExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  // 在这里要尤其注意操作的dst和src，用于活跃分析
  /* TODO: Put your lab5 code here */
  temp::Temp *left = left_->Munch(instr_list, fs);
  temp::Temp *right = right_->Munch(instr_list, fs);
  temp::Temp *res = temp::TempFactory::NewTemp();
  // reference: https://blog.csdn.net/Chauncyxu/article/details/121890457
  // 注意看清楚乘法和除法的细节,用到rax和rdx
  switch (op_) {
    case PLUS_OP:
      // 先把left移到res 因为 addq a, b <=> b=a+b
      instr_list.Append(new assem::MoveInstr("movq `s0, `d0",
                                             new temp::TempList({res}),
                                             new temp::TempList({left})));
      // res是隐藏的(implicit)src
      // 考虑res=res+right 左边是dst右边是src
      // 计算完了以后res里存的是加起来的和
      // 这里的src和dst在后面用来做活跃分析
      instr_list.Append(
          new assem::OperInstr("addq `s0, `d0", new temp::TempList({res}),
                               new temp::TempList({right, res}), nullptr));
      break;
    case MINUS_OP:
      // 减法与加法基本一致
      instr_list.Append(new assem::MoveInstr("movq `s0, `d0",
                                             new temp::TempList({res}),
                                             new temp::TempList({left})));
      instr_list.Append(
          new assem::OperInstr("subq `s0, `d0", new temp::TempList({res}),
                               new temp::TempList({right, res}), nullptr));
      break;
    case MUL_OP:
      // R[%rdx]:R[%rax]<-S×R[%rax] 有符号乘法
      // 将left移动到%rax里，这里returnValue指代%rax
      instr_list.Append(new assem::MoveInstr(
          "movq `s0, `d0", new temp::TempList({reg_manager->ReturnValue()}),
          new temp::TempList({left})));

      // 将right作为imulq的参数，它会与%rax做乘法，并将结果存放在%rax和%rdx中
      // 等价于 R[%rdx]:R[%rax] = S * R[%rax]
      // src: S, rax; dst:rax,rdx
      instr_list.Append(new assem::OperInstr(
          "imulq `s0", reg_manager->OperateRegs(),
          new temp::TempList({right, reg_manager->ReturnValue()}), nullptr));

      // 最后把%rax移动到reg中,这里只取64位，多的就算溢出了
      instr_list.Append(new assem::MoveInstr(
          "movq `s0, `d0", new temp::TempList({res}),
          new temp::TempList({reg_manager->ReturnValue()})));
      break;
    case DIV_OP:
      // cqto
      // R[%rdx]:R[%rax]←符号扩展(R[%rax])
      // idivq S
      // R[%rdx]←R[%rdx]:R[%rax] mod S
      // R[%rax]←R[%rdx]:R[%rax] ÷ S

      // 移动left到%rax 同乘法
      instr_list.Append(new assem::MoveInstr(
          "movq `s0, `d0", new temp::TempList({reg_manager->ReturnValue()}),
          new temp::TempList({left})));

      // cqto进行扩展，直接把rax的符号位复制到rdx
      // dst:rax,rdx src:rax
      instr_list.Append(new assem::OperInstr(
          "cqto", reg_manager->OperateRegs(),
          new temp::TempList({reg_manager->ReturnValue()}), nullptr));

      // 将右值作为idivq的参数，它是除数与%rax相除，并将商和余数存放在%rax和%rdx中
      // 这里被修改的reg是rax和rdx，被读取的是right
      instr_list.Append(
          new assem::OperInstr("idivq `s0", reg_manager->OperateRegs(),
                               new temp::TempList({right}), nullptr));

      // movq %rax, res
      instr_list.Append(new assem::MoveInstr(
          "movq `s0, `d0", new temp::TempList({res}),
          new temp::TempList({reg_manager->ReturnValue()})));
      break;
    default:
      assert(0);
      break;
  }
  return res;
}

temp::Temp *MemExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  // 根据exp中的地址读取数据并存放到res中
  temp::Temp *res = temp::TempFactory::NewTemp();
  temp::Temp *exp_reg = exp_->Munch(instr_list, fs);
  instr_list.Append(new assem::OperInstr("movq (`s0), `d0",
                                         new temp::TempList({res}),
                                         new temp::TempList(exp_reg), nullptr));
  return res;
}

temp::Temp *TempExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  // 所有的FP都会被包裹在TempExp中出现，所以在这里进行替换
  if (temp_ != reg_manager->FramePointer()) return temp_;
  // 出现FP，进行替换 SP+framesize
  // leaq xx_framesize(`s0), `d0"
  // src:rsp dst:res
  temp::Temp *res = temp::TempFactory::NewTemp();
  std::string opstr = "leaq " + std::string(fs.data()) + "(`s0), `d0";
  instr_list.Append(new assem::OperInstr(
      opstr, new temp::TempList({res}),
      new temp::TempList({reg_manager->StackPointer()}), nullptr));
  return res;
}

temp::Temp *EseqExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  stm_->Munch(instr_list, fs);
  return exp_->Munch(instr_list, fs);
}

temp::Temp *NameExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  temp::Temp *res = temp::TempFactory::NewTemp();
  // 执行重定位时，由于是相对寻址，Label代表的是与PC的偏移量，因此直接获取Label的值需要加上%rip
  // symbol(%rip): Points to the symbol in RIP relative way
  // https://sourceware.org/binutils/docs/as/i386_002dMemory.html
  // https://stackoverflow.com/questions/3250277/how-to-use-rip-relative-addressing-in-a-64-bit-assembly-program
  // 貌似mov name, `d0是一样的效果  (p.s: NO！)
  // "leaq name(%rip), `d0"
  std::string str =
      "leaq " + temp::LabelFactory::LabelString(name_) + "(%rip), `d0";
  // 经测试，这样的写法过不了测试
  // "movq " + temp::LabelFactory::LabelString(name_) + ", `d0";
  instr_list.Append(
      new assem::OperInstr(str, new temp::TempList({res}), nullptr, nullptr));
  return res;
}

temp::Temp *ConstExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  // "movq $imm, `d0"
  temp::Temp *res = temp::TempFactory::NewTemp();
  instr_list.Append(
      new assem::OperInstr("movq $" + std::to_string(consti_) + ", `d0",
                           new temp::TempList({res}), nullptr, nullptr));
  return res;
}

temp::Temp *CallExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  temp::Temp *res = temp::TempFactory::NewTemp();
  // MunchArgs生成将所有参数传送到正确位置的代码，并返回对应的寄存器
  temp::TempList *args = args_->MunchArgs(instr_list, fs);

  // CallExp的fun必须是个NameExp
  // 这里也是NameExp唯一用到的地方，jumpStm里面是不管的
  if (typeid(*fun_) != typeid(NameExp)) {
    assert(0);
    return nullptr;
  }
  auto fun_NameExp = dynamic_cast<NameExp *>(fun_);
  auto fun_name = fun_NameExp->name_;
  // callq fun_name
  // call的src由MunchArgs得出
  // call的dst由CallerSaved+RV组成，保守估计
  auto dst_temp_list = reg_manager->CallerSaves();
  dst_temp_list->Append(reg_manager->ReturnValue());
  instr_list.Append(new assem::OperInstr(
      std::string("callq ") + temp::LabelFactory::LabelString(fun_name),
      dst_temp_list, args, nullptr));
  // 把rax转移到res中，如果寄存器分配顺利的话可以直接把res替换成rax
  instr_list.Append(
      new assem::MoveInstr("movq `s0, `d0", new temp::TempList({res}),
                           new temp::TempList({reg_manager->ReturnValue()})));
  return res;
}

temp::TempList *ExpList::MunchArgs(assem::InstrList &instr_list,
                                   std::string_view fs) {
  /* TODO: Put your lab5 code here */
  // 参考setViewShift
  // res存储用到的放在寄存器中的参数对应的寄存器
  auto res = new temp::TempList();
  int ws = reg_manager->WordSize();
  const auto &arg_regs = reg_manager->ArgRegs()->GetList();
  // 遍历用作参数的寄存器列表，把用到的加入res
  auto it = arg_regs.begin();
  bool reg_over = false;

  int i = 0;

  const auto &exp_list = GetList();
  for (tree::Exp *exp : exp_list) {
    temp::Temp *arg_reg = exp->Munch(instr_list, fs);
    if (!reg_over) {
      // movq arg_reg, *it
      instr_list.Append(new assem::MoveInstr("movq `s0, `d0",
                                             new temp::TempList({*it}),
                                             new temp::TempList({arg_reg})));
      res->Append(*it);

      it++;
      if (it == arg_regs.end()) {
        reg_over = true;
      }
    } else {
      // movq arg_reg, offset(SP)  //offset = i*wordsizes
      // 进入函数的时候rsp已经减去了考虑会call的最大参数量的framesize
      // 现在rsp上方的空间就是剩下给超过6个参数的函数来存放参数的
      // 第7个就放在rsp开始向上一个wordsize的位置
      std::string str = "movq `s0, " + std::to_string(i * ws) + "(`s1)";
      instr_list.Append(new assem::OperInstr(
          str, nullptr,
          new temp::TempList({arg_reg, reg_manager->StackPointer()}), nullptr));
      i++;
    }
  }
  return res;
}

}  // namespace tree
