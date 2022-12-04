
void putint(int);

int main()
{
	int i, j, t, n, temp;
	int mult1[20];
	int mult2[20];
	int len1;
	len1 = 20;
	int len2;
	len2 = 20;
	int c1[25];
	int c2[25];
	int result[40];

	mult1[0] = 1;
	mult1[1] = 2;
	mult1[2] = 3;
	mult1[3] = 4;
	mult1[4] = 5;
	mult1[5] = 6;
	mult1[6] = 7;
	mult1[7] = 8;
	mult1[8] = 9;
	mult1[9] = 0;
	mult1[10] = 1;
	mult1[11] = 2;
	mult1[12] = 3;
	mult1[13] = 4;
	mult1[14] = 5;
	mult1[15] = 6;
	mult1[16] = 7;
	mult1[17] = 8;
	mult1[18] = 9;
	mult1[19] = 0;

	mult2[0] = 2;
	mult2[1] = 3;
	mult2[2] = 4;
	mult2[3] = 2;
	mult2[4] = 5;
	mult2[5] = 7;
	mult2[6] = 9;
	mult2[7] = 9;
	mult2[8] = 0;
	mult2[9] = 1;
	mult2[10] = 9;
	mult2[11] = 8;
	mult2[12] = 7;
	mult2[13] = 6;
	mult2[14] = 4;
	mult2[15] = 3;
	mult2[16] = 2;
	mult2[17] = 1;
	mult2[18] = 2;
	mult2[19] = 2;

	i = 0;
	while (i < 40) {
		result[i] = 0;
		i = i + 1;
	}

	i = 0;
	while (i < len1) {
		c1[i] = mult1[i];
		i = i + 1;
	}	

	i = 0;
	while (i < len2) {
		c2[i] = mult2[i];
		i = i + 1;
	}	

	n = len1 + len2 - 1;

	i = 0;
	while (i <= n) {
		result[i]=0;
		i = i + 1;
	}	 

	temp=0;

	i = len2 - 1;
	while (i > -1) {
		t = c2[i];
		j = len1 - 1;
		while (j > -1) {
			temp = result[n] + t * c1[j];
			if(temp >= 10) {
				result[n] = (temp);
				result[n-1] = result[n-1] + temp / 10;
			}
			else
				result[n] = temp;
			j = j - 1;
			n = n - 1;
		}
		n = n + len1 - 1;
		i = i - 1;
	}

	if(result[0] != 0)
		putint(result[0]); 

	i = 1;
	while (i <= len1 + len2 - 1) {
		putint(result[i]); 
		i = i + 1;
	}

	return 0;
}