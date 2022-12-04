/*
 * Author: Lu Chang <chinaluchang@live.com>
 * Create Date:   Fri May 20 01:38:41 2022 +0800
 * Releated Blog: https://www.luu.moe/114
 */
%define parse.error verbose
%locations
%{
#include "ly.h"
extern int yylineno;
extern FILE *yyin;
void yyerror(const char* fmt, ...);
%}


%type  <node> Program Segments  Bigdef        Type ExtDecList     FuncDec             Blockstat   VarList  VarDec    ParamVarDec ExtVar

%type <node>  ParamDef  Stmt     StmList          Def          DecList              Dec                Exp         Args            IntCount 


/*---------------------1.Tokens---------------------*/

%token <data_int> INT              //指定INT的语义值是data_int,from词法分析得到的数值  

%token <data_str> IDENT RELOP TYPE  //指定ID,RELOP 的语义值是data_str,from词法分析得到的标识符字符串 

%token <data_char> CHAR   

%token LP RP LB RB LBT RBT ENDL COMMA   //用bison对该文件编译时，带参数-d，生成的exp.tab.h中给这些单词进行编码，可在lex.l中包含parser.tab.h使用这些单词种类码
//     (  )  {  }  [  ]    ;    ,  
%token PLUS PLUSASS MINUS MINUSASS STAR DIV MOD ASSIGN AND OR NOT IF ELSE WHILE RETURN SELFPLUS SLEFMINUS FOR BREAK CONTINUE
//      +     +=      -     -=      *    /    =       &&  || !  if  else  while  return  ++      --     for  break continue

%left ASSIGN
%left OR
%left AND
%left RELOP
%left SELFPLUS SLEFMINUS
%left MINUSASS PLUSASS
%left PLUS MINUS
%left STAR DIV MOD
%right UMINUS NOT 

%nonassoc LOWER_THEN_ELSE
%nonassoc ELSE

%%

//->segment->全局变量或者函数名定义或声明
Program: Segments{ast_root = new_ast_node("PROGRAM",yylineno,$1,NULL,NULL);$$=ast_root;}; 

/*Segments 零个或多个Bigdef*/
Segments: {$$=NULL;} 
        | Bigdef Segments {$$=new_ast_node("SEGMENT_LIST",yylineno,$1,$2,NULL);}
        ; 

/*接Segment 表示全局变量，结构体，函数声明，函数定义*/
Bigdef: Type ExtDecList ENDL {$$=new_ast_node("EXT_VAR_DEF",yylineno,$1,$2,NULL);}   //全局变量
        //|Type ENDL {} //结构体当前未实现
        |Type FuncDec Blockstat {$$=new_ast_node("FUNC_DEF",yylineno,$1,$2,$3);}         //函数定义
        | error ENDL   {$$=NULL; }
        |Type FuncDec ENDL {$$=new_ast_node("FUNC_DEF_LITE",yylineno,$1,$2,NULL);}//函数声明
        ;
 
/*零个或多个变量的定义VarDec*/
ExtDecList: VarDec {$$=$1;} 
        | VarDec COMMA ExtDecList {$$=new_ast_node("EXT_DEC_LIST",yylineno,$1,$3,NULL);}
        ; 


/*类型描述符,如int float char*/
Type: TYPE {$$=new_ast_node("TYPE",yylineno,NULL,NULL,NULL);$$->data_str=$1;} 
        //|结构体
        ;    


/*对一个变量的定义*/ 
VarDec:  IDENT ASSIGN Exp {$$=new_ast_node("V_D_ASSIGN",yylineno,$3,NULL,NULL);$$->data_str=$1;}   //a = 5.测试样例里不存在a[4]=5一类数组
        | IDENT {$$=new_ast_node("IDENT",yylineno,NULL,NULL,NULL);$$->data_str=$1;}   //标识符，如a
        | VarDec LBT IntCount RBT        {$$=new_ast_node("ARRAY",yylineno,$1,$3,NULL);} //数组，如a[10]，可以叠来多重数组


