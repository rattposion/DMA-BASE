#include "global.h"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>

namespace Config::Aim {
	char kmboxIp[32] = "192.168.2.188";
	char kmboxPort[8] = "5194";
	char kmboxMac[16] = "15CB7019";
}

namespace {
	std::string GetConfigPath() {
		char modulePath[MAX_PATH] = {};
		if (GetModuleFileNameA(nullptr, modulePath, MAX_PATH) == 0) {
			return "Usermode.ini";
		}

		std::string path(modulePath);
		const size_t slash = path.find_last_of("\\/");
		if (slash != std::string::npos) {
			path.resize(slash + 1);
		}
		path += "Usermode.ini";
		return path;
	}

	std::string Trim(std::string value) {
		while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) {
			value.erase(value.begin());
		}
		while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
			value.pop_back();
		}
		return value;
	}

	bool ParseBool(const char* value, bool& out) {
		if (_stricmp(value, "1") == 0 || _stricmp(value, "true") == 0 || _stricmp(value, "yes") == 0) {
			out = true;
			return true;
		}
		if (_stricmp(value, "0") == 0 || _stricmp(value, "false") == 0 || _stricmp(value, "no") == 0) {
			out = false;
			return true;
		}
		return false;
	}

	bool ParseInt(const char* value, int& out) {
		char* end = nullptr;
		const long parsed = std::strtol(value, &end, 10);
		if (end == value || *end != '\0') {
			return false;
		}
		out = static_cast<int>(parsed);
		return true;
	}

	bool ParseFloat(const char* value, float& out) {
		char* end = nullptr;
		const float parsed = std::strtof(value, &end);
		if (end == value || *end != '\0') {
			return false;
		}
		out = parsed;
		return true;
	}

	void WriteBool(std::ofstream& file, const char* key, bool value) {
		file << key << '=' << (value ? '1' : '0') << '\n';
	}

	void WriteInt(std::ofstream& file, const char* key, int value) {
		file << key << '=' << value << '\n';
	}

	void WriteFloat(std::ofstream& file, const char* key, float value) {
		file << key << '=' << value << '\n';
	}

	void WriteString(std::ofstream& file, const char* key, const char* value) {
		file << key << '=' << value << '\n';
	}

	void WriteColor4(std::ofstream& file, const char* prefix, const ImVec4& color) {
		for (int i = 0; i < 4; ++i) {
			char key[64];
			std::snprintf(key, sizeof(key), "%s%d", prefix, i);
			WriteFloat(file, key, (&color.x)[i]);
		}
	}

	void SanitizeKmboxSettings() {
		if (Config::Aim::kmboxDevice < 0 || Config::Aim::kmboxDevice > 2) {
			Config::Aim::kmboxDevice = 0;
		}

		if (Config::Aim::kmboxMonitorPort < 0 || Config::Aim::kmboxMonitorPort > 65535) {
			Config::Aim::kmboxMonitorPort = 8888;
		}

		if (Config::Aim::kmboxComPort < 0) {
			Config::Aim::kmboxComPort = 0;
		}
	}

	bool LoadColor4Key(const char* key, const char* prefix, const char* value, ImVec4& color) {
		const size_t prefixLen = std::strlen(prefix);
		if (std::strncmp(key, prefix, prefixLen) != 0 || key[prefixLen] < '0' || key[prefixLen] > '3' || key[prefixLen + 1] != '\0') {
			return false;
		}

		const int index = key[prefixLen] - '0';
		float parsed = 0.f;
		if (!ParseFloat(value, parsed)) {
			return false;
		}

		(&color.x)[index] = parsed;
		return true;
	}
}

