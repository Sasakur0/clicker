#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdlib>
#include <mutex>
#include <string>
#include <thread>

namespace {

constexpr wchar_t kWindowClassName[] = L"LightweightAutoClickerWindow";
constexpr wchar_t kWindowTitle[] = L"\u8FDE\u70B9\u5668";
constexpr UINT kMsgWorkerFinished = WM_APP + 1;

constexpr int kHotkeyStartId = 1;
constexpr int kHotkeyStopId = 2;

enum ControlId {
    IDC_EDIT_INTERVAL = 1001,
    IDC_COMBO_BUTTON,
    IDC_RADIO_SINGLE,
    IDC_RADIO_DOUBLE,
    IDC_RADIO_POS_CURRENT,
    IDC_RADIO_POS_FIXED,
    IDC_EDIT_X,
    IDC_EDIT_Y,
    IDC_RADIO_COUNT_INFINITE,
    IDC_RADIO_COUNT_LIMITED,
    IDC_EDIT_COUNT,
    IDC_BUTTON_START,
    IDC_BUTTON_STOP,
    IDC_STATIC_STATUS,
    IDC_STATIC_X,
    IDC_STATIC_Y,
};

enum class MouseButtonType {
    Left,
    Right,
    Middle,
};

enum class ClickMode {
    Single,
    Double,
};

enum class PositionMode {
    Current,
    Fixed,
};

struct ClickConfig {
    int intervalMs = 100;
    MouseButtonType button = MouseButtonType::Left;
    ClickMode clickMode = ClickMode::Single;
    PositionMode positionMode = PositionMode::Current;
    int fixedX = 0;
    int fixedY = 0;
    bool infinite = true;
    int clickCount = 0;
};

class ClickWorker {
public:
    ClickWorker() : stopEvent_(CreateEventW(nullptr, TRUE, FALSE, nullptr)) {}

    ~ClickWorker() {
        Stop(true);
        if (stopEvent_ != nullptr) {
            CloseHandle(stopEvent_);
            stopEvent_ = nullptr;
        }
    }

    bool Start(const ClickConfig& config, HWND notifyWindow) {
        JoinFinishedThread();
        if (stopEvent_ == nullptr || running_.load()) {
            return false;
        }

        ResetEvent(stopEvent_);
        stopRequested_.store(false);
        running_.store(true);

        try {
            std::lock_guard<std::mutex> lock(threadMutex_);
            thread_ = std::thread(&ClickWorker::ThreadMain, this, config, notifyWindow);
        } catch (...) {
            running_.store(false);
            return false;
        }

        return true;
    }

    void Stop(bool wait) {
        stopRequested_.store(true);
        if (stopEvent_ != nullptr) {
            SetEvent(stopEvent_);
        }

        if (!wait) {
            return;
        }

        std::thread localThread;
        {
            std::lock_guard<std::mutex> lock(threadMutex_);
            if (thread_.joinable()) {
                localThread = std::move(thread_);
            }
        }

        if (localThread.joinable()) {
            localThread.join();
        }

        running_.store(false);
    }

    void JoinFinishedThread() {
        if (running_.load()) {
            return;
        }

        std::thread localThread;
        {
            std::lock_guard<std::mutex> lock(threadMutex_);
            if (thread_.joinable()) {
                localThread = std::move(thread_);
            }
        }

        if (localThread.joinable()) {
            localThread.join();
        }
    }

    bool IsRunning() const {
        return running_.load();
    }

private:
    void ThreadMain(ClickConfig config, HWND notifyWindow) {
        int remaining = config.clickCount;

        while (!stopRequested_.load()) {
            if (!config.infinite && remaining <= 0) {
                break;
            }

            PerformClick(config);

            if (!config.infinite) {
                --remaining;
                if (remaining <= 0) {
                    break;
                }
            }

            if (WaitOrStop(static_cast<DWORD>(std::max(config.intervalMs, 1)))) {
                break;
            }
        }

        running_.store(false);
        if (notifyWindow != nullptr) {
            PostMessageW(notifyWindow, kMsgWorkerFinished, 0, 0);
        }
    }

    bool WaitOrStop(DWORD waitMs) const {
        return WaitForSingleObject(stopEvent_, waitMs) == WAIT_OBJECT_0;
    }

