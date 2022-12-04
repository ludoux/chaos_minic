/*
 * Author: Lu Chang <chinaluchang@live.com>
 * Create Date:   Fri May 20 01:38:41 2022 +0800
 * Releated Blog: https://www.luu.moe/114
 */
#include <iostream>
#include <fstream>
#include <stdio.h>
#include <cstdarg>
#include <string>
#include <graphviz/gvc.h>
#include <graphviz/gvcext.h>
#include <vector>
#include <sstream>
#include <stack>
using namespace std;
//为20%注释覆盖率而奋斗！
int node_num = 0;
int edge_num = 0;
int _unique = 0;
typedef struct node_s
{
    struct node_s *parent; // 父节点
    std::vector<struct node_s *> children;
    string type;
    string data_str;
    int data_int;
    char data_char;
    int id;
} Node;

struct lex_data
{
    int data_int;
    string data_str;
    char data_char;
};

#define YYSTYPE lex_data
#include "yacc.tab.h"

extern Node *ast_root;
//=============
// 2为正文。
int _used_max_label = 2;
// 2为正文。在条件判断时候进不同block的时候要改这个，以让写irs的时候写对
int G_NOW_LABEL = 2;
string G_FUNC_BLOCK = "_";
// 符号表，全局变量维护no,0，之后每个函数维护一个，函数内部的block层次用level表示
typedef struct symbol_s
{
    string name;
    string type;          // int void
    string kind;          // 全局变量、局部变量、临时变量、\形参T\形参L\返回值（常量不需要？）、函数、临时
    string inname;        //内部存的名字
    int level;            //层级
    int ref_tableid = -1; // 给"函数"指定对应的table
} Symbol;

typedef struct WhileInfo_s
{
    int condition_label;
    int continue_label;
    int break_label;
} WhileInfo;

//为基本块划分而产生的
typedef struct block_s
{
    int label;
    vector<string> irs;
    vector<int> to_labels;
} Block;
//将会在 printIrs 函数里面读取插入
vector<Block *> BlockVec;

//[0] 为全局，后面为每个function一个。每个function按block来分level
vector<vector<Symbol *>> SymbolTable(20);
/*
 * 0号ir表为全局变量declare,后续为每一个函数的
 * 其中[1][0]为第一个函数形参declare主内容
 * [1][1]为返回值以及后续根据符号表扫描插入的declare(局部和临时)
 * [2]为正文（不含declare）
 * [1][3]即为第三方.L3
 * 20个函数，每个函数有200个([3]开始为第三方)label（这么大是为了79测试样例的正常生成，但很明显跑不起来…），每个有max(自己push_back)语句。
 */
vector<vector<vector<string>>> irs(20);
//每个函数的最终的.L1 return
vector<string> irreturn(20);
//因为有全局变量V_D_ASSIGN存在，所以全局变量的赋值要插在main头部
vector<string> main_first_ir;
/*
 * 为while循环中continue、break压label的stack
 * 其实是可以
 */
stack<WhileInfo> whileInfoStack;
void yyerror(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "Grammar Error at Line %d Column %d: ", yylloc.first_line, yylloc.first_column);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, ".\n");
}
//============
vector<int> getSymbolArrayWei(string name, int tableId, int level);
string exp_handler(Node *cur, int cur_table, int cur_level, int cur_label, int true_label = -1, int false_label = -1);
//=============
string int2str(int num)
{
    stringstream ss;
    ss << num;
    return ss.str();
}
//用作 t临时变量数字的生成
string getUniqueIntTxt()
{
    return int2str(_unique++);
}
//用作label数字生成
int getNewLabel()
{
    return ++_used_max_label;
}

//从ir语句中读取存在的Label数字
vector<int> getLabelsInIr(string ir)
{
    vector<int> rt;
    int _start = 0;
    int t = 0;
    while (ir.find(".L", _start) < ir.length())
    {
        //找到了
        //下面的返回值为 . 所在的下标值
        _start = ir.find(".L", _start);
        t = 0;

        for (int i = _start + 2; i < ir.length() && ir[i] <= '9' && ir[i] >= '0'; i++)
        {
            t = t * 10 + ir[i] - '0';
        }
        rt.push_back(t);
        //防止while死循环
        _start++;
    }
    return rt;
}
//插入前查重，若存在则不插
void insertToLabels(Block *it, int label)
{
    if (it->to_labels.size() == 0)
        it->to_labels.push_back(label);
    else
    {
        bool finded = false;
        for (auto la : it->to_labels)
        {
            if (la == label)
            {
                finded = true;
                break;
            }
        }
        if (!finded)
            it->to_labels.push_back(label);
    }
}

//打出ir，但是不是单纯的读取irs vector
//由于return,会加临时变量符号表
string printIrs()
{
    string rt = "";
    //当前的函数是不是要分析的
    bool bool_func_block = false;
    // cout << "=====IR行[0][0](全局变量declare)====" << endl;
    for (auto it : irs[0][0])
    {
        rt = rt + it + "\n";
    }
    //下面这个是遍历存在代码的label
    for (int _tableid = 1; irs[_tableid][2].size() > 0; _tableid++)
    {
        string _func_return_type;
        //下面这个指对应的函数symbol
        Symbol *_it;
        for (int i = 0; i < SymbolTable[0].size(); i++)
        {
            if (SymbolTable[0][i]->kind == "函数" && SymbolTable[0][i]->ref_tableid == _tableid)
            {
                _it = SymbolTable[0][i];
            }
        }
        //如下是函数的define和可能的参数相关语句的生成
        rt = rt + "define " + _it->type + " " + _it->inname + "(";

        //判断要不要打出块
        if (_it->inname == "@" + G_FUNC_BLOCK)
            bool_func_block = true;
        else
            bool_func_block = false;

        //下面为形参相关
        //形参T指在函数声明里面的那个参数
        //后面变成形参L来作为栈内变量
        bool first = true;
        for (int i = 0; i < SymbolTable[_it->ref_tableid].size(); i++)
        {
            if (SymbolTable[_it->ref_tableid][i]->kind == "形参T")
            {
                if (first)
                {
                    rt = rt + SymbolTable[_it->ref_tableid][i]->type + " " + SymbolTable[_it->ref_tableid][i]->inname;
                    first = false;
                }
                else
                {
                    rt = rt + "," + SymbolTable[_it->ref_tableid][i]->type + " " + SymbolTable[_it->ref_tableid][i]->inname;
                }
            }
        }
        rt = rt + ") {\n";

        // cout << "=====IR行[" << _tableid << "][0](相关函数形参declare)====" << endl;
        if (_it->inname == "@main")
        {
            for (auto it : main_first_ir)
            {
                // cout << it << endl;
                rt = rt + it + "\n";
            }
        }

        for (auto it : irs[_tableid][0])
        {
            // cout << it << endl;
            rt = rt + it + "\n";
        }
        //=====分析返回值，的时候要插符号表
        string rt_inname;
        if (SymbolTable[_tableid].size() > 0 && SymbolTable[_tableid][0]->name == "返回值")
        {
            //有返回值return,把exit打出来
            //这里就把返回值的%l取tmp然后输出
            Symbol *tmp_rt = new Symbol();
            rt_inname = "%t" + int2str(SymbolTable[_tableid].size()); // getUniqueIntTxt();
            tmp_rt->name = "temp_rt_" + rt_inname;
            tmp_rt->inname = rt_inname;
            tmp_rt->type = SymbolTable[_tableid][0]->type;
            tmp_rt->kind = "临时";
            tmp_rt->level = 1;

            SymbolTable[_tableid].push_back(tmp_rt);
            // TODO 会不会返回一个数组？？？（=>好像不会）先不管。假如会的话参照代码内别的地方补上 []
            irs[_tableid][1].push_back("declare " + tmp_rt->type + " " + rt_inname);
        }
        //=====
        // cout << "=====IR行[" << _tableid << "][1](相关函数返回值、局部临时)====" << endl;
        for (auto it : irs[_tableid][1])
        {
            // cout << it << endl;
            rt = rt + it + "\n";
        }
        // cout << "=====IR行[" << _tableid << "][2](正文)====" << endl;
        Block *b = new Block();
        if (bool_func_block)
        {
            b->irs.push_back(".L2:");
            b->irs.push_back("entry");
            b->label = 2;
        }
        rt = rt + "entry\n";
        for (auto it : irs[_tableid][2])
        {
            // cout << it << endl;
            rt = rt + it + "\n";
            if (bool_func_block)
            {
                b->irs.push_back(it);
                vector<int> labels = getLabelsInIr(it);
                for (auto label : labels)
                    insertToLabels(b, label);
            }
        }
        if (bool_func_block)
            BlockVec.push_back(b);
        //其他LABEL的
        for (int i = 3; i < 40; i++)
        {
            if (irs[_tableid][i].size() > 0)
            {
                // cout << "=====IR行[" << _tableid << "][" << i << "](.L" << i << ")====" << endl;
                rt = rt + ".L" + int2str(i) + ":\n";
                b = new Block();
                if (bool_func_block)
                {
                    b->label = i;
                    b->irs.push_back(".L" + int2str(b->label) + ":");
                }
                for (int j = 0; j < irs[_tableid][i].size(); j++)
                {
                    // cout << irs[_tableid][i][j] << endl;
                    rt = rt + irs[_tableid][i][j] + "\n";
                    if (bool_func_block)
                    {
                        b->irs.push_back(irs[_tableid][i][j]);
                        vector<int> labels = getLabelsInIr(irs[_tableid][i][j]);
                        for (auto label : labels)
                            insertToLabels(b, label);
                    }
                }
                if (bool_func_block)
                    BlockVec.push_back(b);
            }
        }

        b = new Block();
        b->label = 1;
        b->irs.push_back(".L1:");
        if (SymbolTable[_tableid].size() > 0 && SymbolTable[_tableid][0]->name == "返回值")
        {
            // cout << "=====IR行[" << _tableid << "][_](L1)====" << endl;
            // cout << ".L1:" << endl;
            rt = rt + ".L1:\n";
            //有返回值return,把exit打出来
            // TODO多label的时候也要改
            //这里就把返回值的%l取tmp然后输出

            // cout << rt_inname << " = " << SymbolTable[_tableid][0]->inname << endl;
            rt = rt + rt_inname + " = " + SymbolTable[_tableid][0]->inname + "\n";
            // cout << "exit " << rt_inname << endl;
            rt = rt + "exit " + rt_inname + "\n}\n";
            if (bool_func_block)
            {
                b->irs.push_back(rt_inname + " = " + SymbolTable[_tableid][0]->inname);
                b->irs.push_back("exit " + rt_inname);
            }
        }
        else
        {
            //因为在 STM_LIST 末尾时，都会强制跳转到.L1
            // cout << ".L1:\nexit" << endl;
            rt = rt + ".L1:\nexit" + "\n}\n";
            if (bool_func_block)
            {
                b->irs.push_back("exit");
            }
        }
        if (bool_func_block)
            BlockVec.push_back(b);
    }

    return rt;
}

