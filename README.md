## malloc函数底层进行系统内存申请案例

### **内存相关的系统调用**

接下来讲解内存相关的系统调用；

系统调用即：用户态切换至内核态的方式之一（另外两种是中断和异常），而在申请内存时就需要使用系统调用；

>   **为了安全起见，用户是无法直接操纵硬件的，所以需要通过系统调用使用内核操作；**

本节以malloc为例，进行讲解；

>   **malloc在C语言中用于申请内存空间；**
>
>   <font color="#f00">**malloc本身不是系统调用，但是malloc是brk和mmap系统调用的封装；**</font>
>
>   -   <font color="#f00">**在128KB以内，默认使用的是brk系统调用，进行内存申请；**</font>
>   -   <font color="#f00">**在128KB以上，使用mmap进行内存申请；**</font>

#### **①系统调用brk**

malloc小于128K（阈值可修改）的内存时，用的是brk；

在C语言的`unistd.h`头文件中有一个sbrk的库函数，是对brk的封装；

brk_demo.c

```c
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

```

代码解释：

首先，申请0个字节空间给first，再申请一个字节给second，最后申请0个字节给third；

程序输出如下：

```
0x800040000
0x800040000
0x800040001
```

由于second申请的一个字节返回的是空间的头，所以和first地址相同；

而third在申请时，由于已经被second占用，所以输出为后一个内存区域；

>   <font color="#f00">**从上面的代码可以看出，brk在申请内存时，是连续申请的，所以虚拟地址是连续分配的，`brk其实就是向上扩展heap的上界`；**<font>

修改程序，仅申请一个字节，并把指针强转为int类型：

brk_demo2.c

```c
#include <stdio.h>
#include <unistd.h>

int main() {
    int *first = (int *)sbrk(1);
    *(first + 1) = 123;
    printf("%d\n", *(first + 1));
}

```

此时，first+1实际上是位于第五个字节（int本身是四个字节！）；

所以`*(first + 1) = 123`实际上是对5~8字节赋值为123；

>   <font color="#f00">**注意：我们申请的是第0个字节的区域，但是却给5~8字节进行赋值了！**</font>

执行程序，输出结果如下：

```
123
```

执行没有报错的原因在于：<font color="#f00">**在进行brk时，申请的内存最小空间是一页，即4KB；所以，brk看似是申请了一个字节，实际上是申请了4KB的大小吗，即4096Byte！所以在赋值时，还是在当页进行赋值的，所以不会报错！**</font>

再次修改代码，使用4K区域之后的内存空间：

brk_demo3.c

```c
#include <stdio.h>
#include <unistd.h>

int main() {
    int *first = (int *)sbrk(1);
    *first = 1;
    *(first + 1024) = 123;
    printf("%d\n", *(first + 1024));
}

```

编译并执行：

```bash
jasonkay@jasonkayPC:~/workspace$ gcc test.c -o test.out
jasonkay@jasonkayPC:~/workspace$ ./test.out 
Segmentation fault (core dumped)
```

可以发现，由于在新的一页进行分配，直接报了`Segment Fault`错误；

>   **这是由于没有申请新的一页，却直接用了！**

>   <font color="#f00">**注：上述的实验是在Linux环境下进行了，在Windows下并未复现Segment Fault！**</font>

<br/>

#### **②系统调用mmap**

当malloc申请大于128K的内存时，用的是mmap；

在C语言中对应的申请和释放函数如下所示：

```c
#include<sys/mman.h>

// addr传NULL则不关心起始地址
// 关心地址的话应传个4k的倍数，不然也会归到4k倍数的起始地址;
void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
//释放内存munmap
int munmap(void *addr, size_t length);
```

其中：

-   addr：申请地址的起始位置，如果不关心起始地址，可以传null；
-   length：申请内存大小，必定不能传0；
-   prot：权限标志位，PROT_READ映射区读权限，PROT_WRITE写权限，读写都指定时：PROT_READ|PROT_WRITE；
-   flags：标志位参数，MAP_SHARED:修改了内存数据，会同步到磁盘，MAP_PRIVATE:修改了内存数据，不会同步到磁盘；
-   fd：文件描述符，要映射的文件对应的fd，使用时通过fd打开这个文件；**由于mmap本身是将文件映射到内存，所以如果需要直接申请空间，可以传入-1；**
-   off_t：映射文件的偏移量，作用：映射的时候文件指针的偏移量,必须是4k的整数倍,通常指定0，不偏移；fd如果是-1，off_t直接取0即可；

