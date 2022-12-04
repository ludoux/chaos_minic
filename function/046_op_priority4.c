int a;
int b;
int c;
int d;
int e;

int getint();
void putint(int);

int main()
{
	a=getint();
	b=getint();
	c=getint();
	d=getint();
	e=getint();
	int flag;
	flag = 0;
	if(a-b*c!=d-a/c||a*b/c==e+d||a+b+c==d+e)
	{
		flag=1;
	}
	return flag;
}
