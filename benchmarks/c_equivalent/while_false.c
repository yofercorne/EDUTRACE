#include <stdio.h>

int main(void) {
    int x;
    x = 5;
    while (0) {
        x = x + 100;
        printf("%d\n", x);
    }
    printf("%d\n", x);
    return 0;
}