//生成graphviz示例的block基本块划分图
//其中的blockvt
//是在 printirs 函数里面整好的
void genBlock(Agraph_t *g)
{
    vector<Agnode_t *> aNode;
    aNode.resize(41);
    int node_num = 0;
    for (int i = 0; i < BlockVec.size(); i++)
    {
        if (BlockVec[i] == NULL || BlockVec[i]->irs.size() == 0)
            continue;
        Block *block = BlockVec[i];
        string code = "";
        int line_i = 0;
        //非转义\l表示graphviz换行左对齐
        for (auto line : block->irs)
            code = code + "\\l" + int2str((line_i++)) + ". " + line;
        code = code + "\\l";
        Agnode_t *gv_node = agnode(g, (char *)int2str(node_num++).c_str(), 1);
        agsafeset(gv_node, "shape", "record", "ellipse");
        agsafeset(gv_node, (char *)"label", (char *)(code).c_str(), "");
        aNode[block->label] = gv_node;
    }
    //后面再循环一次来补连线
    for (int i = 0; i < BlockVec.size(); i++)
    {
        if (BlockVec[i] == NULL || BlockVec[i]->irs.size() == 0)
            continue;

        Block *block = BlockVec[i];
        for (auto child : block->to_labels)
            agedge(g, aNode[block->label], aNode[child], NULL, 1);
    }
}

//写符号表，kind 自定义，会写ir里面的declare
void appendSymbol(string name, string type, string kind, string inname, int level, int tableId, int reftableid = -1)
{
    Symbol *rt = new Symbol();
    rt->name = name;
    //如下支持传入 minic 类型，和ir的 i32 i8 等类型
    //并最终转为 ir 类型
    if (type == "int")
        rt->type = "i32";
    else if (type == "char")
        rt->type = "i8";
    else if (type == "void")
        rt->type = "void";
    else if (type[0] == 'i')
        rt->type = type;

    rt->kind = kind;
    rt->inname = inname;
    rt->level = level;
    rt->ref_tableid = reftableid;
    SymbolTable[tableId].push_back(rt);
    string inname_fix_for_array = rt->inname;
    //下面如果是数组，要带 [1][2] 这些的
    if (rt->name.find('[') < rt->name.length())
    {
        //定义的时候补上[1][2]一类的
        vector<int> wei = getSymbolArrayWei(rt->name.substr(0, rt->name.find('[')), tableId, level);
        for (auto it : wei)
            inname_fix_for_array = inname_fix_for_array + "[" + int2str(it) + "]";
    }

    if (kind == "全局变量")
    {
        // tableId 应该横为0
        irs[tableId][0].push_back("declare " + rt->type + " " + inname_fix_for_array);
    }
    else if (kind == "局部变量")
    {
        irs[tableId][1].push_back("declare " + rt->type + " " + inname_fix_for_array);
    }
    else if (kind == "形参L")
    {
        // tableId 应该>0
        irs[tableId][0].push_back("declare " + rt->type + " " + inname_fix_for_array);
    }
    else if (kind == "返回值")
    {
        irs[tableId][1].push_back("declare " + rt->type + " " + rt->inname);
    }
}

//为临时变量写符号表和对应IR声明
void appendSymbolTemp(string type, string inname, int level, int tableId)
{
    Symbol *rt = new Symbol();
    rt->name = "temp_" + inname;
    if (type == "int")
        rt->type = "i32";
    else if (type == "char")
        rt->type = "i8";
    else if (type == "void")
        rt->type = "void";
    else if (type[0] == 'i')
        rt->type = type;
    rt->kind = "临时";
    rt->inname = inname;
    rt->level = level;
    rt->ref_tableid = 0;
    SymbolTable[tableId].push_back(rt);
    irs[tableId][1].push_back("declare " + rt->type + " " + rt->inname);
}

//从a[22][44]转为a
string convertArrayToIdent(string input)
{
    string rt = "";
    for (int i = 0; i < input.length(); i++)
    {
        if (input[i] != '[')
        {
            string s;
            s = input[i];
            rt = rt + s;
        }
        else
            break;
    }
    return rt;
}

//数组ident查询,,仅传入数组，如a[3][4]传a,返回{3,4}
vector<int> getSymbolArrayWei(string name, int tableId, int level)
{
    string _name = "";
    vector<int> rt;
    //先找局部同level
    auto table = SymbolTable[tableId];
    for (auto it : table)
    {
        if (convertArrayToIdent(it->name) == name && it->level == level)
        {
            _name = it->name;
        }
    }
    if (_name == "")
    {
        //局部没有就降level往下面找
        for (int _level = level - 1; _level >= 0; _level--)
        {
            auto table = SymbolTable[tableId];
            for (auto it : table)
            {
                if (convertArrayToIdent(it->name) == name && it->level == _level)
                {
                    _name = it->name;
                }
            }
        }
    }
    if (_name == "")
    {
        //上面都没有就查全局
        auto table = SymbolTable[0];
        for (auto it : table)
        {
            if (convertArrayToIdent(it->name) == name)
            {
                _name = it->name;
            }
        }
    }
    if (_name == "")
    {
        cout << "error @getSymbolArrayWei" << endl;
    }
    int tmp_sum = 0;
    bool working = false;
    for (int i = 0; i < _name.length(); i++)
    {
        if (_name[i] == '[')
        {
            tmp_sum = 0;
            working = true;
        }
        else if (working && _name[i] >= '0' && _name[i] <= '9')
        {
            tmp_sum = tmp_sum * 10 + (_name[i] - '0');
        }
        else if (_name[i] == ']')
        {
            working = false;
            rt.push_back(tmp_sum);
        }
    }
    return rt;
}