/*形参相关*/
ParamVarDec:    IDENT {$$=new_ast_node("IDENT",yylineno,NULL,NULL,NULL);$$->data_str=$1;}   //标识符，如a
                | IDENT LBT RBT        {$$=new_ast_node("IDENT_P_ARRAY",yylineno,NULL,NULL,NULL);$$->data_str=$1;} //数组，如a[]，仅用于形参
                | IDENT LBT INT RBT        {$$=new_ast_node("IDENT_P_ARRAY_INT",yylineno,NULL,NULL,NULL);$$->data_str=$1;$$->data_int=$3;} //数组，如a[10]
                ;

/*用于辅助数组文法*/
IntCount: INT {$$=new_ast_node("INT",yylineno,NULL,NULL,NULL);$$->data_int=$1;}
        ;

/*函数头的定义*/ //a() or a(int b, int c)
FuncDec: IDENT LP VarList RP {$$=new_ast_node("FUNC_DEC",yylineno,$3,NULL,NULL);$$->data_str=$1;}//含参数函数
        |IDENT LP  RP   {$$=new_ast_node("FUNC_DEC",yylineno,NULL,NULL,NULL);$$->data_str=$1;}//无参数函数
        ;  

/*参数列表*/
VarList: ParamDef  {$$=$1;}//一个形参的定义
        | ParamDef COMMA  VarList  {$$=new_ast_node("PARAM_LIST",yylineno,$1,$3,NULL);}
        ;

/*一个形参的定义 int     */
ParamDef: Type ParamVarDec         {$$=new_ast_node("PARAM_DEF",yylineno,$1,$2,NULL);}
        | Type                     {$$=new_ast_node("PARAM_DEF_BUILTIN",yylineno,$1,NULL,NULL);}
        ;



/*花括号括起来的语句块*/
Blockstat: LB StmList RB    {$$=new_ast_node("COMP_STM",yylineno,$2,NULL,NULL);}
        ;

/*一系列语句列表*/
StmList: {$$=NULL; }  
        | Stmt StmList  {$$=new_ast_node("STM_LIST",yylineno,$1,$2,NULL);}
        ;

/*单条语句*/
Stmt: Exp ENDL    {$$=new_ast_node("EXP_STMT",yylineno,$1,NULL,NULL);}//一条表达式
      | Blockstat      {$$=$1;}      //另一个语句块
      | RETURN ENDL   {$$=new_ast_node("RETURN_EMP",yylineno,NULL,NULL,NULL);}//空返回语句
      | RETURN Exp ENDL   {$$=new_ast_node("RETURN",yylineno,$2,NULL,NULL);}//返回语句
      | IF LP Exp RP Stmt %prec LOWER_THEN_ELSE   {$$=new_ast_node("IF_THEN",yylineno,$3,$5,NULL);} //if语句
      | IF LP Exp RP Stmt ELSE Stmt   {$$=new_ast_node("IF_THEN_ELSE",yylineno,$3,$5,$7);}//if-else 语句
      | WHILE LP Exp RP Stmt {$$=new_ast_node("WHILE",yylineno,$3,$5,NULL);}//while 语句
      | Def {$$=$1;}// 在代码中间也定义本地变量
      ;


/*一个变量定义*/
Def: Type DecList ENDL {$$=new_ast_node("VAR_DEF",yylineno,$1,$2,NULL);}
     ;

/*如int a,b,c*/
DecList: Dec  {$$=$1;}
        | Dec COMMA DecList  {$$=new_ast_node("DEC_LIST",yylineno,$1,$3,NULL);}
	;

/*变量名(或者带初始化不考虑)*/
Dec:   VarDec {$$=$1;}
       ;