    void PerformClick(const ClickConfig& config) const {
        if (config.positionMode == PositionMode::Fixed) {
            SetCursorPos(config.fixedX, config.fixedY);
        } else {
            POINT pt{};
            GetCursorPos(&pt);
        }

        SendSingleClick(config.button);

        if (config.clickMode == ClickMode::Double && !stopRequested_.load()) {
            UINT gap = GetDoubleClickTime() / 4;
            gap = std::clamp<UINT>(gap, 40U, 120U);
            if (!WaitOrStop(gap)) {
                SendSingleClick(config.button);
            }
        }
    }

    void SendSingleClick(MouseButtonType button) const {
        DWORD downFlag = MOUSEEVENTF_LEFTDOWN;
        DWORD upFlag = MOUSEEVENTF_LEFTUP;

        switch (button) {
        case MouseButtonType::Left:
            break;
        case MouseButtonType::Right:
            downFlag = MOUSEEVENTF_RIGHTDOWN;
            upFlag = MOUSEEVENTF_RIGHTUP;
            break;
        case MouseButtonType::Middle:
            downFlag = MOUSEEVENTF_MIDDLEDOWN;
            upFlag = MOUSEEVENTF_MIDDLEUP;
            break;
        }

        INPUT inputs[2]{};
        inputs[0].type = INPUT_MOUSE;
        inputs[0].mi.dwFlags = downFlag;
        inputs[1].type = INPUT_MOUSE;
        inputs[1].mi.dwFlags = upFlag;
        SendInput(2, inputs, sizeof(INPUT));
    }

    HANDLE stopEvent_ = nullptr;
    std::thread thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stopRequested_{false};
    std::mutex threadMutex_;
};

struct AppState {
    HINSTANCE instance = nullptr;
    HWND hwnd = nullptr;
    HFONT uiFont = nullptr;

    HWND editInterval = nullptr;
    HWND comboButton = nullptr;
    HWND radioSingle = nullptr;
    HWND radioDouble = nullptr;
    HWND radioPosCurrent = nullptr;
    HWND radioPosFixed = nullptr;
    HWND editX = nullptr;
    HWND editY = nullptr;
    HWND labelX = nullptr;
    HWND labelY = nullptr;
    HWND radioInfinite = nullptr;
    HWND radioLimited = nullptr;
    HWND editCount = nullptr;
    HWND buttonStart = nullptr;
    HWND buttonStop = nullptr;
    HWND labelStatus = nullptr;

    ClickWorker worker;
};

AppState g_app;

std::wstring GetWindowTextString(HWND hwnd) {
    const int length = GetWindowTextLengthW(hwnd);
    if (length <= 0) {
        return {};
    }

    std::wstring text(static_cast<size_t>(length) + 1, L'\0');
    GetWindowTextW(hwnd, text.data(), length + 1);
    text.resize(static_cast<size_t>(length));
    return text;
}

void SetDefaultFont(HWND hwnd) {
    if (g_app.uiFont == nullptr) {
        g_app.uiFont = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    }

    SendMessageW(hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.uiFont), TRUE);
}

HWND CreateControl(
    DWORD exStyle,
    const wchar_t* className,
    const wchar_t* text,
    DWORD style,
    int x,
    int y,
    int width,
    int height,
    HWND parent,
    int id) {
    HWND hwnd = CreateWindowExW(
        exStyle,
        className,
        text,
        style,
        x,
        y,
        width,
        height,
        parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        g_app.instance,
        nullptr);

    if (hwnd != nullptr) {
        SetDefaultFont(hwnd);
    }

    return hwnd;
}

void ShowValidationError(HWND owner, const wchar_t* message, HWND focus = nullptr) {
    MessageBoxW(owner, message, L"\u8F93\u5165\u9519\u8BEF", MB_OK | MB_ICONWARNING);
    if (focus != nullptr) {
        SetFocus(focus);
        SendMessageW(focus, EM_SETSEL, 0, -1);
    }
}

bool TryParseIntStrict(const std::wstring& text, int& value) {
    if (text.empty()) {
        return false;
    }

    errno = 0;
    wchar_t* end = nullptr;
    const long parsed = wcstol(text.c_str(), &end, 10);
    if (errno == ERANGE || end == text.c_str() || *end != L'\0') {
        return false;
    }

    value = static_cast<int>(parsed);
    return true;
}

