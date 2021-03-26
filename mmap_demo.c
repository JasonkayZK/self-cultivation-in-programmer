#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>

int main() {
    // 使用mmap申请100页（100 * 4KB）
    int *a = (int *) mmap(NULL, 100 * 4096,
                          PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS,
                          -1, 0);
    int *b = a;
    // 100页中的每一页都进行赋值（否则，如果申请的没有被使用，操作系统不会分配）
    for (int i = 0; i < 100; ++i) {
        b = (void *) a + (i * 4096);
        // 每一页第一个地址赋值为1
        *b = 1;
    }
    while (1) {
        sleep(1);
    }
}
