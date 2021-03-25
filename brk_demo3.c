#include <stdio.h>
#include <unistd.h>

int main() {
    int *first = (int *)sbrk(1);
    *first = 1;
    *(first + 1024) = 123;
    printf("%d\n", *(first + 1024));
}
