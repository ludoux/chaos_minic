int a;
int b;

int getint();
void putint(int);

int main()
{
	a=getint();
	b=getint();
	int c;
	if (a==b&&a!=3) {
		c = 1;
	}
	else {
		c = 0;
	}
	return c;
}
