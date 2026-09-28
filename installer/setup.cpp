// TMIXTOOL installer and uninstaller.
//
// A compiled executable rather than a script on purpose. A PowerShell
// installer would have to run with the execution policy bypassed on whatever
// machine it lands on, and telling a friend's computer to ignore a security
// control is not something an installer should be doing. A plain Win32 binary
// needs no such concession and leaves a much smaller footprint.
//
// The same binary is copied into the install directory and serves as the
// uninstaller when invoked with /uninstall.

// WIN32_LEAN_AND_MEAN comes from the build definition; defining it here too
// would only produce a redefinition warning.
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>

#include <string>
#include <vector>

#include "resource.h"

namespace {

constexpr const wchar_t* kAppName    = L"TMIXTOOL";
constexpr const wchar_t* kVersion    = L"1.0";
constexpr const wchar_t* kExeName    = L"TMIXTOOL.exe";
constexpr const wchar_t* kSetupName  = L"uninstall.exe";
constexpr const wchar_t* kUninstallKey =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\TMIXTOOL";

// ---------------------------------------------------------------------------
// Small helpers
// ---------------------------------------------------------------------------

std::wstring modulePath()
{
    wchar_t buffer[MAX_PATH] = {};
    GetModuleFileNameW (nullptr, buffer, MAX_PATH);
    return buffer;
}

std::wstring moduleDirectory()
{
    const std::wstring path = modulePath();
    const auto slash = path.find_last_of (L'\\');
    return slash == std::wstring::npos ? path : path.substr (0, slash);
}

std::wstring knownFolder (REFKNOWNFOLDERID id)
{
    PWSTR raw = nullptr;
    if (FAILED (SHGetKnownFolderPath (id, 0, nullptr, &raw)))
        return {};

    std::wstring value (raw);
    CoTaskMemFree (raw);
    return value;
}

std::wstring installDirectory()
{
    return knownFolder (FOLDERID_LocalAppData) + L"\\Programs\\" + kAppName;
}

bool fileExists (const std::wstring& path)
{
    return GetFileAttributesW (path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

bool isDirectory (const std::wstring& path)
{
    const DWORD attributes = GetFileAttributesW (path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY);
}

// Copies a directory tree. Returns false on the first failure and reports the
// path so the message can say what actually went wrong.
bool copyTree (const std::wstring& from, const std::wstring& to, std::wstring& failedAt)
{
    CreateDirectoryW (to.c_str(), nullptr);

    WIN32_FIND_DATAW found = {};
    HANDLE search = FindFirstFileW ((from + L"\\*").c_str(), &found);
    if (search == INVALID_HANDLE_VALUE)
        return false;

    bool ok = true;

    do {
        const std::wstring name = found.cFileName;
        if (name == L"." || name == L"..")
            continue;

        const std::wstring source = from + L"\\" + name;
        const std::wstring target = to + L"\\" + name;

        if (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            ok = copyTree (source, target, failedAt);
        }
        else if (! CopyFileW (source.c_str(), target.c_str(), FALSE))
        {
            ok = false;
            failedAt = source;
        }
    } while (ok && FindNextFileW (search, &found));

    FindClose (search);
    return ok;
}

bool removeTree (const std::wstring& path)
{
    if (! isDirectory (path))
        return true;

    WIN32_FIND_DATAW found = {};
    HANDLE search = FindFirstFileW ((path + L"\\*").c_str(), &found);
    if (search == INVALID_HANDLE_VALUE)
        return false;

    do {
        const std::wstring name = found.cFileName;
        if (name == L"." || name == L"..")
            continue;

        const std::wstring child = path + L"\\" + name;
        if (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            removeTree (child);
        else
        {
            SetFileAttributesW (child.c_str(), FILE_ATTRIBUTE_NORMAL);
            DeleteFileW (child.c_str());
        }
    } while (FindNextFileW (search, &found));

    FindClose (search);
    return RemoveDirectoryW (path.c_str()) != FALSE;
}

bool createShortcut (const std::wstring& linkPath,
                     const std::wstring& target,
                     const std::wstring& workingDirectory,
                     const std::wstring& description)
{
    IShellLinkW* shellLink = nullptr;
    if (FAILED (CoCreateInstance (CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_IShellLinkW, reinterpret_cast<void**> (&shellLink))))
        return false;

    shellLink->SetPath (target.c_str());
    shellLink->SetWorkingDirectory (workingDirectory.c_str());
    shellLink->SetDescription (description.c_str());
    shellLink->SetIconLocation (target.c_str(), 0);

    bool ok = false;
    IPersistFile* file = nullptr;
    if (SUCCEEDED (shellLink->QueryInterface (IID_IPersistFile,
                                              reinterpret_cast<void**> (&file))))
    {
        ok = SUCCEEDED (file->Save (linkPath.c_str(), TRUE));
        file->Release();
    }

    shellLink->Release();
    return ok;
}

std::wstring startMenuLink()
{
    return knownFolder (FOLDERID_Programs) + L"\\" + kAppName + L".lnk";
}

std::wstring desktopLink()
{
    return knownFolder (FOLDERID_Desktop) + L"\\" + kAppName + L".lnk";
}

// ---------------------------------------------------------------------------
// Install
// ---------------------------------------------------------------------------

struct Ui
{
    HWND window     = nullptr;
    HWND status     = nullptr;
    HWND progress   = nullptr;
    HWND desktopBox = nullptr;
    int  step       = 0;
    int  steps      = 6;
};

void report (Ui& ui, const wchar_t* message)
{
    ++ui.step;
    SetWindowTextW (ui.status, message);
    SendMessageW (ui.progress, PBM_SETPOS, min (ui.step, ui.steps), 0);

    // The work below is all on this thread, so the window has to be pumped by
    // hand or it would sit blank until everything finished.
    UpdateWindow (ui.window);
}

std::wstring failureMessage (const std::wstring& what, const std::wstring& path)
{
    std::wstring text = what;
    if (! path.empty())
        text += L"\n\n" + path;
    return text;
}

bool runInstall (Ui& ui)
{
    const std::wstring here = moduleDirectory();
    const std::wstring source = here + L"\\app";
    const std::wstring dest = installDirectory();
    const std::wstring exe = dest + L"\\" + kExeName;

    if (! fileExists (source + L"\\" + kExeName))
    {
        MessageBoxW (ui.window, L"安装包不完整，缺少程序文件。", kAppName, MB_ICONERROR);
        return false;
    }

    report (ui, L"正在复制程序文件...");
    removeTree (dest);

    std::wstring failedAt;
    if (! copyTree (source, dest, failedAt))
    {
        MessageBoxW (ui.window,
                     failureMessage (L"复制文件失败。", failedAt).c_str(),
                     kAppName, MB_ICONERROR);
        return false;
    }

    report (ui, L"正在创建开始菜单项...");
    createShortcut (startMenuLink(), exe, dest, std::wstring (kAppName) + L" 混音时间参考");

    report (ui, L"正在处理快捷方式...");
    if (SendMessageW (ui.desktopBox, BM_GETCHECK, 0, 0) == BST_CHECKED)
        createShortcut (desktopLink(), exe, dest, std::wstring (kAppName) + L" 混音时间参考");
    else
        DeleteFileW (desktopLink().c_str());

    report (ui, L"正在注册卸载信息...");
    // The installer copies itself in as the uninstaller. Taking its own path
    // rather than a fixed name means the packaging step is free to call the
    // launcher whatever it likes.
    CopyFileW (modulePath().c_str(), (dest + L"\\" + kSetupName).c_str(), FALSE);

    HKEY key = nullptr;
    if (RegCreateKeyExW (HKEY_CURRENT_USER, kUninstallKey, 0, nullptr,
                         REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr,
                         &key, nullptr) == ERROR_SUCCESS)
    {
        const std::wstring icon = exe + L",0";
        const std::wstring uninstallCommand =
            L"\"" + dest + L"\\" + kSetupName + L"\" /uninstall";

        const auto setString = [key] (const wchar_t* name, const std::wstring& value) {
            RegSetValueExW (key, name, 0, REG_SZ,
                            reinterpret_cast<const BYTE*> (value.c_str()),
                            static_cast<DWORD> ((value.size() + 1) * sizeof (wchar_t)));
        };

        setString (L"DisplayName",     kAppName);
        setString (L"DisplayVersion",  kVersion);
        setString (L"DisplayIcon",     icon);
        setString (L"InstallLocation", dest);
        setString (L"Publisher",       kAppName);
        setString (L"UninstallString", uninstallCommand);

        DWORD one = 1;
        RegSetValueExW (key, L"NoModify", 0, REG_DWORD,
                        reinterpret_cast<const BYTE*> (&one), sizeof (one));
        RegSetValueExW (key, L"NoRepair", 0, REG_DWORD,
                        reinterpret_cast<const BYTE*> (&one), sizeof (one));

        RegCloseKey (key);
    }

    report (ui, L"安装完成");

    if (MessageBoxW (ui.window,
                     L"安装完成。\n\n要现在启动 TMIXTOOL 吗？",
                     kAppName, MB_ICONINFORMATION | MB_YESNO) == IDYES)
    {
        ShellExecuteW (nullptr, L"open", exe.c_str(), nullptr, dest.c_str(), SW_SHOWNORMAL);
    }

    return true;
}

// ---------------------------------------------------------------------------
// Uninstall
// ---------------------------------------------------------------------------

void runUninstall()
{
    if (MessageBoxW (nullptr, L"将从本机移除 TMIXTOOL。\n\n继续吗？",
                     L"TMIXTOOL 卸载", MB_ICONQUESTION | MB_YESNO) != IDYES)
        return;

    DeleteFileW (startMenuLink().c_str());
    DeleteFileW (desktopLink().c_str());
    RegDeleteKeyW (HKEY_CURRENT_USER, kUninstallKey);

    // This executable lives inside the directory being removed, so Windows
    // will not let it delete itself. A detached shell does it once this
    // process has gone.
    const std::wstring dest = moduleDirectory();
    const std::wstring command =
        L"cmd.exe /c timeout /t 2 /nobreak >nul & rmdir /s /q \"" + dest + L"\"";

    STARTUPINFOW startup = {};
    startup.cb = sizeof (startup);
    PROCESS_INFORMATION process = {};

    if (CreateProcessW (nullptr, const_cast<wchar_t*> (command.c_str()),
                        nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                        nullptr, nullptr, &startup, &process))
    {
        CloseHandle (process.hProcess);
        CloseHandle (process.hThread);
    }

    MessageBoxW (nullptr, L"TMIXTOOL 已移除。", L"TMIXTOOL 卸载",
                 MB_ICONINFORMATION | MB_OK);
}

// ---------------------------------------------------------------------------
// Dialog
// ---------------------------------------------------------------------------

INT_PTR CALLBACK setupDialog (HWND window, UINT message, WPARAM wParam, LPARAM)
{
    static Ui ui;

    switch (message)
    {
        case WM_INITDIALOG:
        {
            ui.window     = window;
            ui.status     = GetDlgItem (window, IDC_STATUS);
            ui.progress   = GetDlgItem (window, IDC_PROGRESS);
            ui.desktopBox = GetDlgItem (window, IDC_DESKTOP);

            SendMessageW (ui.progress, PBM_SETRANGE, 0, MAKELPARAM (0, ui.steps));
            CheckDlgButton (window, IDC_DESKTOP, BST_CHECKED);
            SetDlgItemTextW (window, IDC_DESTPATH, installDirectory().c_str());

            // Centre on the primary display rather than letting Windows pick.
            RECT dialog = {}, screen = {};
            GetWindowRect (window, &dialog);
            SystemParametersInfoW (SPI_GETWORKAREA, 0, &screen, 0);
            SetWindowPos (window, nullptr,
                          screen.left + ((screen.right - screen.left)
                                         - (dialog.right - dialog.left)) / 2,
                          screen.top + ((screen.bottom - screen.top)
                                        - (dialog.bottom - dialog.top)) / 2,
                          0, 0, SWP_NOSIZE | SWP_NOZORDER);

            return TRUE;
        }

        case WM_COMMAND:
            if (LOWORD (wParam) == IDOK)
            {
                EnableWindow (GetDlgItem (window, IDOK), FALSE);
                EnableWindow (GetDlgItem (window, IDCANCEL), FALSE);
                SetCursor (LoadCursorW (nullptr, IDC_WAIT));

                const bool ok = runInstall (ui);

                SetCursor (LoadCursorW (nullptr, IDC_ARROW));
                EnableWindow (GetDlgItem (window, IDOK), TRUE);
                EnableWindow (GetDlgItem (window, IDCANCEL), TRUE);

                if (ok)
                    EndDialog (window, IDOK);

                return TRUE;
            }

            if (LOWORD (wParam) == IDCANCEL)
            {
                EndDialog (window, IDCANCEL);
                return TRUE;
            }
            break;

        case WM_CLOSE:
            EndDialog (window, IDCANCEL);
            return TRUE;
    }

    return FALSE;
}

bool launchedAsUninstaller()
{
    const std::wstring commandLine = GetCommandLineW();
    return commandLine.find (L"/uninstall") != std::wstring::npos;
}

} // namespace

int WINAPI wWinMain (HINSTANCE instance, HINSTANCE, PWSTR, int)
{
    // COM is needed for shortcut creation; the dialog keeps COM on this thread.
    CoInitializeEx (nullptr, COINIT_APARTMENTTHREADED);

    if (launchedAsUninstaller())
    {
        runUninstall();
        CoUninitialize();
        return 0;
    }

    INITCOMMONCONTROLSEX controls = {};
    controls.dwSize = sizeof (controls);
    controls.dwICC  = ICC_PROGRESS_CLASS;
    InitCommonControlsEx (&controls);

    const INT_PTR result = DialogBoxW (instance, MAKEINTRESOURCEW (IDD_SETUP),
                                       nullptr, setupDialog);

    CoUninitialize();
    return result == IDOK ? 0 : 1;
}
