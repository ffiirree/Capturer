<p align="center">
    <img src="src/resources/icons/capturer.png"  height="96">
</p>

<h1 align="center">Capturer</h1>

<p>
    <p align="center">
        <code>Capturer</code>是一款主打快速<code>截图</code>、<code>贴图</code>、<code>视频预览</code>、<code>录屏</code>和<code>录制GIF</code>的软件
    </p>
    <p align="center">
        使用快捷键可以快速选择感兴趣的<code>桌面区域</code>或<code>窗口</code>进行截图/录制，并将截图快速<code>钉在桌面</code>上
    </p>
    <p align="center">
        <img src="capturer.png">
    </p>
</p>

## 下载 & 安装

- `>= Windows 10 2004`
- `>= Ubuntu 20.04`

> Not support Wayland on Linux

从 [Releases](https://github.com/ffiirree/Capturer/releases) 下载系统对应的版本。

### Windows

下载后解压，双击 `capturer.exe` 运行。

### Ubuntu

下载后安装：

```bash
sudo dpkg -i capturer-1.7.0-ubuntu_xx.04-amd64.deb
sudo apt install -f
```

## 快捷键

> 可自定义

|       Keys       | Actions                                       |
| :--------------: | --------------------------------------------- |
|       `F1`       | 截图                                          |
|       `F3`       | 预览剪切板内容并置顶在桌面上                  |
|    `Shift+F3`    | 显示/隐藏所有预览窗口                         |
|       `F2`       | 快速预览文件资源管理器中选中的项目(仅Windows) |
| `Ctrl + Alt + V` | 录制视频，选择区域，`Enter`开始录制           |
| `Ctrl + Alt + G` | 录制GIF，选择区域，`Enter`开始录制            |

> 文件保存在系统默认的图片/视频文件夹中

### 选择框通用快捷键

|             Keys             | Actions        |
| :--------------------------: | -------------- |
|          `Ctrl + A`          | 全屏           |
| `W / A / S / D` or `↑ ← ↓ →` | 逐像素移动窗口 |
|       `Ctrl + ↑ ← ↓ →`       | 逐像素扩大窗口 |
|      `Shift + ↑ ← ↓ →`       | 逐像素缩小窗口 |
|          `RButton`           | 重新选择/退出  |
|            `ESC`             | 退出           |

### 截图

|          Keys          | Actions                          |
| :--------------------: | -------------------------------- |
|          `F1`          | 开始截图                         |
|          `P`           | 截图并贴图                       |
|       `Ctrl + S`       | 截图并保存到文件                 |
|        `Enter`         | 截图并保存到粘贴板               |
| `LButton` Double Click | 截图并保存到粘贴板               |
|       `Page Up`        | 上一次的截图位置                 |
|      `Page Down`       | 下一次的截图位置                 |
|       `Ctrl + C`       | 放大镜存在时，复制取色器中的颜色 |
|         `Tab`          | 放大镜存在时，切换取色颜色格式   |

### 编辑

|         Keys          | Actions                                                    |
| :-------------------: | ---------------------------------------------------------- |
|      `Ctrl + Z`       | UNDO                                                       |
|  `Ctrl + Shift + Z`   | REDO                                                       |
| `Ctrl + C`/`Ctrl + V` | Copy & Paste                                               |
|       `Delete`        | 删除选中的图形                                             |
|        `Shift`        | 椭圆->圆<br>矩形->正方形<br>直线->水平/垂直                |
|        `Space`        | 重新调整截图区域                                           |
|        `Wheel`        | 控制马赛克/橡皮擦直径 <br>放置于菜单上时，控制图形线条宽度 |

### 贴图

|             Keys              | Actions                                   |
| :---------------------------: | ----------------------------------------- |
|          `Ctrl + C`           | 复制贴图                                  |
|          `Ctrl + V`           | 粘贴图片                                  |
|            `Wheel`            | 缩放贴图                                  |
|        `Ctrl + Wheel`         | 调整贴图透明度                            |
|              `I`              | 反转颜色                                  |
|              `G`              | 灰阶显示                                  |
|              `R`              | 顺时针旋转90                              |
|          `Ctrl + R`           | 逆时针旋转90                              |
|              `V`              | 垂直翻转                                  |
|              `H`              | 水平翻转                                  |
|          `Ctrl + T`           | 开启/关闭鼠标坐标下窗口的鼠标穿透         |
|    `LButton` Double Click     | 缩略图模式，贴图显示中心区域125x125的内容 |
|          Drag & Drop          | 拖拽图片到贴图上，则打开并显示拖拽图片    |
|             `ESC`             | 关闭贴图窗口                              |
| `W / A / S / D` or  `↑ ← ↓ →` | 逐像素移动窗口                            |
|           `RButton`           | 菜单                                      |

### 录屏

|       Keys       | Actions                                                |
| :--------------: | ------------------------------------------------------ |
| `Ctrl + Alt + V` | 第一次，开始选择区域                                   |
|     `Enter`      | 开始录制                                               |
| `Ctrl + Alt + V` | 第二次，结束 <br> 视频保存在操作系统默认的`视频`文件夹 |

### 录制GIF

|       Keys       | Actions                                               |
| :--------------: | ----------------------------------------------------- |
| `Ctrl + Alt + G` | 第一次，开始选择区域                                  |
|     `Enter`      | 开始录制                                              |
| `Ctrl + Alt + G` | 第二次，结束 <br> GIF保存在操作系统默认的`图片`文件夹 |

## 录制

录制GIF和视频均支持三种录制模式：`自由区域`、`窗口`和`显示器`。
- `自由区域`：使用鼠标左键选择录制的区域
- `窗口模式`：移动鼠标自动选中窗口区域时为该模式(提示信息中含有窗口名字)
  - 该模式下仅会录制该窗口的内容，不会录制到其他窗口以及二级窗口内容(即使窗口有重叠)
  - 录制中可以自由移动录制窗口位置
  - 录制中可以自由调整录制窗口尺寸
- `显示器模式`：选择显示器或者`CTRL+A`选择全屏时为该模式(提示信息中含有DISPLAY或对应显示器名称)

## From Source

```bash
git clone https://github.com/ffiirree/Capturer.git --recursive

# update submodules
git submodule update --init --recursive
```

### Development Environment

- `C++` : `C++20`
- `Qt` : `>= 6.7`
- `CMake` : `>= 3.27`
- `FFmpeg` : `>= 7.0`
- Windows
  - `Windows` : `>= Windows 10 2004`
  - `Windows SDK` : `>= 10.0.22621.0`
- Linux
  - `Window System` : `X11`

#### Video & Audio Capture Sources & Methods

|        | Windows                        | Linux      |
| :----: | ------------------------------ | ---------- |
| Screen | Windows Graphics Capture       | XCB-XSHM   |
| Audio  | Windows Audio Session API      | PulseAudio |
| Camera | Media Foundation Video Capture | V4L2       |

### Dependencies

#### Windows

- `FFmpeg`: 从[官网](https://ffmpeg.org/download.html#build-windows)下载编译好的`库版本(ffmpeg-xxxxx-shared.7z)`并添加到环境变量中
- `libass`: 从[ShiftMediaProject/libass](https://github.com/ShiftMediaProject/libass/releases)下载并添加到环境变量中

#### Linux (Ubuntu)

```bash
sudo apt install build-essential cmake

sudo apt install libglx-dev libgl1-mesa-dev

# ffmpeg: video / audio
sudo apt install ffmpeg libavcodec-dev libavformat-dev libavutil-dev libavdevice-dev libswscale-dev libavfilter-dev

# pulse: audio
sudo apt install libpulse-dev

# v4l2: camera
sudo apt install libv4l-dev v4l-utils

# x11 & xcb
sudo apt install libx11-dev libxrandr-dev libxcb1-dev libxcb-shm0-dev libxcb-xfixes0-dev libxcb-cursor0 libxkbcommon-dev 
```

### Build

```bash
cd Capturer && mkdir build

cmake -S. -Bbuild -DCMAKE_BUILD_TYPE=Release
cmake --build build --target install --config Release
```

## References

- [ffmpeg_examples](https://github.com/ffiirree/ffmpeg_examples): `FFmpeg`的基础用例
