#include "global.h"

static bool g_menuOpen = true;

namespace {
	struct MenuPage {
		const char* pageTitle;
		const char* icon;
		void (*renderPage)();
	};

	struct MenuSection {
		const char* title;
		std::span<const MenuPage> pages;
	};

	namespace PageRenderers {
		void Aimbot();
		void ESP();
		void Settings();
		void Config();
	}

	constexpr MenuPage COMBAT_PAGES[] = {
		{ "Aimbot", ICON_FA_LOCATION_CROSSHAIRS, &PageRenderers::Aimbot }
	};

	constexpr MenuPage VISUAL_PAGES[] = {
		{ "ESP", ICON_FA_STREET_VIEW, &PageRenderers::ESP }
	};

	constexpr MenuPage SETTINGS_PAGES[] = {
		{ "Settings", ICON_FA_GEARS, &PageRenderers::Settings }
	};

	constexpr MenuPage CONFIG_PAGES[] = {
		{ "Configs", ICON_FA_FOLDER_OPEN, &PageRenderers::Config }
	};

	constexpr MenuSection SECTIONS_DATA[] = {
		{ "COMBAT",			COMBAT_PAGES	},
		{ "VISUALS",		VISUAL_PAGES	},
		{ "MISC",			SETTINGS_PAGES	},
		{ "CONFIGURATION",	CONFIG_PAGES	},
	};

	int pageID = 0;
	int sidebarSize = 200;
	static ImVec2 menuSize = { 625 , 415 };
	static ImVec2 topbarSize = { 380 , 40 };
	
	int selectedPageId = 0;
	const std::span<const MenuSection> allSections = SECTIONS_DATA;
	
	const MenuPage* GetCurrentPage() {
		int currentID = 0;

		for (const auto& section : allSections) {
			for (const auto& page : section.pages) {
				if (currentID == selectedPageId) {
					return &page;
				}

				currentID++;
			}
		}

		return nullptr;
	}

	void SetClickThrough(bool clickThrough) {
		LONG_PTR style = GetWindowLongPtr(Render::g_windowHandle, GWL_EXSTYLE);

		if (clickThrough) {
			style |= WS_EX_TRANSPARENT;
			style |= WS_EX_NOACTIVATE;
		} else {
			style &= ~WS_EX_TRANSPARENT;
			style |= WS_EX_NOACTIVATE;
		}

		SetWindowLongPtr(Render::g_windowHandle, GWL_EXSTYLE, style);
	}

	void HandleInput() {
		if (GetAsyncKeyState(VK_INSERT) & 1) {
			const bool wasOpen = g_menuOpen;
			g_menuOpen = !g_menuOpen;
			SetClickThrough(!g_menuOpen);
			if (wasOpen && !g_menuOpen) {
				Config::SaveSettings();
			}
		}
	}

	static const ImGuiWindowFlags mainWindowFlags =
		ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse
		| ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize
		| ImGuiWindowFlags_NoBackground;

	static const ImGuiWindowFlags topbarWindowFlags =
		ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse
		| ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize
		| ImGuiWindowFlags_NoMove;
}

bool Menu::IsOpen() {
	return g_menuOpen;
}

void Menu::PrepareOverlayInput() {
	if (g_menuOpen) {
		SetClickThrough(false);
		return;
	}

	if (Config::Settings::debug && DebugOverlay::WantsMouseCapture()) {
		SetClickThrough(false);
		return;
	}

	SetClickThrough(true);
}

