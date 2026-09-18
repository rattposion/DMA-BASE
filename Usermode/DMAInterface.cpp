#include "DMAInterface.h"
#include "global.h"
#include <ntstatus.h>

namespace DMAInterface {
    // Global variables to match the kernel interface
    ULONG64 g_DTB = 0;
    UINT32 g_processId = 0;
    NTSTATUS g_lastStatus = 0;
    void* g_dmaHandle = nullptr;

    bool ConnectToDMA() {
        Logger.log(("Attempting to connect to DMA device..."));
        
        // Initialize DMA with the target process
        if (!mem.Init("cod.exe", true, false)) {
            Logger.log(LogLevel::ERROR, ("Failed to initialize DMA!"));
            return false;
        }

        g_dmaHandle = &mem;
        Logger.log(("Successfully connected to DMA device!"));
        return true;
    }

    const UINT32 GetProcessPID(const std::string& processName) {
        if (!g_dmaHandle) return 0;
        
        g_processId = mem.GetPidFromName(processName);
        return g_processId;
    }

    const ULONG64 GetProcessPEB() {
        if (!g_dmaHandle) return 0;
        
        auto peb = mem.GetProcessPebAddress();
        if (peb) {
            Logger.log(("Got process environment block (PEB) {:#x} [PASS]!"), peb);
        } else {
            Logger.log(LogLevel::ERROR, ("Getting process environment block (PEB) [FAIL]!"));
        }
        return peb;
    }

    const ULONG64 GetProcessCR3() {
        if (!g_dmaHandle) return 0;
        
        // For DMA, we don't need to get CR3 separately as it's handled internally
        // But we can return a placeholder or get it from the process info
        g_DTB = 0x1; // Placeholder - DMA handles this internally
        Logger.log(("DMA handles CR3 internally [PASS]!"));
        return g_DTB;
    }

    const ULONG64 GetProcessBaseAddress() {
        if (!g_dmaHandle) return 0;
        
        auto baseAddr = mem.GetBaseDaddy("cod.exe");
        if (baseAddr) {
            Logger.log(("Got base address {:#x} [PASS]!"), baseAddr);
        } else {
            Logger.log(LogLevel::ERROR, ("Getting base address [FAIL]!"));
        }
        return baseAddr;
    }

    bool ReadMemory(const ULONG64 address, void* buffer, const SIZE_T size) {
        if (!g_dmaHandle) return false;
        
        bool result = mem.Read(address, buffer, size);
        if (!result) {
            g_lastStatus = STATUS_UNSUCCESSFUL;
        } else {
            g_lastStatus = STATUS_SUCCESS;
        }
        return result;
    }

    bool WriteMemory(const ULONG64 address, void* buffer, const SIZE_T size) {
        if (!g_dmaHandle) return false;
        
        bool result = mem.Write(address, buffer, size);
        if (!result) {
            g_lastStatus = STATUS_UNSUCCESSFUL;
        } else {
            g_lastStatus = STATUS_SUCCESS;
        }
        return result;
    }

    bool IsValidPointer(const ULONG64 address) {
        if (!g_dmaHandle) return false;
        
        // Try to read a small amount of memory to validate the pointer
        uint8_t testBuffer[1];
        return mem.Read(address, testBuffer, 1);
    }
}
