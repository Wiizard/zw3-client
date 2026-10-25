#pragma once

#include <string>
#include <utility>
#include <vector>

#include <remix/remix_c.h>

namespace Components::Remix
{
	using OptionList = std::vector<std::pair<std::string, std::string>>;

	inline constexpr int confLayer = 0;
	inline constexpr int mapLayer = 1;
	inline constexpr int firstTriggerLayer = 16;

	struct OptionStats
	{
		int knownOptions = 0;
		int fileOptions = 0;
		int ownOptions = 0;
		int layers = 0;
		int overriddenOptions = 0;
		int blendingOptions = 0;
		int unrestorableOptions = 0;
		int confFiles = 0;
		unsigned long long sentOptions = 0;
	};

	void SendOption(const remixapi_Interface* api, const char* name, const std::string& value);

	void SetLayerOptions(int layer, const OptionList& options, float blendSeconds);

	void ClearLayer(int layer, float blendSeconds);

	bool HasLayer(int layer);

	void ResetModifiedOptions();

	void ReloadConfFiles();

	bool TryReadConfFile(const std::wstring& path, OptionList& options);

	std::wstring GetRemixDataDirectory();

	void UpdateOptions(IDirect3DDevice9* device);
	void SetFrontendRendering(bool isFrontend);
	bool IsFrontendRendering();

	void SetDiSampleCount(int count);

	int GetDiSampleCount();

	OptionStats GetOptionStats();

	OptionList DescribeOverriddenOptions();
}