bool ParseIntFromEdit(
    HWND owner,
    HWND edit,
    const wchar_t* emptyMessage,
    const wchar_t* invalidMessage,
    int minValue,
    int maxValue,
    int& result) {
    const std::wstring text = GetWindowTextString(edit);
    if (text.empty()) {
        ShowValidationError(owner, emptyMessage, edit);
        return false;
    }

    if (!TryParseIntStrict(text, result) || result < minValue || result > maxValue) {
        ShowValidationError(owner, invalidMessage, edit);
        return false;
    }

    return true;
}

std::wstring ButtonToString(MouseButtonType button) {
    switch (button) {
    case MouseButtonType::Left:
        return L"\u5DE6\u952E";
    case MouseButtonType::Right:
        return L"\u53F3\u952E";
    case MouseButtonType::Middle:
        return L"\u4E2D\u952E";
    }

    return L"\u5DE6\u952E";
}

std::wstring ClickModeToString(ClickMode mode) {
    return mode == ClickMode::Double ? L"\u53CC\u51FB" : L"\u5355\u51FB";
}

std::wstring PositionModeToString(const ClickConfig& config) {
    if (config.positionMode == PositionMode::Current) {
        return L"\u5F53\u524D\u4F4D\u7F6E";
    }

    return L"\u56FA\u5B9A\u5750\u6807(" + std::to_wstring(config.fixedX) + L", " +
           std::to_wstring(config.fixedY) + L")";
}

std::wstring CountModeToString(const ClickConfig& config) {
    if (config.infinite) {
        return L"\u65E0\u9650\u5FAA\u73AF";
    }

    return L"\u6307\u5B9A\u6B21\u6570 " + std::to_wstring(config.clickCount);
}

std::wstring BuildConfigSummary(const ClickConfig& config) {
    return L"\u95F4\u9694 " + std::to_wstring(config.intervalMs) + L" ms | " +
           L"\u6309\u952E " + ButtonToString(config.button) + L" | " +
           L"\u6A21\u5F0F " + ClickModeToString(config.clickMode) + L" | " +
           L"\u4F4D\u7F6E " + PositionModeToString(config) + L" | " +
           L"\u6B21\u6570 " + CountModeToString(config);
}

void SetStatusIdle() {
    SetWindowTextW(g_app.labelStatus, L"\u72B6\u6001\uFF1A\u7A7A\u95F2\u4E2D");
}

void SetStatusRunning(const ClickConfig& config) {
    std::wstring text = L"\u72B6\u6001\uFF1A\u8FD0\u884C\u4E2D | ";
    text += BuildConfigSummary(config);
    SetWindowTextW(g_app.labelStatus, text.c_str());
}

bool ReadConfigFromUi(HWND owner, ClickConfig& config) {
    if (!ParseIntFromEdit(
            owner,
            g_app.editInterval,
            L"\u8BF7\u8F93\u5165\u70B9\u51FB\u95F4\u9694\uFF08\u6BEB\u79D2\uFF09\u3002",
            L"\u70B9\u51FB\u95F4\u9694\u5FC5\u987B\u662F 1 \u5230 3600000 \u4E4B\u95F4\u7684\u6574\u6570\u3002",
            1,
            3600000,
            config.intervalMs)) {
        return false;
    }

    const LRESULT buttonIndex = SendMessageW(g_app.comboButton, CB_GETCURSEL, 0, 0);
    switch (buttonIndex) {
    case 0:
        config.button = MouseButtonType::Left;
        break;
    case 1:
        config.button = MouseButtonType::Right;
        break;
    case 2:
        config.button = MouseButtonType::Middle;
        break;
    default:
        ShowValidationError(owner, L"\u8BF7\u9009\u62E9\u9F20\u6807\u6309\u952E\u7C7B\u578B\u3002", g_app.comboButton);
        return false;
    }

    config.clickMode = SendMessageW(g_app.radioDouble, BM_GETCHECK, 0, 0) == BST_CHECKED
        ? ClickMode::Double
        : ClickMode::Single;

    config.positionMode = SendMessageW(g_app.radioPosFixed, BM_GETCHECK, 0, 0) == BST_CHECKED
        ? PositionMode::Fixed
        : PositionMode::Current;

    if (config.positionMode == PositionMode::Fixed) {
        const int minX = GetSystemMetrics(SM_XVIRTUALSCREEN);
        const int minY = GetSystemMetrics(SM_YVIRTUALSCREEN);
        const int maxX = minX + GetSystemMetrics(SM_CXVIRTUALSCREEN) - 1;
        const int maxY = minY + GetSystemMetrics(SM_CYVIRTUALSCREEN) - 1;

        if (!ParseIntFromEdit(
                owner,
                g_app.editX,
                L"\u8BF7\u8F93\u5165\u56FA\u5B9A\u5750\u6807 X\u3002",
                L"X \u5750\u6807\u5FC5\u987B\u5728\u865A\u62DF\u5C4F\u5E55\u8303\u56F4\u5185\u3002",
                minX,
                maxX,
                config.fixedX)) {
            return false;
        }

        if (!ParseIntFromEdit(
                owner,
                g_app.editY,
                L"\u8BF7\u8F93\u5165\u56FA\u5B9A\u5750\u6807 Y\u3002",
                L"Y \u5750\u6807\u5FC5\u987B\u5728\u865A\u62DF\u5C4F\u5E55\u8303\u56F4\u5185\u3002",
                minY,
                maxY,
                config.fixedY)) {
            return false;
        }
    }

    config.infinite = SendMessageW(g_app.radioInfinite, BM_GETCHECK, 0, 0) == BST_CHECKED;
    if (!config.infinite) {
        if (!ParseIntFromEdit(
                owner,
                g_app.editCount,
                L"\u8BF7\u8F93\u5165\u70B9\u51FB\u6B21\u6570\u3002",
                L"\u70B9\u51FB\u6B21\u6570\u5FC5\u987B\u662F 1 \u5230 100000000 \u4E4B\u95F4\u7684\u6574\u6570\u3002",
                1,
                100000000,
                config.clickCount)) {
            return false;
        }
    }

    return true;
}

