%filenames parser
%scanner tiger/lex/scanner.h
%baseclass-preinclude tiger/absyn/absyn.h

 /*
  * Please don't modify the lines above.
  */

%union {
  int ival;
  std::string* sval;
  sym::Symbol *sym;
  absyn::Exp *exp;
  absyn::ExpList *explist;
  absyn::Var *var;
  absyn::DecList *declist;
  absyn::Dec *dec;
  absyn::EFieldList *efieldlist;
  absyn::EField *efield;
  absyn::NameAndTyList *tydeclist;
  absyn::NameAndTy *tydec;
  absyn::FieldList *fieldlist;
  absyn::Field *field;
  absyn::FunDecList *fundeclist;
  absyn::FunDec *fundec;
  absyn::Ty *ty;
  }

%token <sym> ID
%token <sval> STRING
%token <ival> INT

%token
  COMMA COLON SEMICOLON LPAREN RPAREN LBRACK RBRACK
  LBRACE RBRACE DOT
  ARRAY IF THEN ELSE WHILE FOR TO DO LET IN END OF
  BREAK NIL
  FUNCTION VAR TYPE

 /* token priority */
%nonassoc ASSIGN
%left OR
%left AND
%nonassoc EQ NEQ LT LE GT GE
%left PLUS MINUS
%left TIMES DIVIDE
%right UMINUS
 /* TODO: Put your lab3 code here */

%type <exp> exp expseq
%type <explist> sequencing sequencing_exps one_exp
%type <var> lvalue 
%type <declist> decs decs_nonempty
%type <dec> decs_nonempty_s vardec
%type <efieldlist> rec rec_nonempty
%type <efield> rec_one
%type <tydeclist> tydec_list
%type <tydec> tydec_one
%type <fieldlist> tyfields tyfields_nonempty
%type <field> tyfield
%type <ty> ty
%type <fundeclist> fundec_list
%type <fundec> fundec_one

%start program
// absyn.h 中的List有空的默认构造函数用来构造空链表
// Exp/Dec 中的List参数指针不能为空 需要链表为空的情况构造空链表传进去
// $$ = $3; $$->Prepend($1);  <=> $$ = $3 -> Prepend($1);

%%
program:  exp  {absyn_tree_ = std::make_unique<absyn::AbsynTree>($1);};

lvalue:  ID  {$$ = new absyn::SimpleVar(scanner_.GetTokPos(), $1);}               //lvalue->id
  |  lvalue DOT ID {$$ = new absyn::FieldVar(scanner_.GetTokPos(), $1, $3);}      //lvalue->lvalue.id
  |  lvalue LBRACK exp RBRACK {$$ = new absyn::SubscriptVar(scanner_.GetTokPos(), $1, $3);}  //lvalue->lvalue[exp]
  //不加过不了d[3] 可能是因为shift-reduce conflict
  |  ID LBRACK exp RBRACK {$$ = new absyn::SubscriptVar(scanner_.GetTokPos(), new absyn::SimpleVar(scanner_.GetTokPos(), $1), $3);}
  ;

expseq:                                                                 //类型为Exp  用在let语句 
                                                                        //SeqExp  exp;exp;exp;exp ...
                                                                        //数量不限 0 或更多个
      sequencing {$$ = new absyn::SeqExp(scanner_.GetTokPos(), $1);}    //expseq -> sequencing (2个及以上)
  |   one_exp {$$ = new absyn::SeqExp(scanner_.GetTokPos(), $1);}       //expseq -> one_exp (1个)
  |   {$$ = new absyn::SeqExp(scanner_.GetTokPos(), new absyn::ExpList());}  //empty!
  ;

one_exp:                                                        //类型为ExpList 只有1个exp 用于let语句
      exp {$$ = new absyn::ExpList($1);}                        //一个exp
  ;

sequencing:                                                                 //序列,两个以上的exp:  exp;exp...
                                                                            //类型为ExpList,用 ; 分隔
      exp SEMICOLON exp {$$ = new absyn::ExpList($3); $$->Prepend($1);}     //exp;exp 最少两个 用于(sequencing)
  |   exp SEMICOLON sequencing {$$ = $3; $$->Prepend($1);}                  //exp;exp;exp...
  ;

sequencing_exps:                                                //exp,...  至少一个exp
                                                                //类型为ExpList,用 , 分隔 用于函数call
      exp {$$ = new absyn::ExpList($1);}                        //explist -> exp
  |   exp COMMA sequencing_exps {$$ = $3; $$->Prepend($1);}     //explist -> exp , explist
  ;
