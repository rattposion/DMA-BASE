#include "global.h"

int __fastcall main(void) {
	const auto readyForUIAccess = Render::PrepareForUIAccess();
	if (readyForUIAccess != ERROR_SUCCESS) {
		Logger.log(
			LogLevel::ERROR, 
			("Could not assign UI access token - ensure you are running as administrator! Status: {:#x}"),
			readyForUIAccess
		);

		std::getchar();
		return EXIT_FAILURE;
	}

	timeBeginPeriod(1);

	Config::LoadSettings();

	const auto connectResult = DMAInterface::ConnectToDMA();

	if (!connectResult) {
		Logger.log(
			LogLevel::ERROR,
			("Could not connect to DMA interface!")
		); 
		
		std::getchar();
		return EXIT_FAILURE;
	} Logger.log(("Connected to DMA interface successfully!"));

	DMAInterface::g_processId = DMAInterface::GetProcessPID(("cod.exe"));
	if (!DMAInterface::g_processId) {
		Logger.log(("Please start Call Of Duty (waiting)..."));

		while (!DMAInterface::g_processId) {
			DMAInterface::g_processId = DMAInterface::GetProcessPID(("cod.exe"));
			Sleep(1000);
		}
	}

	Logger.log(("Testing DMA interface..."));
	globals::g_baseAddress = DMAInterface::GetProcessBaseAddress();
	if (!globals::g_baseAddress) {
		Logger.log(
			LogLevel::ERROR,
			("Getting base address [FAIL]! Last Status: {:#x}"),
			DMAInterface::g_lastStatus
		);

		std::getchar();
		return EXIT_FAILURE;
	}  Logger.log(("Got base address {:#x} [PASS]!"), globals::g_baseAddress);

	globals::g_peb = DMAInterface::GetProcessPEB();
	if (!globals::g_peb) {
		Logger.log(
			LogLevel::ERROR,
			("Getting process environment block (PEB) [FAIL]! Last Status: {:#x}"),
			DMAInterface::g_lastStatus
		);

		std::getchar();
		return EXIT_FAILURE;
	} Logger.log(("Got process environment block (PEB) {:#x} [PASS]!"), globals::g_peb);

	DMAInterface::g_DTB = DMAInterface::GetProcessCR3();
	if (!DMAInterface::g_DTB) {
		Logger.log(
			LogLevel::ERROR,
			("Getting process page mapping level 4 (PML4/CR3) [FAIL]! Last Status: {:#x}"),
			DMAInterface::g_lastStatus
		);

		std::getchar();
		return EXIT_FAILURE;
	} Logger.log(("Fixed process page mapping level 4 (PML4/CR3) {:#x} [PASS]!"), DMAInterface::g_DTB);

	Cache::Start();

	Render::Setup();
	Render::MainLoop();

	Cache::Stop();
	Aimbot::Cleanup();

	timeEndPeriod(1); 

	return 0;
}
