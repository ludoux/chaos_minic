int a;
int b;
int c;
int d;
int e;

void putint(int k);

int main()
{
	a = 1;
	b = 0;
	c = 1;
	d = 2;
	e = 4;
	int flag;
	flag = 0;
	if(a * b / c == e + d && a * (a + b) + c <= d + e || a - (b * c) == d - a / c)
	{
		flag=1;
	}
	putint(flag);
	return flag;
}