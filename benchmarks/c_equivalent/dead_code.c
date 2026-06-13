#include <stdio.h>

int helper(void) {
    printf("1\n");
    return 7;
    printf("999\n");
    printf("888\n");
}

int main(void) {
    printf("%d\n", helper());
    return 0;
    printf("123\n");
}
