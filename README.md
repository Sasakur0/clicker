# 连点器

一个基于 C++ 和原生 Win32 API 实现的轻量化 Windows 鼠标连点器。

项目目标是体积小、启动快、无第三方依赖，适合直接用 Visual Studio 编译运行。

## 功能特性

- 支持毫秒级点击间隔设置
- 支持左键、右键、中键
- 支持单击、双击
- 支持当前鼠标位置或固定坐标点击
- 支持无限循环或指定次数
- 支持全局热键
  - `F6` 开始
  - `F7` 停止
- 独立点击线程，停止逻辑可靠
- 使用 `SendInput` 模拟鼠标点击

## 项目结构

- `main.cpp`
  - 主窗口、控件创建、消息循环、点击线程、输入校验
- `Clicker.sln`
  - Visual Studio 解决方案
- `Clicker.vcxproj`
  - Visual Studio 工程文件

## 开发环境

- Windows
- Visual Studio
- 安装 `Desktop development with C++`

## 编译方法

### 方法一：使用 Visual Studio

1. 打开 `Clicker.sln`
2. 选择 `Release` 或 `Debug`
3. 平台选择 `x64` 或 `Win32`
4. 点击“生成解决方案”，或按 `Ctrl + Shift + B`

编译成功后，生成文件默认位于：

- `build\\x64\\Release\\Clicker.exe`
- 或 `build\\Win32\\Release\\Clicker.exe`

### 方法二：使用命令行

如果已经安装 Visual Studio，并且本机 `MSBuild.exe` 可用，可以在 PowerShell 中执行：

```powershell
cd D:\Person\Clicker
& "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe" Clicker.sln /p:Configuration=Release /p:Platform=x64
```

如果你机器上的 Visual Studio 安装目录不同，请把上面的 `MSBuild.exe` 路径替换为本机实际路径。

## 使用方法

1. 启动程序
2. 设置点击间隔
3. 选择鼠标按键
4. 选择单击或双击
5. 选择点击位置
   - 当前鼠标位置
   - 固定坐标 `X / Y`
6. 选择点击次数
   - 无限循环
   - 指定次数
7. 点击“开始”或按 `F6`
8. 点击“停止”或按 `F7`

## 说明

- 运行中会显示当前配置摘要
- 关闭窗口时会安全停止后台点击线程
- 固定坐标模式下，点击前会先移动鼠标到指定坐标
- 双击模式会发送两次 `down/up` 事件，并使用短间隔保证稳定性

## Release

已编译版本可在 GitHub Release 页面下载：

- `Clicker.exe`

