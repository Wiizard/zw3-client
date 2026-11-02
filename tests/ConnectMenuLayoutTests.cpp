#include "Utils/UiScaling.hpp"

#include <fstream>
#include <iostream>
#include <iterator>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
	void Require(bool value, const std::string& message)
	{
		if (!value) throw std::runtime_error(message);
	}
	struct Rect { float x, y, w, h; int horz = 0, vert = 0; };
	struct Item { std::string body; Rect rect; };
	std::string Read(const char* path)
	{
		std::ifstream input(path);
		Require(input.good(), std::string("cannot read menu: ") + path);
		return {std::istreambuf_iterator<char>(input), {}};
	}
	std::vector<Item> Items(const std::string& source)
	{
		std::vector<Item> result;
		const std::regex items(R"(itemDef\s*\{([^}]+)\})");
		const std::regex rectangle(R"(\brect\s+([-0-9.]+\s+[-0-9.]+\s+[-0-9.]+\s+[-0-9.]+(?:\s+[0-9]+\s+[0-9]+)?))");
		for (auto i = std::sregex_iterator(source.begin(), source.end(), items); i != std::sregex_iterator(); ++i)
		{
			Item item{(*i)[1], {}};
			std::smatch match;
			Require(std::regex_search(item.body, match, rectangle), "loading item is missing its rectangle");
			std::istringstream values(match[1]);
			values >> item.rect.x >> item.rect.y >> item.rect.w >> item.rect.h;
			values >> item.rect.horz >> item.rect.vert;
			result.push_back(std::move(item));
		}
		return result;
	}
	bool IsProgress(const Item& item)
	{
		return item.body.find("zw3_ui_loading_progress") != std::string::npos || (item.rect.w == 382 && item.rect.h == 12);
	}
	std::string Style(const Item& item)
	{
		std::istringstream input(item.body);
		std::string line, result;
		while (std::getline(input, line))
		{
			const auto first = line.find_first_not_of(" \t\r");
			if (first == std::string::npos) continue;
			line = line.substr(first);
			if (line.starts_with("rect ") || line.starts_with("exp rect x") || (IsProgress(item) && line.starts_with("visible "))) continue;
			result += line + '\n';
		}
		return result;
	}
	Utils::UiScaling::Placement Placement(float width, float height)
	{
		Utils::UiScaling::Placement placement{};
		for (int axis = 0; axis < 2; ++axis)
		{
			const float size = axis == 0 ? width : height;
			placement.realViewportSize[axis] = size;
			placement.scaleVirtualToReal[axis] = height / 480.0f;
			placement.scaleVirtualToFull[axis] = size / (axis == 0 ? 640.0f : 480.0f);
			placement.scaleRealToVirtual[axis] = 480.0f / height;
			const float margin = std::floor(size * 0.075f + 0.5f);
			placement.realViewableMin[axis] = margin;
			placement.realViewableMax[axis] = size - margin;
			placement.realAdjustableMax[axis] = size;
		}
		placement.subScreenLeft = (width - 640.0f * placement.scaleVirtualToReal[0]) * 0.5f;
		Utils::UiScaling::Fit(placement);
		return placement;
	}
	Rect Place(const Item& item, const Utils::UiScaling::Placement& placement, float time)
	{
		auto actual = item.rect;
		std::smatch match;
		if (item.body.find("$levelbriefing") != std::string::npos)
		{
			Require(std::regex_search(item.body, match, std::regex(R"(exp rect x\s*\(\s*([-0-9.]+)\s*\+.*\*\s*([0-9.]+)\))")), "preview pan animation is missing");
			actual.x = std::stof(match[1]) + std::sin(time / 2400.0f) * std::stof(match[2]);
		}
		if (item.body.find("mw2_main_cloud_overlay") != std::string::npos)
		{
			Require(std::regex_search(item.body, match, std::regex(R"(exp rect x\s*\(\s*\(\s*0\s*-\s*([0-9.]+)\s*\).+\*\s*\(\s*([0-9.]+)\s*\))")), "cloud scroll animation is missing");
			actual.x = -std::stof(match[1]) - std::fmod(time, 40000.0f) / 40000.0f * std::stof(match[2]);
		}
		Utils::UiScaling::ApplyRect(placement, actual.x, actual.y, actual.w, actual.h, actual.horz, actual.vert);
		return actual;
	}
}

int main(int argc, char** argv)
{
	try
	{
		Require(argc == 2 || argc == 3, "pass installed connect.menu and optionally its original backup");
		const std::string source = Read(argv[1]);
		const auto items = Items(source);
		Require(items.size() == 42, "the original loading-screen layers must be retained");
		Require(!std::regex_search(source.substr(0, source.find("itemDef")), std::regex(R"(\bvisible\s+1\b)")), "loading menu must remain hidden until opened");
		Require(items[37].body.find("wideScreen") == std::string::npos && items[38].body.find("wideScreen") == std::string::npos,
			"the original progress bar must also be visible on narrow displays");
		Require(items[38].body.find("zw3_ui_loading_visible") != std::string::npos, "loading progress visibility binding is missing");
		if (argc == 3)
		{
			const auto original = Items(Read(argv[2]));
			Require(original.size() == items.size(), "original layer count changed");
			for (std::size_t i = 0; i < items.size(); ++i)
			{
				Require(Style(items[i]) == Style(original[i]), "artwork, text, colors, effects or actions changed in layer " + std::to_string(i));
				for (float time : {0.0f, 3770.0f, 11310.0f, 20000.0f, 39999.0f})
				{
					const auto before = Place(original[i], Placement(1920, 1080), time);
					const auto after = Place(items[i], Placement(1920, 1080), time);
					Require(std::abs(before.x - after.x) < 0.01f && std::abs(before.y - after.y) < 0.01f
						&& std::abs(before.w - after.w) < 0.01f && std::abs(before.h - after.h) < 0.01f,
						"original 16:9 placement changed in layer " + std::to_string(i));
				}
			}
		}
		for (const auto size : {std::pair{640.0f, 480.0f}, std::pair{1024.0f, 768.0f}, std::pair{1280.0f, 720.0f}, std::pair{1280.0f, 800.0f},
			std::pair{1920.0f, 1080.0f}, std::pair{1920.0f, 1200.0f}, std::pair{3440.0f, 1440.0f}, std::pair{3840.0f, 2160.0f}, std::pair{800.0f, 1280.0f}})
		{
			for (float time : {0.0f, 3770.0f, 11310.0f, 20000.0f, 39999.0f})
			{
				const auto placement = Placement(size.first, size.second);
				for (std::size_t i = 0; i < items.size(); ++i)
				{
					const auto rect = Place(items[i], placement, time);
					if (i <= 2 || i == 39)
					{
						Require(rect.x <= 0.01f && rect.y <= 0.01f && rect.x + rect.w >= size.first - 0.01f && rect.y + rect.h >= size.second - 0.01f,
							"animated background leaves a screen edge uncovered");
					}
					else if (i >= 6)
					{
						Require(rect.x >= -0.01f && rect.y >= -0.01f && rect.x + rect.w <= size.first + 0.01f && rect.y + rect.h <= size.second + 0.01f,
							"original foreground layer exceeds the screen: " + std::to_string(i));
					}
					else if (i == 4 || i == 5)
					{
						Require(std::abs(rect.x) < 0.01f && std::abs(rect.w - size.first) < 0.01f && rect.y + rect.h >= size.second,
							"bottom gradient does not cover the bottom edge");
					}
				}
			}
		}
		std::cout << "Original connect.menu design preserved; animated backgrounds fill the screen and foreground fits 9 resolutions\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
