#include "STDInclude.hpp"

#include "Options.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <unordered_map>

#include "Core.hpp"
#include "Device.hpp"
#include "Api.hpp"

namespace Components::Remix
{
	static constexpr int maxComponents = 4;

	struct OptionState
	{
		std::string ownValue;
		bool hasOwnValue = false;
		std::string fileValue;
		bool hasFileValue = false;
		std::string sentValue;
		bool hasSentValue = false;

		bool isBlending = false;
		float from[maxComponents] = {};
		float to[maxComponents] = {};
		int components = 0;
		ULONGLONG blendStartMs = 0;
		float blendMs = 0.0f;
		std::string target;
	};

	struct PendingChange
	{
		float blendSeconds = 0.0f;
	};

	static std::unordered_map<std::string, OptionState> options;
	static std::map<int, OptionList> layers;
	static std::map<std::string, PendingChange> pending;
	static bool hasReadRtxConf = false;
	static bool hasLoadedConfFiles = false;
	static OptionStats stats;
	static int diSampleCount = 0;
	static unsigned reassertFrame = 0;
	static constexpr unsigned reassertInterval = 60;

	static std::string Trim(const std::string& text)
	{
		const std::size_t first = text.find_first_not_of(" \t\r\n");

		if (first == std::string::npos)
		{
			return {};
		}

		const std::size_t last = text.find_last_not_of(" \t\r\n");
		return text.substr(first, last - first + 1);
	}

	static int ParseComponents(const std::string& value, float* components)
	{
		int count = 0;
		std::size_t start = 0;

		while (start <= value.size())
		{
			const std::size_t comma = value.find(',', start);
			std::size_t end = value.size();

			if (comma != std::string::npos)
			{
				end = comma;
			}
			const std::string part = Trim(value.substr(start, end - start));

			if (part.empty() || count >= maxComponents)
			{
				return 0;
			}

			char* parseEnd = nullptr;
			const float number = std::strtof(part.c_str(), &parseEnd);

			if (parseEnd == part.c_str() || *parseEnd != '\0')
			{
				return 0;
			}

			components[count] = number;
			++count;

			if (comma == std::string::npos)
			{
				break;
			}

			start = comma + 1;
		}

		return count;
	}

	static bool IsWholeNumber(const std::string& value)
	{
		return value.find_first_of(".eE,") == std::string::npos;
	}

	static std::string FormatComponents(const float* components, int count)
	{
		std::string text;
		char number[32];

		for (int i = 0; i < count; ++i)
		{
			std::snprintf(number, sizeof(number), "%.4f", components[i]);

			if (i > 0)
			{
				text += ", ";
			}

			text += number;
		}

		return text;
	}

	static void Write(const remixapi_Interface* api, const std::string& name, const std::string& value)
	{
		api->SetConfigVariable(name.c_str(), value.c_str());
		Device::NoteFrameEvent("remix option " + name);

		OptionState& state = options[name];
		state.sentValue = value;
		state.hasSentValue = true;
		++stats.sentOptions;
	}

	static const std::string* FindLayerValue(const std::string& name)
	{
		for (auto layer = layers.rbegin(); layer != layers.rend(); ++layer)
		{
			for (const auto& [key, value] : layer->second)
			{
				if (key == name)
				{
					return &value;
				}
			}
		}

		return nullptr;
	}

	static const std::string* FindEffectiveValue(const std::string& name, const OptionState& state)
	{
		if (const std::string* layered = FindLayerValue(name))
		{
			return layered;
		}

		if (state.hasOwnValue)
		{
			return &state.ownValue;
		}

		if (state.hasFileValue)
		{
			return &state.fileValue;
		}

		return nullptr;
	}

	std::wstring GetRemixDataDirectory()
	{
		return Core::ModuleDirectory() + L"zw3_remix\\";
	}

