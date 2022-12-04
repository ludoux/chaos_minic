int arr[6];

int main() {
  int i, sum;

  arr[0] = 1;
  arr[1] = 2;
  arr[2] = 33;
  arr[3] = 4;
  arr[4] = 5;
  arr[5] = 6;

  i = 0;
  sum = 0;
  while (i < 6) {
    sum = sum + arr[i];
    i = i + 1;
  }
  return sum;
}