void UpdateControlState() {
    const bool running = g_app.worker.IsRunning();
    const bool fixedPosition = SendMessageW(g_app.radioPosFixed, BM_GETCHECK, 0, 0) == BST_CHECKED;
    const bool limitedCount = SendMessageW(g_app.radioLimited, BM_GETCHECK, 0, 0) == BST_CHECKED;
    const BOOL enableConfig = running ? FALSE : TRUE;

    EnableWindow(g_app.editInterval, enableConfig);
    EnableWindow(g_app.comboButton, enableConfig);
    EnableWindow(g_app.radioSingle, enableConfig);
    EnableWindow(g_app.radioDouble, enableConfig);
    EnableWindow(g_app.radioPosCurrent, enableConfig);
    EnableWindow(g_app.radioPosFixed, enableConfig);
    EnableWindow(g_app.radioInfinite, enableConfig);
    EnableWindow(g_app.radioLimited, enableConfig);

    EnableWindow(g_app.editX, enableConfig && fixedPosition);
    EnableWindow(g_app.editY, enableConfig && fixedPosition);
    EnableWindow(g_app.labelX, enableConfig && fixedPosition);
    EnableWindow(g_app.labelY, enableConfig && fixedPosition);
    EnableWindow(g_app.editCount, enableConfig && limitedCount);

    EnableWindow(g_app.buttonStart, running ? FALSE : TRUE);
    EnableWindow(g_app.buttonStop, running ? TRUE : FALSE);
}

void StopClicking() {
    g_app.worker.Stop(true);
    g_app.worker.JoinFinishedThread();
    SetStatusIdle();
    UpdateControlState();
}

void StartClicking(HWND owner) {
    if (g_app.worker.IsRunning()) {
        return;
    }

    ClickConfig config;
    if (!ReadConfigFromUi(owner, config)) {
        return;
    }

    if (!g_app.worker.Start(config, owner)) {
        MessageBoxW(owner, L"\u542F\u52A8\u70B9\u51FB\u7EBF\u7A0B\u5931\u8D25\u3002", L"\u9519\u8BEF", MB_OK | MB_ICONERROR);
        return;
    }

    SetStatusRunning(config);
    UpdateControlState();
}

void RegisterHotkeys(HWND hwnd) {
    if (!RegisterHotKey(hwnd, kHotkeyStartId, 0, VK_F6)) {
        MessageBoxW(hwnd, L"\u6CE8\u518C\u5168\u5C40\u70ED\u952E F6 \u5931\u8D25\u3002", L"\u8B66\u544A", MB_OK | MB_ICONWARNING);
    }

    if (!RegisterHotKey(hwnd, kHotkeyStopId, 0, VK_F7)) {
        MessageBoxW(hwnd, L"\u6CE8\u518C\u5168\u5C40\u70ED\u952E F7 \u5931\u8D25\u3002", L"\u8B66\u544A", MB_OK | MB_ICONWARNING);
    }
}