函数返回值：调用成功返回映射区的首地址，调用失败返回MAP_FAILED宏（实际上是(void*)-1）；

>   **munmap函数：**
>
>   函数作用：释放内存映射区；
>
>   函数原型及参数：
>
>   ```c
>   int munmap(void *addr, size_t length);
>   ```
>
>   -   addr：mmap函数的返回值；
>   -   length：mmap的第二个参数，映射区的长度 ；
>
>   函数返回值：成功时，munmap()返回0；失败时返回-1（and errno is set (probably to EINVAL)）；

mmap申请内存的代码如下：

mmap_demo.c

```c
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

```

在代码中，我们申请了100页的内存，并且对每一页进行赋值；

并使用pidstat对进程进行监控；

>   pidstat可以通过`sudo apt install sysstat`安装；

使用pidstat对进程监控，并编译和执行上述程序：

```bash
pidstat -r 1 300
```

-   -r表示：内存监控；
-   1 300表示：每一秒监控一次，共300秒；

编译执行：

```bash
gcc test.c -o a.out
./a.out
```

结果如下：

```
08时51分00秒   UID       PID  minflt/s  majflt/s     VSZ     RSS   %MEM  Command
08时51分01秒  1000      7497    100.00      0.00    2756    1236   0.01  test.out
```

可以看出，监控中出现了100次的min fault，代表了内存的小错误：

因为在赋值时，每一页并没有分配空间，所以需要缺页错误；

>   **缺页不是严重的错误，因为这个缺页对应的不是磁盘内容；**

<br/>

下面使用mmap将一个文件映射至内存；

mmap_demo2.c

```c
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>

int main() {
    sleep(4);

    // 打开文件，获取文件描述符
    int fd = open(".\\1.txt", O_RDONLY, S_IRUSR|S_IWUSR);
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

```

再次通过pidstat监控，结果如下：

```
09时14分03秒   UID       PID  minflt/s  majflt/s     VSZ     RSS   %MEM  Command
09时14分04秒  1000      7625     64.00      1.00    2356     516   0.00  a.out
09时14分04秒  1000      7625     24.00      0.00    2356     516   0.00  a.out
09时14分04秒  1000      7625     14.00      0.00    2356     516   0.00  a.out
09时14分04秒  1000      7625     34.00      0.00    2356     516   0.00  a.out
```

可以看到，首先会触发一次major fault，将文件读入内存；

>   **如果mmap是映射的磁盘文件，也会惰性加载，在初次加载或者页被逐出后再加载的时候，也会缺页，此时为major fault；**

>   使用read也可以读取文件：
>
>   <font color="#f00">**但是使用read系统调用进行文件读取时，会从用户态进入内核态，随后由内核将文件读入内核空间，再由内核空间复制到用户的内存空间；之后再从内核态切换为用户态，继续执行用户程序；**</font>
>
>   <font color="#f00">**而mmap是空间的直接映射：首先将Page Table中的空间映射至磁盘（惰性加载），而在第一次读文件时，发现映射是磁盘空间，进而引起major fault缺页中断，将文件直接读入内存空间，并产生内核空间和用户空间的直接映射；**</font>
>
>   流程图如下：
>
>   ![mmap读取文件.png](https://cdn.jsdelivr.net/gh/jasonkayzk/blog_static@master/images/mmap读取文件.png)
>
>   **所以mmap省去了内核空间→用户空间的拷贝的过程；**
>
>   **虽然mmap实现了零拷贝，但是mmap无法利用buffer/cache内存区域，并且mmap引发的缺页异常和read函数的耗时并无绝对的性能优势，所以read函数和mmap都有各自的应用场景；**

### 相关文章

- Github Pages：[计算机内存综述](https://jasonkayzk.github.io/2021/03/25/%E8%AE%A1%E7%AE%97%E6%9C%BA%E5%86%85%E5%AD%98%E7%BB%BC%E8%BF%B0/)
- 国内Gitee镜像：[计算机内存综述](https://jasonkay.gitee.io/2021/03/25/%E8%AE%A1%E7%AE%97%E6%9C%BA%E5%86%85%E5%AD%98%E7%BB%BC%E8%BF%B0/)

