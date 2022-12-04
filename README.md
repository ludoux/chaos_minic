# **[chaos_minic](https://github.com/ludoux/chaos_minic)** 

by ludoux (Lu Chang \<chinaluchang [at] live [dot] com\>)

luu.moe

[chaos_minic——一份简陋的MiniC编译器（编译原理实验）](https://www.luu.moe/114)

## 编译

直接 `make` 即可，会生成 `minic` 可执行文件。（可能需要安装 graphviz 包）

## 运行

### IR输出
./minic -i -o ir.txt ./function/000.c
### 符号表输出
./minic -s -o sym.txt ./function/000.c

### AST图
./minic -a -o ast.png ./function/000.c

### AST图不修复
./minic -anofix -o ast.png ./function/000.c

### 基本块划分
./minic -c 函数名 -o block.png ./function/000.c

## 注意

遵循 GNU General Public License v3.0。此外，不能提交本项目或仅做小修改后作为课业作业提交给校方。参考/基于本项目的项目也需要开源。