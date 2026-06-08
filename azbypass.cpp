#include "pch.h"
#include <windows.h>
#include <thread>
#include <vector>
#include <random>

namespace Settings {
    const int HurtTimeOffset = 0x150;
    bool IsRunning = true;
}

int GetRandomInt(int min, int max) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(min, max);
    return dis(gen);
}

void PreciseSleep(double ms) {
    static LARGE_INTEGER freq;
    static bool init = QueryPerformanceFrequency(&freq);
    LARGE_INTEGER start, end;
    QueryPerformanceCounter(&start);
    double target = (freq.QuadPart * ms) / 1000.0;
    while (true) {
        QueryPerformanceCounter(&end);
        if ((end.QuadPart - start.QuadPart) >= target) break;
        if (ms > 1.5) std::this_thread::yield(); 
    }
}

uintptr_t GetLocalPlayerPtr() {
    SYSTEM_INFO si; GetSystemInfo(&si);
    MEMORY_BASIC_INFORMATION mbi;
    uintptr_t addr = (uintptr_t)si.lpMinimumApplicationAddress;

    while (addr < (uintptr_t)si.lpMaximumApplicationAddress) {
        if (VirtualQuery((LPCVOID)addr, &mbi, sizeof(mbi)) &&
            mbi.State == MEM_COMMIT &&
            mbi.Protect == PAGE_READWRITE &&
            mbi.Type == MEM_PRIVATE) { 

            for (uintptr_t i = (uintptr_t)mbi.BaseAddress; i < (uintptr_t)mbi.BaseAddress + mbi.RegionSize - 0x200; i += 8) {
                __try {
                    if (*(int*)(i + Settings::HurtTimeOffset) == 10) {
                        return i;
                    }
                }
                __except (EXCEPTION_EXECUTE_HANDLER) { continue; }
            }
        }
        addr += mbi.RegionSize;
    }
    return 0;
}

void MainLoop(HMODULE hModule) {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

    Beep(400, 150); Beep(600, 150);
    uintptr_t pLocalPlayer = 0;
    bool hasReset = false;

    while (Settings::IsRunning) {
        if (GetAsyncKeyState(VK_F7) & 0x8000) break;

        if (GetAsyncKeyState(VK_F8) & 0x8000) {
            pLocalPlayer = GetLocalPlayerPtr();
            if (pLocalPlayer) {
                Beep(1000, 100); Beep(1200, 100);
            }
            Sleep(500);
        }

        if (pLocalPlayer) {
            __try {
                int hurtTime = *(int*)(pLocalPlayer + Settings::HurtTimeOffset);

                if (hurtTime >= 8 && hurtTime <= 10 && !hasReset) {

                    PreciseSleep(0.5 + (GetRandomInt(0, 15) / 10.0));

                    INPUT inputs[2] = {};
                    inputs[0].type = INPUT_KEYBOARD;
                    inputs[0].ki.wVk = VK_SPACE;
                    inputs[1].type = INPUT_KEYBOARD;
                    inputs[1].ki.wVk = 'W';

                    SendInput(2, inputs, sizeof(INPUT));

                    // 3. Maintien de pression variable
                    double hold = (hurtTime == 10) ? 4.2 : 3.2;
                    PreciseSleep(hold + (GetRandomInt(0, 100) / 100.0));

                    // 4. Relâchement
                    inputs[0].ki.dwFlags = KEYEVENTF_KEYUP;
                    inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
                    SendInput(2, inputs, sizeof(INPUT));

                    hasReset = true;
                }
                else if (hurtTime < 5) {
                    hasReset = false;
                }
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                pLocalPlayer = 0; 
            }
        }

        if (!hasReset) PreciseSleep(0.5);
    }

    Beep(200, 400);
    FreeLibraryAndExitThread(hModule, 0);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lp) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        HANDLE h = CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)MainLoop, hModule, 0, nullptr);
        if (h) CloseHandle(h);
    }
    return TRUE;
}
