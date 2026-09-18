#include "pch.h"
#include "Memory.h"
#include "InputManager.h"
#include "Registry.h"
#include "Shellcode.h"
#include <atomic>

namespace {
inline bool VerboseDmaFailures() {
	return Memory::VerboseFailures();
}
}

#define DMA_FAIL_LOG(...) \
	do { \
		if (VerboseDmaFailures()) { \
			printf(__VA_ARGS__); \
		} \
	} while (0)

Memory::Memory()
{
	printf("Loading DMA libraries...\n");
	modules.VMM = LoadLibraryA("vmm.dll");
	modules.FTD3XX = LoadLibraryA("FTD3XX.dll");
	modules.LEECHCORE = LoadLibraryA("leechcore.dll");

	if (!modules.VMM || !modules.FTD3XX || !modules.LEECHCORE)
	{
		printf("vmm: %p\n", modules.VMM);
		printf("ftd: %p\n", modules.FTD3XX);
		printf("leech: %p\n", modules.LEECHCORE);
		printf("[!] Could not load a library\n");
		return;
	}

	this->m_input_keys = std::make_shared<c_keys>();

	printf("Successfully loaded libraries!\n");
}

Memory::~Memory()
{
	if (this->vHandle) {
		VMMDLL_Close(this->vHandle);
	}
	DMA_INITIALIZED = false;
	PROCESS_INITIALIZED = false;
}

bool Memory::DumpMemoryMap(bool debug)
{
	LPCSTR args[] = {const_cast<LPCSTR>(""), const_cast<LPCSTR>("-device"), const_cast<LPCSTR>("fpga://algo=0"), const_cast<LPCSTR>(""), const_cast<LPCSTR>("")};
	int argc = 3;
	if (debug)
	{
		args[argc++] = const_cast<LPCSTR>("-v");
		args[argc++] = const_cast<LPCSTR>("-printf");
	}

	VMM_HANDLE handle = VMMDLL_Initialize(argc, args);
	if (!handle)
	{
		printf("[!] Failed to open a VMM Handle\n");
		return false;
	}

	PVMMDLL_MAP_PHYSMEM pPhysMemMap = NULL;
	if (!VMMDLL_Map_GetPhysMem(handle, &pPhysMemMap))
	{
		printf("[!] Failed to get physical memory map\n");
		VMMDLL_Close(handle);
		return false;
	}

	if (pPhysMemMap->dwVersion != VMMDLL_MAP_PHYSMEM_VERSION)
	{
		printf("[!] Invalid VMM Map Version\n");
		VMMDLL_MemFree(pPhysMemMap);
		VMMDLL_Close(handle);
		return false;
	}

	if (pPhysMemMap->cMap == 0)
	{
		printf("[!] Failed to get physical memory map\n");
		VMMDLL_MemFree(pPhysMemMap);
		VMMDLL_Close(handle);
		return false;
	}

	//Dump map to file
	std::stringstream sb;
	for (DWORD i = 0; i < pPhysMemMap->cMap; i++)
	{
		sb << std::hex << pPhysMemMap->pMap[i].pa << " " << (pPhysMemMap->pMap[i].pa + pPhysMemMap->pMap[i].cb - 1) << std::endl;
	}

	auto temp_path = std::filesystem::temp_directory_path();
	std::ofstream nFile(temp_path.string() + "\\mmap.txt");
	nFile << sb.str();
	nFile.close();

	VMMDLL_MemFree(pPhysMemMap);
	printf("Successfully dumped memory map to file!\n");
	//Little sleep to make sure it's written to file.
	Sleep(3000);
	VMMDLL_Close(handle);
	return true;
}

unsigned char abort2[4] = {0x10, 0x00, 0x10, 0x00};

