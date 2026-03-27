# Clicker

[English](README.md) | [中文](README.zh-CN.md)

A lightweight Windows auto clicker built with C++ and the native Win32 API.

The project is intentionally small, dependency-free, and easy to build directly in Visual Studio.

## Features

- Millisecond-based click interval
- Left, right, and middle mouse button support
- Single-click and double-click modes
- Click at the current cursor position or a fixed `X / Y` coordinate
- Infinite loop or fixed click count
- Global hotkeys
  - `F6` to start
  - `F7` to stop
- Dedicated worker thread with reliable stop handling
- Mouse simulation implemented with `SendInput`

## Project Structure

- `main.cpp`
  - Window creation, controls, message loop, worker thread, and input validation
- `Clicker.sln`
  - Visual Studio solution
- `Clicker.vcxproj`
  - Visual Studio project file

## Requirements

- Windows
- Visual Studio
- `Desktop development with C++` workload installed

## Build

### Visual Studio

1. Open `Clicker.sln`
2. Select `Release` or `Debug`
3. Select `x64` or `Win32`
4. Build the solution with `Ctrl + Shift + B`

Generated binaries are placed under:

- `build\\x64\\Release\\Clicker.exe`
- `build\\Win32\\Release\\Clicker.exe`

### Command Line

If `MSBuild.exe` is available on your machine, build with PowerShell:

```powershell
cd D:\Person\Clicker
& "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe" Clicker.sln /p:Configuration=Release /p:Platform=x64
```

If your Visual Studio installation path is different, replace the `MSBuild.exe` path with the one on your system.

## Usage

1. Launch the application
2. Set the click interval
3. Select the mouse button
4. Select single-click or double-click mode
5. Select the click position
   - Current cursor position
   - Fixed `X / Y` coordinate
6. Select the click count mode
   - Infinite loop
   - Fixed count
7. Click `Start` or press `F6`
8. Click `Stop` or press `F7`

## Notes

- The status bar shows the current running configuration summary
- The worker thread is stopped safely when the window closes
- In fixed-coordinate mode, the cursor is moved before each click
- Double-click mode sends two `down/up` sequences with a short delay for stability

## License

This project is licensed under the MIT License. See [LICENSE](LICENSE).