void CreateMainControls(HWND hwnd) {
    CreateControl(0, L"STATIC", L"\u95F4\u9694\uFF08\u6BEB\u79D2\uFF09\uFF1A", WS_CHILD | WS_VISIBLE, 16, 18, 100, 22, hwnd, -1);
    g_app.editInterval = CreateControl(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        L"100",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | ES_NUMBER,
        122,
        16,
        120,
        24,
        hwnd,
        IDC_EDIT_INTERVAL);

    CreateControl(0, L"STATIC", L"\u9F20\u6807\u6309\u952E\uFF1A", WS_CHILD | WS_VISIBLE, 16, 54, 100, 22, hwnd, -1);
    g_app.comboButton = CreateControl(
        0,
        L"COMBOBOX",
        L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
        122,
        50,
        120,
        200,
        hwnd,
        IDC_COMBO_BUTTON);
    SendMessageW(g_app.comboButton, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"\u5DE6\u952E"));
    SendMessageW(g_app.comboButton, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"\u53F3\u952E"));
    SendMessageW(g_app.comboButton, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"\u4E2D\u952E"));
    SendMessageW(g_app.comboButton, CB_SETCURSEL, 0, 0);

    CreateControl(0, L"STATIC", L"\u70B9\u51FB\u6A21\u5F0F\uFF1A", WS_CHILD | WS_VISIBLE, 16, 90, 100, 22, hwnd, -1);
    g_app.radioSingle = CreateControl(
        0,
        L"BUTTON",
        L"\u5355\u51FB",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTORADIOBUTTON | WS_GROUP,
        122,
        88,
        100,
        22,
        hwnd,
        IDC_RADIO_SINGLE);
    g_app.radioDouble = CreateControl(
        0,
        L"BUTTON",
        L"\u53CC\u51FB",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTORADIOBUTTON,
        232,
        88,
        100,
        22,
        hwnd,
        IDC_RADIO_DOUBLE);
    SendMessageW(g_app.radioSingle, BM_SETCHECK, BST_CHECKED, 0);

    CreateControl(0, L"STATIC", L"\u70B9\u51FB\u4F4D\u7F6E\uFF1A", WS_CHILD | WS_VISIBLE, 16, 126, 100, 22, hwnd, -1);
    g_app.radioPosCurrent = CreateControl(
        0,
        L"BUTTON",
        L"\u5F53\u524D\u9F20\u6807\u4F4D\u7F6E",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTORADIOBUTTON | WS_GROUP,
        122,
        124,
        120,
        22,
        hwnd,
        IDC_RADIO_POS_CURRENT);
    g_app.radioPosFixed = CreateControl(
        0,
        L"BUTTON",
        L"\u56FA\u5B9A\u5750\u6807",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTORADIOBUTTON,
        250,
        124,
        130,
        22,
        hwnd,
        IDC_RADIO_POS_FIXED);
    SendMessageW(g_app.radioPosCurrent, BM_SETCHECK, BST_CHECKED, 0);

    g_app.labelX = CreateControl(0, L"STATIC", L"X:", WS_CHILD | WS_VISIBLE, 138, 156, 18, 22, hwnd, IDC_STATIC_X);
    g_app.editX = CreateControl(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        L"0",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
        160,
        154,
        74,
        24,
        hwnd,
        IDC_EDIT_X);

    g_app.labelY = CreateControl(0, L"STATIC", L"Y:", WS_CHILD | WS_VISIBLE, 248, 156, 18, 22, hwnd, IDC_STATIC_Y);
    g_app.editY = CreateControl(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        L"0",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
        270,
        154,
        74,
        24,
        hwnd,
        IDC_EDIT_Y);

    CreateControl(0, L"STATIC", L"\u70B9\u51FB\u6B21\u6570\uFF1A", WS_CHILD | WS_VISIBLE, 16, 194, 100, 22, hwnd, -1);
    g_app.radioInfinite = CreateControl(
        0,
        L"BUTTON",
        L"\u65E0\u9650\u5FAA\u73AF",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTORADIOBUTTON | WS_GROUP,
        122,
        192,
        110,
        22,
        hwnd,
        IDC_RADIO_COUNT_INFINITE);
    g_app.radioLimited = CreateControl(
        0,
        L"BUTTON",
        L"\u6307\u5B9A\u6B21\u6570",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTORADIOBUTTON,
        240,
        192,
        120,
        22,
        hwnd,
        IDC_RADIO_COUNT_LIMITED);
    SendMessageW(g_app.radioInfinite, BM_SETCHECK, BST_CHECKED, 0);

    g_app.editCount = CreateControl(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        L"10",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | ES_NUMBER,
        362,
        190,
        58,
        24,
        hwnd,
        IDC_EDIT_COUNT);

    g_app.buttonStart = CreateControl(
        0,
        L"BUTTON",
        L"\u5F00\u59CB\uFF08F6\uFF09",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        92,
        236,
        110,
        30,
        hwnd,
        IDC_BUTTON_START);
    g_app.buttonStop = CreateControl(
        0,
        L"BUTTON",
        L"\u505C\u6B62\uFF08F7\uFF09",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        220,
        236,
        110,
        30,
        hwnd,
        IDC_BUTTON_STOP);

    g_app.labelStatus = CreateControl(
        WS_EX_CLIENTEDGE,
        L"STATIC",
        L"\u72B6\u6001\uFF1A\u7A7A\u95F2\u4E2D",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        16,
        284,
        404,
        34,
        hwnd,
        IDC_STATIC_STATUS);
}