exp:
      lvalue {$$ = new absyn::VarExp(scanner_.GetTokPos(), $1);}      //exp -> lvalue
  |   NIL {$$ = new absyn::NilExp(scanner_.GetTokPos());}             //exp -> nil  
  |   INT {$$ = new absyn::IntExp(scanner_.GetTokPos(), $1);}         //exp -> int
  |   STRING {$$ = new absyn::StringExp(scanner_.GetTokPos(), $1);}   //exp -> string
  |   LPAREN sequencing RPAREN {$$ = new absyn::SeqExp(scanner_.GetTokPos(), $2);}  //exp -> (exp;exp...)
  |   LPAREN exp RPAREN {$$ = $2;}                                    //exp -> (exp)
  |   LPAREN RPAREN {$$ = new absyn::VoidExp(scanner_.GetTokPos());}  //exp -> ()
  // following is opExp (| & + - * / - = <> < <= > >=)
  |   exp AND exp  {$$ = new absyn::OpExp(scanner_.GetTokPos(), absyn::AND_OP, $1, $3);}        //exp -> exp & exp
  |   exp OR exp  {$$ = new absyn::OpExp(scanner_.GetTokPos(), absyn::OR_OP, $1, $3);}          //exp -> exp | exp
  |   exp PLUS exp  {$$ = new absyn::OpExp(scanner_.GetTokPos(), absyn::PLUS_OP, $1, $3);}      //exp -> exp + exp
  |   exp MINUS exp  {$$ = new absyn::OpExp(scanner_.GetTokPos(), absyn::MINUS_OP, $1, $3);}    //exp -> exp - exp
  |   exp TIMES exp  {$$ = new absyn::OpExp(scanner_.GetTokPos(), absyn::TIMES_OP, $1, $3);}    //exp -> exp * exp
  |   exp DIVIDE exp  {$$ = new absyn::OpExp(scanner_.GetTokPos(), absyn::DIVIDE_OP, $1, $3);}  //exp -> exp / exp
  |   MINUS exp %prec UMINUS {$$ = new absyn::OpExp(scanner_.GetTokPos(), absyn::MINUS_OP, new absyn::IntExp(scanner_.GetTokPos(), 0), $2);} // exp -> - exp
  |   exp EQ exp  {$$ = new absyn::OpExp(scanner_.GetTokPos(), absyn::EQ_OP, $1, $3);}          //exp -> exp = exp
  |   exp NEQ exp {$$ = new absyn::OpExp(scanner_.GetTokPos(), absyn::NEQ_OP, $1, $3);}         //exp -> exp <> exp
  |   exp LT exp  {$$ = new absyn::OpExp(scanner_.GetTokPos(), absyn::LT_OP, $1, $3);}          //exp -> exp < exp
  |   exp LE exp  {$$ = new absyn::OpExp(scanner_.GetTokPos(), absyn::LE_OP, $1, $3);}          //exp -> exp <= exp
  |   exp GT exp  {$$ = new absyn::OpExp(scanner_.GetTokPos(), absyn::GT_OP, $1, $3);}          //exp -> exp > exp
  |   exp GE exp  {$$ = new absyn::OpExp(scanner_.GetTokPos(), absyn::GE_OP, $1, $3);}          //exp -> exp >= exp
  //  callExp  (函数调用)
  |   ID LPAREN RPAREN {$$ = new absyn::CallExp(scanner_.GetTokPos(), $1, new absyn::ExpList());} // exp -> id() 没有参数也不能传nullptr 创建一个空的ExpList
  |   ID LPAREN sequencing_exps RPAREN {$$ = new absyn::CallExp(scanner_.GetTokPos(), $1, $3);}   // exp -> id(exp {, exp...})
  // ifExp (if-then / if-then-else) (shift-reduce conflict)
  |   IF exp THEN exp {$$ = new absyn::IfExp(scanner_.GetTokPos(), $2, $4, nullptr);}        
  |   IF exp THEN exp ELSE exp {$$ = new absyn::IfExp(scanner_.GetTokPos(), $2, $4, $6);}
  // loopExp (for / while / break)
  |   FOR ID ASSIGN exp TO exp DO exp {$$ = new absyn::ForExp(scanner_.GetTokPos(), $2, $4, $6, $8);} // exp -> for id:=exp to exp do exp
  |   WHILE exp DO exp {$$ = new absyn::WhileExp(scanner_.GetTokPos(), $2, $4);}    // exp -> while exp do exp
  |   BREAK {$$ = new absyn::BreakExp(scanner_.GetTokPos());}                       // exp -> break
  // assignExp 
  |   lvalue ASSIGN exp {$$ = new absyn::AssignExp(scanner_.GetTokPos(), $1, $3);}  // exp -> lvalue := exp
  // letExp
  |   LET decs IN expseq END {$$ = new absyn::LetExp(scanner_.GetTokPos(), $2, $4);}
  // recordExp
  //    |   ID LBRACE RBRACE {$$ = new absyn::RecordExp(scanner_.GetTokPos(), $1, new absyn::EFieldList());}  //这一行不需要了 包含在rec中了
  |   ID LBRACE rec RBRACE {$$ = new absyn::RecordExp(scanner_.GetTokPos(), $1, $3);}
  // arrayExp
  |   ID LBRACK exp RBRACK OF exp {$$ = new absyn::ArrayExp(scanner_.GetTokPos(), $1, $3, $6);} //exp -> id[exp] of exp
  ;

