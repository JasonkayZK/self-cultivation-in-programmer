#include <stdio.h>
#include <unistd.h>

int main() {
    void *first = sbrk(0);
    void *second = sbrk(1);
    void *third = sbrk(0);

    printf("%p\n", first);
    printf("%p\n", second);
    printf("%p\n", third);
}