//传入符号，支持依据数组ident查询,返回inname,测试用例中不存在同函数同level相同名
string getSymbolInname(string name, int level, int tableId)
{
    //先找局部同level
    auto table = SymbolTable[tableId];
    for (auto it : table)
    {
        if (convertArrayToIdent(it->name) == name && it->level == level)
        {
            return it->inname;
        }
    }
    //局部没有就降level往下面找
    for (int _level = level - 1; _level >= 0; _level--)
    {
        auto table = SymbolTable[tableId];
        for (auto it : table)
        {
            if (convertArrayToIdent(it->name) == name && it->level == _level)
            {
                return it->inname;
            }
        }
    }
    //上面都没有就查全局
    table = SymbolTable[0];
    for (auto it : table)
    {
        if (convertArrayToIdent(it->name) == name)
        {
            return it->inname;
        }
    }

    cout << "error @getSymbolInname" << endl;
    return "";
}

//支持数组指针 *开头
Symbol *getSymbol(string inname, int level, int tableId)
{
    auto table = SymbolTable[tableId];
    if (inname[0] == '*')
    {
        inname = inname.substr(1);
    }

    for (auto it : table)
    {
        if (it->inname == inname && it->level == level)
        {
            return it;
        }
    }
    //局部没有就降level往下面找
    for (int _level = level - 1; _level >= 0; _level--)
    {
        for (auto it : table)
        {
            if (it->inname == inname && it->level == _level)
            {
                return it;
            }
        }
    }
    //上面都没有就查全局
    table = SymbolTable[0];
    for (auto it : table)
    {
        if (it->inname == inname)
        {
            return it;
        }
    }
    cout << "error @getSymbol" << endl;
    return NULL;
}

string printSymbol()
{
    string rt = "";
    for (int i = 0; i < SymbolTable.size(); i++)
    {
        if (SymbolTable[i].size() > 0)
        {
            rt = rt + "========符号表[" + int2str(i) + "]==========\n";
            rt = rt + "[name] [type] [kind] [inname] [level] [ref_tableid]?\n";
            for (int j = 0; j < SymbolTable[i].size(); j++)
            {
                rt = rt + SymbolTable[i][j]->name + " " + SymbolTable[i][j]->type + " " + SymbolTable[i][j]->kind + " " + SymbolTable[i][j]->inname + " " + int2str(SymbolTable[i][j]->level) + " (" + int2str(SymbolTable[i][j]->ref_tableid) + ")\n";
            }
        }
    }
    return rt;
}

void genNodes(Agraph_t *g, Agnode_t *parent_gv, Node *next_node)
{
    char ch[next_node->type.length() + 1];
    strcpy(ch, (next_node->type).c_str());
    Agnode_t *gv_node = agnode(g, (char *)int2str(node_num++).c_str(), 1);
    agsafeset(gv_node, (char *)"label", (char *)(next_node->type + "\n(" + next_node->data_str + "," + to_string(next_node->data_int) + "," + to_string(next_node->data_char) + ")" + "@" + int2str(next_node->id)).c_str(), "");
    if (parent_gv != NULL)
    {
        agedge(g, parent_gv, gv_node, NULL, 1);
    }
    for (int i = 0; i < next_node->children.size(); i++)
    {
        genNodes(g, gv_node, next_node->children[i]);
    }
}
//尽可能把全局或局部变量转换为临时，仅用于取值。支持数组指针(*开头)
string convertToTemp(string inname, int cur_table, int cur_level, int cur_label)
{
    string rt = inname;
    //*%t 数组指针
    if (inname[0] == '*' && inname[1] == '%' && inname[2] == 't')
    {
        string t = "%t" + getUniqueIntTxt();
        //这里的_type为类似i32型
        string _type = getSymbol(inname, cur_level, cur_table)->type;
        _type = _type.substr(0, _type.find('*'));
        appendSymbolTemp(_type, t, cur_level, cur_table);
        //写ir 临时=全局/局部
        irs[cur_table][cur_label].push_back(t + " = " + inname);
        rt = t;
    }
    else if ((inname[0] == '%' && inname[1] == 'l') || inname[0] == '@')
    {
        //右值为全局或局部，先取到临时
        string t = "%t" + getUniqueIntTxt();
        //读这个变量类型
        string _type = getSymbol(inname, cur_level, cur_table)->type;
        appendSymbolTemp(_type, t, cur_level, cur_table);
        //写ir 临时=全局/局部
        irs[cur_table][cur_label].push_back(t + " = " + inname);
        rt = t;
    }
    return rt;
}
/*
 * 处理and or 相关
 * and or 主要在 if while condition 里面
 * 这里处理 and or 是通过br bc 跳转来实现的
 * 通过传入的true/false label,让函数内部根据and or 以及对应前后的正确性来br
 * 本来是想用一个变量来保存最终的结果的
 * 但是这样子就要用l变量，这是不允许的
 * 所以用了比较绕的br bc来做
 */
int and_or_single(Node *and_or, int true_label, int false_label, int cur_table, int cur_level, int cur_label)
{
    //返回自己左侧的label值（最深
    // or
    //短路求值
    //两个都假才假
    //短路求值
    //不返回值，直接根据正错来写br
    int rt_label = cur_label;
    int single_next_label = getNewLabel(); //当下的and_or 短路失败，判断第二个

    string first, second, finalrt, tt;
    // TODO 要不要套temp呢？先不套吧
    if (and_or->children[0]->type == "AND" || and_or->children[0]->type == "OR")
    {
        /*
         * 如果左侧还有 and or 的话
         * 就深入进去
         * 这样子就会进到最底下的and or 节点
         * 然后来进行判断
         */
        if (and_or->type == "AND")
        {
            //多个and、or
            int left_child_label = getNewLabel();
            rt_label = and_or_single(and_or->children[0], single_next_label, false_label, cur_table, cur_level, left_child_label);
            /*
            自己and true送自己右边，false送上层主false）
            */
        }
        else if (and_or->type == "OR")
        {
            //多个and、or
            int left_child_label = getNewLabel();
            rt_label = and_or_single(and_or->children[0], true_label, single_next_label, cur_table, cur_level, left_child_label);
            /*
            自己or true送上层true,false送自己右边
            */
        }
    }
    else
    {
        /*
         * 这里说明左侧没有 and or 了
         * 而是一个具体的表达式
         */
        first = exp_handler(and_or->children[0], cur_table, cur_level, cur_label, true_label, false_label);
        string first_type = getSymbol(first, cur_level, cur_table)->type;
        string first_cmp_result = first;
        if (first_cmp_result[0] == '*')
        {
            string t = "%t" + getUniqueIntTxt();
            appendSymbolTemp("i32", t, cur_level, cur_table);
            irs[cur_table][cur_label].push_back(t + " = " + first_cmp_result);
            first_cmp_result = t;
        }
        /*
         * 如下根据 and or的特征来进行短路求值
         */
        if (and_or->type == "AND")
        {
            irs[cur_table][cur_label].push_back("bc " + first_cmp_result + ", label .L" + int2str(single_next_label) + ", label .L" + int2str(false_label));
        }
        else if (and_or->type == "OR")
        {
            irs[cur_table][cur_label].push_back("bc " + first_cmp_result + ", label .L" + int2str(true_label) + ", label .L" + int2str(single_next_label));
        }
    }
    //为059修复，防止 and_or 里面调用函数，把label给空了
    // 所以这里遇到 label 不同的情况，做一个跳转
    if (rt_label != cur_label)
    {
        cout << "cur_label:" << cur_label << " rt_label:" << rt_label << endl;
        if (irs[cur_table][cur_label].size() > 0 && irs[cur_table][cur_label].back().find("br") == 0)
        {
            /* code */
        }
        else
        {
            irs[cur_table][cur_label].push_back("br label .L" + int2str(rt_label));
        }
    }
    //左侧判断完毕，现在开始判断右侧的
    //=====
    if (and_or->children[1]->type != "AND" && and_or->children[1]->type != "OR")
    {
        // 这边右侧就是表达式了，和上面逻辑相同
        second = exp_handler(and_or->children[1], cur_table, cur_level, single_next_label);
        string second_cmp_result = second;
        if (second_cmp_result[0] == '*')
        {
            string t = "%t" + getUniqueIntTxt();
            appendSymbolTemp("i32", t, cur_level, cur_table);
            irs[cur_table][cur_label].push_back(t + " = " + second_cmp_result);
            second_cmp_result = t;
        }
        if (and_or->type == "AND")
        {
            irs[cur_table][single_next_label].push_back("bc " + second_cmp_result + ", label .L" + int2str(true_label) + ", label .L" + int2str(false_label));
        }
        else if (and_or->type == "OR")
        {
            irs[cur_table][single_next_label].push_back("bc " + second_cmp_result + ", label .L" + int2str(true_label) + ", label .L" + int2str(false_label));
        }
    }
    else
    {
        and_or_single(and_or->children[1], true_label, false_label, cur_table, cur_level, single_next_label);
    }

    return rt_label;
}

