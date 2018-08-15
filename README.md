# Capturer
`Capturer`是使用`Qt`开发的一款**截图**、**录屏**和**录制GIF**软件，支持`Windows`和`Linux`系统，特点是操作简单。

## 下载
https://github.com/ffiirree/Capturer/releases

## 快捷键
### 选择框通用
 - `W / A / S / D`              : (逐像素)移动选中区域
 - `Ctrl + UP/DOWN/LEFT/RIGHT`  : (逐像素)扩大边界
 - `Shift + UP/DOWN/LEFT/RIGHT` : (逐像素)缩小边界
 - `Ctrl + A`                   : 全屏
 - `ESC`                        : 退出

### 截图
 - `F1` : 截图

### 录屏
 - `Ctrl+Alt+V`第一次，开始选择区域，回车后开始录屏
 - `Ctrl+Alt+V`第二次，结束，视频保存在`视频`文件夹

### 录制GIF
 - `Ctrl+Alt+G`第一次，开始选择区域，回车后开始录屏
 - `Ctrl+Alt+G`第一次，结束，GIF保存在`图片`文件夹

## 源码使用
```
git clone https://github.com/ffiirree/Capturer.git
git submodule init
git submodule update
```
在Qt Creator中打开

> 注: 录屏和录制gif依赖于`ffmpeg`

## 效果
![image](https://github.com/ffiirree/Capturer/blob/master/capturer.gif)