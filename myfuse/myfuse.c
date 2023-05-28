#define FUSE_USE_VERSION 31
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
//#include <ctime.h>

#define FUSE_SUPER_MAGIC 0x65735546
#define BLOCKSIZE (1024UL * 4)
#define MAX_NAME 255
#define MAX_INODE (1024UL * 1024)
#define MAX_BLOCKS (1024UL * 1024)
/*
* 目录操作：新建目录:mkdir, getattr；删除目录: rmdir; 遍历目录:readdir; 进入目录：access
* 文件操作: 新建文件：getattr, mknod,open, write, release; 删除文件: unlink
* 状态查看: statfs
*/

FILE *fp;

struct memfs {
    struct statvfs statvfs;
};

static struct memfs memfs = {
    .statvfs = {
        .f_bsize = BLOCKSIZE,   //文件系统的基本块大小
        .f_frsize = BLOCKSIZE,  //fragment size

        .f_blocks = MAX_BLOCKS, //size of fs in f_frsize units
        .f_bfree = MAX_BLOCKS,  //number of free blocks
        .f_bavail = MAX_BLOCKS, //number of free blocks for unprivileged users

        .f_files = MAX_INODE,   //number of inodes
        .f_ffree = MAX_INODE,   //number of free inodes
        .f_favail = MAX_INODE,  //number of free inodes for unprivileged users

        .f_fsid = 0x0123456701234567,
        .f_namemax = MAX_NAME,
    },
};


//file indoes store in rbtree
struct memfs_file {
    char *path; //file path
    void *data; //file content
    //u8 free_on_delete;
    struct stat vstat; //file status
    int is_dir;
    struct memfs_file *parent;
    struct memfs_file *child_array[MAX_INODE];
    int child_array_size;
    //pthread_mutex_t lock;
};

struct memfs_file *file_root;

static struct memfs_file *__create_node(const char *path, mode_t mode, int is_dir, struct memfs_file *parent){
    fprintf(fp, "Hello, start create node\n");
    struct memfs_file *pf = NULL;
    pf = malloc(sizeof(struct memfs_file));
    if(!pf){
        return NULL;
    }
    memset(pf, 0, sizeof(struct memfs_file));
    pf -> path = strdup(path);
    //将mode传入vstat中
    pf -> vstat.st_mode = mode;
    if(!pf->path){
        free(pf);
        fprintf(fp, "Error: empty path\n");
        return NULL;
    }
    pf -> data = NULL;
    pf -> vstat.st_mode = mode | (is_dir ? S_IFDIR : S_IFREG);
    pf -> is_dir = is_dir;
    pf -> parent = parent;
    if(parent){
        fprintf(fp, "parent: %s\n", parent->path);
    }
    //pf -> child_array = malloc(sizeof(struct memfs_file *) * MAX_INODE);
    pf -> child_array_size = 0;
    pf -> vstat.st_nlink = is_dir ? 2 : 1; //目录的链接数为2， 文件的链接数为1, 目录每多一个子目录，链接数加1
    pf -> vstat.st_uid = getuid();
    pf -> vstat.st_gid = getgid();
    pf -> vstat.st_size = 0;
    pf -> vstat.st_blksize = BLOCKSIZE;
    pf -> vstat.st_blocks = 0;
    pf -> vstat.st_atime = time(NULL);
    pf -> vstat.st_mtime = time(NULL);
    pf -> vstat.st_ctime = time(NULL);
    //printf("Hello, finish create node\n");

    //node_array_size++;
    //node_array[node_array_size - 1] = pf;
    return pf;
}

const char *getSubstringBeforeLastSlash(const char *str) {
  const char *lastSlash = strrchr(str, '/');
  if (lastSlash == NULL) {
    return str;
  } else {
    char *substring = malloc(lastSlash - str + 1);
    memcpy(substring, str, lastSlash - str);
    substring[lastSlash - str] = '\0';
    return substring;
  }
}

const char *getSubstringAfterLastSlash(const char *str) {
  const char *lastSlash = strrchr(str, '/');
  if (lastSlash == NULL) {
    return str;
  } else {
    return lastSlash + 1;
  }
}

int isSubString(const char *a, const char *b){
    int len_a = strlen(a);
    int len_b = strlen(b);
    for(int i = 0; i < len_a && i < len_b; i++){
        if(a[i] != b[i]){
            return 0;
        }
    }
    return 1;
}

