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
`Ctrl + W / A / S / D`  | 逐像素扩大窗口
`Ctrl + ↑ ← ↓ →`        | 逐像素缩小窗口
`ESC`                   | 退出

### 截图

Keys | Actions
:-:|---
`F1`                    | 开始截图
`P`                     | 截图并贴图
`Ctrl + S`              | 截图并保存到文件
`Page Up`               | 上一个截图记录
`Page Down`             | 下一个截图记录
`Ctrl + C`              | 放大镜存在时，取色
`Tab`                   | 放大镜存在时，切换取色颜色格式
`Enter`                 | 截图并保存到粘贴板
`LButton` Double Click  | 截图并保存到粘贴板

### 编辑

Keys | Actions
:-:|---
`Ctrl + Z`              | UNDO
`Ctrl + Shift + Z`      | REDO
`Delete`                | 删除选中的图形
`Shift`                 | 椭圆->圆<br>矩形->正方形<br>直线->水平/垂直
`Space`                 | 重新修改截图区域
`Wheel`                 | 控制马赛克/橡皮擦直径; 放置于菜单上时，控制图形边框宽度

### 贴图

Keys | Actions
:-:|---
`F3`            | 将粘贴板中的内容作为图片贴出<br>(文本内容也会渲染为图片)，如果粘贴板中的路径(路径为文本)为图片，则会贴出该图片
`Shift + F3`    | 显示/隐藏所有贴出的贴图
`Wheel`         | 缩放贴图
`Ctrl + Wheel`  | 更改贴图透明度
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

## 源码使用


```bash
git clone https://github.com/ffiirree/Capturer.git --recursive
```

> 本项目开发使用的`Qt`版本为`Qt 5.12.10`

### Windows 10 & Visual Studio 2019

使用`Visual Studio 2019`直接打开，或者直接使用命令编译

```bash
cd Capturer
mdkir build
cd build
cmake -A x64 .. -DCMAKE_INSTALL_PREFIX=D:\\"Program Files (x86)"\\Capturer
cmake --build . --config Release --target install
```

### Linux (Ubuntu 20.04)

```bash
sudo apt install build-essential ffmpeg gcc g++ cmake qt5-default libqt5x11extras5-dev qttools5-dev qttools5-dev-tools
cd Capturer
mkdir build && cd build
cmake ..
make -j8

# package 'xx.deb'
make package
```

### 安装FFmpeg

#### Windows

从[官网](https://ffmpeg.zeranoe.com/builds/)下载编译好的二进制文件，放到安装或者添加环境变量中。

#### Ubuntu

``` bash
sudo apt install ffmpeg
```

## Todo

### 截图

- [ ] 显示截图历史列表
- [ ] 保存和加载截图历史
- [ ] 截图分组

### 贴图

- [ ] 90度旋转贴图
- [ ] 自动对齐
- [ ] Ctrl 多贴图选中
- [ ] 多贴图移动
- [ ] 多贴图透明度操作
- [ ] 贴图背景，透明/棋盘色

### 编辑

- [ ] 截图和贴图编辑统一
- [ ] 文本通过拖动文本框调整大小
- [ ] 文本框旋转
- [ ] 滤镜功能
- [ ] 复制粘贴元素
- [ ] 鼠标中键控制颜色透明度
- [ ] 提供推荐颜色组，更好看
- [ ] 记忆历史使用的颜色，支持自定义固定颜色

### 录屏

- [ ] 录制摄像头
- [ ] 录制扬声器
- [ ] 添加水印