	bool TryReadConfFile(const std::wstring& path, OptionList& result)
	{
		std::ifstream file(path);

		if (!file)
		{
			return false;
		}

		std::string line;

		while (std::getline(file, line))
		{
			const std::string trimmed = Trim(line);

			if (trimmed.empty() || trimmed[0] == '#')
			{
				continue;
			}

			const std::size_t equals = trimmed.find('=');

			if (equals == std::string::npos)
			{
				continue;
			}

			const std::string name = Trim(trimmed.substr(0, equals));
			const std::string value = Trim(trimmed.substr(equals + 1));

			if (name.empty() || value.empty())
			{
				continue;
			}

			result.emplace_back(name, value);
		}

		return true;
	}

	static void ReadRtxConf()
	{
		hasReadRtxConf = true;

		OptionList fileOptions;

		if (!TryReadConfFile(Core::ModuleDirectory() + L"rtx.conf", fileOptions))
		{
			LogWarn() << "remix: no rtx.conf beside the game, options a map or trigger changes without a value of ours"
						 " cannot be put back when it ends";
			return;
		}

		for (const auto& [name, value] : fileOptions)
		{
			OptionState& state = options[name];
			state.fileValue = value;
			state.hasFileValue = true;
		}

		stats.fileOptions = static_cast<int>(fileOptions.size());
		Log() << "remix: " << stats.fileOptions << " options read from rtx.conf as the values to fall back to";
	}

	static void MarkPending(const std::string& name, float blendSeconds)
	{
		PendingChange& change = pending[name];
		change.blendSeconds = std::max(change.blendSeconds, blendSeconds);
	}

	static void MarkLayerPending(const OptionList& list, float blendSeconds)
	{
		for (const auto& [name, value] : list)
		{
			MarkPending(name, blendSeconds);
		}
	}

	void SendOption(const remixapi_Interface* api, const char* name, const std::string& value)
	{
		OptionState& state = options[name];
		state.ownValue = value;
		state.hasOwnValue = true;

		if (FindLayerValue(name))
		{
			return;
		}

		state.isBlending = false;
		Write(api, name, value);
	}

	void SetLayerOptions(int layer, const OptionList& list, float blendSeconds)
	{
		const auto existing = layers.find(layer);

		if (existing != layers.end())
		{
			MarkLayerPending(existing->second, blendSeconds);
		}

		layers[layer] = list;
		MarkLayerPending(list, blendSeconds);
	}

	void ClearLayer(int layer, float blendSeconds)
	{
		const auto existing = layers.find(layer);

		if (existing == layers.end())
		{
			return;
		}

		MarkLayerPending(existing->second, blendSeconds);
		layers.erase(existing);
	}

	bool HasLayer(int layer)
	{
		return layers.find(layer) != layers.end();
	}

	void ResetModifiedOptions()
	{
		for (const auto& [layer, list] : layers)
		{
			MarkLayerPending(list, 0.0f);
		}

		layers.clear();
		hasLoadedConfFiles = true;

		Log() << "remix: every option a conf file, map file or trigger changed is back to our own value or rtx.conf";
	}

