#pragma once

#include "Dvar.hpp"

namespace Components
{
	class MapRotation : public Component
	{
	public:
		MapRotation();

		static bool Contains(const std::string& key, const std::string& value);

		static nlohmann::json ToJson();

	private:
		class RotationData
		{
		public:
			using RotationEntry = std::pair<std::string, std::string>;
			using RotationCallback = std::function<void(const std::string&)>;

			void Randomize();

			void AddEntry(const std::string& key, const std::string& value);

			[[nodiscard]] std::size_t GetEntriesSize() const;
			const RotationEntry& GetNextEntry();
			[[nodiscard]] const RotationEntry& PeekNextEntry() const;

			void SetHandler(const std::string& key, const RotationCallback& callback);
			void CallHandler(const RotationEntry& entry) const;

			bool TryParse(const std::string& data, std::string& invalidKey);

			[[nodiscard]] bool IsEmpty() const;
			[[nodiscard]] bool Contains(const std::string& key, const std::string& value) const;
			[[nodiscard]] bool ContainsHandler(const std::string& key) const;

			[[nodiscard]] nlohmann::json ToJson() const;

		private:
			std::vector<RotationEntry> entries;
			std::unordered_map<std::string, RotationCallback> handlers;
			std::size_t index = 0;
		};

		static Dvar::Var sv_mapRotation;
		static Dvar::Var sv_mapRotationCurrent;
		static Dvar::Var sv_randomMapRotation;
		static Dvar::Var sv_dontRotate;
		static Dvar::Var sv_nextMap;

		static RotationData dedicatedRotation;

		static void RandomizeMapRotation();
		static void ParseRotation(const std::string& data);
		static void LoadMapRotation();

		static void AddMapRotationCommands();
		static void RegisterMapRotationDvars();

		static bool ShouldRotate();
		static void ApplyMap(const std::string& map);
		static void ApplyGametype(const std::string& gametype);
		static void ApplyExec(const std::string& name);
		static void RestartCurrentMap();
		static void ApplyRotation(RotationData& rotation);
		static void ApplyMapRotationCurrent(const std::string& data);

		static void SetNextMap(const RotationData& rotation);
		static void SetNextMap(const char* value);
		static void ClearNextMap();

		static void SV_MapRotate_f();

		static void ExitLevel_Hk();
	};
}