void Menu::RenderMenu() {
	HandleInput();

	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	Render::PollOverlayKeyboardKeys();
	ImGui::NewFrame();
	Render::PollOverlayTextInput();
	{
		Loop::RunLoop();

		if (Config::Settings::debug) {
			DebugOverlay::Render(Cache::GetDebugStats());
		}

		if (g_menuOpen) {
			ImGuiStyle& style = ImGui::GetStyle();

			static ImVec4 colorPick = ImAdd::Hex2RGBA(0x7099FF, 1.0f);
			style.Colors[ImGuiCol_SliderGrab] = colorPick;
			style.Colors[ImGuiCol_ScrollbarGrab] = colorPick;

			// topbar
			{
				ImGui::SetNextWindowPos({ static_cast<float>(Render::g_screenWidth / 2) - (topbarSize.x / 2), 0 }, ImGuiCond_Always);
				ImGui::Begin("##topbar", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);
				{
					ImGui::PushStyleColor(ImGuiCol_Separator, ImAdd::Hex2RGBA(0xC8C8D2, 1.0f));
					{
						ImGui::Text("Made By Aiden");
						ImGui::SameLine();

						ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
						ImGui::SameLine();

						ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
						ImGui::SameLine();

						ImGui::PushStyleColor(ImGuiCol_Button, ImAdd::Hex2RGBA(0x6B1D1D, 1.0f));
						ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImAdd::Hex2RGBA(0x812222, 1.0f));
						ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImAdd::Hex2RGBA(0x4E1313, 1.0f));
						if (ImGui::Button(ICON_FA_POWER_OFF " Exit", ImVec2(80, 24))) {
							Config::SaveSettings();
							Render::RequestExit();
						}
						ImGui::PopStyleColor(3);

						const auto& now = std::chrono::system_clock::now();
						std::string formattedTime = std::format("{:%d-%m-%Y : %I:%M%p}", now);
						ImGui::Text(formattedTime.c_str());
						ImGui::SameLine();

						ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
						ImGui::SameLine();

						const auto& io = ImGui::GetIO();
						const CacheDebugStats cacheStats = Cache::GetDebugStats();
						ImGui::Text("Draw %.0f | ESP %.0fHz %.1fms", io.Framerate, cacheStats.snapshotHz, cacheStats.dataAgeMs);
					}
					ImGui::PopStyleColor();
				}
				ImGui::End();
			}

			ImGui::SetNextWindowPos({ (Render::g_screenWidth / 2) - (menuSize.x / 2) , (Render::g_screenHeight / 2) - (menuSize.y / 2) }, ImGuiCond_FirstUseEver);
			ImGui::SetNextWindowSize(menuSize);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
			ImGui::Begin("##aidens_main_window", nullptr, mainWindowFlags);
			{
				ImGui::PopStyleVar(2);
				// sidebar
				{
					{
						ImGui::GetWindowDrawList()->AddRectFilled(ImGui::GetWindowPos(), ImGui::GetWindowPos() + ImVec2(sidebarSize, ImGui::GetWindowSize().y), ImGui::GetColorU32(ImGuiCol_ChildBg), style.WindowRounding, ImDrawFlags_RoundCornersLeft);
						ImGui::GetWindowDrawList()->AddRectFilled(ImGui::GetWindowPos() + ImVec2(sidebarSize + style.WindowBorderSize, 0), ImGui::GetWindowPos() + ImGui::GetWindowSize(), ImGui::GetColorU32(ImGuiCol_WindowBg), style.WindowRounding, ImDrawFlags_RoundCornersRight);

						if (style.WindowBorderSize > 0.0f)
						{
							ImGui::GetWindowDrawList()->AddRect(ImGui::GetWindowPos(), ImGui::GetWindowPos() + ImGui::GetWindowSize(), ImGui::GetColorU32(ImGuiCol_Border), style.WindowRounding, 0, style.WindowBorderSize);
							ImGui::GetWindowDrawList()->AddLine(ImGui::GetWindowPos() + ImVec2(sidebarSize, 0), ImGui::GetWindowPos() + ImVec2(sidebarSize, ImGui::GetWindowSize().y), ImGui::GetColorU32(ImGuiCol_Border), style.WindowBorderSize);
						}
					}

					ImGui::BeginChild("##sidebar", ImVec2(sidebarSize, 0), true, ImGuiWindowFlags_NoBackground);
					{
						int selectedPageNum = 0;

						for (const auto& section : allSections) {
							ImAdd::SeparatorText(section.title);
							for (const auto& page : section.pages) {
								ImAdd::RadioButtonIcon(page.icon, page.pageTitle, selectedPageNum, &selectedPageId, ImVec2(-0.1f, 0));
								selectedPageNum++;
							}
						}
					}
					ImGui::EndChild();
				}

				// main content
				{
					ImGui::SameLine(sidebarSize);
					ImGui::BeginChild("##content", ImVec2(0, 0), true, ImGuiWindowFlags_NoBackground);
					{
						ImGui::BeginChild("##titlebar", ImVec2(ImGui::GetWindowSize().x - style.WindowPadding.x * 2, ImGui::GetFontSize() + style.WindowPadding.y * 2), true);
						{
							ImGui::Text("Call Of Diddy | External");
						}
						ImGui::EndChild();

						ImGui::BeginChild("##main", ImVec2(0, 0), true);
						{
							const auto* currentPage = GetCurrentPage();
							if (currentPage && currentPage->renderPage) {
								currentPage->renderPage();
							} else {
								ImGui::Text("Select a page from the sidebar.");
							}
						}
						ImGui::EndChild();
					}
					ImGui::EndChild();
				}
			}
			ImGui::End();
		}
	}
	ImGui::EndFrame();

	Render::PresentImGuiFrame();
}