void Config::LoadSettings() {
	const std::string configPath = GetConfigPath();
	std::ifstream file(configPath);
	if (!file.is_open()) {
		return;
	}

	std::string line;
	while (std::getline(file, line)) {
		line = Trim(line);
		if (line.empty() || line[0] == '#' || line[0] == ';') {
			continue;
		}

		const size_t equalPos = line.find('=');
		if (equalPos == std::string::npos) {
			continue;
		}

		const std::string key = Trim(line.substr(0, equalPos));
		const std::string value = Trim(line.substr(equalPos + 1));
		if (key.empty()) {
			continue;
		}

		const char* k = key.c_str();
		const char* v = value.c_str();

		bool boolValue = false;
		int intValue = 0;
		float floatValue = 0.f;

#define LOAD_BOOL(name) if (key == #name && ParseBool(v, boolValue)) { name = boolValue; continue; }
#define LOAD_INT(name) if (key == #name && ParseInt(v, intValue)) { name = intValue; continue; }
#define LOAD_FLOAT(name) if (key == #name && ParseFloat(v, floatValue)) { name = floatValue; continue; }
#define LOAD_STRING(name, size) \
	if (key == #name) { \
		std::snprintf(name, size, "%s", v); \
		continue; \
	}

		using namespace Config;

		LOAD_BOOL(Aim::enable)
		LOAD_FLOAT(Aim::smoothingX)
		LOAD_FLOAT(Aim::smoothingY)
		LOAD_BOOL(Aim::humanisation)
		LOAD_FLOAT(Aim::FOV)
		LOAD_BOOL(Aim::showFOV)
		LOAD_INT(Aim::aimkey)
		LOAD_INT(Aim::aimbone)
		LOAD_BOOL(Aim::targetLine)
		LOAD_BOOL(Aim::visibleCheck)
		LOAD_BOOL(Aim::teamCheck)
		LOAD_FLOAT(Aim::maxAimbotDistance)
		LOAD_BOOL(Aim::useKmbox)
		LOAD_INT(Aim::kmboxDevice)
		LOAD_INT(Aim::kmboxComPort)
		LOAD_INT(Aim::movementType)
		LOAD_INT(Aim::movementTime)
		LOAD_INT(Aim::kmboxMonitorPort)
		LOAD_STRING(Aim::kmboxIp, sizeof(Aim::kmboxIp))
		LOAD_STRING(Aim::kmboxPort, sizeof(Aim::kmboxPort))
		LOAD_STRING(Aim::kmboxMac, sizeof(Aim::kmboxMac))

		LOAD_BOOL(ESP::enable)
		LOAD_BOOL(ESP::box)
		LOAD_INT(ESP::boxType)
		LOAD_BOOL(ESP::boxFilled)
		LOAD_BOOL(ESP::indicator)
		LOAD_BOOL(ESP::nickname)
		LOAD_BOOL(ESP::health)
		LOAD_INT(ESP::healthBarPos)
		LOAD_BOOL(ESP::snaplines)
		LOAD_BOOL(ESP::distance)
		LOAD_BOOL(ESP::skeleton)
		LOAD_INT(ESP::maxESPDistance)
		LOAD_INT(ESP::liveRefreshDistance)
		LOAD_BOOL(Settings::debug)
		LOAD_BOOL(Settings::useLiveRenderReads)

#undef LOAD_BOOL
#undef LOAD_INT
#undef LOAD_FLOAT
#undef LOAD_STRING

		if (LoadColor4Key(k, "Aim_targetLineColor", v, Aim::targetLineColor)) continue;
		if (LoadColor4Key(k, "ESP_boxVisibleOutlineColor", v, ESP::boxVisibleOutlineColor)) continue;
		if (LoadColor4Key(k, "ESP_boxInvisibleOutlineColor", v, ESP::boxInvisibleOutlineColor)) continue;
		if (LoadColor4Key(k, "ESP_boxVisibleFilledColor", v, ESP::boxVisibleFilledColor)) continue;
		if (LoadColor4Key(k, "ESP_boxInvisibleFilledColor", v, ESP::boxInvisibleFilledColor)) continue;
		if (LoadColor4Key(k, "ESP_nicknameVisibleColor", v, ESP::nicknameVisibleColor)) continue;
		if (LoadColor4Key(k, "ESP_nicknameInvisibleColor", v, ESP::nicknameInvisibleColor)) continue;
		if (LoadColor4Key(k, "ESP_snaplinesVisibleColor", v, ESP::snaplinesVisibleColor)) continue;
		if (LoadColor4Key(k, "ESP_snaplinesInvisibleColor", v, ESP::snaplinesInvisibleColor)) continue;
		if (LoadColor4Key(k, "ESP_distanceVisibleColor", v, ESP::distanceVisibleColor)) continue;
		if (LoadColor4Key(k, "ESP_distanceInvisibleColor", v, ESP::distanceInvisibleColor)) continue;
		if (LoadColor4Key(k, "ESP_skeletonVisibleColor", v, ESP::skeletonVisibleColor)) continue;
		if (LoadColor4Key(k, "ESP_skeletonInvisibleColor", v, ESP::skeletonInvisibleColor)) continue;
	}

	SanitizeKmboxSettings();
}

