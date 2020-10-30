#include <windows.h>
#include <MinHook.h>
#include <iostream>
#include <random>
#pragma execution_character_set("utf-8")

typedef void(__fastcall* SomeProcessStunMaybe)(void*, void*, void*, char);
typedef void(__cdecl* CTimer__Update) ();
typedef void(__cdecl* CMDPROC) (char*);

SomeProcessStunMaybe fpSomeProcessStunMaybe = NULL;
CTimer__Update fpCTimerHook = NULL;

static WNDPROC m_pOrigWnd;
static bool unHooked = false;

bool stunOn = false;
bool canFree = false;
unsigned char chance = 0;
DWORD SampNetGame = 0x21A0E8;
DWORD SampAddClientCommand = 0x65AD0;

#pragma pack(push, 1)
class CInput {
public:
    void* pD3DDevice;
    void* pDXUTDialog;
    void* pDXUTEditBox;
    CMDPROC				pCMDs[144];
    char				szCMDNames[144][33];
    int					iCMDCount;
    int					iInputEnabled;
    char				szInputBuffer[129];
    char				szRecallBufffer[10][129];
    char				szCurrentBuffer[129];
    int					iCurrentRecall;
    int					iTotalRecalls;
    CMDPROC				pszDefaultCMD;
};
#pragma pack(pop)
struct flags {
    unsigned int bHasBulletProofVest : 1;
    unsigned int bUsingMobilePhone : 1;
    unsigned int bUpperBodyDamageAnimsOnly : 1;
    unsigned int bStuckUnderCar : 1;
    unsigned int bKeepTasksAfterCleanUp : 1; // If true ped will carry on with task even after cleanup
    unsigned int bIsDyingStuck : 1;
    unsigned int bIgnoreHeightCheckOnGotoPointTask : 1; // set when walking round buildings, reset when task quits
    unsigned int bForceDieInCar : 1;
};

std::mt19937 randEngine{ std::random_device{}() };
std::uniform_int_distribution<unsigned __int32> distribute(1, 99);

void TurnChance(char* params);

void Free() {
    MH_DisableHook(reinterpret_cast<void*>(0x4B3FC0));
    unHooked = true;
    reinterpret_cast<flags*>(*reinterpret_cast<__int8**>(0xB6F5F0) + 0x478)->bUpperBodyDamageAnimsOnly = 0;
    if (canFree) {
        CInput* pChat = *reinterpret_cast<CInput**>(reinterpret_cast<DWORD>(GetModuleHandle(L"samp.dll")) + 0x21A0E8);
        for (auto i = 0; i < 144; i++) {
            if (pChat->pCMDs[i] == &TurnChance) {
                memset(&pChat->pCMDs[i], 0, 4);
                memset(pChat->szCMDNames[i], 0, 33);
                memset(&pChat->szCMDNames[i], 0, 4);
            }
        }
    }
}

auto __stdcall WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)->LRESULT {
    if (unHooked == false) {
        if (msg == WM_KEYDOWN) {
            if (wParam == VK_F2) {
                *reinterpret_cast<UINT8*>(0x4B4403) ^= 0x9F;
                stunOn = !stunOn;
            }
            else if (wParam == VK_END) {
                Free();
            }
        }
    }
    return CallWindowProcA(m_pOrigWnd, hWnd, msg, wParam, lParam);
}

void TurnChance(char* params) {
    sscanf_s(params, "%hhu", &chance);
    return;
}

void __fastcall HookProcessStunMaybe(void* ecx0, void* EDX, void* dis, char a3) {
    if (stunOn) {
        unsigned char result = 0;
        if (distribute(randEngine) < chance) {
            result = 1;
        }
        // return;
        reinterpret_cast<flags*>(*reinterpret_cast<__int8**>(0xB6F5F0) + 0x478)->bUpperBodyDamageAnimsOnly = result;
    }
    else {
        // return fpSomeProcessStunMaybe(ecx0, EDX, dis, a3);
        reinterpret_cast<flags*>(*reinterpret_cast<__int8**>(0xB6F5F0) + 0x478)->bUpperBodyDamageAnimsOnly = 0;
    }
    return fpSomeProcessStunMaybe(ecx0, EDX, dis, a3);
}

void __cdecl HOOK_CTimer__Update() {
    void *pChat = *reinterpret_cast<void**>(reinterpret_cast<DWORD>(GetModuleHandle(L"samp.dll")) + SampNetGame);
    static bool inited = false;
    if (pChat != nullptr && inited == false) {
        m_pOrigWnd = (WNDPROC)SetWindowLongA(**reinterpret_cast<HWND**>(0xC17054), GWL_WNDPROC, (LONG)WndProcHandler);
        ((void(__thiscall*) (const void*, const char*, CMDPROC))(reinterpret_cast<DWORD>(GetModuleHandle(L"samp.dll")) + SampAddClientCommand))(pChat, "aschance", &TurnChance);
        // Хук не снимаем, можем чужой хук затереть
        // MH_DisableHook(reinterpret_cast<LPVOID>(0x561B10));
    }
    return fpCTimerHook();
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH: 
    {
        MH_Initialize();
        auto base = reinterpret_cast<DWORD>(GetModuleHandle(L"samp.dll"));
        auto ntheader = reinterpret_cast<IMAGE_NT_HEADERS*>(base + reinterpret_cast<IMAGE_DOS_HEADER*>(base)->e_lfanew);
        auto ep = ntheader->OptionalHeader.AddressOfEntryPoint;
        switch (ep) {
        case 0x31DF13: canFree = true;
        case 0x3195DD: SampNetGame = 0x21A0EC; SampAddClientCommand = 0x65BA0; break; // R2
        case 0xCC4D0:  SampNetGame = 0x26E8DC; SampAddClientCommand = 0x69000; break; // R3
        case 0xCBCB0:  SampNetGame = 0x26EA0C; SampAddClientCommand = 0x69730; break; // R4
        }
        MH_CreateHook(reinterpret_cast<void*>(0x4B3FC0), &HookProcessStunMaybe, reinterpret_cast<void**>(&fpSomeProcessStunMaybe));
        MH_EnableHook(reinterpret_cast<void*>(0x4B3FC0));
        MH_CreateHook(reinterpret_cast<void*>(0x561B10), &HOOK_CTimer__Update, reinterpret_cast<void**>(&fpCTimerHook));
        MH_EnableHook(reinterpret_cast<void*>(0x561B10));
    }
        break;
    case DLL_PROCESS_DETACH:
        Free();
        break;
    }
    return TRUE;
}