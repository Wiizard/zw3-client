#pragma once

#include "Types.hpp"
#include "Turntable.hpp"

namespace Components::Menu
{
	struct Position
	{
		float x, y, z;
	};

	struct SceneView
	{
		Position origin;
		std::array<Position, 3> axis;
		float tan_half_fov_x, tan_half_fov_y;
	};

	struct BackdropGraft
	{
		Game::XModel* model;
		std::vector<std::uint16_t> surfaces;
		bool operator==(const BackdropGraft&) const = default;
	};

	struct BackdropModel
	{
		Game::XModel* model;
		std::optional<std::string> tag;
		std::optional<std::vector<std::uint16_t>> surfaces;
		std::vector<BackdropGraft> grafts;
		bool operator==(const BackdropModel&) const = default;
	};

	struct BackdropFigure
	{
		std::vector<BackdropModel> models;
		std::vector<std::string> hidden_tags;
		std::optional<std::string> animation;
		std::array<float, 3> origin{};
		std::array<float, 3> pivot{};
		float yaw = 0;
		float background_blur = 0;
	};

	struct BackdropDirection
	{
		std::string map;
		std::optional<SceneView> view;
		std::optional<BackdropFigure> figure;
	};

	// Models and surfaces supplied by a subject must remain loaded until the
	// renderer has finished with the figure. Direct applies to the next frame.
	class Backdrop : public Component
	{
	public:
		Backdrop();
		static bool IsEnabled();
		static bool IsShown();
		static bool IsStartupLoading();
		static bool Hides(const Game::Material* material);
		static void Direct(BackdropDirection direction);
		static void AddZone(std::string name);
		static void AddSubject(std::string name, std::function<std::optional<BackdropFigure>()> subject);
		static std::optional<BackdropFigure> FindFigure(std::string_view name);
		static std::optional<Position> FindTag(const BackdropFigure& figure, std::string_view tag);
		static void BeforeZoneLoad(const Game::XZoneInfo* zones, unsigned int count);
	};
}
