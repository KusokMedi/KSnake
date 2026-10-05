#include "util.h"

#include <cerrno>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <signal.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace ks {
namespace {

bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

}  // namespace

std::string trim_str(const std::string& s) {
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return std::string();
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

uint32_t fnv1a(const std::string& s) {
    uint32_t h = 2166136261u;
    for (char c : s) {
        h ^= static_cast<uint8_t>(c);
        h *= 16777619u;
    }
    return h;
}

bool parse_version(const std::string& s, int out[3]) {
    out[0] = out[1] = out[2] = 0;
    size_t i = 0;
    while (i < s.size() && !is_digit(s[i])) ++i;
    if (i >= s.size()) return false;
    for (int part = 0; part < 3; ++part) {
        if (i >= s.size() || !is_digit(s[i])) break;
        int v = 0;
        while (i < s.size() && is_digit(s[i])) {
            if (v < 100000) v = v * 10 + (s[i] - '0');
            ++i;
        }
        out[part] = v;
        if (i < s.size() && s[i] == '.') {
            ++i;
        } else {
            break;
        }
    }
    return true;
}

std::string format_version(const int v[3]) {
    std::string s = std::to_string(v[0]) + "." + std::to_string(v[1]);
    if (v[2] != 0) s += "." + std::to_string(v[2]);
    return s;
}

int compare_versions(const int a[3], const int b[3]) {
    for (int i = 0; i < 3; ++i) {
        if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
    }
    return 0;
}

#ifdef _WIN32
std::wstring utf8_to_wide(const std::string& s) {
    if (s.empty()) return std::wstring();
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
    if (n <= 0) return std::wstring();
    std::wstring w(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), w.data(), n);
    return w;
}
#endif

namespace {

#ifdef _WIN32

// Правила кавычения CreateProcess: аргумент оборачивается в кавычки только если
// внутри есть пробел или кавычка, а кавычка внутри удваивается и обратным слэшем.
std::string quote_win_arg(const std::string& s) {
    if (!s.empty() && s.find_first_of(" \t\n\v\"") == std::string::npos) return s;
    std::string out = "\"";
    for (char c : s) {
        if (c == '\\') {
            out += c;
        } else if (c == '"') {
            out += "\\\"";
        } else {
            out += c;
        }
    }
    out += "\"";
    return out;
}

// Job Object, который убивает дочерние процессы при закрытии последнего хэндла:
// без него curl пережил бы игру и остался бы висеть в фоне.
struct KillOnCloseJob {
    HANDLE handle = nullptr;
    explicit KillOnCloseJob(bool enabled) {
        if (!enabled) return;
        handle = CreateJobObjectW(nullptr, nullptr);
        if (!handle) return;
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION info{};
        info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(handle, JobObjectExtendedLimitInformation, &info,
                                sizeof(info));
    }
    ~KillOnCloseJob() {
        if (handle) CloseHandle(handle);
    }
    KillOnCloseJob(const KillOnCloseJob&) = delete;
    KillOnCloseJob& operator=(const KillOnCloseJob&) = delete;
};

// Общий запуск: wait=true — ждём и читаем код возврата.
bool spawn(const std::vector<std::string>& args, bool wait, int* exit_code) {
    if (args.empty() || args[0].empty()) return false;
    if (exit_code) *exit_code = -1;

    std::wstring cmd;
    for (size_t i = 0; i < args.size(); ++i) {
        if (i) cmd += L' ';
        cmd += utf8_to_wide(quote_win_arg(args[i]));
    }
    std::vector<wchar_t> cmd_buf(cmd.begin(), cmd.end());
    cmd_buf.push_back(L'\0');

    STARTUPINFOW si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    // Job Object создаём только для run_process: там он спасает от осиротевшего
    // curl при выходе из игры. Для spawn_detached ребёнок (xdg-open) обязан
    // пережить игру, поэтому закрытие хэндла сразу после запуска убило бы его.
    KillOnCloseJob job(wait);
    if (!CreateProcessW(nullptr, cmd_buf.data(), nullptr, nullptr, FALSE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        return false;
    }
    if (job.handle) AssignProcessToJobObject(job.handle, pi.hProcess);
    CloseHandle(pi.hThread);
    if (!wait) {
        CloseHandle(pi.hProcess);
        return true;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 0;
    if (GetExitCodeProcess(pi.hProcess, &code)) {
        if (exit_code) *exit_code = static_cast<int>(code);
    }
    CloseHandle(pi.hProcess);
    return true;
}

#else  // POSIX

bool spawn(const std::vector<std::string>& args, bool wait, int* exit_code) {
    if (args.empty() || args[0].empty()) return false;
    if (exit_code) *exit_code = -1;

    std::vector<char*> argv;
    argv.reserve(args.size() + 1);
    for (const std::string& a : args) argv.push_back(const_cast<char*>(a.c_str()));
    argv.push_back(nullptr);

    pid_t pid = fork();
    if (pid < 0) return false;
    if (pid == 0) {
        prctl(PR_SET_PDEATHSIG, SIGTERM);
        // Родитель мог умереть между fork и prctl — тогда уже ничего не делаем.
        if (getppid() == 1) _exit(127);
        setsid();
        int devnull = open("/dev/null", O_RDWR);
        if (devnull >= 0) {
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
            if (devnull > STDERR_FILENO) close(devnull);
        }
        execvp(argv[0], argv.data());
        _exit(127);
    }
    if (!wait) return true;
    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) break;  // ECHILD и прочие: ждать больше нечего
    }
    if (exit_code) {
        *exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
    }
    return true;
}

#endif

}  // namespace

bool run_process(const std::vector<std::string>& args, int* exit_code) {
    return spawn(args, true, exit_code);
}

bool spawn_detached(const std::vector<std::string>& args) {
    return spawn(args, false, nullptr);
}

}  // namespace ks