/*表达式中可能遇到的IDENT和ARRAY（中括号里面可能是exp）*/
ExtVar:  IDENT {$$=new_ast_node("IDENT",yylineno,NULL,NULL,NULL);$$->data_str=$1;}   //标识符，如a
        | ExtVar LBT Exp RBT        {$$=new_ast_node("ARRAY",yylineno,$1,$3,NULL);} //数组，如a[10]，a[i]，a[i+1]
        ;

/*运算表达式*/
Exp:  //二元运算
        Exp ASSIGN Exp {$$=new_ast_node("ASSIGN",yylineno,$1,$3,NULL);}//二元运算
      | Exp AND Exp   {$$=new_ast_node("AND",yylineno,$1,$3,NULL);}//逻辑与
      | Exp OR Exp    {$$=new_ast_node("OR",yylineno,$1,$3,NULL);}//逻辑或
      | Exp RELOP Exp {$$=new_ast_node("RELOP",yylineno,$1,$3,NULL);$$->data_str=$2;}//关系表达式
      | Exp PLUS Exp  {$$=new_ast_node("PLUS",yylineno,$1,$3,NULL);}//五则则运算
      | Exp MINUS Exp {$$=new_ast_node("MINUS",yylineno,$1,$3,NULL);}
      | Exp STAR Exp  {$$=new_ast_node("MUL",yylineno,$1,$3,NULL);}
      | Exp DIV Exp   {$$=new_ast_node("DIV",yylineno,$1,$3,NULL);}
      | Exp MOD Exp   {$$=new_ast_node("MOD",yylineno,$1,$3,NULL);}
      //-------额外实现-------
      | Exp PLUSASS Exp  {$$=new_ast_node("PLUSASS",yylineno,$1,$3,NULL);} //复合加
      | Exp SELFPLUS      {$$=new_ast_node("USELFPLUS",yylineno,$1,NULL,NULL);}  //自增
      | SELFPLUS Exp     {$$=new_ast_node("MSELFPLUS",yylineno,$2,NULL,NULL);}  //自增
      | Exp MINUSASS Exp {$$=new_ast_node("MINUSASS",yylineno,$1,$3,NULL);} //复合减
      | Exp SLEFMINUS  {$$=new_ast_node("USLEFMINUS",yylineno,$1,NULL,NULL);} //自减
      | SLEFMINUS Exp {$$=new_ast_node("MSLEFMINUS",yylineno,$2,NULL,NULL);} //自减
      //-------以上-------

      //一元运算
      | LP Exp RP     {$$=$2;}//括号表达式
      | MINUS Exp %prec UMINUS   {$$=new_ast_node("UMINUS",yylineno,$2,NULL,NULL);}//取负
      | NOT Exp       {$$=new_ast_node("NOT",yylineno,$2,NULL,NULL);}//逻辑或
      
      //不包含运算符，较特殊的表达式
      | IDENT LP Args RP {$$=new_ast_node("FUNC_CALL",yylineno,$3,NULL,NULL);$$->data_str=$1;}  //函数调用(含参)
      | IDENT LP RP      {$$=new_ast_node("FUNC_CALL",yylineno,NULL,NULL,NULL);$$->data_str=$1;}//函数调用(无参)

      
      //最基本表达式
      | ExtVar  {$$=$1;}//"IDENT"或"ARRAY"数组
      | INT           {$$=new_ast_node("INT",yylineno,NULL,NULL,NULL);$$->data_int=$1;}      //整常数常量
      | CHAR          {$$=new_ast_node("CHAR",yylineno,NULL,NULL,NULL);$$->data_char=$1;} //字符常量
      | BREAK         {$$=new_ast_node("BREAK",yylineno,NULL,NULL,NULL);}
      | CONTINUE      {$$=new_ast_node("CONTINUE",yylineno,NULL,NULL,NULL);}
      ;

/*实参列表*/
Args:  Exp COMMA Args    {$$=new_ast_node("ARG_LIST",yylineno,$1,$3,NULL);}
       | Exp               {$$=new_ast_node("ARG_LIST",yylineno,$1,NULL,NULL);}
       ;
       
%%