#ifndef SDCARD_H
#define SDCARD_H

// 声明文件路径数组结构体
typedef struct FilePathArray {
    char file_paths[100][300];
    int file_count;
} FilePathArray;

// 声明全局文件路径数组
extern FilePathArray file_array;

// 声明初始化SD卡的函数
void init_sd_card(void);

bool sdcard_is_inited();

#endif // SDCARD_H
