#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>

int main() {
    sleep(4);

    // 打开文件，获取文件描述符
    int fd = open("./1.txt", O_RDONLY, S_IRUSR|S_IWUSR);
    struct stat sb;
    if (fstat(fd, &sb) == -1) {
        perror("cannot get file size\n");
    }
    printf("file size is %ld\n", sb.st_size);

    // 映射文件到内存中
    char *file_in_memory = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);

    // 打印文件
    for (int i = 0; i < sb.st_size; ++i) {
        printf("%c", file_in_memory[i]);
    }

    // 关闭
    munmap(file_in_memory, sb.st_size);
    close(fd);
}
