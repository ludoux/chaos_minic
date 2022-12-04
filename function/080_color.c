int dp[18][18][18][18][18][7];
int list[200];

int getint();
void putint(int);
void putch(int);

int equal(int a, int b) {
    if (a == b)
        return 1;
    return 0;
}

int dfs(int a, int b, int c, int d, int e, int last){
    if(dp[a][b][c][d][e][last] != -1)
        return dp[a][b][c][d][e][last];
    if(a + b + c + d + e == 0)
        return 1;
    int ans;
    ans = 0;
    if (a) ans = (ans + (a - equal(last, 2)) * dfs(a - 1, b, c, d, e, 1)) % 1000000007;
    if (b) ans = (ans + (b - equal(last, 3)) * dfs(a + 1, b - 1, c, d, e, 2)) % 1000000007;
    if (c) ans = (ans + (c - equal(last, 4)) * dfs(a, b + 1, c - 1, d, e, 3)) % 1000000007;
    if (d) ans = (ans + (d - equal(last, 5)) * dfs(a, b, c + 1, d - 1, e, 4)) % 1000000007;
    if (e) ans = (ans + e * dfs(a, b, c, d + 1, e - 1, 5)) % 1000000007;
    dp[a][b][c][d][e][last] = ans % 1000000007;
    return dp[a][b][c][d][e][last];
}

int cns[20];
 
int main(){
    int n;
    int i; 
    n = getint();
    i = 0;
    while (i < 18) {
        int j;
        j = 0;
        while(j < 18) {
            int k;
            k = 0;
            while(k < 18) {
                int l;
                l = 0;
                while (l < 18) {
                    int m;
                    m = 0;
                    while (m < 18) {
                        int h;
                        h = 0;
                        while (h < 7) {
                            dp[i][j][k][l][m][h] = -1;
                            h = h + 1;
                        }
                        m = m + 1;
                    }
                    l = l + 1;
                }
                k = k + 1;
            }
            j = j + 1;
        }
        i = i + 1;
    }
    
    i = 0;
    while (i < n) {
        list[i] = getint();
        cns[list[i]] = cns[list[i]] + 1;
        i = i + 1;
    }

    int ans;
    ans = dfs(cns[1], cns[2], cns[3], cns[4], cns[5], 0);

    putint(ans);

    return ans;
}