	void ReloadConfFiles()
	{
		hasLoadedConfFiles = true;

		const std::wstring folder = GetRemixDataDirectory() + L"conf\\";
		std::set<std::wstring> names;

		WIN32_FIND_DATAW found{};
		const HANDLE search = FindFirstFileW((folder + L"*.conf").c_str(), &found);

		if (search != INVALID_HANDLE_VALUE)
		{
			do
			{
				if (!(found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
				{
					names.insert(found.cFileName);
				}
			}
			while (FindNextFileW(search, &found));

			FindClose(search);
		}

		OptionList merged;

		for (const std::wstring& name : names)
		{
			OptionList fileOptions;

			if (TryReadConfFile(folder + name, fileOptions))
			{
				merged.insert(merged.end(), fileOptions.begin(), fileOptions.end());
				Log() << "remix: conf file " << name << " gives " << fileOptions.size() << " options";
			}
		}

		std::reverse(merged.begin(), merged.end());

		stats.confFiles = static_cast<int>(names.size());

		if (merged.empty())
		{
			ClearLayer(confLayer, 0.0f);
			return;
		}

		SetLayerOptions(confLayer, merged, 0.0f);
	}

	static void StartChange(const remixapi_Interface* api, const std::string& name, float blendSeconds, ULONGLONG now)
	{
		OptionState& state = options[name];
		const std::string* effective = FindEffectiveValue(name, state);

		if (!effective)
		{
			state.isBlending = false;
			++stats.unrestorableOptions;
			LogWarn() << "remix: " << name << " was changed by a layer that has ended, but neither we nor rtx.conf"
						 " hold a value for it, so it keeps the last one";
			return;
		}

		float to[maxComponents] = {};
		float from[maxComponents] = {};
		const int toCount = ParseComponents(*effective, to);
		int fromCount = 0;

		if (state.hasSentValue)
		{
			fromCount = ParseComponents(state.sentValue, from);
		}

		const bool canBlend = blendSeconds > 0.0f && toCount > 0 && toCount == fromCount
			&& !(toCount == 1 && IsWholeNumber(*effective));

		if (!canBlend)
		{
			state.isBlending = false;

			if (!state.hasSentValue || state.sentValue != *effective)
			{
				Write(api, name, *effective);
			}

			return;
		}

		state.isBlending = true;
		state.components = toCount;
		std::copy(from, from + maxComponents, state.from);
		std::copy(to, to + maxComponents, state.to);
		state.blendStartMs = now;
		state.blendMs = blendSeconds * 1000.0f;
		state.target = *effective;
	}

	static void AdvanceBlends(const remixapi_Interface* api, ULONGLONG now)
	{
		int blending = 0;

		for (auto& [name, state] : options)
		{
			if (!state.isBlending)
			{
				continue;
			}

			const float progress = std::clamp(static_cast<float>(now - state.blendStartMs) / state.blendMs, 0.0f, 1.0f);

			if (progress >= 1.0f)
			{
				state.isBlending = false;
				Write(api, name, state.target);
				continue;
			}

			const float eased = progress * progress * (3.0f - 2.0f * progress);
			float current[maxComponents] = {};

			for (int i = 0; i < state.components; ++i)
			{
				current[i] = state.from[i] + (state.to[i] - state.from[i]) * eased;
			}

			Write(api, name, FormatComponents(current, state.components));
			++blending;
		}

		stats.blendingOptions = blending;
	}

	void UpdateOptions(IDirect3DDevice9* device)
	{
		const remixapi_Interface* api = TryGetApi(device);

		if (!api)
		{
			return;
		}

		if (!hasReadRtxConf)
		{
			ReadRtxConf();
		}

		if (!hasLoadedConfFiles)
		{
			ReloadConfFiles();
		}

		const ULONGLONG now = GetTickCount64();

		if (!pending.empty())
		{
			std::map<std::string, PendingChange> changes;
			changes.swap(pending);

			for (const auto& [name, change] : changes)
			{
				StartChange(api, name, change.blendSeconds, now);
			}
		}

		AdvanceBlends(api, now);

		if (diSampleCount > 0 && ++reassertFrame % reassertInterval == 0)
		{
			SendOption(api, "rtx.di.initialSampleCount", std::to_string(diSampleCount));
		}
	}

	void SetDiSampleCount(int count)
	{
		diSampleCount = std::clamp(count, 0, 64);
	}

	int GetDiSampleCount()
	{
		return diSampleCount;
	}

	OptionStats GetOptionStats()
	{
		stats.knownOptions = static_cast<int>(options.size());
		stats.layers = static_cast<int>(layers.size());

		int own = 0;
		int overridden = 0;

		for (const auto& [name, state] : options)
		{
			if (state.hasOwnValue)
			{
				++own;
			}

			if (FindLayerValue(name))
			{
				++overridden;
			}
		}

		stats.ownOptions = own;
		stats.overriddenOptions = overridden;

		return stats;
	}

	OptionList DescribeOverriddenOptions()
	{
		OptionList result;

		for (const auto& [name, state] : options)
		{
			if (const std::string* layered = FindLayerValue(name))
			{
				result.emplace_back(name, *layered);
			}
		}

		std::sort(result.begin(), result.end());

		return result;
	}
}
