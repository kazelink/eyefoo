#include "utils.h"
#include "types.h"
#include <stdarg.h>
#include <stdio.h>
#include <wchar.h>

#define LOG_MAX_BYTES (256u * 1024u)

static BOOL s_logEnabled = FALSE;

static BOOL Util_GetModuleSiblingPath(const wchar_t *name, wchar_t *path, size_t cch) {
    DWORD len;
    wchar_t *slash;

    if (!name || !path || cch == 0) {
        return FALSE;
    }

    len = GetModuleFileNameW(NULL, path, (DWORD)cch);
    if (len == 0 || len >= cch) {
        return FALSE;
    }

    slash = wcsrchr(path, L'\\');
    if (!slash) {
        return FALSE;
    }

    slash++;
    return swprintf(slash, cch - (size_t)(slash - path), L"%ls", name) >= 0;
}

static BOOL GetIniPath(wchar_t *path, size_t cch) {
    return Util_GetModuleSiblingPath(L"config.ini", path, cch);
}

static BOOL Util_ReadIniBool(const wchar_t *key, BOOL defaultValue) {
    wchar_t path[MAX_PATH];

    if (!GetIniPath(path, MAX_PATH)) {
        return defaultValue;
    }

    return GetPrivateProfileIntW(L"Settings", key, defaultValue ? 1 : 0, path) != 0;
}

static void Util_SetLogEnabled(BOOL enabled) {
    s_logEnabled = enabled ? TRUE : FALSE;
}

static void Util_RefreshLogEnabledFromIni(void) {
    Util_SetLogEnabled(Util_ReadIniBool(L"DebugLog", FALSE));
}

static void Util_TrimTrailingWhitespace(wchar_t *text) {
    size_t len;

    if (!text) {
        return;
    }

    len = wcslen(text);
    while (len > 0) {
        wchar_t ch = text[len - 1];
        if (ch != L'\r' && ch != L'\n' && ch != L' ' && ch != L'\t' && ch != L'.') {
            break;
        }
        text[--len] = L'\0';
    }
}

static void Util_FormatSystemMessage(DWORD err, wchar_t *buf, size_t cch) {
    DWORD rc;

    if (!buf || cch == 0) {
        return;
    }

    buf[0] = L'\0';
    rc = FormatMessageW(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        err,
        0,
        buf,
        (DWORD)cch,
        NULL);
    if (!rc) {
        swprintf(buf, cch, L"(no system message)");
        return;
    }

    Util_TrimTrailingWhitespace(buf);
}

static void Util_RotateLogIfNeeded(const wchar_t *path, DWORD bytesToAppend) {
    WIN32_FILE_ATTRIBUTE_DATA attrs;
    ULONGLONG size;
    wchar_t backupPath[MAX_PATH];

    if (!path) {
        return;
    }

    if (!GetFileAttributesExW(path, GetFileExInfoStandard, &attrs)) {
        return;
    }

    size = ((ULONGLONG)attrs.nFileSizeHigh << 32) | attrs.nFileSizeLow;
    if (size + bytesToAppend <= LOG_MAX_BYTES) {
        return;
    }

    if (!Util_GetModuleSiblingPath(L"eye_reminder.log.1", backupPath, MAX_PATH)) {
        return;
    }

    DeleteFileW(backupPath);
    MoveFileExW(path, backupPath, MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED);
}