bool Memory::SetFPGA()
{
	ULONG64 qwID = 0, qwVersionMajor = 0, qwVersionMinor = 0;
	if (!VMMDLL_ConfigGet(this->vHandle, LC_OPT_FPGA_FPGA_ID, &qwID) && VMMDLL_ConfigGet(this->vHandle, LC_OPT_FPGA_VERSION_MAJOR, &qwVersionMajor) && VMMDLL_ConfigGet(this->vHandle, LC_OPT_FPGA_VERSION_MINOR, &qwVersionMinor))
	{
		printf("[!] Failed to lookup FPGA device, Attempting to proceed\n\n");
		return false;
	}

	printf("[+] VMMDLL_ConfigGet");
	printf(" ID = %lli", qwID);
	printf(" VERSION = %lli.%lli\n", qwVersionMajor, qwVersionMinor);

	if ((qwVersionMajor >= 4) && ((qwVersionMajor >= 5) || (qwVersionMinor >= 7)))
	{
		HANDLE handle;
		LC_CONFIG config = {.dwVersion = LC_CONFIG_VERSION, .szDevice = "existing"};
		handle = LcCreate(&config);
		if (!handle)
		{
			printf("[!] Failed to create FPGA device\n");
			return false;
		}

		LcCommand(handle, LC_CMD_FPGA_CFGREGPCIE_MARKWR | 0x002, 4, reinterpret_cast<PBYTE>(&abort2), NULL, NULL);
		printf("[-] Register auto cleared\n");
		LcClose(handle);
	}

	return true;
}

namespace {
	void ConfigureDmaPerformance(VMM_HANDLE vHandle) {
		if (!vHandle) {
			return;
		}

		VMMDLL_ConfigSet(vHandle, VMMDLL_OPT_CONFIG_IS_REFRESH_ENABLED, 1);

		constexpr ULONG64 disableRefreshTicks = 1000000;
		VMMDLL_ConfigSet(vHandle, VMMDLL_OPT_CONFIG_PROCCACHE_TICKS_PARTIAL, disableRefreshTicks);
		VMMDLL_ConfigSet(vHandle, VMMDLL_OPT_CONFIG_PROCCACHE_TICKS_TOTAL, disableRefreshTicks);
		printf("[+] DMA configured: background refresh on, process cache refresh disabled\n");
	}
}

bool Memory::Init(std::string process_name, bool memMap, bool debug)
{
	if (!DMA_INITIALIZED)
	{
		printf("Initializing DMA...\n");
	reinit:
		LPCSTR args[] = {const_cast<LPCSTR>(""), const_cast<LPCSTR>("-device"), const_cast<LPCSTR>("fpga://algo=0"), const_cast<LPCSTR>(""), const_cast<LPCSTR>(""), const_cast<LPCSTR>("")};
		DWORD argc = 3;
		if (debug)
		{
			args[argc++] = const_cast<LPCSTR>("-v");
			args[argc++] = const_cast<LPCSTR>("-printf");
		}

		std::string path = "";
		if (memMap)
		{
			auto temp_path = std::filesystem::temp_directory_path();
			path = (temp_path.string() + "\\mmap.txt");
			bool dumped = false;
			if (!std::filesystem::exists(path))
				dumped = this->DumpMemoryMap(debug);
			else
				dumped = true;
			printf("Dumping memory map to file...\n");
			if (!dumped)
			{
				printf("[!] ERROR: Could not dump memory map!\n");
				printf("Defaulting to no memory map!\n");
			}
			else
			{
				printf("Dumped memory map!\n");

				//Add the memory map to the arguments and increase arg count.
				args[argc++] = const_cast<LPSTR>("-memmap");
				args[argc++] = const_cast<LPSTR>(path.c_str());
			}
		}
		this->vHandle = VMMDLL_Initialize(argc, args);
		if (!this->vHandle)
		{
			if (memMap)
			{
				memMap = false;
				printf("[!] Initialization failed with Memory map? Try without MMap\n");
				goto reinit;
			}
			printf("[!] Initialization failed! Is the DMA in use or disconnected?\n");
			return false;
		}

		ConfigureDmaPerformance(this->vHandle);

		ULONG64 FPGA_ID = 0, DEVICE_ID = 0;

		VMMDLL_ConfigGet(this->vHandle, LC_OPT_FPGA_FPGA_ID, &FPGA_ID);
		VMMDLL_ConfigGet(this->vHandle, LC_OPT_FPGA_DEVICE_ID, &DEVICE_ID);

		printf("FPGA ID: %llu\n", FPGA_ID);
		printf("DEVICE ID: %llu\n", DEVICE_ID);
		printf("DMA initialization success!\n");

		if (!this->SetFPGA())
		{
			printf("[!] Could not set FPGA!\n");
			VMMDLL_Close(this->vHandle);
			return false;
		}

		DMA_INITIALIZED = TRUE;
	}
	else
		printf("DMA already initialized!\n");

	if (PROCESS_INITIALIZED)
	{
		printf("Process already initialized!\n");
		return true;
	}

	current_process.PID = GetPidFromName(process_name);
	if (!current_process.PID)
	{
		printf("[!] Could not get PID from name!\n");
		return false;
	}
	current_process.process_name = process_name;
	
	if (!mem.FixCr3())
		printf("Failed to fix CR3\n");
	else
		printf("CR3 fixed\n");

	current_process.base_address = GetBaseDaddy(process_name);
	if (!current_process.base_address)
	{
		printf("[!] Could not get base address!\n");
		return false;
	}

	current_process.base_size = GetBaseSize(process_name);
	if (!current_process.base_size)
	{
		printf("[!] Could not get base size!\n");
		return false;
	}

	printf("Process information of %s\n", process_name.c_str());
	printf("PID: %i\n", current_process.PID);
	printf("Base Address: 0x%llx\n", current_process.base_address);
	printf("Base Size: 0x%llx\n", current_process.base_size);

	PROCESS_INITIALIZED = TRUE;

	return true;
}