// 返回局部变量。cur_level用来读符号表，label用来直接写irs.由于and、or只和while if 连接，要求while if分析的时候要传 真、假要跳转的位置
string exp_handler(Node *cur, int cur_table, int cur_level, int cur_label, int true_label, int false_label)
{
    //关于二元运算相关
    if (cur->type == "ASSIGN")
    {
        string le, ri;
        //左值肯定为全局或者局部
        le = exp_handler(cur->children[0], cur_table, cur_level, cur_label);
        ri = exp_handler(cur->children[1], cur_table, cur_level, cur_label);
        string finalri = convertToTemp(ri, cur_table, cur_level, cur_label);
        irs[cur_table][cur_label].push_back(le + " = " + finalri);
        return "";
    }
    else if (cur->type == "PLUS" || cur->type == "MINUS" || cur->type == "MUL" || cur->type == "DIV" || cur->type == "MOD")
    {
        string le, ri;
        le = exp_handler(cur->children[0], cur_table, cur_level, cur_label);
        ri = exp_handler(cur->children[1], cur_table, cur_level, cur_label);
        string finalle = convertToTemp(le, cur_table, cur_level, cur_label);
        string finalri = convertToTemp(ri, cur_table, cur_level, cur_label);
        //为此加法式子的返回存储
        string t = "%t" + getUniqueIntTxt();
        appendSymbolTemp("i32", t, cur_level, cur_table);

        if (cur->type == "PLUS")
        {
            irs[cur_table][cur_label].push_back(t + "=add " + finalle + ", " + finalri);
        }
        else if (cur->type == "MINUS")
        {
            irs[cur_table][cur_label].push_back(t + "=sub " + finalle + ", " + finalri);
        }
        else if (cur->type == "MUL")
        {
            irs[cur_table][cur_label].push_back(t + "=mul " + finalle + ", " + finalri);
        }
        else if (cur->type == "DIV")
        {
            irs[cur_table][cur_label].push_back(t + "=div " + finalle + ", " + finalri);
        }
        else if (cur->type == "MOD")
        {
            irs[cur_table][cur_label].push_back(t + "=mod " + finalle + ", " + finalri);
        }
        return t;
    }
    else if (cur->type == "RELOP")
    {

        string le, ri;
        //左值肯定为全局或者局部，但还是套了那个是为了处理数组（*开头）
        le = exp_handler(cur->children[0], cur_table, cur_level, cur_label);
        ri = exp_handler(cur->children[1], cur_table, cur_level, cur_label);
        string finalle = convertToTemp(le, cur_table, cur_level, cur_label);
        // 070 fix
        if (finalle[0] == '*')
        {
            string t = "%t" + getUniqueIntTxt();
            appendSymbolTemp("i32", t, cur_level, cur_table);
            irs[cur_table][cur_label].push_back(t + " = " + finalle);
            finalle = t;
        }
        string finalri = convertToTemp(ri, cur_table, cur_level, cur_label);
        string t = "%t" + getUniqueIntTxt();
        appendSymbolTemp("i1", t, cur_level, cur_table);
        string op = cur->data_str;
        if (op == ">")
        {
            irs[cur_table][cur_label].push_back(t + "=cmp gt " + finalle + ", " + finalri);
        }
        else if (op == "<")
        {
            irs[cur_table][cur_label].push_back(t + "=cmp lt " + finalle + ", " + finalri);
        }
        else if (op == ">=")
        {
            irs[cur_table][cur_label].push_back(t + "=cmp ge " + finalle + ", " + finalri);
        }
        else if (op == "<=")
        {
            irs[cur_table][cur_label].push_back(t + "=cmp le " + finalle + ", " + finalri);
        }
        else if (op == "==")
        {
            irs[cur_table][cur_label].push_back(t + "=cmp eq " + finalle + ", " + finalri);
        }
        else if (op == "!=")
        {
            irs[cur_table][cur_label].push_back(t + "=cmp ne " + finalle + ", " + finalri);
        }
        return t;
    }
    else if (cur->type == "AND" || cur->type == "OR")
    {
        // TODO 双层不用手动插ir,多的话要
        int and_or_in = and_or_single(cur, true_label, false_label, cur_table, cur_level, cur_label);
    }
    else if (cur->type == "PLUSASS")
    {
        /* code */
        /* 测试样例没有，就不实现捏
         */
    }
    else if (cur->type == "USELFPLUS")
    {
        /* code */
    }
    else if (cur->type == "MSELFPLUS")
    {
        /* code */
    }
    else if (cur->type == "MINUSASS")
    {
        /* code */
    }
    else if (cur->type == "USLEFMINUS")
    {
        /* code */
    }
    else if (cur->type == "MSLEFMINUS")
    {
        /* code */
    }
    //一元运算
    else if (cur->type == "UMINUS")
    {
        string child;
        child = exp_handler(cur->children[0], cur_table, cur_level, cur_label);
        string t = "%t" + getUniqueIntTxt();
        appendSymbolTemp("i32", t, cur_level, cur_table);
        irs[cur_table][cur_label].push_back(t + "=neg " + child);
        return t;
    }
    else if (cur->type == "NOT")
    {
        int uminus_count = 1;
        Node *child = cur->children[0];
        while (child->type == "NOT")
        {
            uminus_count++;
            child = child->children[0];
        }

        string deep_child;
        deep_child = exp_handler(child, cur_table, cur_level, cur_label);
        // 070 fix
        if (deep_child[0] == '*')
        {
            string t = "%t" + getUniqueIntTxt();
            appendSymbolTemp("i32", t, cur_level, cur_table);
            irs[cur_table][cur_label].push_back(t + " = " + deep_child);
            deep_child = t;
        }
        string t = "%t" + getUniqueIntTxt();
        appendSymbolTemp("i1", t, cur_level, cur_table);
        if (uminus_count % 2 == 0)
        {
            //偶数个!!，相当于没有取反.但是还是要变成i1变量
            irs[cur_table][cur_label].push_back(t + "=cmp ne " + deep_child + ", 0");
        }
        else
        {
            //奇数个!,取反
            irs[cur_table][cur_label].push_back(t + "=cmp eq " + deep_child + ", 0");
        }
        return t;
    }
    //函数调用
    else if (cur->type == "FUNC_CALL")
    {
        //函数调用要先用一个 %t 临时变量接住
        string _inname = "@" + cur->data_str; //; = getSymbolInname(cur->data_str,cur_level,cur_table);
        string _type;
        if (cur->data_str == "getint" || cur->data_str == "getch" || cur->data_str == "getarray")
        {
            _type = "i32";
        }
        else if (cur->data_str == "putint" || cur->data_str == "putch" || cur->data_str == "putarray")
        {
            _type = "void";
        }
        else
        {
            _type = getSymbol(_inname, cur_level, cur_table)->type;
        }

        //调用后的值
        string t;
        if (cur->children.size() == 0)
        {
            //无参数
            if (_type == "void")
            {
                //无返回值
                irs[cur_table][cur_label].push_back("call " + _type + " " + _inname + "()");
                return "";
            }
            else
            {
                //有返回值
                t = "%t" + getUniqueIntTxt();
                appendSymbolTemp(_type, t, cur_level, cur_table);
                irs[cur_table][cur_label].push_back(t + "=call " + _type + " " + _inname + "()");
                return t;
            }
        }
        else
        {
            //有参数
            Node *arg_list = cur->children[0];
            string rt = "call " + _type + " " + _inname + "(";
            for (int i = 0; i < arg_list->children.size(); i++)
            {
                Node *arg = arg_list->children[i];
                string arg_txt = exp_handler(arg, cur_table, cur_level, cur_label);
                if (arg_txt[0] <= '9' && arg_txt[0] >= '0')
                {
                    //为INT
                    // TODO 支持chari8 booli1
                    rt += "i32 " + arg_txt;
                }
                else
                {
                    Symbol *arg_sym = getSymbol(arg_txt, cur_level, cur_table);
                    //为067特别优化。讲真我觉得是老师编译器的bug,不过还是自己“优化”吧
                    if (cur->data_str == "getarray" || (cur->data_str == "putarray" && i == 1))
                    {
                        t = "%t" + getUniqueIntTxt();
                        appendSymbolTemp("i32", t + "[100]", cur_level, cur_table);
                        irs[cur_table][cur_label].push_back(t + " = " + arg_txt.substr(1));
                        rt += "i32 " + t + "[100]";
                    }
                    else
                    {
                        //参考068 72 ，要转成i32 的t
                        if (_type == "i32" && arg_txt[0] == '*' || cur->data_str == "putint")
                        {
                            t = "%t" + getUniqueIntTxt();
                            appendSymbolTemp("i32", t, cur_level, cur_table);
                            irs[cur_table][cur_label].push_back(t + " = " + arg_txt);
                            rt += "i32 " + t;
                        }
                        else if (arg_sym->name.find('[') < arg_sym->name.length())
                        {
                            //如果arg_sym name有[，则这是一个数组传参的，参考 061
                            t = "%t" + getUniqueIntTxt();
                            appendSymbolTemp("i32*", t, cur_level, cur_table);
                            irs[cur_table][cur_label].push_back(t + " = " + arg_txt);
                            rt += "i32* " + t;
                        }
                        else
                        {
                            rt += arg_sym->type + " " + arg_txt;
                        }
                    }
                }

                if (i < arg_list->children.size() - 1)
                {
                    rt += ",";
                }
            }
            rt += ")";
            if (_type == "void")
            {
                //无返回值
                irs[cur_table][cur_label].push_back(rt);
                return "";
            }
            else
            {
                //有返回值
                t = "%t" + getUniqueIntTxt();
                appendSymbolTemp(_type, t, cur_level, cur_table);
                irs[cur_table][cur_label].push_back(t + " = " + rt);
                return t;
            }
        }
    }
    else if (cur->type == "IDENT")
    {
        return getSymbolInname(cur->data_str, cur_level, cur_table); //返回inname
    }
    else if (cur->type == "INT")
    {
        return int2str(cur->data_int);
    }
    else if (cur->type == "ARRAY")
    {
        Node *ident_parent = cur;
        string name = "";
        int wei = 1;
        //深入到最深的ARRAY节点
        while (ident_parent->children.size() > 0 && ident_parent->children[0]->type == "ARRAY")
        {
            ident_parent = ident_parent->children[0];
            wei++;
        }
        name = ident_parent->children[0]->data_str;
        string kuohaonei = "";
        //一维
        if (wei == 1)
        {
            //指比如 a[5] 里面5的符号
            string pianyi_symbol = exp_handler(ident_parent->children[1], cur_table, cur_level, cur_label);
            string final_pianyi_symbol = convertToTemp(pianyi_symbol, cur_table, cur_level, cur_label);
            string t = "%t" + getUniqueIntTxt();
            appendSymbolTemp("i32", t, cur_level, cur_table);
            irs[cur_table][cur_label].push_back(t + "=mul " + final_pianyi_symbol + ", 4");
            string t2 = "%t" + getUniqueIntTxt();
            appendSymbolTemp("i32*", t2, cur_level, cur_table);
            irs[cur_table][cur_label].push_back(t2 + "=add " + getSymbolInname(name, cur_level, cur_table) + ", " + t);
            return "*" + t2;
        }
        else
        {
            //下面这个比较玄学，写不动注释
            /*
            int a[5][6][7][8];
                a[1][2][3][4]可以表示为
            起始+4宽度*下式
            1*6+2)*7+3)*8+4
            */
            vector<int> weiDetail = getSymbolArrayWei(name, cur_table, cur_level);
            string finalrt = "";
            for (int i = 0; i < wei - 1; i++)
            {
                string pianyi_symbol;
                if (finalrt == "")
                {
                    pianyi_symbol = exp_handler(ident_parent->children[1], cur_table, cur_level, cur_label);
                }
                else
                {
                    pianyi_symbol = finalrt;
                }
                /*
                 * 再套一个tmp
                 * 因为有可能出现带*的，但是老师编译器很明显也不认得……
                 */
                string t = "%t" + getUniqueIntTxt();
                appendSymbolTemp("i32", t, cur_level, cur_table);
                string final_pianyi_symbol = convertToTemp(pianyi_symbol, cur_table, cur_level, cur_label);
                irs[cur_table][cur_label].push_back(t + "=mul " + final_pianyi_symbol + ", " + int2str(weiDetail[i + 1]));

                string t2 = "%t" + getUniqueIntTxt();
                appendSymbolTemp("i32", t2, cur_level, cur_table);
                irs[cur_table][cur_label].push_back(t2 + "=add " + t + ", " + exp_handler(ident_parent->parent->children[1], cur_table, cur_level, cur_label));

                finalrt = t2;
                ident_parent = ident_parent->parent;
            }
            string t = "%t" + getUniqueIntTxt();
            appendSymbolTemp("i32", t, cur_level, cur_table);
            irs[cur_table][cur_label].push_back(t + "=mul " + finalrt + ", 4");
            string t2 = "%t" + getUniqueIntTxt();
            appendSymbolTemp("i32*", t2, cur_level, cur_table);
            irs[cur_table][cur_label].push_back(t2 + "=add " + getSymbolInname(name, cur_level, cur_table) + ", " + t);
            return "*" + t2;
        }
    }
    else if (cur->type == "BREAK")
    {
        /*
         * 这个都是为了防止末尾多个 br
         * 后面就不再说了
         */
        if (irs[cur_table][cur_label].size() > 0 && irs[cur_table][cur_label].back().find("br") == 0)
        {
            /* code */
        }
        else
        {
            irs[cur_table][cur_label].push_back("br label .L" + int2str(whileInfoStack.top().break_label));
        }
    }

    return "";
}

