#include "UI/Menu.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <string>
#include <unordered_map>

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
		constexpr float kOverrideEpsilon = 1e-5F;

		// Shared draft so "General" and "Exemptions" both refresh / flush live config to globals
		// whenever either SKSEMenuFramework section is drawn (avoids stale g_settings if only one tab runs).
		static Config::Settings g_configDraft{};

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

		std::unordered_map<std::string, float> BuildOverrideLookup(
			const std::vector<std::pair<std::string, float>>& pairs)
		{
			std::unordered_map<std::string, float> out;
			out.reserve(pairs.size());
			for (const auto& [id, val] : pairs) {
				out[id] = val;
			}
			return out;
		}

		std::size_t CountScenesMatchingAnimFilter(const std::vector<std::string>& sceneIds, const char* animFilter)
		{
			if (animFilter == nullptr || animFilter[0] == '\0') {
				return sceneIds.size();
			}
			std::size_t n = 0;
			for (const auto& sceneId : sceneIds) {
				if (ContainsInsensitive(sceneId, animFilter)) {
					++n;
				}
			}
			return n;
		}

		void __stdcall RenderSettings()
		{
			g_configDraft = Config::Get();
			auto& ui = g_configDraft;

			if (ImGui::CollapsingHeader("Filtering Mode", ImGuiTreeNodeFlags_DefaultOpen)) {
				ImGui::Spacing();
				ImGui::TextUnformatted("Filtering Mode:");
				ImGui::SameLine();
				if (ImGui::RadioButton("Off##filterMode", static_cast<int>(ui.mode) == 0)) {
					ui.mode = Config::Mode::Off;
				}
				ImGui::SameLine();
				if (ImGui::RadioButton("Strict##filterMode", static_cast<int>(ui.mode) == 1)) {
					ui.mode = Config::Mode::Strict;
				}
				ImGui::SameLine();
				if (ImGui::RadioButton("Soft (Closest Match)##filterMode", static_cast<int>(ui.mode) == 2)) {
					ui.mode = Config::Mode::Soft;
				}
				ImGui::SameLine();
				if (ImGui::RadioButton("Debug (Log but allow any)##filterMode", static_cast<int>(ui.mode) == 3)) {
					ui.mode = Config::Mode::Debug;
				}
				ImGui::SetItemTooltip(
					"Off: OStim behaves as if this mod were not filtering scenes. "
					"Strict: reject or hide choices that are outside the height spread tolerance. "
					"Soft: if nothing matches exactly, use the closest playable scene. "
					"Debug: same checks as strict, but when nothing matches, log a warning and allow any valid scene (for troubleshooting).");

				ImGui::SliderFloat("Height Difference Tolerance", &ui.tolerance, 0.0F, 0.5F, "%.3f");
				ImGui::SetItemTooltip(
					"Max allowed difference between actors' actual height spread and the scene's intended spread.");
				ImGui::Dummy(ImVec2(0.0F, 6.0F));
			}

			if (ImGui::CollapsingHeader("Actor Filters", ImGuiTreeNodeFlags_DefaultOpen)) {
				ImGui::Spacing();
				ImGui::Checkbox("Filter Player Scenes", &ui.applyToPlayerScenes);
				ImGui::SetItemTooltip(
					"When enabled, scenes involving the player character are restricted by size-difference rules.");

				ImGui::Checkbox("Filter NPC Scenes", &ui.applyToNpcScenes);
				ImGui::SetItemTooltip(
					"When enabled, NPC-only scenes are also restricted. Turn off to let NPCs pick any scene.");

				ImGui::Checkbox("Filter Auto-Progression", &ui.applyInAutoMode);
				ImGui::SetItemTooltip(
					"Filters OStim's automatic scene progression. Turn off if you only want filtering when manually picking scenes.");
				ImGui::Dummy(ImVec2(0.0F, 6.0F));
			}

			if (ImGui::CollapsingHeader("Persistence", ImGuiTreeNodeFlags_DefaultOpen)) {
				ImGui::Spacing();
				if (ImGui::Button("Reload from disk")) {
					Config::Reload();
					ui = Config::Get();
				}
				ImGui::SetItemTooltip("Reload all settings from OStimSizeDifferenceManager.ini without saving the UI state.");
				ImGui::SameLine();
				if (ImGui::Button("Save Settings")) {
					Config::Save();
				}
				ImGui::SetItemTooltip("Write current settings to OStimSizeDifferenceManager.ini.");
				ImGui::Dummy(ImVec2(0.0F, 4.0F));
			}

			Config::Set(g_configDraft);
		}

		struct OverrideDraftMeta
		{
			bool hadKey{ false };
			float effective{ 0.0F };
		};

		void __stdcall RenderExemptions()
		{
			g_configDraft = Config::Get();

			static char packSearchBuffer[256]{};
			static char animSearchBuffer[256]{};
			static std::unordered_map<std::string, float> overrideDraftByScene;
			static std::unordered_map<std::string, OverrideDraftMeta> overrideDraftMeta;
			static std::unordered_map<std::string, float> packOverrideDraft;

			const auto cache = SceneCache::Get();

			ImVec2 fullAvail{};
			ImGui::GetContentRegionAvail(&fullAvail);
			const float halfW = std::max(100.0F, fullAvail.x * 0.5F - 8.0F);
			ImGui::BeginGroup();
			ImGui::TextUnformatted("Pack Name Filter");
			ImGui::SetNextItemWidth(halfW);
			ImGui::InputTextWithHint("##packFilter", "Filter packs…", packSearchBuffer, sizeof(packSearchBuffer));
			ImGui::SetItemTooltip("Show only packs whose name matches (case-insensitive).");
			ImGui::EndGroup();
			ImGui::SameLine();
			ImGui::BeginGroup();
			ImGui::TextUnformatted("Animation Name Filter");
			ImGui::SetNextItemWidth(halfW);
			ImGui::InputTextWithHint("##animFilter", "Filter scenes…", animSearchBuffer, sizeof(animSearchBuffer));
			ImGui::SetItemTooltip("Show only scenes whose ID matches (case-insensitive). When set, empty packs are hidden.");
			ImGui::EndGroup();
			ImGui::Spacing();

			if (!cache->IsReady()) {
				ImGui::Spacing();
				ImGui::TextUnformatted("Loading scene metadata…");
				Config::Set(g_configDraft);
				return;
			}

			const auto overridePairs = cache->GetOverridesCopy();
			const auto overrideLookup = BuildOverrideLookup(overridePairs);

			ImVec2 childAvail{};
			ImGui::GetContentRegionAvail(&childAvail);
			const float reservedBelow = 52.0F;
			const float childH = std::max(180.0F, childAvail.y - reservedBelow);

			if (ImGui::BeginChild("exemptions_list", ImVec2(0.0F, childH), ImGuiChildFlags_Border)) {
				const auto packScenes = cache->GetPackScenes();
				const bool animFilterActive = animSearchBuffer[0] != '\0';

				for (const auto& [packName, sceneIds] : packScenes) {
					if (!ContainsInsensitive(packName, packSearchBuffer)) {
						continue;
					}
					if (animFilterActive && CountScenesMatchingAnimFilter(sceneIds, animSearchBuffer) == 0) {
						continue;
					}

					if (!ImGui::TreeNode(packName.c_str())) {
						continue;
					}

					ImGui::PushID(packName.c_str());

					bool allExempt = true;
					bool noneExempt = true;
					for (const auto& sceneId : sceneIds) {
						if (!ContainsInsensitive(sceneId, animSearchBuffer)) {
							continue;
						}
						const bool ex = cache->IsExempt(sceneId);
						allExempt = allExempt && ex;
						noneExempt = noneExempt && !ex;
					}
					const bool mixedExempt = !allExempt && !noneExempt;
					bool packExemptVal = allExempt;

					ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, mixedExempt);
					if (ImGui::Checkbox("Exempt Pack", &packExemptVal)) {
						for (const auto& sceneId : sceneIds) {
							if (!ContainsInsensitive(sceneId, animSearchBuffer)) {
								continue;
							}
							cache->ToggleExemption(sceneId, packExemptVal);
						}
					}
					ImGui::PopItemFlag();
					ImGui::SetItemTooltip("When checked, every scene in this pack is always allowed.");

					float& packOvDraft = packOverrideDraft[packName];
					ImGui::TextUnformatted("Override pack");
					ImGui::SameLine();
					ImGui::SetNextItemWidth(110.0F);
					ImGui::InputFloat("##packOv", &packOvDraft, 0.0F, 0.0F, "%.3f",
						ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
					ImGui::SameLine();
					if (ImGui::Button("Apply##packOvApply")) {
						for (const auto& sceneId : sceneIds) {
							if (!ContainsInsensitive(sceneId, animSearchBuffer)) {
								continue;
							}
							cache->SetOverride(sceneId, packOvDraft);
							overrideDraftByScene[sceneId] = packOvDraft;
							auto& meta = overrideDraftMeta[sceneId];
							meta.hadKey = true;
							meta.effective = packOvDraft;
						}
					}
					ImGui::SetItemTooltip("Apply this scale-diff override to every visible scene in the pack.");

					ImGui::Spacing();

					const ImGuiTableFlags tableFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
						ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable;
					const float tableHeight = 350.0F;
					if (ImGui::BeginTable("scenes", 7, tableFlags, ImVec2(0.0F, tableHeight))) {
						ImGui::TableSetupColumn("Scene ID", ImGuiTableColumnFlags_WidthStretch);
						ImGui::TableSetupColumn("Actor 1", ImGuiTableColumnFlags_WidthFixed, 64.0F);
						ImGui::TableSetupColumn("Actor 2", ImGuiTableColumnFlags_WidthFixed, 64.0F);
						ImGui::TableSetupColumn("Other Actors?", ImGuiTableColumnFlags_WidthFixed, 128.0F);
						ImGui::TableSetupColumn("Scale Diff", ImGuiTableColumnFlags_WidthFixed, 72.0F);
						ImGui::TableSetupColumn("Exempt", ImGuiTableColumnFlags_WidthFixed, 56.0F);
						ImGui::TableSetupColumn("Override", ImGuiTableColumnFlags_WidthFixed, 168.0F);
						ImGui::TableSetupScrollFreeze(0, 1);
						ImGui::TableHeadersRow();

						for (const auto& sceneId : sceneIds) {
							if (!ContainsInsensitive(sceneId, animSearchBuffer)) {
								continue;
							}

							ImGui::TableNextRow();
							ImGui::TableNextColumn();
							ImGui::TextUnformatted(sceneId.c_str());

							const auto info = cache->GetSceneInfo(sceneId);
							const float authored = info.has_value() ? info->diff : 0.0F;

							ImGui::TableNextColumn();
							if (info.has_value()) {
								ImGui::Text("%.2f", static_cast<double>(info->maxScale));
							} else {
								ImGui::TextUnformatted("?");
							}

							ImGui::TableNextColumn();
							if (info.has_value()) {
								ImGui::Text("%.2f", static_cast<double>(info->minScale));
							} else {
								ImGui::TextUnformatted("?");
							}

							ImGui::TableNextColumn();
							if (info.has_value() && info->actorCount > 2) {
								const int extras = info->actorCount - 2;
								char otherBuf[64]{};
								std::snprintf(otherBuf, sizeof(otherBuf), "%d extra actor(s)", extras);
								ImGui::TextUnformatted(otherBuf);
							} else {
								ImGui::TextUnformatted("none");
							}

							ImGui::TableNextColumn();
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

							const auto ovIt = overrideLookup.find(sceneId);
							const bool hasOverrideKey = ovIt != overrideLookup.end();
							const float cachedOverride = hasOverrideKey ? ovIt->second : authored;
							const float effectiveFromCache = hasOverrideKey ? cachedOverride : authored;
							const bool showAsAuthored =
								!hasOverrideKey || std::fabs(cachedOverride - authored) <= kOverrideEpsilon;

							float& draft = overrideDraftByScene[sceneId];
							auto& dmeta = overrideDraftMeta[sceneId];
							if (dmeta.hadKey != hasOverrideKey ||
								std::fabs(dmeta.effective - effectiveFromCache) > kOverrideEpsilon) {
								draft = effectiveFromCache;
								dmeta.hadKey = hasOverrideKey;
								dmeta.effective = effectiveFromCache;
							}

							if (showAsAuthored) {
								ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65F, 0.65F, 0.65F, 1.0F));
							}
							ImGui::SetNextItemWidth(78.0F);
							ImGui::InputFloat("##ov", &draft, 0.0F, 0.0F, "%.3f",
								ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
							if (showAsAuthored) {
								ImGui::PopStyleColor(1);
							}

							if (ImGui::IsItemDeactivatedAfterEdit()) {
								cache->SetOverride(sceneId, draft);
								dmeta.hadKey = true;
								dmeta.effective = draft;
							}

							ImGui::SameLine();
							const bool canReset =
								hasOverrideKey && std::fabs(cachedOverride - authored) > kOverrideEpsilon;
							if (!canReset) {
								ImGui::BeginDisabled();
							}
							if (ImGui::Button("Reset##ovReset")) {
								cache->SetOverride(sceneId, authored);
								draft = authored;
								dmeta.hadKey = true;
								dmeta.effective = authored;
							}
							if (!canReset) {
								ImGui::EndDisabled();
							}
							ImGui::SetItemTooltip("Clear override and use the scene's authored scale diff.");

							ImGui::PopID();
						}
						ImGui::EndTable();
					}

					ImGui::PopID();
					ImGui::TreePop();
				}
			}
			ImGui::EndChild();

			ImGui::Spacing();
			if (ImGui::Button("Save Overrides to JSON")) {
				cache->SaveUserOverrides();
			}
			ImGui::SetItemTooltip("Write exemptions and overrides to OStimSizeDifferenceManager_Overrides.json.");

			Config::Set(g_configDraft);
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