DWORD Memory::GetPidFromName(std::string process_name)
{
	DWORD pid = 0;
	VMMDLL_PidGetFromName(this->vHandle, (LPSTR)process_name.c_str(), &pid);
	return pid;
}

std::vector<int> Memory::GetPidListFromName(std::string name)
{
	PVMMDLL_PROCESS_INFORMATION process_info = NULL;
	DWORD total_processes = 0;
	std::vector<int> list = { };

	if (!VMMDLL_ProcessGetInformationAll(this->vHandle, &process_info, &total_processes))
	{
		printf("[!] Failed to get process list\n");
		return list;
	}

	for (size_t i = 0; i < total_processes; i++)
	{
		auto process = process_info[i];
		if (strstr(process.szNameLong, name.c_str()))
			list.push_back(process.dwPID);
	}

	return list;
}

std::vector<std::string> Memory::GetModuleList(std::string process_name)
{
	std::vector<std::string> list = { };
	PVMMDLL_MAP_MODULE module_info = NULL;
	if (!VMMDLL_Map_GetModuleU(this->vHandle, current_process.PID, &module_info, VMMDLL_MODULE_FLAG_NORMAL))
	{
		printf("[!] Failed to get module list\n");
		return list;
	}

	for (size_t i = 0; i < module_info->cMap; i++)
	{
		auto module = module_info->pMap[i];
		list.push_back(module.uszText);
	}

	return list;
}

VMMDLL_PROCESS_INFORMATION Memory::GetProcessInformation()
{
	VMMDLL_PROCESS_INFORMATION info = { };
	SIZE_T process_information = sizeof(VMMDLL_PROCESS_INFORMATION);
	ZeroMemory(&info, sizeof(VMMDLL_PROCESS_INFORMATION));
	info.magic = VMMDLL_PROCESS_INFORMATION_MAGIC;
	info.wVersion = VMMDLL_PROCESS_INFORMATION_VERSION;

	if (!VMMDLL_ProcessGetInformation(this->vHandle, current_process.PID, &info, &process_information))
	{
		printf("[!] Failed to find process information\n");
		return { };
	}

	return info;
}

CUSTOM_PEB Memory::GetProcessPeb()
{
	auto info = GetProcessInformation();
	if (info.win.vaPEB)
	{
		return Read<CUSTOM_PEB>(info.win.vaPEB);
	}
	printf("[!] Failed to find the processes PEB\n");
	return { };
}

uintptr_t Memory::GetProcessPebAddress()
{
	auto info = GetProcessInformation();
	if (info.win.vaPEB)
	{
		return info.win.vaPEB;
	}
	printf("[!] Failed to find the processes PEB\n");
	return { };
}

size_t Memory::GetBaseDaddy(std::string module_name)
{
	std::wstring str(module_name.begin(), module_name.end());

	PVMMDLL_MAP_MODULEENTRY module_info;
	if (!VMMDLL_Map_GetModuleFromNameW(this->vHandle, current_process.PID, const_cast<LPWSTR>(str.c_str()), &module_info, VMMDLL_MODULE_FLAG_NORMAL))
	{
		printf("[!] Couldn't find Base Address for %s\n", module_name.c_str());
		return 0;
	}

	printf("[+] Found Base Address for %s at 0x%p\n", module_name.c_str(), module_info->vaBase);
	return module_info->vaBase;
}

