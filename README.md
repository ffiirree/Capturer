# Capturer

![image](/capturer.png)


`Capturer`是使用`Qt`开发的一款**截图**、**录屏**和**录制GIF**软件，支持`Windows`和`Linux`系统。
> `录屏`和`录制GIF`依赖于`FFmpeg`，请[安装FFmpeg](#安装FFmpeg).

## 快捷键

### 选择框通用快捷键

Keys | Actions
:-:|---
`Ctrl + A`              | 全屏
`W / A / S / D`         | 逐像素移动窗口
`↑ ← ↓ →`               | 逐像素移动窗口
`Ctrl + ↑ ← ↓ →`        | 逐像素扩大窗口
`Shift + ↑ ← ↓ →`       | 逐像素缩小窗口
`ESC`                   | 退出

### 截图

Keys | Actions
:-:|---
`F1(默认，可修改)`       | 开始截图
`P`                     | 截图并贴图
`Ctrl + S`              | 截图并保存到文件
`Page Up`               | 上一个截图位置
`Page Down`             | 下一个截图位置
`Ctrl + C`              | 放大镜存在时，取色
`Tab`                   | 放大镜存在时，切换取色颜色格式
`Enter`                 | 截图并保存到粘贴板
`LButton` Double Click  | 截图并保存到粘贴板

### 编辑

Keys | Actions
:-:|---
`Ctrl + Z`              | UNDO
`Ctrl + Shift + Z`      | REDO
`Ctrl + C`/`Ctrl + V`   | Copy & Paste
`Delete`                | 删除选中的图形
`Shift`                 | 椭圆->圆<br>矩形->正方形<br>直线->水平/垂直
`Space`                 | 重新调整截图区域
`Wheel`                 | 控制马赛克/橡皮擦直径 <br>放置于菜单上时，控制图形线条宽度

### 贴图

Keys | Actions
:-:|---
`F3`            | 将粘贴板中的内容作为图片贴出<br>(文本内容也会渲染为图片)，如果粘贴板中的路径(路径为文本)为图片，则会贴出该图片
`Shift + F3`    | 显示/隐藏所有贴出的贴图
`Wheel`         | 缩放贴图
`Ctrl + Wheel`  | 调整贴图透明度
`G`             | 灰阶显示
`R`             | 顺时针旋转90
`Ctrl + R`      | 逆时针旋转90
`V`             | 垂直翻转
`H`             | 水平翻转
`LButton` Double Click   | 缩略图模式，贴图显示中心区域125x125的内容
Drag & Drop     | 拖拽图片到贴图上，则打开并显示拖拽图片
`ESC`           | 关闭贴图窗口
`W / A / S / D` | 逐像素移动窗口
`↑ ← ↓ →`       | 逐像素移动窗口
`LButton`       | 菜单

### 录屏

Keys | Actions
:-:|---
`Ctrl + Alt + V`    | 第一次，开始选择区域
`Enter`             | 开始录制
`Ctrl + Alt + V`    | 第二次，结束 <br> 视频保存在操作系统默认的`视频`文件夹

### 录制GIF

Keys | Actions
:-:|---
`Ctrl + Alt + G`    | 第一次，开始选择区域
`Enter`             | 开始录制
`Ctrl + Alt + G`    | 第二次，结束 <br> GIF保存在操作系统默认的`图片`文件夹

## From Source

```bash
git clone https://github.com/ffiirree/Capturer.git --recursive

# update submodules
git submodule update --init --recursive
```

> 本项目开发使用的`Qt`版本为`Qt 5.12.12`, `FFmpeg`版本为4.4.1

### Windows

#### Install FFmpeg

从[官网](https://ffmpeg.org/download.html#build-windows)下载编译好的`库版本(ffmpeg-xxxxx-shared.7z)`，添加根目录和bin目录到环境变量中。

#### 编译

使用`Visual Studio 2022`打开(CMake工程)编译 或 直接使用命令编译

```bash
cd Capturer
mdkir build
cd build
cmake -A x64 .. -DCMAKE_INSTALL_PREFIX=D:\\"Program Files (x86)"\\Capturer
cmake --build . --config Release --target install
```

### Linux (Ubuntu 20.04)

```bash
sudo apt install build-essential gcc g++ cmake 
sudo apt install ffmpeg libavcodec-dev libavformat-dev libavutil-dev libavdevice-dev libswscale-dev libavfilter-dev

# Ubuntu 20.04 
sudo apt install qt5-default  
# Ubuntu 22.04
sudo apt install qtbase5-dev

sudo apt install libqt5x11extras5-dev qttools5-dev qttools5-dev-tools

# pulse
sudo apt install libpulse-dev

# v4l2
sudo apt install libv4l-dev v4l-utils

cd Capturer
mkdir build && cd build
cmake ..
make -j8

# package 'xx.deb'
make package
```

## FFmpeg代码示例

可以参考 [ffmpeg_examples](https://github.com/ffiirree/ffmpeg_examples)，有不少`FFmpeg`的基础用法。