// 以下是关于记录类型创建或者赋值或声明中的EFieldList相关的声明
rec:                                    //record记录类型列表  使用EFiledList {id=exp,...}
      {$$ = new absyn::EFieldList();}   //empty record
  |   rec_nonempty {$$ = $1;}           //nonempty record
  ;

rec_nonempty:                                                   //非空记录类型列表  
      rec_one            {$$ = new absyn::EFieldList($1);}      // rec
  |   rec_one COMMA rec_nonempty   {$$ = $3 -> Prepend($1);}    // rec , reclist
  ;

rec_one:  
      ID EQ exp   {$$ = new absyn::EField($1, $3);}             // id = exp
  ;


decs:                                           //let 中声明
      decs_nonempty {$$ = $1;}                  //非空声明
  |   {$$ = new absyn::DecList();}              //空声明
  ;

decs_nonempty:
      decs_nonempty_s {$$ = new absyn::DecList($1);}            // decs -> dec
  |   decs_nonempty_s decs_nonempty {$$ = $2->Prepend($1);}     // decs -> dec decs
  ;

decs_nonempty_s:
      vardec {$$ = $1;}                                                      //变量声明
  |   tydec_list {$$ = new absyn::TypeDec(scanner_.GetTokPos(), $1);}       //类型声明(一组)
  |   fundec_list {$$ = new absyn::FunctionDec(scanner_.GetTokPos(), $1);}  //函数声明(一组)
  ;

//type_id 包括 int string 都是ID
vardec:
      VAR ID ASSIGN exp {$$ = new absyn::VarDec(scanner_.GetTokPos(), $2, nullptr, $4);}        //var id := exp
  |   VAR ID COLON ID ASSIGN exp {$$ = new absyn::VarDec(scanner_.GetTokPos(), $2, $4, $6);}    //var id : type-id := exp
  ;

// 一组typedec可以互相递归调用
tydec_list:
      tydec_one {$$ = new absyn::NameAndTyList($1);}
  |   tydec_one tydec_list {$$ = $2 -> Prepend($1);}
  ;

tydec_one : 
      TYPE ID EQ ty {$$ = new absyn::NameAndTy($2, $4);}        // type type-id = ty
  ;

ty:
      ID {$$ = new absyn::NameTy(scanner_.GetTokPos(), $1);}                            //ty -> type-id
  |   LBRACE tyfields RBRACE {$$ = new absyn::RecordTy(scanner_.GetTokPos(), $2);}      //ty -> {tyfields}
  |   ARRAY OF ID {$$ = new absyn::ArrayTy(scanner_.GetTokPos(), $3);}                  //ty -> array of type-id
  ;

// fieldList 类型 {id:type-id,...} {}
tyfields:
      tyfields_nonempty {$$ = $1;}          // {id:type-id,...}
  |   {$$ = new absyn::FieldList();}        // {} emptyList
  ;

tyfields_nonempty:
      tyfield {$$ = new absyn::FieldList($1);}
  |   tyfield COMMA tyfields_nonempty {$$ = $3 -> Prepend($1);}
  ;

tyfield:
      ID COLON ID {$$ = new absyn::Field(scanner_.GetTokPos(), $1, $3);} // id : type_id
  ;

//函数类型定义(一组)
fundec_list:
      fundec_one {$$ = new absyn::FunDecList($1);}
  |   fundec_one fundec_list {$$ = $2 -> Prepend($1);}
  ;
      
fundec_one:
      FUNCTION ID LPAREN tyfields RPAREN EQ exp {$$ = new absyn::FunDec(scanner_.GetTokPos(), $2, $4, nullptr, $7);}        //fundec -> function id {tyfields} = exp
  |   FUNCTION ID LPAREN tyfields RPAREN COLON ID EQ exp {$$ = new absyn::FunDec(scanner_.GetTokPos(), $2, $4, $7, $9);}    //fundec -> function id {tyfields} : type-id = exp
  ;