size_t Memory::GetBaseSize(std::string module_name)
{
	std::wstring str(module_name.begin(), module_name.end());

	PVMMDLL_MAP_MODULEENTRY module_info;
	auto bResult = VMMDLL_Map_GetModuleFromNameW(this->vHandle, current_process.PID, const_cast<LPWSTR>(str.c_str()), &module_info, VMMDLL_MODULE_FLAG_NORMAL);
	if (bResult)
	{
		printf("[+] Found Base Size for %s at 0x%p\n", module_name.c_str(), module_info->cbImageSize);
		return module_info->cbImageSize;
	}
	return 0;
}

uintptr_t Memory::GetExportTableAddress(std::string import, std::string process, std::string module)
{
	PVMMDLL_MAP_EAT eat_map = NULL;
	PVMMDLL_MAP_EATENTRY export_entry = NULL;
	bool result = VMMDLL_Map_GetEATU(mem.vHandle, mem.GetPidFromName(process) /*| VMMDLL_PID_PROCESS_WITH_KERNELMEMORY*/, const_cast<LPSTR>(module.c_str()), &eat_map);
	if (!result)
	{
		printf("[!] Failed to get Export Table\n");
		return 0;
	}

	if (eat_map->dwVersion != VMMDLL_MAP_EAT_VERSION)
	{
		VMMDLL_MemFree(eat_map);
		eat_map = NULL;
		printf("[!] Invalid VMM Map Version\n");
		return 0;
	}

	uintptr_t addr = 0;
	for (int i = 0; i < eat_map->cMap; i++)
	{
		export_entry = eat_map->pMap + i;
		if (strcmp(export_entry->uszFunction, import.c_str()) == 0)
		{
			addr = export_entry->vaFunction;
			break;
		}
	}

	VMMDLL_MemFree(eat_map);
	eat_map = NULL;

	return addr;
}

uintptr_t Memory::GetImportTableAddress(std::string import, std::string process, std::string module)
{
	PVMMDLL_MAP_IAT iat_map = NULL;
	PVMMDLL_MAP_IATENTRY import_entry = NULL;
	bool result = VMMDLL_Map_GetIATU(mem.vHandle, mem.GetPidFromName(process) /*| VMMDLL_PID_PROCESS_WITH_KERNELMEMORY*/, const_cast<LPSTR>(module.c_str()), &iat_map);
	if (!result)
	{
		printf("[!] Failed to get Import Table\n");
		return 0;
	}

	if (iat_map->dwVersion != VMMDLL_MAP_IAT_VERSION)
	{
		VMMDLL_MemFree(iat_map);
		iat_map = NULL;
		printf("[!] Invalid VMM Map Version\n");
		return 0;
	}

	uintptr_t addr = 0;
	for (int i = 0; i < iat_map->cMap; i++)
	{
		import_entry = iat_map->pMap + i;
		if (strcmp(import_entry->uszFunction, import.c_str()) == 0)
		{
			addr = import_entry->vaFunction;
			break;
		}
	}

	VMMDLL_MemFree(iat_map);
	iat_map = NULL;

	return addr;
}

uint64_t cbSize = 0x80000;
//callback for VfsFileListU
VOID cbAddFile(_Inout_ HANDLE h, _In_ LPCSTR uszName, _In_ ULONG64 cb, _In_opt_ PVMMDLL_VFS_FILELIST_EXINFO pExInfo)
{
	if (strcmp(uszName, "dtb.txt") == 0)
		cbSize = cb;
}

struct Info
{
	uint32_t index;
	uint32_t process_id;
	uint64_t dtb;
	uint64_t kernelAddr;
	std::string name;
};

