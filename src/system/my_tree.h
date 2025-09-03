#ifndef _MY_TREE_H_
#define _MY_TREE_H_

#ifdef __cplusplus
extern "C" {
#endif
// #define DIRENT_STRUCT_FILE_PATH "/userdata/xyl_work/dirent_srtuct_file1.txt"
#define DIRENT_STRUCT_FILE_PATH "dirent_srtuct_file1.txt"
/* #define DIRENT_STRUCT_FILE_BACK_PATH "/run/media/mmcblk0p4/dirent_srtuct_file1_back.txt" */
int write_dirent_struct_file(char path[]);
int trans_dirent_struct_file(int mode,int analysis_type);

#ifdef __cplusplus
}
#endif

#endif /* _UTILS_H_ */