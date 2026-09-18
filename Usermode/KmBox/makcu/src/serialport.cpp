#include "../include/serialport.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <vector>
#include <string>
#include <cstring>
#include <chrono>
#include <future>
#include <expected>
#include <charconv>
#include <string_view>
#include <utility>
#include <limits>
#include <variant>

#ifdef _WIN32
#include <setupapi.h>
#include <devguid.h>
#include <cfgmgr32.h>
#pragma comment(lib, "setupapi.lib")
#else
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <dirent.h>
#include <fstream>
#include <regex>
#include <libudev.h>
#include <errno.h>
#include <poll.h>
#endif

namespace makcu {

namespace {
using ParsedTrackedResponse = std::pair<int, std::string>;

std::expected<ParsedTrackedResponse, std::monostate> parseTrackedResponse(std::string_view content) {
	const size_t hashPos = content.find('#');
	if (hashPos == std::string_view::npos) {
		return std::unexpected(std::monostate{});
	}

	const std::string_view idAndPayload = content.substr(hashPos + 1);
	const size_t colonPos = idAndPayload.find(':');
	if (colonPos == std::string_view::npos) {
		return std::unexpected(std::monostate{});
	}

	const std::string_view idView = idAndPayload.substr(0, colonPos);
	int cmdId = 0;
	const auto [parseEnd, parseErr] =
	    std::from_chars(idView.data(), idView.data() + idView.size(), cmdId);
	if (parseErr != std::errc{} || parseEnd != idView.data() + idView.size()) {
		return std::unexpected(std::monostate{});
	}

	std::string result(idAndPayload.substr(colonPos + 1));
	return ParsedTrackedResponse{cmdId, std::move(result)};
}
} // namespace

SerialPort::SerialPort()
	: m_baudRate(115200)
	, m_timeout(100)
	, m_isOpen(false)
#ifdef _WIN32
	, m_handle(INVALID_HANDLE_VALUE)
#else
	, m_fd(-1)
#endif
{
#ifdef _WIN32
	memset(&m_dcb, 0, sizeof(m_dcb));
	memset(&m_timeouts, 0, sizeof(m_timeouts));
#endif
}

SerialPort::~SerialPort() {
	close();
}

bool SerialPort::open(const std::string& port, uint32_t baudRate) {
	for (;;) {
		std::unique_lock<std::mutex> lock(m_mutex);
		if (m_isOpen) {
			lock.unlock();
			close();
			continue;
		}

		m_portName = port;
		m_baudRate.store(baudRate, std::memory_order_relaxed);

#ifdef _WIN32
		std::string devicePath = "\\\\.\\" + port;
#else
		std::string devicePath = "/dev/" + port;
#endif

		if (!platformOpen(devicePath)) {
			return false;
		}

		if (!platformConfigurePort()) {
			platformClose();
			return false;
		}

		m_isOpen = true;

		m_listenerThread = std::jthread([this](std::stop_token stopToken) {
			listenerLoop(stopToken);
		});

		return true;
	}
}

void SerialPort::close() {
	if (m_listenerThread.joinable()) {
		m_listenerThread.request_stop();
		if (std::this_thread::get_id() != m_listenerThread.get_id()) {
			m_listenerThread.join();
		}
	}

	std::lock_guard<std::mutex> lock(m_mutex);

	if (!m_isOpen.load(std::memory_order_acquire)) {
		return;
	}

	m_isOpen.store(false, std::memory_order_release);

	std::vector<std::unique_ptr<PendingCommand>> commandsToCancel;
	{
		std::lock_guard<std::mutex> cmdLock(m_commandMutex);
		commandsToCancel.reserve(m_pendingCommands.size());
		for (auto& [id, cmd] : m_pendingCommands) {
			commandsToCancel.push_back(std::move(cmd));
		}
		m_pendingCommands.clear();
		m_pendingCommandOrder.clear();
	}

	for (auto& cmd : commandsToCancel) {
		try {
			cmd->promise.set_exception(std::make_exception_ptr(
			                               std::runtime_error("Connection closed")));
		}
		catch (...) {
		}
	}

	platformClose();
	m_lastButtonMask.store(0, std::memory_order_release);
}

bool SerialPort::isOpen() const noexcept {
	return m_isOpen;
}

bool SerialPort::isActuallyConnected() const {
	if (!m_isOpen) {
		return false;
	}

#ifdef _WIN32
	if (m_handle == INVALID_HANDLE_VALUE) {
		return false;
	}

	DCB dcb;
	return GetCommState(m_handle, &dcb) != 0;
#else
	if (m_fd < 0) {
		return false;
	}

	struct pollfd pfd;
	pfd.fd = m_fd;
	pfd.events = POLLERR | POLLHUP | POLLNVAL;
	pfd.revents = 0;

	int result = poll(&pfd, 1, 0);

	if (result < 0) {
		return false;
	}

	if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
		return false;
	}

