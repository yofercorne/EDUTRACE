#include <stdio.h>

long factorial(long n) {
    long r = 1;
    while (n > 1) {
        r = r * n;
        n = n - 1;
    }
    return r;
}

int main() {
    printf("%ld\n", factorial(10));
    return 0;
}
