#pragma once

// Only define these structures if they're not already defined by Windows headers
#ifndef _CUSTOM_PEB_LDR_DATA_DEFINED
#define _CUSTOM_PEB_LDR_DATA_DEFINED
typedef struct _CUSTOM_PEB_LDR_DATA
{
	BYTE Reserved1[8];
	PVOID Reserved2[3];
	LIST_ENTRY InMemoryOrderModuleList;
} CUSTOM_PEB_LDR_DATA, *PCUSTOM_PEB_LDR_DATA;
#endif

#ifndef _CUSTOM_UNICODE_STRING_DEFINED
#define _CUSTOM_UNICODE_STRING_DEFINED
typedef struct _CUSTOM_UNICODE_STRING
{
	USHORT Length;
	USHORT MaximumLength;
	PWSTR Buffer;
} CUSTOM_UNICODE_STRING, *PCUSTOM_UNICODE_STRING;
#endif

#ifndef _CUSTOM_LDR_DATA_TABLE_ENTRY_DEFINED
#define _CUSTOM_LDR_DATA_TABLE_ENTRY_DEFINED
typedef struct _CUSTOM_LDR_DATA_TABLE_ENTRY
{
	PVOID Reserved1[2];
	LIST_ENTRY InMemoryOrderLinks;
	PVOID Reserved2[2];
	PVOID DllBase;
	PVOID Reserved3[2];
	CUSTOM_UNICODE_STRING FullDllName;
	BYTE Reserved4[8];
	PVOID Reserved5[3];
#pragma warning(push)
#pragma warning(disable: 4201) // we'll always use the Microsoft compiler
	union
	{
		ULONG CheckSum;
		PVOID Reserved6;
	} DUMMYUNIONNAME;
#pragma warning(pop)
	ULONG TimeDateStamp;
} CUSTOM_LDR_DATA_TABLE_ENTRY, *PCUSTOM_LDR_DATA_TABLE_ENTRY;
#endif

#ifndef _CUSTOM_RTL_USER_PROCESS_PARAMETERS_DEFINED
#define _CUSTOM_RTL_USER_PROCESS_PARAMETERS_DEFINED
typedef struct _CUSTOM_RTL_USER_PROCESS_PARAMETERS
{
	BYTE Reserved1[16];
	PVOID Reserved2[10];
	CUSTOM_UNICODE_STRING ImagePathName;
	CUSTOM_UNICODE_STRING CommandLine;
} CUSTOM_RTL_USER_PROCESS_PARAMETERS, *PCUSTOM_RTL_USER_PROCESS_PARAMETERS;
#endif

//This function pointer is undocumented and just valid for windows 2000. Therefore I guess. 
typedef VOID (WINAPI*PPS_POST_PROCESS_INIT_ROUTINE)(VOID);

#ifndef _CUSTOM_PEB_DEFINED
#define _CUSTOM_PEB_DEFINED
typedef struct _CUSTOM_PEB
{
	BYTE reserved_0[2];
	BYTE is_debugging;
	BYTE reserved_1[13];
	uint64_t image;
	PCUSTOM_PEB_LDR_DATA Ldr;
	PCUSTOM_RTL_USER_PROCESS_PARAMETERS ProcessParameters;
	PVOID Reserved4[3];
	PVOID AtlThunkSListPtr;
	PVOID Reserved5;
	ULONG Reserved6;
	PVOID Reserved7;
	ULONG Reserved8;
	ULONG AtlThunkSListPtr32;
	PVOID Reserved9[45];
	BYTE Reserved10[96];
	PPS_POST_PROCESS_INIT_ROUTINE PostProcessInitRoutine;
	BYTE Reserved11[128];
	PVOID Reserved12[1];
	ULONG SessionId;
} CUSTOM_PEB, *PCUSTOM_PEB;
#endif