	return true;
#endif
}

std::future<std::string> SerialPort::sendTrackedCommand(const std::string& command,
        bool expectResponse,
        std::chrono::milliseconds timeout) {
	if (!m_isOpen.load(std::memory_order_acquire)) {
		std::promise<std::string> promise;
		promise.set_exception(std::make_exception_ptr(
		                          std::runtime_error("Port not open")));
		return promise.get_future();
	}

	constexpr size_t MAX_COMMAND_LENGTH = 512;
	if (command.length() > MAX_COMMAND_LENGTH) {
		std::promise<std::string> promise;
		promise.set_exception(std::make_exception_ptr(
		                          std::runtime_error("Command too long (max " + std::to_string(MAX_COMMAND_LENGTH) + " chars)")));
		return promise.get_future();
	}

	int cmdId = -1;
	std::future<std::string> future;

	{
		std::lock_guard<std::mutex> lock(m_commandMutex);
		cmdId = generateCommandId();
		if (cmdId <= 0) {
			std::promise<std::string> promise;
			promise.set_exception(std::make_exception_ptr(
			                          std::runtime_error("No command IDs available")));
			return promise.get_future();
		}

		auto pendingCmd = std::make_unique<PendingCommand>(cmdId, command, expectResponse, timeout);
		future = pendingCmd->promise.get_future();
		m_pendingCommands.emplace(cmdId, std::move(pendingCmd));
		m_pendingCommandOrder.push_back(cmdId);
	}

	std::string trackedCommand = expectResponse ?
	                             command + "#" + std::to_string(cmdId) + "\r\n" :
	                             command + "\r\n";

	ssize_t bytesWritten = platformWrite(trackedCommand.c_str(), trackedCommand.length());

	if (bytesWritten != static_cast<ssize_t>(trackedCommand.length())) {
		std::lock_guard<std::mutex> lock(m_commandMutex);
		auto it = m_pendingCommands.find(cmdId);
		if (it != m_pendingCommands.end()) {
			try {
				std::string errorMsg = "Write failed";
				if (bytesWritten < 0) {
					errorMsg += " (" + getLastPlatformError() + ")";
				}
				else {
					errorMsg += " (partial write: " + std::to_string(bytesWritten) +
					            "/" + std::to_string(trackedCommand.length()) + " bytes)";
				}
				it->second->promise.set_exception(std::make_exception_ptr(
				                                      std::runtime_error(errorMsg)));
			}
			catch (...) {
			}
			m_pendingCommands.erase(it);
			auto orderIt = std::find(m_pendingCommandOrder.begin(), m_pendingCommandOrder.end(), cmdId);
			if (orderIt != m_pendingCommandOrder.end()) {
				m_pendingCommandOrder.erase(orderIt);
			}
		}
	}

	platformFlush();
	return future;
}

bool SerialPort::sendCommand(const std::string& command) {
	if (!m_isOpen.load(std::memory_order_acquire)) {
		return false;
	}

	constexpr size_t MAX_COMMAND_LENGTH = 512;
	if (command.length() > MAX_COMMAND_LENGTH) {
		return false;
	}

	std::string fullCommand = command + "\r\n";

	ssize_t bytesWritten = platformWrite(fullCommand.c_str(), fullCommand.length());
	if (bytesWritten == static_cast<ssize_t>(fullCommand.length())) {
		return platformFlush();
	}

	return false;
}