static int aimTabId = 0;
void PageRenderers::Aimbot() {
	ImAdd::TabBox(
		"##aim_tab_box",
		&aimTabId,
		{
			ICON_FA_PERSON_RIFLE " Settings",
			ICON_FA_GEAR " Misc"
		},
		ImVec2(0, ImGui::GetFrameHeight())
	); ImGui::Separator();

	ImAdd::Checkbox("Enable", &Config::Aim::enable);
	if (Config::Aim::enable) {
		if (aimTabId == 0) {
			ImGui::Spacing(); ImGui::Spacing();
			{
				ImAdd::SliderFloat("Smoothing X Axis", &Config::Aim::smoothingX, 1.f, 100.f);
				ImAdd::SliderFloat("Smoothing Y Axis", &Config::Aim::smoothingY, 1.f, 100.f);
			}
			ImGui::Spacing(); ImGui::Spacing();
			{
				ImAdd::Combo(
					"Target Aimbone",
					&Config::Aim::aimbone,
					"Head\0Neck\0Chest\0Pelvis\0Smart\0"
				);
			}
			ImGui::Spacing(); ImGui::Spacing();
			{
				ImAdd::ToggleButton("Visible Check", &Config::Aim::visibleCheck);
			}
		} else {
			ImGui::Spacing(); ImGui::Spacing();
			{
				ImAdd::ToggleButton("Target Line", &Config::Aim::targetLine);
				if (Config::Aim::targetLine) {
					ImAdd::ColorEdit4("Target Line Colour", (float*)&Config::Aim::targetLineColor);
				}
			}

			ImGui::Spacing(); ImGui::Spacing();
			{
				ImAdd::SliderFloat("Maximum Distance", &Config::Aim::maxAimbotDistance, 1.f, 300.f);
			}

			ImGui::Spacing(); ImGui::Spacing();
			{
				ImAdd::SliderFloat("Aim FOV", &Config::Aim::FOV, 1.f, 600.f);
				ImAdd::ToggleButton("Show FOV", &Config::Aim::showFOV);
			}

			ImGui::Spacing(); ImGui::Spacing();
			{
				ImAdd::Hotkey("\t  Aimkey", &Config::Aim::aimkey);
			}
		}
	} else {
		ImGui::Text("Please enable to view settings");
	}
}

static int espTabId = 0;
void PageRenderers::ESP() {
	ImAdd::TabBox(
		"##esp_tab_box",
		&espTabId,
		{
			ICON_FA_EYE " Settings",
			ICON_FA_PALETTE " Misc"
		},
		ImVec2(0, ImGui::GetFrameHeight())
	); ImGui::Separator();


	ImAdd::ToggleButton("Enable", &Config::ESP::enable);
	if (Config::ESP::enable) {
		if (espTabId == 0) {
			ImGui::Spacing(); ImGui::Spacing();
			{
				ImAdd::ToggleButton("Enable Box", &Config::ESP::box);
				if (Config::ESP::box) {
					ImAdd::Combo(
						"Box Type",
						&Config::ESP::boxType,
						"Normal\0Cornered\0"
					);

					ImAdd::ToggleButton("Filled Box", &Config::ESP::boxFilled);
				}
			}

			ImGui::Spacing(); ImGui::Spacing();
			{
				ImAdd::ToggleButton("Indicators", &Config::ESP::indicator);
			}

			ImGui::Spacing(); ImGui::Spacing();
			{
				ImAdd::ToggleButton("Nickname", &Config::ESP::nickname);
			}

			ImGui::Spacing(); ImGui::Spacing();
			{
				ImAdd::ToggleButton("Health Bar", &Config::ESP::health);
				if (Config::ESP::health) {
					ImAdd::Combo(
						"Health Bar Position",
						&Config::ESP::healthBarPos,
						"Left\0Right\0Top\0Bottom\0"
					);
				}
			}

			ImGui::Spacing(); ImGui::Spacing();
			{
				ImAdd::ToggleButton("Snaplines", &Config::ESP::snaplines);
			}

			ImGui::Spacing(); ImGui::Spacing();
			{
				ImAdd::ToggleButton("Distance", &Config::ESP::distance);
			}

			ImGui::Spacing(); ImGui::Spacing();
			{
				ImAdd::ToggleButton("Skeleton", &Config::ESP::skeleton);
			}

			ImGui::Spacing(); ImGui::Spacing();
			{
				ImAdd::SliderInt("Max ESP Distance", &Config::ESP::maxESPDistance, 1, 300);
			}
		} else {
			if (Config::ESP::enable) {
				if (Config::ESP::box) {
					ImGui::Spacing(); ImGui::Spacing();
					{
						ImAdd::ColorEdit4("Box Visible Color", (float*)&Config::ESP::boxVisibleOutlineColor);
						ImAdd::ColorEdit4("Box Invisible Color", (float*)&Config::ESP::boxInvisibleOutlineColor);

						if (Config::ESP::boxFilled) {
							ImAdd::ColorEdit4("Box Filled Visible Color", (float*)&Config::ESP::boxVisibleFilledColor);
							ImAdd::ColorEdit4("Box Filled Invisible Color", (float*)&Config::ESP::boxInvisibleFilledColor);
						}
					}
				}

				if (Config::ESP::nickname) {
					ImGui::Spacing(); ImGui::Spacing();
					{
						ImAdd::ColorEdit4("Nickname Visible Color", (float*)&Config::ESP::nicknameVisibleColor);
						ImAdd::ColorEdit4("Nickname Invisible Color", (float*)&Config::ESP::nicknameInvisibleColor);
					}
				}

				if (Config::ESP::snaplines) {
					ImGui::Spacing(); ImGui::Spacing();
					{
						ImAdd::ColorEdit4("Snaplines Visible Color", (float*)&Config::ESP::snaplinesVisibleColor);
						ImAdd::ColorEdit4("Snaplines Invisible Color", (float*)&Config::ESP::snaplinesInvisibleColor);
					}
				}

				if (Config::ESP::distance) {
					ImGui::Spacing(); ImGui::Spacing();
					{
						ImAdd::ColorEdit4("Distance Visible Color", (float*)&Config::ESP::distanceVisibleColor);
						ImAdd::ColorEdit4("Distance Invisible Color", (float*)&Config::ESP::distanceInvisibleColor);
					}
				}

				if (Config::ESP::skeleton) {
					ImGui::Spacing(); ImGui::Spacing();
					{
						ImAdd::ColorEdit4("Skeleton Visible Color", (float*)&Config::ESP::skeletonVisibleColor);
						ImAdd::ColorEdit4("Skeleton Invisible Color", (float*)&Config::ESP::skeletonInvisibleColor);
					}
				}
			}
		}
	} else {
		ImGui::Text("Please enable to view settings");
	}
}

