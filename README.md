# Capturer
`Capturer`是使用`Qt`开发的一款**截图**、**录屏**和**录制GIF**软件，支持`Windows`和`Linux`系统，操作非常简单。

## 下载
https://github.com/ffiirree/Capturer/releases

## 快捷键
### 截图
 - `F1` : 截图

### 录屏
 - `Ctrl + Alt + V`第一次，开始选择区域，**回车**后开始录屏
 - `Ctrl + Alt + V`第二次，结束，视频保存在操作系统默认的`视频`文件夹

### 录制GIF
 - `Ctrl + Alt + G`第一次，开始选择区域，**回车**后开始录屏
 - `Ctrl + Alt + G`第一次，结束，GIF保存在操作系统默认的`图片`文件夹

### 选择框通用
 - `W / A / S / D`              : (逐像素)移动选中区域
 - `Ctrl + UP/DOWN/LEFT/RIGHT`  : (逐像素)扩大边界
 - `Shift + UP/DOWN/LEFT/RIGHT` : (逐像素)缩小边界
 - `Ctrl + A`                   : 全屏
 - `ESC`                        : 退出

## 源码使用
### 源代码下载
```
git clone https://github.com/ffiirree/Capturer.git
git submodule init
git submodule update
```
### 安装FFmpeg
#### Windows
从[官网](https://ffmpeg.zeranoe.com/builds/)下载编译好的二进制文件。

#### Ubuntu
```
sudo apt install ffmpeg
```

## 效果
![image](/capturer.gif)