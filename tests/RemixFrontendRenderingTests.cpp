#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <d3d9.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "Components/Modules/Remix/Options.hpp"
#include "Components/Modules/Remix/Core.hpp"
#include "Components/Modules/Remix/Api.hpp"

namespace
{
	std::wstring fixtureDirectory;
	std::vector<std::string> raytracingWrites;
	remixapi_Interface api{};

	remixapi_ErrorCode REMIXAPI_PTR CaptureOption(const char* key, const char* value)
	{
		if (std::string(key) == "rtx.enableRaytracing") raytracingWrites.emplace_back(value);
		return REMIXAPI_ERROR_CODE_SUCCESS;
	}

	void RequireMode(const std::string& mode)
	{
		if (raytracingWrites.empty() || raytracingWrites.back() != mode)
			throw std::runtime_error("Unexpected raytracing mode during lobby/map transition: expected " + mode);
	}
}

namespace Components::Remix
{
	LogLine::~LogLine() = default;
	LogLine& LogLine::operator<<(const std::wstring&) { return *this; }
	LogLine& LogLine::operator<<(const wchar_t*) { return *this; }
	const remixapi_Interface* TryGetApi(IDirect3DDevice9*) { return &api; }
	namespace Core { std::wstring ModuleDirectory() { return fixtureDirectory; } }
	namespace Device { void NoteFrameEvent(const std::string&) {} }
}

int main(int argc, char** argv)
{
	try
	{
		using namespace Components::Remix;
		const std::string baseline = argc > 1 ? argv[1] : "True";
		const auto fixture = std::filesystem::path("build") / ("remix-frontend-fixture-" + std::to_string(GetCurrentProcessId()));
		std::filesystem::create_directories(fixture);
		fixtureDirectory = std::filesystem::absolute(fixture).wstring() + L"\\";
		if (baseline != "default") std::ofstream(fixture / "rtx.conf") << "rtx.enableRaytracing = " << baseline << '\n';
		api.SetConfigVariable = CaptureOption;
		auto* device = reinterpret_cast<IDirect3DDevice9*>(1);
		for (int visit = 0; visit < 3; ++visit)
		{
			SetFrontendRendering(true);
			if (!IsFrontendRendering()) throw std::runtime_error("Menu draw hooks did not enter raster mode");
			UpdateOptions(device);
			RequireMode("False");
			const auto writes = raytracingWrites.size();
			SetFrontendRendering(true);
			UpdateOptions(device);
			if (raytracingWrites.size() != writes) throw std::runtime_error("An unchanged lobby rewrote the rendering mode");
			SetFrontendRendering(false);
			if (IsFrontendRendering()) throw std::runtime_error("Game draw hooks did not restore Remix mode");
			UpdateOptions(device);
			RequireMode(baseline == "default" ? "True" : baseline);
		}
		SetFrontendRendering(true);
		SetLayerOptions(mapLayer, { { "rtx.enableRaytracing", "True" } }, 0.0f);
		UpdateOptions(device);
		RequireMode("False");
		SendOption(&api, "rtx.enableRaytracing", "True");
		RequireMode("False");
		ResetModifiedOptions();
		UpdateOptions(device);
		RequireMode("False");
		SetFrontendRendering(false);
		UpdateOptions(device);
		RequireMode("True");
		SetLayerOptions(mapLayer, { { "rtx.enableRaytracing", "False" } }, 0.0f);
		UpdateOptions(device);
		RequireMode("False");
		SetFrontendRendering(true);
		UpdateOptions(device);
		SetFrontendRendering(false);
		UpdateOptions(device);
		RequireMode("False");
		ClearLayer(mapLayer, 0.0f);
		UpdateOptions(device);
		RequireMode("True");
		std::cout << "Frontend rendering: repeated lobby/map transitions, config " << baseline
			<< ", map overrides, live settings and option resets passed\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