static void Util_WriteUtf8Line(const wchar_t *msg) {
    wchar_t path[MAX_PATH];
    wchar_t line[2048];
    SYSTEMTIME st;
    HANDLE hFile;
    LARGE_INTEGER fileSize;
    int chars;
    int bytesNeeded;
    char utf8[4096];
    DWORD written;
    static const unsigned char bom[] = { 0xEF, 0xBB, 0xBF };

    if (!msg || !s_logEnabled || !Util_GetModuleSiblingPath(L"eye_reminder.log", path, MAX_PATH)) {
        return;
    }

    GetLocalTime(&st);
    chars = swprintf(
        line,
        sizeof(line) / sizeof(line[0]),
        L"[%04u-%02u-%02u %02u:%02u:%02u.%03u][pid=%lu][tid=%lu] %ls\r\n",
        st.wYear,
        st.wMonth,
        st.wDay,
        st.wHour,
        st.wMinute,
        st.wSecond,
        st.wMilliseconds,
        (unsigned long)GetCurrentProcessId(),
        (unsigned long)GetCurrentThreadId(),
        msg);
    if (chars < 0) {
        return;
    }

    bytesNeeded = WideCharToMultiByte(CP_UTF8, 0, line, -1, NULL, 0, NULL, NULL);
    if (bytesNeeded <= 1 || bytesNeeded > (int)sizeof(utf8)) {
        return;
    }
    if (!WideCharToMultiByte(CP_UTF8, 0, line, -1, utf8, bytesNeeded, NULL, NULL)) {
        return;
    }

    Util_RotateLogIfNeeded(path, (DWORD)(bytesNeeded - 1));

    hFile = CreateFileW(
        path,
        FILE_APPEND_DATA | GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        return;
    }

    if (GetFileSizeEx(hFile, &fileSize) && fileSize.QuadPart == 0) {
        WriteFile(hFile, bom, (DWORD)sizeof(bom), &written, NULL);
    }
    WriteFile(hFile, utf8, (DWORD)(bytesNeeded - 1), &written, NULL);
    CloseHandle(hFile);
}

static const wchar_t *Util_ExceptionName(DWORD code) {
    switch (code) {
    case EXCEPTION_ACCESS_VIOLATION:          return L"ACCESS_VIOLATION";
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:    return L"ARRAY_BOUNDS_EXCEEDED";
    case EXCEPTION_BREAKPOINT:               return L"BREAKPOINT";
    case EXCEPTION_DATATYPE_MISALIGNMENT:    return L"DATATYPE_MISALIGNMENT";
    case EXCEPTION_FLT_DENORMAL_OPERAND:     return L"FLT_DENORMAL_OPERAND";
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:       return L"FLT_DIVIDE_BY_ZERO";
    case EXCEPTION_FLT_INVALID_OPERATION:    return L"FLT_INVALID_OPERATION";
    case EXCEPTION_FLT_OVERFLOW:             return L"FLT_OVERFLOW";
    case EXCEPTION_FLT_STACK_CHECK:          return L"FLT_STACK_CHECK";
    case EXCEPTION_ILLEGAL_INSTRUCTION:      return L"ILLEGAL_INSTRUCTION";
    case EXCEPTION_IN_PAGE_ERROR:            return L"IN_PAGE_ERROR";
    case EXCEPTION_INT_DIVIDE_BY_ZERO:       return L"INT_DIVIDE_BY_ZERO";
    case EXCEPTION_INT_OVERFLOW:             return L"INT_OVERFLOW";
    case EXCEPTION_INVALID_DISPOSITION:      return L"INVALID_DISPOSITION";
    case EXCEPTION_NONCONTINUABLE_EXCEPTION: return L"NONCONTINUABLE_EXCEPTION";
    case EXCEPTION_PRIV_INSTRUCTION:         return L"PRIV_INSTRUCTION";
    case EXCEPTION_SINGLE_STEP:              return L"SINGLE_STEP";
    case EXCEPTION_STACK_OVERFLOW:           return L"STACK_OVERFLOW";
    default:                                 return L"UNKNOWN_EXCEPTION";
    }
}

