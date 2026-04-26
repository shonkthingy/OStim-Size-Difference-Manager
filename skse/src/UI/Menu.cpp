#include "UI/Menu.h"

#include <algorithm>
#include <cctype>
#include <string>

#include "Config/Config.h"
#include "Plugin.h"
#include "SceneCache/SceneCache.h"
#include "UI/SKSEMenuFramework.h"

#include "PCH.h"
#include "spdlog/spdlog.h"

namespace SizeDiff::UI
{
	namespace
	{
		bool ContainsInsensitive(const std::string& haystack, const char* needle)
		{
			if (needle == nullptr || needle[0] == '\0') {
				return true;
			}
			std::string h = haystack;
			std::string n(needle);
			const auto lower = [](unsigned char c) { return static_cast<char>(std::tolower(c)); };
			std::transform(h.begin(), h.end(), h.begin(), lower);
			std::transform(n.begin(), n.end(), n.begin(), lower);
			return h.find(n) != std::string::npos;
		}

		void __stdcall RenderSettings()
		{
			static bool seeded = false;
			static Config::Settings ui{};
			if (!seeded) {
				ui = Config::Get();
				seeded = true;
			}

			int modeSel = static_cast<int>(ui.mode);
			modeSel = std::clamp(modeSel, 0, 2);
			ImGui::Combo("Filtering Mode", &modeSel, "Off\0Soft\0Strict\0");
			ui.mode = static_cast<Config::Mode>(modeSel);
			ImGui::SetItemTooltip(
				"Off = no filtering. Soft = intermediate matching. Strict = only allow scenes matching actor height spread.");

			ImGui::SliderFloat("Height Difference Tolerance", &ui.tolerance, 0.0F, 0.5F, "%.3f");
			ImGui::SetItemTooltip(
				"Max allowed difference between actors' actual height spread and the scene's intended spread.");

			ImGui::Checkbox("Filter Player Scenes", &ui.applyToPlayerScenes);
			ImGui::SetItemTooltip(
				"When enabled, scenes involving the player character are restricted by size-difference rules.");

			ImGui::Checkbox("Filter NPC Scenes", &ui.applyToNpcScenes);
			ImGui::SetItemTooltip(
				"When enabled, NPC-only scenes are also restricted. Turn off to let NPCs pick any scene.");

			ImGui::Checkbox("Filter Auto-Progression", &ui.applyInAutoMode);
			ImGui::SetItemTooltip(
				"Filters OStim's automatic scene progression. Turn off if you only want filtering when manually picking scenes.");

			int fb = std::clamp(ui.fallbackBehavior, 0, 2);
			ImGui::Combo("Fallback Behaviour", &fb,
				"Strict (never break rules)\0Soft (allow closest match)\0Debug (log but still allow)\0");
			ui.fallbackBehavior = fb;
			ImGui::SetItemTooltip(
				"0 = Strict (never break rules), 1 = Soft (allow closest match), 2 = Debug (log but still allow).");

			if (ImGui::Button("Reload from disk")) {
				Config::Reload();
				ui = Config::Get();
			}
			ImGui::SetItemTooltip("Reload all settings from OStimSizeDifferenceManager.ini without saving the UI state.");
			ImGui::SameLine();
			if (ImGui::Button("Save Settings")) {
				Config::Set(ui);
				Config::Save();
			}
			ImGui::SetItemTooltip("Write current settings to OStimSizeDifferenceManager.ini.");
		}

		void __stdcall RenderExemptions()
		{
			static char searchBuffer[256]{};
			static std::string overrideSceneId;
			static float overrideVal = 0.0F;

			const auto cache = SceneCache::Get();

			ImGui::InputText("Search scenes...", searchBuffer, sizeof(searchBuffer));
			ImGui::SetItemTooltip("Filter the scene list; matching is case-insensitive.");

			if (!cache->IsReady()) {
				ImGui::Spacing();
				ImGui::TextUnformatted("Loading scene metadata…");
				return;
			}

			const auto packScenes = cache->GetPackScenes();

			for (const auto& [packName, sceneIds] : packScenes) {
				if (!ImGui::TreeNode(packName.c_str())) {
					continue;
				}

				ImGui::PushID(packName.c_str());
				if (ImGui::BeginTable("scenes", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
					ImGui::TableSetupColumn("Scene ID");
					ImGui::TableSetupColumn("Scale Diff");
					ImGui::TableSetupColumn("Exempt");
					ImGui::TableSetupColumn("Override");
					ImGui::TableHeadersRow();

					for (const auto& sceneId : sceneIds) {
						if (!ContainsInsensitive(sceneId, searchBuffer)) {
							continue;
						}

						ImGui::TableNextRow();
						ImGui::TableNextColumn();
						ImGui::TextUnformatted(sceneId.c_str());

						ImGui::TableNextColumn();
						const auto info = cache->GetSceneInfo(sceneId);
						if (info.has_value()) {
							ImGui::Text("%.2f", static_cast<double>(info->diff));
						} else {
							ImGui::TextUnformatted("Hub/Unknown");
						}

						ImGui::TableNextColumn();
						ImGui::PushID(sceneId.c_str());
						bool isExempt = cache->IsExempt(sceneId);
						if (ImGui::Checkbox("##exempt", &isExempt)) {
							cache->ToggleExemption(sceneId, isExempt);
						}
						ImGui::SetItemTooltip("When checked, this scene is always allowed.");

						ImGui::TableNextColumn();
						if (ImGui::Button("Set Override")) {
							overrideSceneId = sceneId;
							overrideVal = info.has_value() ? info->diff : 0.0F;
							for (const auto& [oid, val] : cache->GetOverridesCopy()) {
								if (oid == sceneId) {
									overrideVal = val;
									break;
								}
							}
							ImGui::OpenPopup("OverrideScenePopup");
						}
						ImGui::PopID();
					}
					ImGui::EndTable();
				}
				ImGui::PopID();
				ImGui::TreePop();
			}

			if (ImGui::BeginPopup("OverrideScenePopup")) {
				ImGui::Text("Scene: %s", overrideSceneId.c_str());
				ImGui::SliderFloat("Authored Scale Diff", &overrideVal, 0.0F, 1.0F);
				if (ImGui::Button("Apply")) {
					cache->SetOverride(overrideSceneId, overrideVal);
					ImGui::CloseCurrentPopup();
				}
				ImGui::SameLine();
				if (ImGui::Button("Cancel")) {
					ImGui::CloseCurrentPopup();
				}
				ImGui::EndPopup();
			}

			ImGui::Spacing();
			if (ImGui::Button("Save Overrides to JSON")) {
				cache->SaveUserOverrides();
			}
			ImGui::SetItemTooltip("Write exemptions and overrides to OStimSizeDifferenceManager_Overrides.json.");
		}
	}

	void Register()
	{
		if (!SKSEMenuFramework::IsInstalled()) {
			spdlog::warn("SKSEMenuFramework.dll not found under Data/SKSE/Plugins; in-game menu disabled");
			return;
		}
		SKSEMenuFramework::SetSection("OStim Size Difference");
		SKSEMenuFramework::AddSectionItem("General Settings", RenderSettings);
		SKSEMenuFramework::AddSectionItem("Exemptions & Overrides", RenderExemptions);
		spdlog::info("Registered SKSE Menu Framework UI for {}", kPluginName);
	}
}
