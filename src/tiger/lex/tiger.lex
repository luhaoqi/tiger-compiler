%filenames = "scanner"

invisible [\000-\040]

%x COMMENT STR IGNORE

%%

 /* reserved words */
"array" {adjust(); return Parser::ARRAY;}
"if" {adjust(); return Parser::IF;}
"then" {adjust(); return Parser::THEN;}
"else" {adjust(); return Parser::ELSE;}
"while" {adjust(); return Parser::WHILE;}
"for" {adjust(); return Parser::FOR;}
"to" {adjust(); return Parser::TO;}
"do" {adjust(); return Parser::DO;}
"let" {adjust(); return Parser::LET;}
"in" {adjust(); return Parser::IN;}
"end" {adjust(); return Parser::END;}
"of" {adjust(); return Parser::OF;}
"break" {adjust(); return Parser::BREAK;}
"nil" {adjust(); return Parser::NIL;}
"function" {adjust(); return Parser::FUNCTION;}
"var" {adjust(); return Parser::VAR;}
"type" {adjust(); return Parser::TYPE;}

/* operator */
"+" {adjust(); return Parser::PLUS;}
"-" {adjust(); return Parser::MINUS;}
"*" {adjust(); return Parser::TIMES;}
"/" {adjust(); return Parser::DIVIDE;}
"=" {adjust(); return Parser::EQ;}
"<>" {adjust(); return Parser::NEQ;}
"<" {adjust(); return Parser::LT;}
"<=" {adjust(); return Parser::LE;}
">" {adjust(); return Parser::GT;}
">=" {adjust(); return Parser::GE;}
"&" {adjust(); return Parser::AND;}
"|" {adjust(); return Parser::OR;}
":=" {adjust(); return Parser::ASSIGN;}

/* some symbols */
"(" {adjust(); return Parser::LPAREN;}
")" {adjust(); return Parser::RPAREN;}
"[" {adjust(); return Parser::LBRACK;}
"]" {adjust(); return Parser::RBRACK;}
"{" {adjust(); return Parser::LBRACE;}
"}" {adjust(); return Parser::RBRACE;}
"," {adjust(); return Parser::COMMA;}
":" {adjust(); return Parser::COLON;}
";" {adjust(); return Parser::SEMICOLON;}
"." {adjust(); return Parser::DOT;}

/* ID and INT */
[a-zA-Z][a-zA-Z0-9_]* {adjust(); return Parser::ID;}
[0-9]+ {adjust(); return Parser::INT;}

/* comment */
"/*"                    {
                            adjust();
                            begin(StartCondition__::COMMENT);
                            comment_level_ = 1;
                        }

<COMMENT>{
    .|\n                {adjustStr();}
    "*/"                {
                            adjustStr();
                            comment_level_ -= 1;
                            if (!comment_level_) begin(StartCondition__::INITIAL);
                        }
    "/*"                {
                            adjustStr(); 
                            comment_level_++; 
                        }
}

/* String */
    \"                  {
                            adjust();
                            begin(StartCondition__::STR);
                            string_buf_.clear();
                        }
<STR>{
    \\n                 {adjustStr(); string_buf_ += '\n';}
    \\t                 {adjustStr(); string_buf_ += '\t';}
    \\\^[A-Z]           {adjustStr(); string_buf_ += matched()[2] - 'A' + 1;}
    \\[0-9]{3}          {adjustStr(); string_buf_ += (char) atoi(matched().c_str() + 1);}    
    \\\"                {adjustStr(); string_buf_ += '\"';}
    \\\\                {adjustStr(); string_buf_ += '\\';}
    \\[ \n\t\f]+\\      {adjustStr(); }

    \"                  {
              adjustStr();
              setMatched(string_buf_);
              string_buf_.clear();
              begin(StartCondition__::INITIAL); 
              return Parser::STRING;
    }

    .                   {adjustStr(); string_buf_ += matched(); }
}


 /*
  * skip white space chars.
  * space, tabs and LF
  */
[ \t]+ {adjust();}
\n {adjust(); errormsg_->Newline();}

 /* end of file */
<<EOF>>  return 0;

 /* illegal input */
. {adjust(); errormsg_->Error(errormsg_->tok_pos_, "illegal token");}