static struct memfs_file *__search(const char *path){
    struct memfs_file *cur_pf = file_root;
    fprintf(fp, "Hello, start search\n");
    fprintf(fp, "path: %s\n", path);
    //check the linked list tree:
    struct memfs_file *tmp = file_root;
    while(tmp){
        fprintf(fp, "tmp: %s\n", tmp->path);
        for(int i = 0; i < tmp->child_array_size; i++){
            fprintf(fp, "child: %s\n", tmp->child_array[i]->path);
        }
        break;
    }
    while(cur_pf){
        if(strcmp(cur_pf->path, path) == 0){
            return cur_pf;
        }
        int max_len = 0;
        struct memfs_file *max_pf = NULL;
        for(int i = 0; i < cur_pf->child_array_size; i++){
            if(isSubString(cur_pf->child_array[i]->path, path)){
                if(strlen(cur_pf->child_array[i]->path) > max_len){
                    max_len = strlen(cur_pf->child_array[i]->path);
                    max_pf = cur_pf->child_array[i];
                }
            }
        }
        if(max_pf){
            cur_pf = max_pf;
        }
        else{
            return NULL;
        }
    }
    return NULL;
}

static int __delete(const char *path){
    int res = 0;
    struct memfs_file *pf = NULL;
    pf = __search(path);
    if(!pf){
        return -ENOENT;
    }
    pf->parent->child_array_size--;
    for(int i = 0; i < pf->parent->child_array_size; i++){
        if(strcmp(pf->parent->child_array[i]->path, pf->path) == 0){
            for(int j = i; j < pf->parent->child_array_size; j++){
                pf->parent->child_array[j] = pf->parent->child_array[j+1];
            }
            break;
        }
    }

    free(pf);
    return res;
}

static int __insert(struct memfs_file *pf){
    int res = 0;
    if(!pf){
        return -ENOENT;
    }
    if(pf -> parent != NULL){
         pf->parent->child_array[pf->parent->child_array_size] = pf;
        pf->parent->child_array_size++;
        pf -> parent -> vstat.st_nlink++;
    }
    return res;
}

static void __update_time(struct memfs_file *pf){
    struct memfs_file *cur_pf = pf;
    while(cur_pf){
        cur_pf->vstat.st_ctime = time(NULL);
        cur_pf = cur_pf->parent;
    }
}

static int memfs_statfs(const char *path, struct statvfs *stbuf){
    fprintf(fp,"%s: %s\n", __FUNCTION__, path);
    *stbuf = memfs.statvfs;
    return 0;
}

static int memfs_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi){
    //用于查询结点是否存在，对于存在的结点，还可以查询其属性。
    (void)fi;
    fprintf(fp,"%s: %s\n", __FUNCTION__, path);
    memset(stbuf, 0, sizeof(struct stat));

    //pthread_mutex_lock(&memfs.lock);
    struct memfs_file *pf = __search(path);
    if(!pf) {
        fprintf(fp, "Error: cannot find node\n");
        return -ENOENT;
    }
    else {
        *stbuf = pf->vstat;
        return 0;
    }
    //pthread_mutex_unlock(&memfs.lock);
}

static int memfs_access(const char *path, int mask){
    //用于进入目录
    //printf("%s: %s\n", __FUNCTION__, path);
    //printf("%s: %s\n", __FUNCTION__, path);
    fprintf(fp, "%s: %s\n", __FUNCTION__, path);
    int res = 0;

    //pthread_mutex_lock(&memfs.lock);
    struct memfs_file *pf = __search(path);
    if(!pf){
        res = -ENOENT;
    }
    //pthread_mutex_unlock(&memfs.lock);
    return res;
}

static int memfs_mkdir(const char *path, mode_t mode){
    //新建目录
    //printf("%s: %s\n", __FUNCTION__, path);
    fprintf(fp, "%s: %s\n", __FUNCTION__, path);
    fprintf(fp, "mode:%ho\n",mode);
    int res = 0;
    struct memfs_file *pf = NULL;

    pf = __search(path);
    if(pf){
        fprintf(fp, "Hello, node already exist\n");
        return -EEXIST;
    }

    //pthread_mutex_lock(&memfs.lock);
    fprintf(fp, "getSubstringBeforeLastSlash: %s\n", getSubstringBeforeLastSlash(path));
    char *parent_path = getSubstringBeforeLastSlash(path);
    if(strcmp(parent_path, "") == 0){
        parent_path = "/";
    }
    struct memfs_file *par = __search(parent_path);
    pf = __create_node(path, mode, 1, par);
    fprintf(fp, "Hello, finish create node\n");
    res = __insert(pf);
    fprintf(fp, "Hello, finish insert node\n");
    //pthread_mutex_unlock(&memfs.lock);
    __update_time(pf);
    return res;
}

static int memfs_rmdir(const char *path){
    fprintf(fp,"%s: %s\n", __FUNCTION__, path);
    int res = 0;
    //printf("%s: %s\n", __FUNCTION__, path);
    //pthread_mutex_lock(&memfs.lock);
    if(__delete(path) == -ENOENT){
        res = -ENOENT;
    }
    //pthread_mutex_unlock(&memfs.lock);
    return res;
}