void Config::SaveSettings() {
	const std::string configPath = GetConfigPath();
	std::ofstream file(configPath, std::ios::trunc);
	if (!file.is_open()) {
		return;
	}

	file << "# Usermode settings (auto-saved)\n";

	using namespace Config;

	WriteBool(file, "Aim::enable", Aim::enable);
	WriteFloat(file, "Aim::smoothingX", Aim::smoothingX);
	WriteFloat(file, "Aim::smoothingY", Aim::smoothingY);
	WriteBool(file, "Aim::humanisation", Aim::humanisation);
	WriteFloat(file, "Aim::FOV", Aim::FOV);
	WriteBool(file, "Aim::showFOV", Aim::showFOV);
	WriteInt(file, "Aim::aimkey", Aim::aimkey);
	WriteInt(file, "Aim::aimbone", Aim::aimbone);
	WriteBool(file, "Aim::targetLine", Aim::targetLine);
	WriteBool(file, "Aim::visibleCheck", Aim::visibleCheck);
	WriteBool(file, "Aim::teamCheck", Aim::teamCheck);
	WriteFloat(file, "Aim::maxAimbotDistance", Aim::maxAimbotDistance);
	WriteBool(file, "Aim::useKmbox", Aim::useKmbox);
	WriteInt(file, "Aim::kmboxDevice", Aim::kmboxDevice);
	WriteInt(file, "Aim::kmboxComPort", Aim::kmboxComPort);
	WriteInt(file, "Aim::movementType", Aim::movementType);
	WriteInt(file, "Aim::movementTime", Aim::movementTime);
	WriteInt(file, "Aim::kmboxMonitorPort", Aim::kmboxMonitorPort);
	WriteString(file, "Aim::kmboxIp", Aim::kmboxIp);
	WriteString(file, "Aim::kmboxPort", Aim::kmboxPort);
	WriteString(file, "Aim::kmboxMac", Aim::kmboxMac);
	WriteColor4(file, "Aim_targetLineColor", Aim::targetLineColor);

	WriteBool(file, "ESP::enable", ESP::enable);
	WriteBool(file, "ESP::box", ESP::box);
	WriteInt(file, "ESP::boxType", ESP::boxType);
	WriteBool(file, "ESP::boxFilled", ESP::boxFilled);
	WriteBool(file, "ESP::indicator", ESP::indicator);
	WriteBool(file, "ESP::nickname", ESP::nickname);
	WriteBool(file, "ESP::health", ESP::health);
	WriteInt(file, "ESP::healthBarPos", ESP::healthBarPos);
	WriteBool(file, "ESP::snaplines", ESP::snaplines);
	WriteBool(file, "ESP::distance", ESP::distance);
	WriteBool(file, "ESP::skeleton", ESP::skeleton);
	WriteInt(file, "ESP::maxESPDistance", ESP::maxESPDistance);
	WriteInt(file, "ESP::liveRefreshDistance", ESP::liveRefreshDistance);
	WriteBool(file, "Settings::debug", Settings::debug);
	WriteBool(file, "Settings::useLiveRenderReads", Settings::useLiveRenderReads);

	WriteColor4(file, "ESP_boxVisibleOutlineColor", ESP::boxVisibleOutlineColor);
	WriteColor4(file, "ESP_boxInvisibleOutlineColor", ESP::boxInvisibleOutlineColor);
	WriteColor4(file, "ESP_boxVisibleFilledColor", ESP::boxVisibleFilledColor);
	WriteColor4(file, "ESP_boxInvisibleFilledColor", ESP::boxInvisibleFilledColor);
	WriteColor4(file, "ESP_nicknameVisibleColor", ESP::nicknameVisibleColor);
	WriteColor4(file, "ESP_nicknameInvisibleColor", ESP::nicknameInvisibleColor);
	WriteColor4(file, "ESP_snaplinesVisibleColor", ESP::snaplinesVisibleColor);
	WriteColor4(file, "ESP_snaplinesInvisibleColor", ESP::snaplinesInvisibleColor);
	WriteColor4(file, "ESP_distanceVisibleColor", ESP::distanceVisibleColor);
	WriteColor4(file, "ESP_distanceInvisibleColor", ESP::distanceInvisibleColor);
	WriteColor4(file, "ESP_skeletonVisibleColor", ESP::skeletonVisibleColor);
	WriteColor4(file, "ESP_skeletonInvisibleColor", ESP::skeletonInvisibleColor);
}