void SerialPort::listenerLoop(std::stop_token stopToken) {
	std::vector<uint8_t> readBuffer(BUFFER_SIZE);
	std::vector<uint8_t> lineBuffer(LINE_BUFFER_SIZE);
	size_t linePos = 0;
	enum class ButtonPrefixState : uint8_t {
		NONE,
		K,
		KM,
		KM_DOT
	};
	ButtonPrefixState buttonPrefixState = ButtonPrefixState::NONE;

	auto lastCleanup = std::chrono::steady_clock::now();
	constexpr auto cleanupInterval = std::chrono::milliseconds(50);

	auto appendTextByte = [&](uint8_t byte) {
		if (byte == 0x0A) {
			if (linePos > 0) {
				std::string line(lineBuffer.begin(), lineBuffer.begin() + linePos);
				linePos = 0;
				if (!line.empty()) {
					processResponse(line);
				}
			}
			return;
		}

		if (byte == 0x0D) {
			return;
		}

		if (linePos < LINE_BUFFER_SIZE - 1) {
			lineBuffer[linePos++] = byte;
			return;
		}

		linePos = 0;
		buttonPrefixState = ButtonPrefixState::NONE;
	};

	while (!stopToken.stop_requested() && m_isOpen.load(std::memory_order_acquire)) {
		try {
			size_t bytesAvailable = platformBytesAvailable();
			if (bytesAvailable == 0) {
				std::this_thread::sleep_for(std::chrono::milliseconds(2));
				continue;
			}

			size_t bytesToRead = std::min<size_t>(bytesAvailable, static_cast<size_t>(BUFFER_SIZE));
			ssize_t bytesRead = platformRead(readBuffer.data(), bytesToRead);

			if (bytesRead <= 0) {
				std::this_thread::sleep_for(std::chrono::milliseconds(2));
				continue;
			}

			for (ssize_t i = 0; i < bytesRead; ++i) {
				const uint8_t byte = readBuffer[i];
				bool consumed = false;

				auto flushPrefixAsText = [&]() {
					switch (buttonPrefixState) {
						case ButtonPrefixState::K:
							appendTextByte('k');
							break;
						case ButtonPrefixState::KM:
							appendTextByte('k');
							appendTextByte('m');
							break;
						case ButtonPrefixState::KM_DOT:
							appendTextByte('k');
							appendTextByte('m');
							appendTextByte('.');
							break;
						case ButtonPrefixState::NONE:
						default:
							break;
					}
					buttonPrefixState = ButtonPrefixState::NONE;
				};

				if (linePos == 0 || buttonPrefixState != ButtonPrefixState::NONE) {
					switch (buttonPrefixState) {
						case ButtonPrefixState::NONE:
							if (byte == 'k') {
								buttonPrefixState = ButtonPrefixState::K;
								consumed = true;
							}
							break;
						case ButtonPrefixState::K:
							if (byte == 'm') {
								buttonPrefixState = ButtonPrefixState::KM;
								consumed = true;
							}
							else {
								flushPrefixAsText();
							}
							break;
						case ButtonPrefixState::KM:
							if (byte == '.') {
								buttonPrefixState = ButtonPrefixState::KM_DOT;
								consumed = true;
							}
							else {
								flushPrefixAsText();
							}
							break;
						case ButtonPrefixState::KM_DOT:
							if (byte < 32) {
								handleButtonData(byte);
								buttonPrefixState = ButtonPrefixState::NONE;
								consumed = true;
							}
							else {
								flushPrefixAsText();
							}
							break;
					}
				}

				if (consumed) {
					continue;
				}

				appendTextByte(byte);
			}

			auto now = std::chrono::steady_clock::now();
			if (now - lastCleanup > cleanupInterval) {
				cleanupTimedOutCommands();
				lastCleanup = now;
			}

		}
		catch (...) {
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
			if (!m_isOpen.load(std::memory_order_acquire)) {
				break;
			}
		}
	}
}