static LONG WINAPI Util_UnhandledExceptionFilter(EXCEPTION_POINTERS *info) {
    DWORD code = 0;
    DWORD flags = 0;
    void *address = NULL;

    if (info && info->ExceptionRecord) {
        code = info->ExceptionRecord->ExceptionCode;
        flags = info->ExceptionRecord->ExceptionFlags;
        address = info->ExceptionRecord->ExceptionAddress;
    }

    Util_LogFormat(
        L"Unhandled exception: code=0x%08lX (%ls), flags=0x%08lX, address=%p",
        (unsigned long)code,
        Util_ExceptionName(code),
        (unsigned long)flags,
        address);

    if (info && info->ExceptionRecord &&
        (code == EXCEPTION_ACCESS_VIOLATION || code == EXCEPTION_IN_PAGE_ERROR) &&
        info->ExceptionRecord->NumberParameters >= 2) {
        const wchar_t *op = L"unknown";
        ULONG_PTR action = info->ExceptionRecord->ExceptionInformation[0];
        void *faultAddr = (void *)(ULONG_PTR)info->ExceptionRecord->ExceptionInformation[1];

        if (action == 0) {
            op = L"read";
        } else if (action == 1) {
            op = L"write";
        } else if (action == 8) {
            op = L"execute";
        }

        Util_LogFormat(L"Access violation detail: operation=%ls, target=%p", op, faultAddr);
    }

    if (info && info->ContextRecord) {
#if defined(_WIN64)
        Util_LogFormat(
            L"Crash context: RIP=%p RSP=%p RBP=%p",
            (void *)(ULONG_PTR)info->ContextRecord->Rip,
            (void *)(ULONG_PTR)info->ContextRecord->Rsp,
            (void *)(ULONG_PTR)info->ContextRecord->Rbp);
#elif defined(_WIN32)
        Util_LogFormat(
            L"Crash context: EIP=%p ESP=%p EBP=%p",
            (void *)(ULONG_PTR)info->ContextRecord->Eip,
            (void *)(ULONG_PTR)info->ContextRecord->Esp,
            (void *)(ULONG_PTR)info->ContextRecord->Ebp);
#endif
    }

    return EXCEPTION_CONTINUE_SEARCH;
}

void Util_LogInit(void) {
    wchar_t exePath[MAX_PATH];

    Util_RefreshLogEnabledFromIni();
    if (GetModuleFileNameW(NULL, exePath, MAX_PATH)) {
        Util_LogFormat(L"========== process start: %ls ==========", exePath);
    } else {
        Util_Log(L"========== process start ==========");
    }
}

void Util_LogShutdown(void) {
    Util_Log(L"========== process exit ==========");
}

void Util_Log(const wchar_t *msg) {
    if (!msg || !s_logEnabled) {
        return;
    }

    OutputDebugStringW(L"[EyeReminder] ");
    OutputDebugStringW(msg);
    OutputDebugStringW(L"\n");
    Util_WriteUtf8Line(msg);
}

void Util_LogFormat(const wchar_t *fmt, ...) {
    wchar_t buf[1024];
    va_list ap;

    if (!fmt) {
        return;
    }

    va_start(ap, fmt);
    if (vswprintf(buf, sizeof(buf) / sizeof(buf[0]), fmt, ap) >= 0) {
        Util_Log(buf);
    }
    va_end(ap);
}

void Util_LogLastError(const wchar_t *api) {
    wchar_t buf[512];
    wchar_t sysMsg[256];
    DWORD err = GetLastError();

    Util_FormatSystemMessage(err, sysMsg, sizeof(sysMsg) / sizeof(sysMsg[0]));
    swprintf(
        buf,
        sizeof(buf) / sizeof(buf[0]),
        L"%ls failed (error=%lu, message=%ls)",
        api ? api : L"(unknown)",
        (unsigned long)err,
        sysMsg);
    Util_Log(buf);
}

void Util_InstallCrashHandlers(void) {
    SetUnhandledExceptionFilter(Util_UnhandledExceptionFilter);
    Util_Log(L"Unhandled exception filter installed");
}

BOOL Reg_GetAutoStart(void) {
    HKEY hk;
    BOOL on = FALSE;

    if (!RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_QUERY_VALUE, &hk)) {
        on = !RegQueryValueExW(hk, APP_NAME, NULL, NULL, NULL, NULL);
        RegCloseKey(hk);
    }
    return on;
}

void Reg_SetAutoStart(BOOL on) {
    HKEY hk;
    LONG rc = RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hk);
    if (rc != ERROR_SUCCESS) {
        SetLastError((DWORD)rc);
        Util_LogLastError(L"RegOpenKeyExW(Run)");
        return;
    }

    if (on) {
        wchar_t p[MAX_PATH];
        if (!GetModuleFileNameW(NULL, p, MAX_PATH)) {
            Util_LogLastError(L"GetModuleFileNameW");
            RegCloseKey(hk);
            return;
        }

        {
            wchar_t cmd[MAX_PATH + 3];
            swprintf(cmd, MAX_PATH + 3, L"\"%ls\"", p);
            rc = RegSetValueExW(hk, APP_NAME, 0, REG_SZ, (BYTE *)cmd,
                (DWORD)((wcslen(cmd) + 1) * sizeof(wchar_t)));
            if (rc != ERROR_SUCCESS) {
                SetLastError((DWORD)rc);
                Util_LogLastError(L"RegSetValueExW(AutoStart)");
            }
        }
    } else {
        rc = RegDeleteValueW(hk, APP_NAME);
        if (rc != ERROR_SUCCESS && rc != ERROR_FILE_NOT_FOUND) {
            SetLastError((DWORD)rc);
            Util_LogLastError(L"RegDeleteValueW(AutoStart)");
        }
    }
    RegCloseKey(hk);
}

