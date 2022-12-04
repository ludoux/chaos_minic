int n;

int getint();
void putint(int);
void putch(int);

int main()
{
    //newline=10;
    int i;
    int j;
    //m = 1478;
    //int t;
    i=getint();
    j=getint();
    int temp;
    temp=i;
    i=j;
    j=temp;

    putint(i);
    temp = 10;
    putch(temp);
    putint(j);
    temp = 10;
    putch(temp);
    
    return 0;
}
