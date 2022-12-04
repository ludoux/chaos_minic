int main(){
    int a[5][5];
    int m, n, i, j;

    m = 0;
    while(m < 5) {

        n = 0;
        while(n < 5) {

            a[m][n] = m + 1;
            n = n + 1;
        }

        m = m + 1;
    }

    m = 2;
    n = 3;
    j = 0;
    while(j < 5){
        i = a[m-1][j];
        a[m-1][j] = a[n-1][j];
        a[n-1][j] = i;
        j = j + 1;
    }
    return a[2][0];
}