/*
 * 下面这个就是大的handle器
 * 其中exp 相关挪到了 exp_handler 里面
 * 这个主要是要对着ast图来看
 * 毕竟写的比较奇怪，不是严格的按照递归的来写的
 */
void ana(Node *cur, int cur_table, int cur_level, int optional_finished_br_label)
{
    if (cur != NULL)
    {
        //如文本，就是一些DEF
        if (cur->type == "EXT_VAR_DEF" || cur->type == "VAR_DEF" || cur->type == "PARAM_DEF")
        {
            //全局变量，应该是PROGRAM->SEGMENT_LIST->EXT_VAR_DEF
            string _type = cur->children[0]->data_str; // children[0]为type
            Node *secondChild = cur->children[1];
            if (secondChild->type == "IDENT")
            {
                if (cur->type == "EXT_VAR_DEF")
                {
                    appendSymbol(secondChild->data_str, _type, "全局变量", "@" + secondChild->data_str, 0, 0);
                }
                else if (cur->type == "VAR_DEF")
                {
                    appendSymbol(secondChild->data_str, _type, "局部变量", "%l" + getUniqueIntTxt(), cur_level, cur_table);
                }
                else if (cur->type == "PARAM_DEF")
                {
                    //形参
                    string t, l;
                    l = "%l" + getUniqueIntTxt();
                    t = "%t" + getUniqueIntTxt();
                    appendSymbol(secondChild->data_str, _type, "形参L", l, cur_level, cur_table);
                    appendSymbol(secondChild->data_str, _type, "形参T", t, cur_level, cur_table);
                    //读取形参并转换为栈内变量
                    irs[cur_table][2].push_back(l + " = " + t);
                }
            }
            else if (secondChild->type == "IDENT_P_ARRAY")
            {
                //仅用于形参 int a[]
                // TODO 形参
                // 如上，不知道是不是TODO,忘了有没有实现了

                if (cur->type == "PARAM_DEF")
                {
                    //按老师的ir编译器，这个一定要有容量且为0
                    string t, l;
                    l = "%l" + getUniqueIntTxt();
                    t = "%t" + getUniqueIntTxt();
                    appendSymbol(secondChild->data_str + "[0]", _type, "形参L", l, cur_level, cur_table);
                    //为了在printirs把参数打上[0]，所以这里inname写上了
                    appendSymbol(secondChild->data_str + "[0]", _type, "形参T", t + "[0]", cur_level, cur_table);
                    //读取形参并转换为栈内变量
                    irs[cur_table][2].push_back(l + " = " + t);
                }
                else
                {
                    //只是随便的一个error提示
                    //原则上是不会走到这里的
                    cout << "errorrrrrrrrrrrr @ random3224" << endl;
                }
            }
            else if (secondChild->type == "IDENT_P_ARRAY_INT")
            {
                // TODO
                //这个为形参 如 a[10]
                //但是好像目前测试样例还没有这种形式
                //所以就没有实现
                cout << "ERRRRRRRRRRRRRRROR IDENT_P_ARRAY_INT called。WAIT TO FUNC" << endl;
            }
            else if (secondChild->type == "V_D_ASSIGN")
            {
                //定义时同时a = 5，如 int a = 5
                //但好像老师把这个测试样例给改没了
                if (cur->type == "EXT_VAR_DEF")
                {
                    //目前测试样例（018）中，全局变量的 V_D_ASSIGN都是i32类型
                    appendSymbol(secondChild->data_str, _type, "全局变量", "@" + secondChild->data_str, 0, 0);
                    main_first_ir.push_back("@" + secondChild->data_str + " = " + int2str(secondChild->children[0]->data_int));
                }
                else if (cur->type == "VAR_DEF")
                {
                    string t = "%l" + getUniqueIntTxt();
                    appendSymbol(secondChild->data_str, _type, "局部变量", t, cur_level, cur_table);

                    //从 ASSIGN 里面复制并修改
                    string ri = exp_handler(secondChild->children[0], cur_table, cur_level, G_NOW_LABEL);
                    string finalri = convertToTemp(ri, cur_table, cur_level, G_NOW_LABEL);
                    irs[cur_table][G_NOW_LABEL].push_back(t + " = " + finalri);
                }
            }
            else
            {
                //这里应该是*_DEC_LIST
                if (secondChild->type != "DEC_LIST" && secondChild->type != "EXT_DEC_LIST" && secondChild->type != "ARRAY")
                {
                    //不应该走到这个步骤。假如发生的花要再检查一下YACC
                    cout << "ERRRRRRRRRRRRRRRRRRRRRRRR @DEC_LIST check" << endl;
                    cout << "Is: " << secondChild->type << " id:" << secondChild->id <<endl;
                }

                for (auto it : secondChild->children)
                {
                    //这边是定义相关
                    if (it->type == "IDENT")
                    {

                        string _name = it->data_str;
                        if (it->parent->type == "ARRAY")
                        {
                            //这是一维数组
                            _name = _name + "[" + int2str(it->parent->children[1]->data_int) + "]";
                        }
                        if (cur->type == "EXT_VAR_DEF")
                        {
                            //普通全局变量
                            appendSymbol(_name, _type, "全局变量", "@" + it->data_str, 0, 0);
                        }
                        else if (cur->type == "VAR_DEF")
                        {
                            //普通局部变量
                            appendSymbol(_name, _type, "局部变量", "%l" + getUniqueIntTxt(), cur_level, cur_table);
                        }
                    }
                    else if (it->type == "ARRAY")
                    {
                        // N维数组
                        Node *ident = it;
                        //下面这个是带 [1] 类型的
                        string name_with_wei = "";
                        //下面这个就是不带的
                        string name_no_wei = "";
                        while (ident->children.size() > 0 && ident->type == "ARRAY")
                        {
                            ident = ident->children[0];
                        }
                        name_with_wei = ident->data_str;
                        name_no_wei = ident->data_str;

                        ident = ident->parent;
                        while (ident->type == "ARRAY")
                        {
                            name_with_wei = name_with_wei + "[" + int2str(ident->children[1]->data_int) + "]";
                            ident = ident->parent;
                        }

                        if (cur->type == "EXT_VAR_DEF")
                        {
                            appendSymbol(name_with_wei, _type, "全局变量", "@" + name_no_wei, 0, 0);
                        }
                        else if (cur->type == "VAR_DEF")
                        {
                            appendSymbol(name_with_wei, _type, "局部变量", "%l" + getUniqueIntTxt(), cur_level, cur_table);
                        }
                    }
                    else if (it->type == "V_D_ASSIGN")
                    {
                        //从不是LIST的那个代码里面复制修改
                        if (cur->type == "EXT_VAR_DEF")
                        {
                            //目前测试样例（018）中，全局变量的 V_D_ASSIGN都是i32类型
                            appendSymbol(it->data_str, _type, "全局变量", "@" + it->data_str, 0, 0);
                            main_first_ir.push_back("@" + it->data_str + " = " + int2str(it->children[0]->data_int));
                        }
                        else if (cur->type == "VAR_DEF")
                        {
                            string t = "%l" + getUniqueIntTxt();
                            appendSymbol(it->data_str, _type, "局部变量", t, cur_level, cur_table);

                            //从 ASSIGN 里面复制并修改
                            string ri = exp_handler(it->children[0], cur_table, cur_level, G_NOW_LABEL);
                            string finalri = convertToTemp(ri, cur_table, cur_level, G_NOW_LABEL);
                            irs[cur_table][G_NOW_LABEL].push_back(t + " = " + finalri);
                        }
                    }
                }
            }
        }
        else if (cur->type == "PROGRAM")
        {
            //最顶上的头头
            ana(cur->children[0], 0, 0, -1);
        }
        else if (cur->type == "SEGMENT_LIST")
        {
            int _table = 0;
            for (auto it : cur->children)
            {
                //提升table.
                if (it->type == "FUNC_DEF")
                {
                    _table++;
                    // label清2
                    G_NOW_LABEL = 2;
                    _used_max_label = 2;
                    _unique = 0;
                }

                ana(it, _table, cur_table, -1);
            }
        }
        else if (cur->type == "FUNC_DEF_LITE") //函数声明
        {
            // TODO 目前好像测试用例里面没有函数声明
            //只存符号表就行
        }
        else if (cur->type == "FUNC_DEF") //函数调用
        {
            //此时的TABLEID已经增加了，

            string _type = cur->children[0]->data_str;
            //存符号表，ref_table_id指示当前的这个func的临时函数要在哪个table_id里
            // func类似全局变量存table[0]
            appendSymbol(cur->children[1]->data_str, _type, "函数", "@" + cur->children[1]->data_str, 0, 0, cur_table); // cur->children[1]为 FUNC_DEC

            if (_type != "void")
            {
                string __irtype = "";
                if (_type == "int")
                {
                    __irtype = "i32";
                }
                else if (_type == "char")
                {
                    __irtype = "i8";
                }

                //给返回值(local)插ir
                appendSymbol("返回值", _type, "返回值", "%l" + getUniqueIntTxt(), 1, cur_table);
            }

            ana(cur->children[1], cur_table, 1, -1); //可能会有形参
            ana(cur->children[2], cur_table, 0, -1); //为COMP_STM，在COMP_STM提升level                                                                       // cur->children[2] 为COMP_STM
        }
        else if (cur->type == "FUNC_DEC")
        {
            for (auto it : cur->children)
            {
                ana(it, cur_table, 1, -1);
            }
        }
        else if (cur->type == "PARAM_LIST")
        {
            for (auto it : cur->children)
            {
                ana(it, cur_table, 1, -1);
            }
        }
        else if (cur->type == "COMP_STM")
        {
            //在FUNC_DEF下面
            //提升了level
            ana(cur->children[0], cur_table, cur_level + 1, optional_finished_br_label); //只有一个child，为STM_LIST
        }
        else if (cur->type == "STM_LIST")
        {
            //在COMP_STM下面
            for (auto it : cur->children)
            {
                ana(it, cur_table, cur_level, optional_finished_br_label);
            }
            if (optional_finished_br_label > 0)
            {
                if (optional_finished_br_label == G_NOW_LABEL)
                {
                    optional_finished_br_label = 1;
                    cout << "WARNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNING!@optional_finished_br_label.ignore this br " << G_NOW_LABEL << endl;
                }
                else
                {
                    if (irs[cur_table][G_NOW_LABEL].size() > 0 && irs[cur_table][G_NOW_LABEL].back().find("br") == 0)
                    {
                        /* code */
                    }
                    else
                    {
                        irs[cur_table][G_NOW_LABEL].push_back("br label .L" + int2str(optional_finished_br_label));
                    }
                }
            }
            if (cur->parent->parent->type == "FUNC_DEF")
            {
                cout << "function_finished" << endl;
                if (irs[cur_table][G_NOW_LABEL].size() > 0 && irs[cur_table][G_NOW_LABEL].back().find("br") == 0)
                {
                    /* code */
                }
                else
                {
                    irs[cur_table][G_NOW_LABEL].push_back("br label .L1");
                }
            }
        }
        //===Stmt
        else if (cur->type == "EXP_STMT")
        {
            //只有一个childern且为 Exp
            exp_handler(cur->children[0], cur_table, cur_level, G_NOW_LABEL);
        }
        else if (cur->type == "RETURN")
        {
            // return的时候不写exit相关，是在最终printir的时候再根据是否有返回值来打
            //只改写返回值local
            //只有一个childern且为 Exp
            string r = convertToTemp(exp_handler(cur->children[0], cur_table, cur_level, G_NOW_LABEL), cur_table, cur_level, G_NOW_LABEL);
            irs[cur_table][G_NOW_LABEL].push_back(SymbolTable[cur_table][0]->inname + " = " + r);
            if (irs[cur_table][G_NOW_LABEL].size() > 0 && irs[cur_table][G_NOW_LABEL].back().find("br") == 0)
            {
                /* code */
            }
            else
            {
                irs[cur_table][G_NOW_LABEL].push_back("br label .L1");
            }
        }
        else if (cur->type == "RETURN_EMP")
        {
            //空返回
            if (irs[cur_table][G_NOW_LABEL].size() > 0 && irs[cur_table][G_NOW_LABEL].back().find("br") == 0)
            {
                /* code */
            }
            else
            {
                irs[cur_table][G_NOW_LABEL].push_back("br label .L1");
            }
        }
        else if (cur->type == "IF_THEN")
        {

            // if里面为另一个label
            int _label = G_NOW_LABEL;
            int if_block_label = getNewLabel();
            //本if_then的外层
            int out_block_label = getNewLabel(); // optional_finished_br_label;
            string con = exp_handler(cur->children[0], cur_table, cur_level, G_NOW_LABEL, if_block_label, out_block_label);
            //为了防止里面有*导致老师的编译器不识别，所以再套一个tmp
            if (con[0] == '*')
            {
                string t = "%t" + getUniqueIntTxt();
                appendSymbolTemp("i32", t, cur_level, cur_table);
                irs[cur_table][_label].push_back(t + " = " + con);
                con = t;
            }
            // and or 因为它直接写br,所以空
            if (con != "")
            {
                irs[cur_table][_label].push_back("bc " + con + ", label .L" + int2str(if_block_label) + ", label .L" + int2str(out_block_label));
            }

            G_NOW_LABEL = if_block_label;
            ana(cur->children[1], cur_table, G_NOW_LABEL, out_block_label);

            G_NOW_LABEL = out_block_label;
        }
        else if (cur->type == "IF_THEN_ELSE")
        {
            // if里面为另一个label
            int _label = G_NOW_LABEL;
            int if_true_block_label = getNewLabel();
            int if_false_block_label = getNewLabel();
            //下面指if-then-else外的第一个label
            int out_block_label = getNewLabel(); // optional_finished_br_label;
            /*if (optional_finished_br_label == -1)
            {
                out_block_label = getNewLabel();
            }*/
            string con = exp_handler(cur->children[0], cur_table, cur_level, G_NOW_LABEL, if_true_block_label, if_false_block_label);
            if (con[0] == '*')
            {
                string t = "%t" + getUniqueIntTxt();
                appendSymbolTemp("i32", t, cur_level, cur_table);
                irs[cur_table][_label].push_back(t + " = " + con);
                con = t;
            }
            // and or 因为它直接写br,所以空
            if (con != "")
            {
                irs[cur_table][_label].push_back("bc " + con + ", label .L" + int2str(if_true_block_label) + ", label .L" + int2str(if_false_block_label));
            }

            G_NOW_LABEL = if_true_block_label;
            ana(cur->children[1], cur_table, G_NOW_LABEL, out_block_label);

            G_NOW_LABEL = if_false_block_label;
            ana(cur->children[2], cur_table, G_NOW_LABEL, out_block_label);

            G_NOW_LABEL = out_block_label;
        }
        else if (cur->type == "WHILE")
        {
            // while里面为另一个label
            int _label = G_NOW_LABEL;
            int while_condition_label = getNewLabel();
            // while 的判断要单独一个Label
            if (irs[cur_table][_label].size() > 0 && irs[cur_table][_label].back().find("br") == 0)
            {
                /* code */
            }
            else
            {
                irs[cur_table][_label].push_back("br label .L" + int2str(while_condition_label));
            }

            int while_block_label = getNewLabel();
            int out_block_label = getNewLabel(); // optional_finished_br_label;
            //为break continue存储数据
            WhileInfo *info = new WhileInfo();
            info->condition_label = while_condition_label;
            info->continue_label = while_block_label;
            info->break_label = out_block_label;
            whileInfoStack.push(*info);
            string con = exp_handler(cur->children[0], cur_table, cur_level, while_condition_label, while_block_label, out_block_label);
            if (con[0] == '*')
            {
                string t = "%t" + getUniqueIntTxt();
                appendSymbolTemp("i32", t, cur_level, cur_table);
                irs[cur_table][while_condition_label].push_back(t + " = " + con);
                con = t;
            }
            // and or 因为它直接写br,所以空
            if (con != "")
            {
                irs[cur_table][while_condition_label].push_back("bc " + con + ", label .L" + int2str(while_block_label) + ", label .L" + int2str(out_block_label));
            }

            G_NOW_LABEL = while_block_label;
            //结束回到 while_condition_label
            ana(cur->children[1], cur_table, G_NOW_LABEL, while_condition_label);
            //这个时候应该执行完了，G_NOW_LABEL
            G_NOW_LABEL = out_block_label;
            whileInfoStack.pop();
        }
    }
}