void SerialPort::handleButtonData(uint8_t data) {
	const uint8_t lastMask = m_lastButtonMask.load(std::memory_order_acquire);
	if (data == lastMask) {
		return;
	}

	m_lastButtonMask.store(data, std::memory_order_release);

	ButtonCallback callbackCopy;
	{
		std::lock_guard<std::mutex> lock(m_buttonCallbackMutex);
		callbackCopy = m_buttonCallback;
	}
	if (!callbackCopy) {
		return;
	}

	const uint8_t changedBits = static_cast<uint8_t>(data ^ lastMask);
	for (int bit = 0; bit < 5; ++bit) {
		if (changedBits & (1 << bit)) {
			const bool isPressed = (data & (1 << bit)) != 0;
			try {
				callbackCopy(bit, isPressed);
			}
			catch (...) {
			}
		}
	}
}

void SerialPort::processResponse(const std::string& response) {
	std::string_view content = response;
	if (content.starts_with(">>> ")) {
		content = content.substr(4);
	}

	const auto parsedResponse = parseTrackedResponse(content);
	std::string untrackedContent{content};
	if (parsedResponse) {
		auto [cmdId, result] = parsedResponse.value();
		{
			std::lock_guard<std::mutex> lock(m_commandMutex);
			auto it = m_pendingCommands.find(cmdId);
			if (it != m_pendingCommands.end()) {
				try {
					it->second->promise.set_value(result);
				}
				catch (...) {
				}
				m_pendingCommands.erase(it);
				auto orderIt = std::find(m_pendingCommandOrder.begin(), m_pendingCommandOrder.end(), cmdId);
				if (orderIt != m_pendingCommandOrder.end()) {
					m_pendingCommandOrder.erase(orderIt);
				}
				return;
			}
		}

		untrackedContent = std::move(result);
	}

	std::lock_guard<std::mutex> lock(m_commandMutex);
	while (!m_pendingCommandOrder.empty()) {
		int cmdId = m_pendingCommandOrder.front();
		m_pendingCommandOrder.pop_front();
		auto it = m_pendingCommands.find(cmdId);
		if (it == m_pendingCommands.end()) {
			continue;
		}
		try {
			it->second->promise.set_value(untrackedContent);
		}
		catch (...) {
		}
		m_pendingCommands.erase(it);
		break;
	}
}

void SerialPort::cleanupTimedOutCommands() {
	auto now = std::chrono::steady_clock::now();

	std::lock_guard<std::mutex> lock(m_commandMutex);
	auto it = m_pendingCommands.begin();
	while (it != m_pendingCommands.end()) {
		auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
		                   now - it->second->timestamp);

		if (elapsed > it->second->timeout) {
			int timedOutId = it->first;
			try {
				it->second->promise.set_exception(std::make_exception_ptr(
				                                      std::runtime_error("Command timeout")));
			}
			catch (...) {
			}
			it = m_pendingCommands.erase(it);
			auto orderIt = std::find(m_pendingCommandOrder.begin(), m_pendingCommandOrder.end(), timedOutId);
			if (orderIt != m_pendingCommandOrder.end()) {
				m_pendingCommandOrder.erase(orderIt);
			}
		}
		else {
			++it;
		}
	}
}

int SerialPort::generateCommandId() {
	constexpr uint32_t MAX_ATTEMPTS = 65536;
	for (uint32_t attempts = 0; attempts < MAX_ATTEMPTS; ++attempts) {
		constexpr uint32_t MAX_COMMAND_ID = static_cast<uint32_t>((std::numeric_limits<int>::max)());
		if (m_commandCounter >= MAX_COMMAND_ID) {
			m_commandCounter = 0;
		}

		++m_commandCounter;
		const int candidateId = static_cast<int>(m_commandCounter);
		if (m_pendingCommands.find(candidateId) == m_pendingCommands.end()) {
			return candidateId;
		}
	}

	return -1;
}

void SerialPort::setButtonCallback(ButtonCallback callback) {
	std::lock_guard<std::mutex> lock(m_buttonCallbackMutex);
	m_buttonCallback = std::move(callback);
}

bool SerialPort::setBaudRate(uint32_t baudRate) {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_baudRate.store(baudRate, std::memory_order_relaxed);

	if (m_isOpen) {
		return platformConfigurePort();
	}
	return true;
}

uint32_t SerialPort::getBaudRate() const noexcept {
	return m_baudRate.load(std::memory_order_relaxed);
}

std::string SerialPort::getPortName() const {
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_portName;
}

