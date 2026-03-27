# 连点器

[English](README.md) | [中文](README.zh-CN.md)

一个基于 C++ 和原生 Win32 API 实现的轻量化 Windows 鼠标连点器。

项目目标是体积小、启动快、无第三方依赖，适合直接用 Visual Studio 编译运行。

## 功能特性

- 支持毫秒级点击间隔设置
- 支持左键、右键、中键
- 支持单击、双击
- 支持当前鼠标位置或固定 `X / Y` 坐标点击
- 支持无限循环或指定次数
- 支持全局热键
  - `F6` 开始
  - `F7` 停止
- 使用独立工作线程，停止逻辑可靠
- 使用 `SendInput` 模拟鼠标点击

## 项目结构

- `main.cpp`
  - 主窗口、控件创建、消息循环、点击线程、输入校验
- `Clicker.sln`
  - Visual Studio 解决方案
- `Clicker.vcxproj`
  - Visual Studio 工程文件

## 环境要求

- Windows
- Visual Studio
- 已安装 `Desktop development with C++` 工作负载

## 编译方法

### 使用 Visual Studio

1. 打开 `Clicker.sln`
2. 选择 `Release` 或 `Debug`
3. 选择 `x64` 或 `Win32`
4. 按 `Ctrl + Shift + B` 生成解决方案

生成文件默认位于：

- `build\\x64\\Release\\Clicker.exe`
- `build\\Win32\\Release\\Clicker.exe`

### 使用命令行

如果本机可以使用 `MSBuild.exe`，可以在 PowerShell 中执行：

```powershell
cd D:\Person\Clicker
& "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe" Clicker.sln /p:Configuration=Release /p:Platform=x64
```

如果你的 Visual Studio 安装目录不同，请替换为本机实际的 `MSBuild.exe` 路径。

## 使用方法

1. 启动程序
2. 设置点击间隔
3. 选择鼠标按键
4. 选择单击或双击
5. 选择点击位置
   - 当前鼠标位置
   - 固定 `X / Y` 坐标
6. 选择点击次数模式
   - 无限循环
   - 指定次数
7. 点击 `开始` 或按 `F6`
8. 点击 `停止` 或按 `F7`

## 说明

- 状态栏会显示当前运行配置摘要
- 关闭窗口时会安全停止后台点击线程
- 固定坐标模式下，每次点击前会先移动鼠标
- 双击模式会发送两次 `down/up` 序列，并带短间隔保证稳定性

## 许可证

本项目基于 MIT License 开源，详见 [LICENSE](LICENSE)。
