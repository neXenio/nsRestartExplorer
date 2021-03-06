#include "nsRestartExplorer.h"

#include <RestartManager.h>
#pragma comment(lib, "Rstrtmgr.lib")
#include <Tlhelp32.h>
#include <atlstr.h>

#include <atomic>
#include <chrono>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

/* RunDll32 */
void CALLBACK nsRE(HWND hwnd, HINSTANCE hinst, LPTSTR lpszCmdLine, int nCmdShow)
{
    RestartExplorer();
}

class SnapShotHandle
{
  public:
    SnapShotHandle()
        : _snapShot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0))
    {
    }

    SnapShotHandle(const SnapShotHandle &) = delete;
    SnapShotHandle &operator=(const SnapShotHandle &) = delete;

    HANDLE handle() const { return _snapShot; }

    ~SnapShotHandle() { CloseHandle(_snapShot); }

  protected:
    HANDLE _snapShot;
};

class RmSessionHandle
{
  public:
    RmSessionHandle()
    {
        DWORD   session                            = 0x0;
        wchar_t sessionKey[CCH_RM_SESSION_KEY + 1] = {0};

        auto result = RmStartSession(&session, 0x0, sessionKey);
        if (result != ERROR_SUCCESS) {
            throw std::runtime_error(
                "RestartManager could not be started (errorCode: " +
                std::to_string(result) + ")");
        }

        _session = session;
    }

    RmSessionHandle(const RmSessionHandle &) = delete;
    RmSessionHandle &operator=(const RmSessionHandle &) = delete;

    DWORD session() const { return _session; }

    ~RmSessionHandle() { RmEndSession(_session); }

  protected:
    DWORD _session;
};

struct EnumWindowCounter {
    EnumWindowCounter(bool makeNotActive)
        : makeNotActive(makeNotActive){};

    std::atomic<std::size_t> counter = 0;
    const bool               makeNotActive;
};

std::size_t countOpenFileExplorerWindows(bool makeNotActive = false)
{
    auto helper = EnumWindowCounter(makeNotActive);

    EnumWindows(
        [](HWND hwnd, LPARAM param) {
            auto helper = reinterpret_cast<EnumWindowCounter *>(param);

            TCHAR szClassName[MAX_PATH] = {0};
            GetClassName(hwnd, szClassName, MAX_PATH);

            if ((0 == lstrcmp(szClassName, TEXT("Explorer�WClass"))) ||
                (0 == lstrcmp(szClassName, TEXT("Cabinet�WClass")))) {
                if ((GetWindowLong(hwnd, GWL_STYLE) & WS_VISIBLE) ==
                    WS_VISIBLE) {
                    ++(helper->counter);
                    if (helper->makeNotActive) {
                        ShowWindow(hwnd, SW_SHOWNA);
                    }
                }
            }
            return TRUE;
        },
        (LPARAM)&helper);

    return helper.counter;
}

void waitTillFileExplorerWindowsAreOpen(std::size_t expectedWindowCount)
{
    static const auto tick = std::chrono::milliseconds(200);
    // max time to wait till first window opens (50 * 200ms = 10s)
    static const std::size_t maxCounter = 50;
    // max time we wait totally (100 * 200ms = 20s)
    static const std::size_t maxTotallyCounter = 100;
    // max time to wait till next window opens (5 * 200ms = 1s)
    static const std::size_t counterSinceFirstWindowOpen = 5;

    if (expectedWindowCount == 0) {
        return;
    }

    std::size_t counter         = 0;
    std::size_t totalCounter    = 0;
    std::size_t openWindows     = 0;
    std::size_t prevWindowsOpen = 0;
    do {
        std::this_thread::sleep_for(tick);
        openWindows = countOpenFileExplorerWindows(true);
        if (prevWindowsOpen != openWindows) {
            totalCounter += counter;
            counter         = 0;
            prevWindowsOpen = openWindows;
        } else if (prevWindowsOpen != 0 &&
                   counter > counterSinceFirstWindowOpen) {
            throw std::runtime_error(
                "Error during waiting for reopening the explorer. No new "
                "window is opening after the previous one. Waited (1s). "
                "Expected window count: " +
                std::to_string(expectedWindowCount) +
                ", but open windows count: " + std::to_string(openWindows));
        } else if (prevWindowsOpen == 0 && counter > maxCounter) {
            throw std::runtime_error(
                "Error during waiting for reopening the explorer. No new "
                "window is opening after 10s waiting time. Expected window "
                "count: " +
                std::to_string(expectedWindowCount));
        } else if (totalCounter > maxTotallyCounter) {
            throw std::runtime_error(
                "Error during waiting for reopening the explorer. Tooks to "
                "long for waiting (20s). Expected window count: " +
                std::to_string(expectedWindowCount) +
                ", but open windows count: " + std::to_string(openWindows));
        }
        ++counter;
    } while (openWindows < expectedWindowCount);

    std::this_thread::sleep_for(tick);
}

