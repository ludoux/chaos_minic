/*
 * Author: Lu Chang <chinaluchang@live.com>
 * Create Date:   Fri May 20 01:38:41 2022 +0800
 * Releated Blog: https://www.luu.moe/114
 */
#include "ly.h"
int node_id = 0;
Node * ast_root = NULL;
Node * new_ast_node(string type, int lineno, Node * s1 , Node * s2 , Node * s3)
{
    Node * node = new Node();
    node->id=node_id++;
    node->type = type;
    node->parent = NULL;
    if (s1 != NULL)
    {
        node->children.push_back(s1);
        s1->parent = node;
    }
    if (s2 != NULL)
    {
        node->children.push_back(s2);
        s2->parent = node;
    }
    if (s3 != NULL)
    {
        node->children.push_back(s3);
        s3->parent = node;
    }
    
    /*nd->sons.push_back(first_son);
    first_son->parent = nd;

    if(second_son != nullptr) {
        nd->sons.push_back(second_son);
        second_son->parent = nd;
    }

    if(third_son != nullptr) {
        nd->sons.push_back(third_son);
        third_son->parent = nd;
    }*/

    return node;
}