bool SerialPort::write(std::span<const uint8_t> data) {
	if (!m_isOpen.load(std::memory_order_acquire)) {
		return false;
	}

	if (data.empty()) {
		return true;
	}

	ssize_t bytesWritten = platformWrite(data.data(), data.size());
	if (bytesWritten != static_cast<ssize_t>(data.size())) {
		return false;
	}

	return platformFlush();
}

bool SerialPort::write(const std::vector<uint8_t>& data) {
	return write(std::span<const uint8_t>(data.data(), data.size()));
}

bool SerialPort::write(const std::string& data) {
	return sendCommand(data);
}

std::vector<uint8_t> SerialPort::read(size_t maxBytes) {
	std::vector<uint8_t> buffer;
	if (!m_isOpen || maxBytes == 0) {
		return buffer;
	}

	buffer.resize(maxBytes);
	ssize_t bytesRead = platformRead(buffer.data(), maxBytes);
	if (bytesRead > 0) {
		buffer.resize(bytesRead);
	}
	else {
		buffer.clear();
	}

	return buffer;
}

std::string SerialPort::readString(size_t maxBytes) {
	auto data = read(maxBytes);
	return std::string(data.begin(), data.end());
}

size_t SerialPort::available() const {
	std::lock_guard<std::mutex> lock(m_mutex);
	if (!m_isOpen) {
		return 0;
	}

	return platformBytesAvailable();
}

bool SerialPort::flush() {
	std::lock_guard<std::mutex> lock(m_mutex);
	if (!m_isOpen) {
		return false;
	}

	return platformFlush();
}

void SerialPort::setTimeout(uint32_t timeoutMs) {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_timeout.store(timeoutMs, std::memory_order_relaxed);
	if (m_isOpen.load(std::memory_order_acquire)) {
		platformUpdateTimeouts();
	}
}

uint32_t SerialPort::getTimeout() const noexcept {
    return m_timeout.load(std::memory_order_relaxed);
}

std::string SerialPort::getLastError() {
    return getLastPlatformError();
}

std::vector<std::string> SerialPort::getAvailablePorts() {
	std::vector<std::string> ports;

#ifdef _WIN32
	HKEY hKey;
	if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\DEVICEMAP\\SERIALCOMM",
	                  0, KEY_READ, &hKey) == ERROR_SUCCESS) {
		char valueName[256];
		char data[256];
		DWORD valueNameSize, dataSize, dataType;
		DWORD index = 0;

		while (true) {
			valueNameSize = sizeof(valueName);
			dataSize = sizeof(data);

			LONG result = RegEnumValueA(hKey, index++, valueName, &valueNameSize,
			                            nullptr, &dataType,
			                            reinterpret_cast<BYTE*>(data), &dataSize);

			if (result == ERROR_NO_MORE_ITEMS) {
				break;
			}

			if (result == ERROR_SUCCESS && dataType == REG_SZ) {
				ports.emplace_back(data);
			}
		}

		RegCloseKey(hKey);
	}
#else
	DIR* dir = opendir("/dev");
	if (dir) {
		struct dirent* entry;
		while ((entry = readdir(dir)) != nullptr) {
			std::string name(entry->d_name);
			if (name.substr(0, 6) == "ttyUSB" || name.substr(0, 6) == "ttyACM") {
				ports.push_back(name);
			}
		}
		closedir(dir);
	}
#endif

	std::sort(ports.begin(), ports.end());
	return ports;
}