std::optional<RM_UNIQUE_PROCESS> getUniqueProcess(DWORD dwProcessId)
{
    FILETIME          CreationTime = {};
    FILETIME          ExitTime     = {};
    FILETIME          KernelTime   = {};
    FILETIME          UserTime     = {};
    RM_UNIQUE_PROCESS uniqueProcess;
    uniqueProcess.dwProcessId = dwProcessId;

    auto hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, dwProcessId);
    if (hProcess) {
        if (!GetProcessTimes(hProcess, &CreationTime, &ExitTime, &KernelTime,
                             &UserTime)) {
            throw std::runtime_error(
                "Failed to get the process times for process ID {}" +
                std::to_string(dwProcessId));
        }

        uniqueProcess.ProcessStartTime = CreationTime;
        return uniqueProcess;
    } else {
        auto er = GetLastError();
        if (ERROR_ACCESS_DENIED == er) {
            // OpenProcess will fail when not elevated and the target process is
            // in another user context. -> will be ignored
            return std::nullopt;
        } else {
            throw std::runtime_error("Failed to open the process ID {}" +
                                     std::to_string(dwProcessId));
        }
    }
}

std::vector<RM_UNIQUE_PROCESS> ProcFindAllIdsFromExeName(const TCHAR *exeName)
{
    DWORD          er        = ERROR_SUCCESS;
    BOOL           fContinue = FALSE;
    PROCESSENTRY32 peData    = {sizeof(peData)};

    auto snapshot = SnapShotHandle();

    if (INVALID_HANDLE_VALUE == snapshot.handle()) {
        throw std::runtime_error("not able to get system snapshot");
    }

    fContinue = Process32First(snapshot.handle(), &peData);

    std::vector<RM_UNIQUE_PROCESS> result;

    while (fContinue) {
        if (0 == lstrcmpi(peData.szExeFile, exeName)) {
            auto uniqueProc = getUniqueProcess(peData.th32ProcessID);
            if (uniqueProc.has_value()) {
                result.push_back(uniqueProc.value());
            }
        }
        fContinue = Process32Next(snapshot.handle(), &peData);
    }

    er = ::GetLastError();
    if (ERROR_NO_MORE_FILES != er) {
        throw std::runtime_error("GetNextProcess failed with error code: " +
                                 std::to_string(er));
    }

    return result;
}

bool restartWindowsExplorer()
{
    try {
        auto data = ProcFindAllIdsFromExeName(TEXT("explorer.exe"));

        auto sessionHandle = RmSessionHandle();

        auto res = RmRegisterResources(sessionHandle.session(), 0, nullptr,
                                       data.size(), data.data(), 0, nullptr);
        if (res != ERROR_SUCCESS) {
            throw std::runtime_error(
                "Error during RmRegisterResources, code: " +
                std::to_string(res));
        }

        std::size_t openWindows = countOpenFileExplorerWindows();

        res = RmShutdown(sessionHandle.session(), RmShutdownOnlyRegistered,
                         [](UINT) {});
        if (res != ERROR_SUCCESS) {
            // at least try to restart it
            RmRestart(sessionHandle.session(), 0, [](UINT) {});
            throw std::runtime_error("Error during RmShutDown, code: " +
                                     std::to_string(res));
        }

        res = RmRestart(sessionHandle.session(), 0, [](UINT) {});
        if (res != ERROR_SUCCESS) {
            throw std::runtime_error("Error during RmRestart, code: " +
                                     std::to_string(res));
        }

        // wait till File Explorer windows are open again, to avoid that they
        // overlap the installation window.
        waitTillFileExplorerWindowsAreOpen(openWindows);
    } catch (const std::exception &ex) {
#ifdef UNICODE
        pushString(CStringW(ex.what()));
#else
        pushString(ex.what());
#endif
        return false;
    }

    return true;
}

BOOL RestartExplorer() { return restartWindowsExplorer(); }
