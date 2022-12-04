/*
 * Author: Lu Chang <chinaluchang@live.com>
 * Create Date:   Fri May 20 01:38:41 2022 +0800
 * Releated Blog: https://www.luu.moe/114
 */
#include <iostream>
#include <stdio.h>
#include <string>
#include <vector>
using namespace std;

typedef struct node_s
{
    struct node_s *parent;    // 父节点
    std::vector<struct node_s *> children;
    string type;
    string data_str;
    int data_int;
    char data_char;
    int id;
}Node;

struct lex_data
{
    int data_int;
    string data_str;
    char data_char;
    Node * node;
};

#define YYSTYPE lex_data
extern Node * ast_root;

extern "C"//为了能够在C++程序里面调用C函数，必须把每一个需要使用的C函数，其声明都包括在extern "C"{}块里面，这样C++链接时才能成功链接它们。extern "C"用来在C++环境下设置C链接类型。
{	//yacc.y中也有类似的这段extern "C"，可以把它们合并成一段，放到共同的头文件main.h中
	int yywrap(void);
	int yylex(void);//这个是lex生成的词法分析函数，yacc的yyparse()里会调用它，如果这里不声明，生成的yacc.tab.c在编译时会找不到该函数
}
Node * new_ast_node(string type,
    int lineno,
    Node * s1, 
    Node * s2,
    Node * s3);
//Node * new_ast_node(string type, int lineno, Node * s1 = NULL, Node * s2 = NULL, Node * s3 = NULL);