bool Memory::FixCr3()
{
	PVMMDLL_MAP_MODULEENTRY module_entry = NULL;
	bool result = VMMDLL_Map_GetModuleFromNameU(this->vHandle, current_process.PID, const_cast<LPSTR>(current_process.process_name.c_str()), &module_entry, NULL);
	if (result)
		return true; //Doesn't need to be patched lol

	if (!VMMDLL_InitializePlugins(this->vHandle))
	{
		printf("[-] Failed VMMDLL_InitializePlugins call\n");
		return false;
	}

	//have to sleep a little or we try reading the file before the plugin initializes fully
	std::this_thread::sleep_for(std::chrono::milliseconds(500));

	while (true)
	{
		BYTE bytes[4] = {0};
		DWORD i = 0;
		auto nt = VMMDLL_VfsReadW(this->vHandle, const_cast<LPWSTR>(L"\\misc\\procinfo\\progress_percent.txt"), bytes, 3, &i, 0);
		if (nt == VMMDLL_STATUS_SUCCESS && atoi(reinterpret_cast<LPSTR>(bytes)) == 100)
			break;

		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	VMMDLL_VFS_FILELIST2 VfsFileList;
	VfsFileList.dwVersion = VMMDLL_VFS_FILELIST_VERSION;
	VfsFileList.h = 0;
	VfsFileList.pfnAddDirectory = 0;
	VfsFileList.pfnAddFile = cbAddFile; //dumb af callback who made this system

	result = VMMDLL_VfsListU(this->vHandle, const_cast<LPSTR>("\\misc\\procinfo\\"), &VfsFileList);
	if (!result)
		return false;

	//read the data from the txt and parse it
	const size_t buffer_size = cbSize;
	std::unique_ptr<BYTE[]> bytes(new BYTE[buffer_size]);
	DWORD j = 0;
	auto nt = VMMDLL_VfsReadW(this->vHandle, const_cast<LPWSTR>(L"\\misc\\procinfo\\dtb.txt"), bytes.get(), buffer_size - 1, &j, 0);
	if (nt != VMMDLL_STATUS_SUCCESS)
		return false;

	std::vector<uint64_t> possible_dtbs = { };
	std::string lines(reinterpret_cast<char*>(bytes.get()));
	std::istringstream iss(lines);
	std::string line = "";

	while (std::getline(iss, line))
	{
		Info info = { };

		std::istringstream info_ss(line);
		if (info_ss >> std::hex >> info.index >> std::dec >> info.process_id >> std::hex >> info.dtb >> info.kernelAddr >> info.name)
		{
			if (info.process_id == 0) //parts that lack a name or have a NULL pid are suspects
				possible_dtbs.push_back(info.dtb);
			if (current_process.process_name.find(info.name) != std::string::npos)
				possible_dtbs.push_back(info.dtb);
		}
	}

	//loop over possible dtbs and set the config to use it til we find the correct one
	for (size_t i = 0; i < possible_dtbs.size(); i++)
	{
		auto dtb = possible_dtbs[i];
		VMMDLL_ConfigSet(this->vHandle, VMMDLL_OPT_PROCESS_DTB | current_process.PID, dtb);
		result = VMMDLL_Map_GetModuleFromNameU(this->vHandle, current_process.PID, const_cast<LPSTR>(current_process.process_name.c_str()), &module_entry, NULL);
		if (result)
		{
			printf("[+] Patched DTB\n");
			return true;
		}
	}

	printf("[-] Failed to patch module\n");
	return false;
}

bool Memory::DumpMemory(uintptr_t address, std::string path)
{
	printf("[!] Memory dumping currently does not rebuild the IAT table, imports will be missing from the dump.\n");
	IMAGE_DOS_HEADER dos { };
	Read(address, &dos, sizeof(IMAGE_DOS_HEADER));

	//Check if memory has a PE 
	if (dos.e_magic != 0x5A4D) //Check if it starts with MZ
	{
		printf("[-] Invalid PE Header\n");
		return false;
	}

	IMAGE_NT_HEADERS64 nt;
	Read(address + dos.e_lfanew, &nt, sizeof(IMAGE_NT_HEADERS64));

	//Sanity check
	if (nt.Signature != IMAGE_NT_SIGNATURE || nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC)
	{
		printf("[-] Failed signature check\n");
		return false;
	}
	//Shouldn't change ever. so const 
	const size_t target_size = nt.OptionalHeader.SizeOfImage;
	//Crashes if we don't make it a ptr :(
	auto target = std::unique_ptr<uint8_t[]>(new uint8_t[target_size]);

	//Read whole modules memory
	Read(address, target.get(), target_size);
	auto nt_header = (PIMAGE_NT_HEADERS64)(target.get() + dos.e_lfanew);
	auto sections = (PIMAGE_SECTION_HEADER)(target.get() + dos.e_lfanew + FIELD_OFFSET(IMAGE_NT_HEADERS, OptionalHeader) + nt.FileHeader.SizeOfOptionalHeader);

	for (size_t i = 0; i < nt.FileHeader.NumberOfSections; i++, sections++)
	{
		//Rewrite the file offsets to the virtual addresses
		printf("[!] Rewriting file offsets at 0x%p size 0x%p\n", sections->VirtualAddress, sections->Misc.VirtualSize);
		sections->PointerToRawData = sections->VirtualAddress;
		sections->SizeOfRawData = sections->Misc.VirtualSize;
	}

	auto debug = (PIMAGE_DEBUG_DIRECTORY)(target.get() + nt_header->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG].VirtualAddress);
	debug->PointerToRawData = debug->AddressOfRawData;

	//Dump file
	const auto dumped_file = CreateFileW(std::wstring(path.begin(), path.end()).c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_COMPRESSED, NULL);
	if (dumped_file == INVALID_HANDLE_VALUE)
	{
		printf("[!] Failed creating file: %i\n", GetLastError());
		return false;
	}

	if (!WriteFile(dumped_file, target.get(), static_cast<DWORD>(target_size), NULL, NULL))
	{
		printf("[!] Failed writing file: %i\n", GetLastError());
		CloseHandle(dumped_file);
		return false;
	}

	printf("[+] Successfully dumped memory at %s\n", path.c_str());
	CloseHandle(dumped_file);
	return true;
}