void PageRenderers::Settings() {
	ImGui::Text("Overlay");
	ImGui::Spacing();
	ImAdd::ToggleButton("Debug", &Config::Settings::debug);
	ImGui::TextDisabled("Shows player detection stats on the overlay. Click and drag the panel to move it.");
}

void PageRenderers::Config() {
	ImGui::Text("Input device");
	ImGui::TextWrapped("Select how mouse input is sent.");

	if (Kmbox::IsNetDevice()) {
		ImGui::TextColored(ImVec4(0.3f, 1.f, 0.3f, 1.f), "Status: KmboxNet connected");
	} else if (Kmbox::IsMakcuDevice()) {
		ImGui::TextColored(ImVec4(0.3f, 1.f, 0.3f, 1.f), "Status: MAKCU connected");
	} else if (Kmbox::IsDeviceConnected()) {
		ImGui::TextColored(ImVec4(0.3f, 1.f, 0.3f, 1.f), "Status: Kmbox serial connected");
	} else {
		ImGui::TextColored(ImVec4(1.f, 0.8f, 0.3f, 1.f), "Status: Not connected (optional)");
	}

	ImGui::Spacing();
	ImAdd::ToggleButton("Enable device", &Config::Aim::useKmbox);
	ImAdd::Combo("Device", &Config::Aim::kmboxDevice, "Net\0Serial\0MAKCU\0");

	if (Config::Aim::kmboxDevice == 0) {
		ImGui::InputTextWithHint("Kmbox IP", "192.168.2.188", Config::Aim::kmboxIp, IM_ARRAYSIZE(Config::Aim::kmboxIp));
		ImGui::InputTextWithHint("Kmbox Port", "5194", Config::Aim::kmboxPort, IM_ARRAYSIZE(Config::Aim::kmboxPort));
		ImGui::InputTextWithHint("Kmbox MAC", "15CB7019", Config::Aim::kmboxMac, IM_ARRAYSIZE(Config::Aim::kmboxMac));
		ImGui::InputInt("Monitor Port", &Config::Aim::kmboxMonitorPort);
	} else if (Config::Aim::kmboxDevice == 2) {
		ImGui::InputInt("MAKCU COM Port (0 = auto)", &Config::Aim::kmboxComPort);
		if (Config::Aim::kmboxComPort < 0) {
			Config::Aim::kmboxComPort = 0;
		}
		ImGui::TextDisabled("Uses official makcu-cpp. Click Reconnect after plugging in.");
	}

	ImGui::Spacing();
	if (ImGui::Button("Reconnect")) {
		Config::Aim::useKmbox = true;
		if (Config::Aim::kmboxDevice < 0 || Config::Aim::kmboxDevice > 2) {
			Config::Aim::kmboxDevice = 0;
		}
		Aimbot::Reconnect();
	}

	if (Config::Aim::kmboxDevice == 2 && Kmbox::IsMakcuDevice()) {
		ImGui::SameLine();
		if (ImGui::Button("Test Movement")) {
			Kmbox::TestMakcuMovement();
		}
	}
}