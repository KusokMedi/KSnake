// KSnake single-file launcher for Windows.
//
// The game exe, SDL2.dll, SDL2_ttf.dll and assets/font.ttf are embedded
// into this binary as RCDATA resources (see build_release.sh --single-exe).
// At startup they are extracted to a temporary folder and the game is
// launched from there, so the result behaves like a normal folder install
// but ships as a single .exe. Scores are kept in %APPDATA% by the game
// (SDL_GetPrefPath), so nothing is lost when the temp folder is removed.

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>

#include <cstring>
#include <string>
#include <vector>

namespace {

const int RC_GAME = 101;
const int RC_SDL2 = 102;
const int RC_SDL2TTF = 103;
const int RC_FONT = 104;

bool extract_resource(int id, const std::wstring& dest) {
    HRSRC res = FindResourceW(nullptr, MAKEINTRESOURCEW(id), RT_RCDATA);
    if (!res) return false;
    HGLOBAL mem = LoadResource(nullptr, res);
    if (!mem) return false;
    void* data = LockResource(mem);
    DWORD size = SizeofResource(nullptr, res);
    if (!data || size == 0) return false;

    HANDLE f = CreateFileW(dest.c_str(), GENERIC_WRITE, 0, nullptr,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    BOOL ok = WriteFile(f, data, size, &written, nullptr);
    CloseHandle(f);
    return ok != FALSE && written == size;
}

bool make_dir(const std::wstring& path) {
    if (CreateDirectoryW(path.c_str(), nullptr) != FALSE) return true;
    return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

}  // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    wchar_t buf[32768];
    DWORD n = GetTempPathW(static_cast<DWORD>(sizeof(buf) / sizeof(buf[0])), buf);
    const std::wstring temp_root(buf, n);

    std::wstring dir;
    for (int i = 0; i < 10000; ++i) {
        std::wstring cand = temp_root + L"KSnake-data-" +
                            std::to_wstring(GetCurrentProcessId());
        if (i > 0) cand += L"-" + std::to_wstring(i);
        if (GetFileAttributesW(cand.c_str()) == INVALID_FILE_ATTRIBUTES) {
            dir = cand;
            break;
        }
    }
    if (dir.empty()) return 1;

    if (!make_dir(dir) || !make_dir(dir + L"\\assets")) return 2;

    if (!extract_resource(RC_GAME, dir + L"\\KSnake.exe")) return 3;
    if (!extract_resource(RC_SDL2, dir + L"\\SDL2.dll")) return 4;
    if (!extract_resource(RC_SDL2TTF, dir + L"\\SDL2_ttf.dll")) return 5;
    if (!extract_resource(RC_FONT, dir + L"\\assets\\font.ttf")) return 6;

    const wchar_t* raw = GetCommandLineW();
    std::vector<wchar_t> cmd(raw, raw + std::wcslen(raw) + 1);

    STARTUPINFOW si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    const std::wstring app = dir + L"\\KSnake.exe";
    BOOL ok = CreateProcessW(app.c_str(), cmd.data(), nullptr, nullptr, FALSE,
                             0, nullptr, dir.c_str(), &si, &pi);
    if (!ok) return 7;

    CloseHandle(pi.hThread);
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);

    DeleteFileW((dir + L"\\KSnake.exe").c_str());
    DeleteFileW((dir + L"\\SDL2.dll").c_str());
    DeleteFileW((dir + L"\\SDL2_ttf.dll").c_str());
    DeleteFileW((dir + L"\\assets\\font.ttf").c_str());
    RemoveDirectoryW((dir + L"\\assets").c_str());
    RemoveDirectoryW(dir.c_str());

    return static_cast<int>(code);
}