LRESULT HandleCommand(HWND hwnd, WPARAM wParam, LPARAM lParam) {
    const WORD controlId = LOWORD(wParam);
    const WORD notifyCode = HIWORD(wParam);
    (void)lParam;

    switch (controlId) {
    case IDC_BUTTON_START:
        if (notifyCode == BN_CLICKED) {
            StartClicking(hwnd);
        }
        return 0;

    case IDC_BUTTON_STOP:
        if (notifyCode == BN_CLICKED) {
            StopClicking();
        }
        return 0;

    case IDC_RADIO_POS_CURRENT:
    case IDC_RADIO_POS_FIXED:
    case IDC_RADIO_COUNT_INFINITE:
    case IDC_RADIO_COUNT_LIMITED:
        if (notifyCode == BN_CLICKED) {
            UpdateControlState();
        }
        return 0;
    }

    return 0;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        g_app.hwnd = hwnd;
        CreateMainControls(hwnd);
        RegisterHotkeys(hwnd);
        UpdateControlState();
        return 0;

    case WM_COMMAND:
        return HandleCommand(hwnd, wParam, lParam);

    case WM_HOTKEY:
        if (wParam == kHotkeyStartId) {
            StartClicking(hwnd);
        } else if (wParam == kHotkeyStopId) {
            StopClicking();
        }
        return 0;

    case kMsgWorkerFinished:
        g_app.worker.JoinFinishedThread();
        SetStatusIdle();
        UpdateControlState();
        return 0;

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        UnregisterHotKey(hwnd, kHotkeyStartId);
        UnregisterHotKey(hwnd, kHotkeyStopId);
        g_app.worker.Stop(true);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

bool CreateMainWindow(int nCmdShow) {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.hInstance = g_app.instance;
    wc.lpfnWndProc = WndProc;
    wc.lpszClassName = kWindowClassName;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.style = CS_HREDRAW | CS_VREDRAW;

    if (!RegisterClassExW(&wc)) {
        return false;
    }

    RECT rect{0, 0, 440, 340};
    AdjustWindowRectEx(&rect, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE, 0);

    HWND hwnd = CreateWindowExW(
        0,
        kWindowClassName,
        kWindowTitle,
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        rect.right - rect.left,
        rect.bottom - rect.top,
        nullptr,
        nullptr,
        g_app.instance,
        nullptr);

    if (hwnd == nullptr) {
        return false;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);
    return true;
}

}  // namespace

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    g_app.instance = hInstance;

    if (!CreateMainWindow(nCmdShow)) {
        MessageBoxW(nullptr, L"\u521B\u5EFA\u4E3B\u7A97\u53E3\u5931\u8D25\u3002", L"\u9519\u8BEF", MB_OK | MB_ICONERROR);
        return 0;
    }

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return static_cast<int>(msg.wParam);
}