/*
 * 这是一个神奇的函数
 * 具体原因是，yacc分析中，由于右递归的存在，会出现list接list的情况
 * 这个用途在于将上着合并到最上面的list
 * 来好分析。毕竟这些都是同一级的
 * 后续IR生成需要基于此修复后的ast图
 * 假如要看原本的yacc导出来的，main函数中不执行此即可
 * 但是就没法做IR生成了
 */
void fix_ast(Node *cur)
{
    int ch_size = cur->children.size();
    for (int i = 0; i < cur->children.size(); i++)
    {
        Node *nod = cur->children[i];
        //白名单政策
        // 只对如下的来合并
        if ((cur->type == "SEGMENT_LIST" || cur->type == "STM_LIST" || cur->type == "DEF_LIST" || cur->type == "EXT_DEC_LIST" || cur->type == "DEC_LIST" || cur->type == "PARAM_LIST" || cur->type == "ARG_LIST") && nod->type == cur->type && nod->data_char == cur->data_char && nod->data_int == cur->data_int && nod->data_str == cur->data_str)
        {
            //把nod的子节点全部结到cur下面
            for (int i = 0; i < nod->children.size(); i++)
            {
                nod->children[i]->parent = cur;
                cur->children.push_back(nod->children[i]);
            }
            // free(nod);
            auto iter = cur->children.erase(std::begin(cur->children) + i); // Delete the [i] element
            i = 0;
        }
    }
    for (int i = 0; i < cur->children.size(); i++)
    {
        fix_ast(cur->children[i]);
    }
}
/*
 * 强制将if、while语句内部套进COMP_STM 和 STM_LIST
 * 由于语法识别规则，在只有单句没有花括号的情况下
 * 不会进stm_list
 * 这里就是为了修复此问题
 */
