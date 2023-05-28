#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
int main(){
    
    pid_t pid;
    // OPEN FILES
    int fd;
    fd = open("test.txt" , O_RDWR | O_CREAT | O_TRUNC);
    if (fd == -1)
    {
        /* code */
        printf("Error in opening file");
        return 1;
    }
    //write 'hello fcntl!' to file
    /* code */
    char str[] = "hello fcntl!";
    write(fd, str, strlen(str));

    // DUPLICATE FD

    /* code */
    int new_fd = fcntl(fd, F_DUPFD, 0);
    printf("new fd: %d\n", new_fd);

    if(new_fd == -1){
        printf("Error when copy file descriptor");
        return 1;
    }

    pid = fork();

    if(pid < 0){
        // FAILS
        printf("error in fork");
        return 1;
    }
    
    struct flock fl;

    if(pid > 0){
        printf("Hi, this is parent process.\n");
        // PARENT PROCESS
        //set the lock
        fl.l_type = F_WRLCK;
        //设置锁的类型为写入锁，其他进程无法以写入模式打开被锁定的文件
        fl.l_whence = SEEK_SET;
        //设置锁的起始位置，SEEK_SET表示从文件头开始计算偏移量
        //注意：l_start是相对l_where的偏移量，合起来表示锁的起始位置
        fl.l_start = 0;
        //设为0
        fl.l_len = 0;
        //l_len表示锁的范围，为0表示从起始到文件末
        //append 'b'
        if(fcntl(new_fd, F_SETLKW, &fl) == -1){
            printf("Error: Fail to lock file.");
            return 1;
        }
        char ch = 'b';
        write(new_fd, &ch, sizeof(ch));
        //unlock
        fl.l_type = F_UNLCK;
        if(fcntl(new_fd, F_SETLKW, &fl) == -1){
            printf("Error: Fail to unlock file.");
            return 1;
        }
        sleep(3);

        // printf("%s", str); the feedback should be 'hello fcntl!ba'
        
        //读取文件内容并输出
        char buf[1<<12];
        lseek(new_fd, 0, SEEK_SET);//设置文件读写位置为文件开头
        int len = read(new_fd, buf, sizeof(buf));
        if(len == -1){
            printf("Error: Fail to read file.");
            return 1;
        }
        buf[len] = '\0';
        printf("%s\n", buf);
        close(fd);

        exit(0);

    } else {
        // CHILD PROCESS
        sleep(2);
        printf("Hi, this is child process.\n");
        //get the lock
        fl.l_type = F_WRLCK;
        fl.l_whence = SEEK_SET;
        fl.l_start = 0;
        fl.l_len = 0;
        if(fcntl(new_fd, F_SETLKW,&fl) == -1){
            printf("Error: Fail to lock file.");
            return 1;
        }
        //append 'a'
        char ch = 'a';
        write(fd, &ch, sizeof(ch));
        //unlock:
        fl.l_type = F_UNLCK;
        if(fcntl(new_fd, F_SETLKW, &fl) == -1){
            printf("Error: Fail to unlock file.");
            return 1;
        }
        close(new_fd);
        exit(0);
    }
    
    close(fd);
    return 0;
}