#include <stdio.h>

long factorial(long n) {
    if (n == 0) return 1;
    return n * factorial(n - 1);
}

int main() {
    printf("%ld\n", factorial(10));
    return 0;
}