void fix_ast2(Node *cur)
{
    //强制将if、while语句内部套进COMP_STM 和 STM_LIST
    for (int i = 0; i < cur->children.size(); i++)
    {
        for (int i = 0; i < cur->children.size(); i++)
        {
            Node *nod = cur->children[i];
            if (nod->type == "IF_THEN" && nod->children[1]->type != "COMP_STM")
            {
                Node *n1 = new Node();
                n1->type = "COMP_STM";
                Node *n2 = new Node();
                n2->type = "STM_LIST";
                n1->children.push_back(n2);
                n2->parent = n1;

                n1->parent = cur;
                n2->children.push_back(nod->children[1]);
                auto iter = nod->children.erase(std::begin(nod->children) + 1); // Delete the [i] element
                nod->children.push_back(n1);
            }
            if (nod->type == "IF_THEN_ELSE" && nod->children[1]->type != "COMP_STM")
            {
                Node *n1 = new Node();
                n1->type = "COMP_STM";
                Node *n2 = new Node();
                n2->type = "STM_LIST";
                n1->children.push_back(n2);
                n2->parent = n1;

                n1->parent = cur;
                n2->children.push_back(nod->children[1]);
                auto iter = nod->children.erase(std::begin(nod->children) + 1); // Delete the [i] element
                nod->children.insert(nod->children.begin() + 1, n1);            //在指定位置，例如在第2个元素前插入一个元素
            }
            if (nod->type == "IF_THEN_ELSE" && nod->children[2]->type != "COMP_STM")
            {
                Node *n1 = new Node();
                n1->type = "COMP_STM";
                Node *n2 = new Node();
                n2->type = "STM_LIST";
                n1->children.push_back(n2);
                n2->parent = n1;

                n1->parent = cur;
                n2->children.push_back(nod->children[2]);
                auto iter = nod->children.erase(std::begin(nod->children) + 2);
                // Delete the [i] element
                nod->children.insert(nod->children.begin() + 2, n1);
                //在指定位置，例如在第3个元素前插入一个元素
            }
            if (nod->type == "WHILE" && nod->children[1]->type != "COMP_STM")
            {
                Node *n1 = new Node();
                n1->type = "COMP_STM";
                Node *n2 = new Node();
                n2->type = "STM_LIST";
                n1->children.push_back(n2);
                n2->parent = n1;

                n1->parent = cur;
                n2->children.push_back(nod->children[1]);
                auto iter = nod->children.erase(std::begin(nod->children) + 1);
                // Delete the [i] element
                nod->children.push_back(n1);
            }
        }
    }
    for (int i = 0; i < cur->children.size(); i++)
    {
        fix_ast2(cur->children[i]);
    }
}

