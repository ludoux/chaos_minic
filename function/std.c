#include <stdio.h>

int getint()
{
    int d;

    scanf("%d", &d);

    return d;
}

int getch()
{
    char d;

    scanf("%c", &d);

    return d;
}

int getarray(int a[])
{
    int n, i;

    // 获取元素个数
    scanf("%d",&n);

    // 获取元素内容
    for(i = 0; i < n; ++i) {
        scanf("%d",&a[i]);
    }

    return n;
}

void putint(int k)
{
    printf("%d", k);
}

void putch(int c)
{
    printf("%c", (char)c);
}

void putarray(int n, int d[])
{
    int k;

    // 输出元素个数
    printf("%d:", n);

    // 输出元素内容，空格分割
    for(k = 0; k < n; k ++) {
        printf(" %d", d[k]);
    }

    // 输出换行符
    printf("\n");
}

void putstr(char * str)
{
    printf("%s", str);
}
