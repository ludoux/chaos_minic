/*
 * Author: Lu Chang <chinaluchang@live.com>
 * Create Date:   Fri May 20 01:38:41 2022 +0800
 * Releated Blog: https://www.luu.moe/114
 */
%{
#include "ly.h"
#include "yacc.tab.h"
int yycolumn=1;
#define YY_USER_ACTION yylloc.first_line=yylloc.last_line=yylineno; \
yylloc.first_column=yycolumn;	yylloc.last_column=yycolumn+yyleng-1; yycolumn+=yyleng;//每次匹配之前执行的动作,yylloc表示当前词法单元所对应的位置信息

void comment();
int hex2int(char * input);
int oct2int(char * input);
%}
%option yylineno

id   [A-Za-z_][A-Za-z0-9_]*  
int    [0-9]+
hex [0-9a-fA-F]+
char [A-Z]|[a-z]|"!"|"@"|"#"|"$"|"%"|"^"|"&"|"*"|"("|")"|"_"|"-"|"+"|"="|"\\"|"{"|"}"|"["|"]"|":"|";"|"'"|"<"|">"|","|"."|"?"|"/"|"~"|"`"

%%


[0][x]{hex}        {yylval.data_int=hex2int(yytext); return INT;}
[0]{int}        {yylval.data_int=oct2int(yytext); return INT;}
{int}        {yylval.data_int=atoi(yytext); return INT;}
"'"{char}"'" {yylval.data_char=yytext[1]; return CHAR;}
"void"        {yylval.data_str=yytext;return TYPE;}
"int"        {yylval.data_str=yytext;return TYPE;}
"char"       {yylval.data_str=yytext;return TYPE;}

"return"     {return RETURN;}
"if"         {return IF;}
"else"       {return ELSE;}
"while"      {return WHILE;}
"for"        {return FOR;}
"break"      {return BREAK;}
"continue"   {return CONTINUE;}

{id}       {yylval.data_str=yytext; return IDENT;}
";"			{return ENDL;}
","			{return COMMA;}
">"|"<"|">="|"<="|"=="|"!=" {yylval.data_str=yytext;return RELOP;}
"="			{return ASSIGN;}
"+"			{return PLUS;}
"+="        {return PLUSASS;}
"++"       {return SELFPLUS;}
"-"			{return MINUS;}
"-="        {return MINUSASS;}
"--"        {return SLEFMINUS;}
"*"			{return STAR;}
"/"			{return DIV;}
"%"         {return MOD;}
"&&"        {return AND;}
"||"        {return OR;}
"!"			{return NOT;}
"("			{return LP;}//parentheses
")"			{return RP;}
"{"			{return LB;}//braces
"}"			{return RB;}
"["         {return LBT;}//brackets
"]"         {return RBT;}
"//"[^\n]*\n  ;
"/*"([^\*]|(\*)*[^\*/])*(\*)*"*/" ;
[\n]        {yycolumn=1;}   
[ \r\t]     {}   
.			{printf("Error type A :Mysterious character \"%s\"\n\t at Line %d\n",yytext,yylineno);}
%%
int yywrap()
{
	return 1;
}

//以0开头
int oct2int(char * input)
{
    int rt=0;
    for(int i=1;input[i]!='\0';i++)
    {
        int t;
        t = input[i] - '0';
        rt = rt*8+t;
    }
    return rt;
}

//以0x开头，
int hex2int(char * input)
{
    int rt=0;
    for(int i=2;input[i]!='\0';i++)
    {
        int t;
        if (input[i] <= '9')
        {
            t= input[i] - '0';
        }
        else if(input[i] >= 'A' && input[i] <= 'F')
        {
            t = input[i] - 'A' + 10;
        }
        else if(input[i] >= 'a' && input[i] <= 'f')
        {
            t = input[i] - 'a' + 10;
        }
        rt = rt*16+t;
    }
    return rt;
}