void Config_Load(void) {
    wchar_t path[MAX_PATH];

    if (!GetIniPath(path, MAX_PATH)) {
        g_cfg.workMin = DEF_WORK_MIN;
        g_cfg.focusMin = DEF_FOCUS_MIN;
        g_cfg.debugLog = FALSE;
        g_cfg.autoStart = Reg_GetAutoStart();
        Util_SetLogEnabled(g_cfg.debugLog);
        return;
    }

    g_cfg.workMin = GetPrivateProfileIntW(L"Settings", L"WorkMin", DEF_WORK_MIN, path);
    g_cfg.focusMin = GetPrivateProfileIntW(L"Settings", L"FocusMin", DEF_FOCUS_MIN, path);
    g_cfg.debugLog = GetPrivateProfileIntW(L"Settings", L"DebugLog", 0, path) != 0;
    if (g_cfg.workMin < 1 || g_cfg.workMin > 180) g_cfg.workMin = DEF_WORK_MIN;
    if (g_cfg.focusMin < 10 || g_cfg.focusMin > 720) g_cfg.focusMin = DEF_FOCUS_MIN;
    g_cfg.autoStart = Reg_GetAutoStart();
    Util_SetLogEnabled(g_cfg.debugLog);
    Util_LogFormat(
        L"Config loaded: workMin=%d, focusMin=%d, autoStart=%d, debugLog=%d, path=%ls",
        g_cfg.workMin,
        g_cfg.focusMin,
        g_cfg.autoStart ? 1 : 0,
        g_cfg.debugLog ? 1 : 0,
        path);
}

void Config_Save(void) {
    wchar_t path[MAX_PATH];
    wchar_t bWork[16];
    wchar_t bFocus[16];
    wchar_t bDebug[16];

    if (!GetIniPath(path, MAX_PATH)) {
        return;
    }

    swprintf(bWork, 16, L"%d", g_cfg.workMin);
    swprintf(bFocus, 16, L"%d", g_cfg.focusMin);
    swprintf(bDebug, 16, L"%d", g_cfg.debugLog ? 1 : 0);

    WritePrivateProfileStringW(L"Settings", L"WorkMin", bWork, path);
    WritePrivateProfileStringW(L"Settings", L"FocusMin", bFocus, path);
    WritePrivateProfileStringW(L"Settings", L"DebugLog", bDebug, path);
    WritePrivateProfileStringW(L"Settings", L"RestMin", NULL, path);
    WritePrivateProfileStringW(L"Settings", L"AutoStart", NULL, path);

    Util_SetLogEnabled(g_cfg.debugLog);
    Util_LogFormat(
        L"Config saved: workMin=%d, focusMin=%d, autoStart=%d, debugLog=%d, path=%ls",
        g_cfg.workMin,
        g_cfg.focusMin,
        g_cfg.autoStart ? 1 : 0,
        g_cfg.debugLog ? 1 : 0,
        path);
}

HFONT Util_Font(int pt, BOOL bold) {
    HDC hdc = GetDC(NULL);
    int h = -MulDiv(pt, GetDeviceCaps(hdc, LOGPIXELSY), 72);
    ReleaseDC(NULL, hdc);
    return CreateFontW(h, 0, 0, 0, bold ? FW_SEMIBOLD : FW_NORMAL,
        FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI");
}

HFONT Util_HeavyFont(int pt) {
    HDC hdc = GetDC(NULL);
    int h = -MulDiv(pt, GetDeviceCaps(hdc, LOGPIXELSY), 72);
    ReleaseDC(NULL, hdc);
    return CreateFontW(h, 0, 0, 0, FW_BOLD,
        FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Consolas");
}