std::vector<std::string> SerialPort::findMakcuPorts() {
	std::vector<std::string> makcuPorts;

#ifdef _WIN32
	auto allPorts = getAvailablePorts();
	HDEVINFO deviceInfoSet = SetupDiGetClassDevs(&GUID_DEVCLASS_PORTS,
	                         nullptr, nullptr, DIGCF_PRESENT);
	if (deviceInfoSet == INVALID_HANDLE_VALUE) {
		return makcuPorts;
	}

	SP_DEVINFO_DATA deviceInfoData;
	deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

	for (DWORD i = 0; SetupDiEnumDeviceInfo(deviceInfoSet, i, &deviceInfoData); i++) {
		char description[256] = { 0 };
		char portName[256] = { 0 };

		if (SetupDiGetDeviceRegistryPropertyA(deviceInfoSet, &deviceInfoData,
		                                      SPDRP_DEVICEDESC, nullptr,
		                                      reinterpret_cast<BYTE*>(description),
		                                      sizeof(description), nullptr)) {
			std::string desc(description);

			if (desc.find("USB-Enhanced-SERIAL CH343") != std::string::npos ||
			        desc.find("USB-SERIAL CH340") != std::string::npos) {

				HKEY hDeviceKey = SetupDiOpenDevRegKey(deviceInfoSet, &deviceInfoData,
				                                       DICS_FLAG_GLOBAL, 0,
				                                       DIREG_DEV, KEY_READ);
				if (hDeviceKey != INVALID_HANDLE_VALUE) {
					DWORD portNameSize = sizeof(portName);

					if (RegQueryValueExA(hDeviceKey, "PortName", nullptr, nullptr,
					                     reinterpret_cast<BYTE*>(portName),
					                     &portNameSize) == ERROR_SUCCESS) {
						std::string port(portName);
						if (std::find(allPorts.begin(), allPorts.end(), port) != allPorts.end()) {
							makcuPorts.emplace_back(port);
						}
					}
					RegCloseKey(hDeviceKey);
				}
			}
		}
	}

	SetupDiDestroyDeviceInfoList(deviceInfoSet);
#else
	struct udev* udev = udev_new();
	if (!udev) {
		return makcuPorts;
	}

	struct udev_enumerate* enumerate = udev_enumerate_new(udev);
	udev_enumerate_add_match_subsystem(enumerate, "tty");
	udev_enumerate_scan_devices(enumerate);

	struct udev_list_entry* devices = udev_enumerate_get_list_entry(enumerate);
	struct udev_list_entry* entry;

	udev_list_entry_foreach(entry, devices) {
		const char* path = udev_list_entry_get_name(entry);
		struct udev_device* dev = udev_device_new_from_syspath(udev, path);

		if (dev) {
			struct udev_device* parent = udev_device_get_parent_with_subsystem_devtype(dev, "usb", "usb_device");
			if (parent) {
				const char* idVendor = udev_device_get_sysattr_value(parent, "idVendor");
				const char* idProduct = udev_device_get_sysattr_value(parent, "idProduct");

				bool isMakcuDevice = false;

				if (idVendor && idProduct &&
				        strcmp(idVendor, "1a86") == 0 && strcmp(idProduct, "55d3") == 0) {
					isMakcuDevice = true;
				}

				if (!isMakcuDevice) {
					const char* product = udev_device_get_sysattr_value(parent, "product");
					if (product) {
						std::string productStr(product);
						if (productStr.find("USB-Enhanced-SERIAL CH343") != std::string::npos ||
						        productStr.find("USB-SERIAL CH340") != std::string::npos) {
							isMakcuDevice = true;
						}
					}
				}

				if (isMakcuDevice) {
					const char* devNode = udev_device_get_devnode(dev);
					if (devNode) {
						std::string portName = std::string(devNode).substr(5);
						makcuPorts.push_back(portName);
					}
				}
			}
			udev_device_unref(dev);
		}
	}

	udev_enumerate_unref(enumerate);
	udev_unref(udev);
#endif

	std::sort(makcuPorts.begin(), makcuPorts.end());
	makcuPorts.erase(std::unique(makcuPorts.begin(), makcuPorts.end()), makcuPorts.end());
	return makcuPorts;
}

