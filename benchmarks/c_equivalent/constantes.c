#include <stdio.h>

int main() {
    long x;
    long y;

    x = 2 + 3 * 4;
    y = (x + 0) * 1;

    if (2 < 3) {
        printf("%ld\n", y);
    } else {
        printf("0\n");
    }

    while (0) {
        printf("999\n");
    }

    return 0;
}