static const char* hexdigits =
	"\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
	"\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
	"\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
	"\000\001\002\003\004\005\006\007\010\011\000\000\000\000\000\000"
	"\000\012\013\014\015\016\017\000\000\000\000\000\000\000\000\000"
	"\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
	"\000\012\013\014\015\016\017\000\000\000\000\000\000\000\000\000"
	"\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
	"\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
	"\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
	"\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
	"\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
	"\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
	"\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
	"\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
	"\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000";

static uint8_t GetByte(const char* hex)
{
	return static_cast<uint8_t>((hexdigits[hex[0]] << 4) | (hexdigits[hex[1]]));
}

uint64_t Memory::FindSignature(const char* signature, const char* mask, uint64_t range_start, uint64_t range_end, int PID)
{
	if (!signature || signature[0] == '\0' || !mask || mask[0] == '\0' || range_start >= range_end)
		return 0;

	if (PID == 0)
		PID = current_process.PID;

	std::vector<uint8_t> buffer(range_end - range_start);
	if (!VMMDLL_MemReadEx(this->vHandle, PID, range_start, buffer.data(), buffer.size(), 0, VMMDLL_FLAG_NOCACHE))
		return 0;

	const char* pat = signature;
	const char* pat_mask = mask;
	uint64_t first_match = 0;

	for (uint64_t i = range_start; i < range_end; i++)
	{
		if (*pat_mask == 'x' && (*pat == '?' || buffer[i - range_start] == GetByte(pat)))
		{
			if (!first_match)
				first_match = i;

			// Move to the next pattern byte
			pat += (*pat == '?') ? 2 : 3;
			pat_mask++;

			// Check if the pattern is fully matched
			if (!*pat_mask)
				break;
		}
		else
		{
			// Reset pattern matching
			pat = signature;
			pat_mask = mask;
			first_match = 0;
		}
	}

	return first_match;
}

bool Memory::Write(uintptr_t address, void* buffer, size_t size) const
{
	if (!VMMDLL_MemWrite(this->vHandle, current_process.PID, address, static_cast<PBYTE>(buffer), size))
	{
		DMA_FAIL_LOG("[!] Failed to write Memory at 0x%p\n", address);
		return false;
	}
	return true;
}

bool Memory::Write(uintptr_t address, void* buffer, size_t size, int pid) const
{
	if (!VMMDLL_MemWrite(this->vHandle, pid, address, static_cast<PBYTE>(buffer), size))
	{
		DMA_FAIL_LOG("[!] Failed to write Memory at 0x%p\n", address);
		return false;
	}
	return true;
}

bool Memory::Read(uintptr_t address, void* buffer, size_t size) const
{
	DWORD read_size = 0;
	if (!VMMDLL_MemReadEx(this->vHandle, current_process.PID, address, static_cast<PBYTE>(buffer), size, &read_size, VMMDLL_FLAG_NOCACHE))
	{
		DMA_FAIL_LOG("[!] Failed to read Memory at 0x%p\n", address);
		return false;
	}

	return (read_size == size);
}

bool Memory::Read(uintptr_t address, void* buffer, size_t size, int pid) const
{
	DWORD read_size = 0;
	if (!VMMDLL_MemReadEx(this->vHandle, pid, address, static_cast<PBYTE>(buffer), size, &read_size, VMMDLL_FLAG_NOCACHE))
	{
		DMA_FAIL_LOG("[!] Failed to read Memory at 0x%p\n", address);
		return false;
	}
	return (read_size == size);
}

