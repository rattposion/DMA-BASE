#pragma once
#include "DMALibrary/Memory/Memory.h"

namespace DMAInterface {
    // Global variables to match the kernel interface
    extern ULONG64 g_DTB;
    extern UINT32 g_processId;
    extern NTSTATUS g_lastStatus;
    extern void* g_dmaHandle;

    // Initialize DMA interface
    bool ConnectToDMA();

    // Process management
    const UINT32 GetProcessPID(const std::string& processName);
    const ULONG64 GetProcessPEB();
    const ULONG64 GetProcessCR3();
    const ULONG64 GetProcessBaseAddress();

    // Memory operations
    bool ReadMemory(const ULONG64 address, void* buffer, const SIZE_T size);
    bool WriteMemory(const ULONG64 address, void* buffer, const SIZE_T size);
    bool IsValidPointer(const ULONG64 address);

    inline bool IsLikelyValidPointer(const ULONG64 address) {
        return address >= 0x10000;
    }

    // Template functions for type-safe memory operations
    template<typename T>
    T Read(const ULONG64 address) {
        T buffer{};
        if (ReadMemory(address, &buffer, sizeof(T))) {
            return buffer;
        }
        return T{};
    }

    template<typename T>
    bool Write(const ULONG64 address, const T& value) {
        return WriteMemory(address, const_cast<void*>(static_cast<const void*>(&value)), sizeof(T));
    }
}


