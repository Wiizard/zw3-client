#pragma once

#include "Dvar.hpp"
#include "UIScript.hpp"

namespace Components
{
	class Theatre : public Component
	{
	public:
		Theatre();

		static void StopRecording();

	private:
		class DemoInfo
		{
		public:
			std::string name;
			std::string mapname;
			std::string gametype;
			std::string author;
			int length;
			std::time_t timeStamp;

			[[nodiscard]] nlohmann::json to_json() const;
		};

		static DemoInfo currentInfo;
		static unsigned int currentSelection;
		static std::vector<DemoInfo> demos;

		static Dvar::Var cl_autoRecord;
		static Dvar::Var cl_demosKeep;

		static char baselineSnapshot[131072];
		static int baselineSnapshotMsgLen;
		static int baselineSnapshotMsgOff;

		static void WriteBaseline();

		static void LoadDemos(const UIScript::Token& token);
		static void DeleteDemo(const UIScript::Token& token);
		static void PlayDemo(const UIScript::Token& token);

		static unsigned int GetDemoCount();
		static const char* GetDemoText(unsigned int item, int column);
		static void SelectDemo(unsigned int index);

		static int CL_GetSnapshot_Hk(int localClientNum, int snapshotNumber, Game::snapshot_s* snapshot);
		static void CL_ParseServerMessage_Hk(int localClientNum, Game::msg_t* msg);
		static void GamestateWrite_Hk(Game::msg_t* msg, int byte);
		static void RecordGamestate_Hk(const void* buffer, int length, Game::fileHandle_t file);
		static void BaselineToFile_Hk();
		static bool CL_ServerTimedOut_Hk();

		static int CL_FirstSnapshot_Hk();
		static void SV_SpawnServer_Hk();

		static void CG_CompassDrawPlayerMapLocationSelector_Hk(int localClientNum, Game::CompassType compassType, const Game::rectDef_s* parentRect, const Game::rectDef_s* rect, Game::Material* material, float* color);
		static void CL_WriteDemoClientArchive_Hk(void(*write)(const void* buffer, int len, int localClientNum), const Game::playerState_s* ps, const float* viewangles, const float* selectedLocation, float selectedLocationAngle, int localClientNum, int index);

		static Game::fileHandle_t RecordStub(const char* file);
		static void StopRecordStub(Game::fileHandle_t file);

	public:
		static void StoreBaseline(Game::msg_t* snapshotMsg);
		static bool AdjustTimeDelta();
	};
}