VMMDLL_SCATTER_HANDLE Memory::CreateScatterHandle() const
{
	const VMMDLL_SCATTER_HANDLE ScatterHandle = VMMDLL_Scatter_Initialize(this->vHandle, current_process.PID, VMMDLL_FLAG_NOCACHE);
	if (!ScatterHandle)
		DMA_FAIL_LOG("[!] Failed to create scatter handle\n");
	return ScatterHandle;
}

VMMDLL_SCATTER_HANDLE Memory::CreateScatterHandle(int pid) const
{
	const VMMDLL_SCATTER_HANDLE ScatterHandle = VMMDLL_Scatter_Initialize(this->vHandle, pid, VMMDLL_FLAG_NOCACHE);
	if (!ScatterHandle)
		DMA_FAIL_LOG("[!] Failed to create scatter handle\n");
	return ScatterHandle;
}

VMMDLL_SCATTER_HANDLE Memory::GetScatterHandle() const
{
	thread_local VMMDLL_SCATTER_HANDLE tlsScatterHandle = nullptr;
	if (!tlsScatterHandle) {
		tlsScatterHandle = VMMDLL_Scatter_Initialize(
			this->vHandle,
			current_process.PID,
			VMMDLL_FLAG_NOCACHE
		);
	}

	return tlsScatterHandle;
}

void Memory::CloseScatterHandle(VMMDLL_SCATTER_HANDLE handle)
{
	VMMDLL_Scatter_CloseHandle(handle);
}

void Memory::AddScatterReadRequest(VMMDLL_SCATTER_HANDLE handle, uint64_t address, void* buffer, size_t size)
{
	AddPageOptimizedScatterRead(handle, address, buffer, size);
}

void Memory::AddPageOptimizedScatterRead(VMMDLL_SCATTER_HANDLE handle, uint64_t address, void* buffer, size_t size)
{
	if (!handle || !buffer || size == 0) {
		return;
	}

	auto* out = static_cast<PBYTE>(buffer);
	size_t remaining = size;
	uint64_t curAddr = address;

	while (remaining > 0) {
		const uint64_t pageOffset = curAddr & 0xFFF;
		const size_t chunk = (std::min)(remaining, static_cast<size_t>(0x1000 - pageOffset));

		if (!VMMDLL_Scatter_PrepareEx(handle, curAddr, static_cast<DWORD>(chunk), out, NULL)) {
			DMA_FAIL_LOG(
				"[!] Failed to prepare page-optimized scatter read at 0x%p (page 0x%llx, chunk %zu)\n",
				curAddr,
				curAddr & ~0xFFFull,
				chunk
			);
		}

		curAddr += chunk;
		out += chunk;
		remaining -= chunk;
	}
}

void Memory::AddScatterWriteRequest(VMMDLL_SCATTER_HANDLE handle, uint64_t address, void* buffer, size_t size)
{
	if (!VMMDLL_Scatter_PrepareWrite(handle, address, static_cast<PBYTE>(buffer), size))
	{
		DMA_FAIL_LOG("[!] Failed to prepare scatter write at 0x%p\n", address);
	}
}

void Memory::ExecuteReadScatter(VMMDLL_SCATTER_HANDLE handle, int pid)
{
	if (pid == 0)
		pid = current_process.PID;

	if (!VMMDLL_Scatter_ExecuteRead(handle))
	{
		DMA_FAIL_LOG("[-] Failed to Execute Scatter Read\n");
	}
	//Clear after using it
	if (!VMMDLL_Scatter_Clear(handle, pid, VMMDLL_FLAG_NOCACHE))
	{
		DMA_FAIL_LOG("[-] Failed to clear Scatter\n");
	}
}

void Memory::ExecuteWriteScatter(VMMDLL_SCATTER_HANDLE handle, int pid)
{
	if (pid == 0)
		pid = current_process.PID;

	if (!VMMDLL_Scatter_Execute(handle))
	{
		DMA_FAIL_LOG("[-] Failed to Execute Scatter Read\n");
	}
	//Clear after using it
	if (!VMMDLL_Scatter_Clear(handle, pid, VMMDLL_FLAG_NOCACHE))
	{
		DMA_FAIL_LOG("[-] Failed to clear Scatter\n");
	}
}