int main(int argc, char *argv[])
{
    for (int i = 0; i < irs.size(); i++)
    {
        irs[i].resize(200);
    }
    string filename = "test.c";
    string dumpfile = "_dump.ir";
    string symbolfile = "_sym.txt";
    string astfile = "_ast.png";
    string blockfile = "_block.png";
    //不修复ast树
    bool nofixast = false;
    cout << argc << endl;
    if (argc == 1)
    {
        dumpfile = "dump.ir";
        astfile = "ast.png";
    }
    else if (argc == 2)
    {
        filename = (argv[1]);
        dumpfile = "dump.ir";
        astfile = "ast.png";
    }
    else if (argc == 5)
    {
        filename = (argv[4]);
        if (strcmp(argv[1], "-s") == 0)
        {
            //输出符号表
            symbolfile = argv[3];
        }
        else if (strcmp(argv[1], "-i") == 0)
        {
            //输出ir
            dumpfile = (argv[3]);
        }
        else if (strcmp(argv[1], "-a") == 0)
        {
            //输出ast树
            astfile = (argv[3]);
        }
        else if (strcmp(argv[1], "-anofix") == 0)
        {
            //输出ast树(no fix)
            astfile = (argv[3]);
            nofixast = true;
        }
    }
    else if (argc == 6)
    {
        //基本块划分
        G_FUNC_BLOCK = (argv[2]);
        blockfile = (argv[4]);
        filename = (argv[5]);
    }
    const char *sFile = (char *)filename.c_str();
    FILE *fp = fopen(sFile, "r");
    if (fp == NULL)
    {
        printf("cannot open %s\n", sFile);
        return -1;
    }
    extern FILE *yyin;
    // yyin和yyout都是FILE*类型
    yyin = fp;
    // yacc会从yyin读取输入，yyin默认是标准输入，这里改为磁盘文件。yacc默认向yyout输出，可修改yyout改变输出目的

    printf("-----begin parsing %s\n", sFile);
    yyparse();
    //使yacc开始读取输入和解析，它会调用lex的yylex()读取记号
    puts("-----end parsing");
    fclose(fp);
    Agraph_t *g = agopen((char *)"ast", Agdirected, nullptr);
    Agraph_t *g_block = agopen((char *)"block", Agdirected, nullptr);
    if (!nofixast)
    {
        //修复ast树
        fix_ast(ast_root);
        fix_ast2(ast_root);
        cout << "end_fix" << endl;
    }
    //==
    Agnode_t *gv_node = agnode(g, (char *)int2str(node_num++).c_str(), 1);
    agsafeset(gv_node, "shape", "plaintext", "");
    agsafeset(gv_node, "fontsize", "20", "");
    agsafeset(gv_node, (char *)"label", "GitHub.com/ludoux\\nchaos_minic repo\\nluu.moe/114", "");
    //==
    genNodes(g, NULL, ast_root);
    cout << "end gennodes" << endl;
    if (astfile[0] != '_')
    {
        GVC_t *gv = gvContext();
        gvLayout(gv, g, "dot");
        gvRenderFilename(gv, g, "png", astfile.c_str());
        agclose(g);
        gvFreeContext(gv);
        cout << "end output" << endl;
        if (argc == 5)
        {
            return 0;
        }
    }
    // ir分析
    ana(ast_root, 0, 0, -1);
    cout << "end ana" << endl;
    string dumpir = printIrs();
    string dumpsymbol = printSymbol();
    if (dumpfile[0] != '_')
    {
        ofstream ofs(dumpfile, ios::out);
        ofs << dumpir;
        ofs.close();
    }
    else if (symbolfile[0] != '_')
    {
        ofstream ofs(symbolfile, ios::out);
        ofs << dumpsymbol;
        ofs.close();
    }
    if (G_FUNC_BLOCK[0] != '_')
    {
        genBlock(g_block);
        GVC_t *gv = gvContext();
        gvLayout(gv, g_block, "dot");
        gvRenderFilename(gv, g_block, "png", blockfile.c_str());
        agclose(g_block);
        gvFreeContext(gv);
        cout << "end g_block" << endl;
    }

    return 0;
}