bool SerialPort::platformOpen(const std::string& devicePath) {
	std::lock_guard<std::mutex> nativeLock(m_nativeHandleMutex);
#ifdef _WIN32
	m_handle = CreateFileA(
	               devicePath.c_str(),
	               GENERIC_READ | GENERIC_WRITE,
	               0,
	               nullptr,
	               OPEN_EXISTING,
	               FILE_ATTRIBUTE_NORMAL,
	               nullptr
	           );
	return m_handle != INVALID_HANDLE_VALUE;
#else
	m_fd = ::open(devicePath.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
	return m_fd >= 0;
#endif
}

void SerialPort::platformClose() {
	std::lock_guard<std::mutex> nativeLock(m_nativeHandleMutex);
#ifdef _WIN32
	if (m_handle != INVALID_HANDLE_VALUE) {
		CloseHandle(m_handle);
		m_handle = INVALID_HANDLE_VALUE;
	}
#else
	if (m_fd >= 0) {
		::close(m_fd);
		m_fd = -1;
	}
#endif
}

bool SerialPort::platformConfigurePort() {
	std::lock_guard<std::mutex> nativeLock(m_nativeHandleMutex);
#ifdef _WIN32
	m_dcb.DCBlength = sizeof(DCB);

	if (!GetCommState(m_handle, &m_dcb)) {
		return false;
	}

	m_dcb.BaudRate = m_baudRate.load(std::memory_order_relaxed);
	m_dcb.ByteSize = 8;
	m_dcb.Parity = NOPARITY;
	m_dcb.StopBits = ONESTOPBIT;
	m_dcb.fBinary = TRUE;
	m_dcb.fParity = FALSE;
	m_dcb.fOutxCtsFlow = FALSE;
	m_dcb.fOutxDsrFlow = FALSE;
	m_dcb.fDtrControl = DTR_CONTROL_DISABLE;
	m_dcb.fDsrSensitivity = FALSE;
	m_dcb.fTXContinueOnXoff = FALSE;
	m_dcb.fOutX = FALSE;
	m_dcb.fInX = FALSE;
	m_dcb.fErrorChar = FALSE;
	m_dcb.fNull = FALSE;
	m_dcb.fRtsControl = RTS_CONTROL_DISABLE;
	m_dcb.fAbortOnError = FALSE;

	if (!SetCommState(m_handle, &m_dcb)) {
		return false;
	}

	platformUpdateTimeoutsUnlocked();
	return true;
#else
	return false;
#endif
}

void SerialPort::platformUpdateTimeouts() {
	std::lock_guard<std::mutex> nativeLock(m_nativeHandleMutex);
	platformUpdateTimeoutsUnlocked();
}

void SerialPort::platformUpdateTimeoutsUnlocked() {
#ifdef _WIN32
	m_timeouts.ReadIntervalTimeout = 1;
	m_timeouts.ReadTotalTimeoutConstant = 10;
	m_timeouts.ReadTotalTimeoutMultiplier = 1;
	m_timeouts.WriteTotalTimeoutConstant = 10;
	m_timeouts.WriteTotalTimeoutMultiplier = 1;
	SetCommTimeouts(m_handle, &m_timeouts);
#endif
}

ssize_t SerialPort::platformWrite(const void* data, size_t length) {
	std::lock_guard<std::mutex> nativeLock(m_nativeHandleMutex);
#ifdef _WIN32
	DWORD bytesWritten = 0;
	bool success = WriteFile(m_handle, data, static_cast<DWORD>(length), &bytesWritten, nullptr);
	return success ? static_cast<ssize_t>(bytesWritten) : -1;
#else
	return -1;
#endif
}

ssize_t SerialPort::platformRead(void* buffer, size_t maxBytes) {
	std::lock_guard<std::mutex> nativeLock(m_nativeHandleMutex);
#ifdef _WIN32
	DWORD bytesRead = 0;
	bool success = ReadFile(m_handle, buffer, static_cast<DWORD>(maxBytes), &bytesRead, nullptr);
	return success ? static_cast<ssize_t>(bytesRead) : -1;
#else
	return -1;
#endif
}

size_t SerialPort::platformBytesAvailable() const {
	std::lock_guard<std::mutex> nativeLock(m_nativeHandleMutex);
#ifdef _WIN32
	COMSTAT comStat;
	DWORD errors;
	if (ClearCommError(m_handle, &errors, &comStat)) {
		return comStat.cbInQue;
	}
	return 0;
#else
	return 0;
#endif
}

bool SerialPort::platformFlush() {
	std::lock_guard<std::mutex> nativeLock(m_nativeHandleMutex);
#ifdef _WIN32
	return FlushFileBuffers(m_handle) != 0;
#else
	return false;
#endif
}

std::string SerialPort::getLastPlatformError() {
#ifdef _WIN32
	DWORD error = GetLastError();
	return "Windows error: " + std::to_string(error);
#else
	return "errno: " + std::to_string(errno);
#endif
}

} // namespace makcu