static int memfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,off_t offset, struct fuse_file_info *fi,enum fuse_readdir_flags flags){
    (void)offset;
    (void)fi;
    (void)flags;
    //遍历目录
    fprintf(fp,"%s: %s\n", __FUNCTION__, path);
    //printf("%s: %s\n", __FUNCTION__, path);
    //pthread_mutex_lock(&memfs.lock);
    struct memfs_file *pf = __search(path);
    if(!pf){
        return -ENOENT;
    }
    filler(buf, ".", NULL, 0, 0);
    if(strcmp(path, "/") != 0){
        filler(buf, "..", NULL, 0, 0);
    }
        
    for(int i = 0; i < pf->child_array_size; i++){
        fprintf(fp, "In readdir: %s\n", pf->child_array[i]->path);
        char *name = getSubstringAfterLastSlash(pf->child_array[i]->path);
        filler(buf, name, &pf -> vstat, 0, 0);
    }
    //pthread_mutex_unlock(&memfs.lock);
    return 0;
}

static int memfs_mknod(const char *path, mode_t mode, dev_t rdev){
    fprintf(fp,"%s: %s\n", __FUNCTION__, path);
    //新建结点：
    struct memfs_file *pf = NULL;
    struct memfs_file *par = NULL;
    char *parent_path = getSubstringBeforeLastSlash(path);
    if(strcmp(parent_path, "") == 0){
        parent_path = "/";
    }
    par = __search(parent_path);
    pf = __create_node(path, mode, 0, par);
    __insert(pf);
    __update_time(pf);
    return 0;
}

static int memfs_utimes(const char *path, const struct timespec tv[2], struct fuse_file_info *fi){
    return 0;
}

static int memfs_unlink(const char *path){
    fprintf(fp,"%s: %s\n", __FUNCTION__, path);
    //删除结点
    int res = __delete(path);
    return res;
}

static int memfs_open(const char *path, struct fuse_file_info *fi){
    fprintf(fp,"%s: %s\n", __FUNCTION__, path);
    //打开文件
    (void) fi;
    struct memfs_file *pf = __search(path);
    if(!pf){
        return -ENOENT;
    }
    if(pf->is_dir){
        return -EISDIR;
    }
    return 0;
}

static int memfs_release(const char *path, struct fuse_file_info *fi){
    printf("%s: %s\n", __FUNCTION__, path);
    (void) fi;
    return 0;
}

static int memfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi){
    fprintf(fp,"%s: %s\n", __FUNCTION__, path);
    //读文件
    (void) fi;
    struct memfs_file *pf = __search(path);
    if(!pf){
        return -ENOENT;
    }
    if(pf->is_dir){
        return -EISDIR;
    }
    if(offset >= pf->vstat.st_size){
        return 0;
    }
    if(offset + size > pf->vstat.st_size){
        size = pf->vstat.st_size - offset;
    }
    memcpy(buf, pf->data + offset, size);
    return size;
}

static int memfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi){
    fprintf(fp,"%s: %s\n", __FUNCTION__, path);
    //写文件
    (void) fi;
    struct memfs_file *pf = __search(path);
    if(!pf){
        return -ENOENT;
    }
    if(pf->is_dir){
        return -EISDIR;
    }
    if(offset + size > pf->vstat.st_size){
        pf->data = realloc(pf->data, offset + size);
        pf->vstat.st_size = offset + size;
    }
    //offset开始写入size大小的buf
    memcpy(pf->data + offset, buf, size);
    __update_time(pf);
    return size;
}

static struct fuse_operations memfs_oper = {
    .getattr	= memfs_getattr,
    .access     = memfs_access,
    
    .readdir	= memfs_readdir,
    
    .open    = memfs_open,
    .read    = memfs_read,
    .write   = memfs_write,
    .release = memfs_release,
    
    .mknod   = memfs_mknod,
    .utimens = memfs_utimes,

    .unlink  = memfs_unlink,
    

    .mkdir   = memfs_mkdir,
    .rmdir   = memfs_rmdir,

    .statfs  = memfs_statfs,
};

/*
* getattr: for all most all
* cd : access
* ls : readdir
* mkdir: mkdir
* rmdir: rmdir
* touch: mknod, utimens
* rm: unlink
*/

void init_array(){
    file_root = __create_node("/", 0755, 1, NULL);
    __insert(file_root);
    printf("Hello, finish init array\n");
}

int main(int argc, char *argv[]) {
    printf("hello\n");
    fp = fopen("log.txt", "w");
    if(fp == NULL){
        printf("Error: cannot open log.txt\n");
        return -1;
    }
    printf("Hello World!\n");
    init_array();
    
    fprintf(fp,"%s","Hello World!\n");
    int ret;
    ret = fuse_main(argc, argv, &memfs_oper, NULL);
    //fuse_opt_free_args(&args);
    fclose(stdout);
    return ret;
}