#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include "my_tree.h"

#include "fifo_engine.h"

static FILE *tree_fp;
static char file_buff[1024];
static unsigned int bd_size = 4*1024*1024;
static unsigned int bd_offset = 7*4;

//depth ：
static void get_dirent_struct(char path[], int depth)
{
    DIR *p_dir = NULL;
	char tmp[255];  //路径备份
    struct stat stat_info;
    struct dirent *p_dirent = NULL;
    int ret;
    __off_t bd_cnt;

    //打开目录
    p_dir = opendir(path);
    if(p_dir == NULL) {
        printf("opendir error");
        return ;
    }

    while((p_dirent = readdir(p_dir)) != NULL){
		if((strcmp(p_dirent->d_name,"..") == 0) || (strcmp(p_dirent->d_name,".") == 0))
			continue;

        sprintf(tmp,"%s%s", path, p_dirent->d_name);
        stat(tmp, &stat_info);

        if(p_dirent->d_type==DT_REG)
        {
            // bd_cnt = (stat_info.st_size - bd_offset) / 4;
            bd_cnt = stat_info.st_size;
            struct tm* tmNow = localtime(&stat_info.st_mtim.tv_sec);
            sprintf(file_buff,"%s|%ld|%04d-%02d-%02d_%02d:%02d:%02d\n", tmp, bd_cnt, tmNow->tm_year+1900,tmNow->tm_mon+1,tmNow->tm_mday,tmNow->tm_hour,tmNow->tm_min,tmNow->tm_sec);
            fputs(file_buff, tree_fp);
            memset(file_buff,0,1024);
        } else if(p_dirent->d_type==DT_DIR){
            struct tm* tmNow = localtime(&stat_info.st_mtim.tv_sec);
            sprintf(file_buff,"%s|%04d-%02d-%02d_%02d:%02d:%02d\n", tmp, tmNow->tm_year+1900,tmNow->tm_mon+1,tmNow->tm_mday,tmNow->tm_hour,tmNow->tm_min,tmNow->tm_sec);
            fputs(file_buff, tree_fp);
            memset(file_buff,0,1024);
            get_dirent_struct(tmp, depth+1);
        }
    }

    closedir(p_dir);
    return;
}
//BD_COUNT 盘大小
int write_dirent_struct_file(char path[])
{
    int free_size = 0;
    tree_fp = fopen(DIRENT_STRUCT_FILE_PATH, "wb+");
	if (!tree_fp) {
		printf("(Error):%s Cannot open file %s\n",__func__,DIRENT_STRUCT_FILE_PATH);
		return 1;
	}

    sprintf(file_buff,"%s\n", path);
    fputs(file_buff, tree_fp);
    memset(file_buff,0,1024);

    get_dirent_struct(path, 0);

    fclose(tree_fp);
    return 0;
}

int trans_dirent_struct_file(int mode,int analysis_type)
{
    int ret,free_size = 0;
    char* buf = NULL;
    tree_fp = fopen(DIRENT_STRUCT_FILE_PATH, "rb+");
	if (!tree_fp) {
		printf("(Error):%s Cannot open file %s\n",__func__,DIRENT_STRUCT_FILE_PATH);
		return 1;
	}
    if(ret = axi_fifo_write_file_buf(tree_fp,mode,analysis_type)){
        printf("(Error):%s trans file message ret:%d\n",__func__,ret);
        return -1;
    }
    
    fclose(tree_fp);
    return 0;
}
// int main(){
//     write_dirent_struct_file(".");
//     return 0;
// }