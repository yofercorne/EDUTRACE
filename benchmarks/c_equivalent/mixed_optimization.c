#include <stdio.h>

int main(void) {
    int x;
    int y;

    x = 2 + 3 * 4;
    y = (x + 0) * 1;

    if (1) {
        printf("%d\n", y);
    } else {
        printf("999\n");
    }

    while (0) {
        printf("888\n");
    }

    return 0;
    printf("777\n");
}
