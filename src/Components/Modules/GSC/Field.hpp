#pragma once

namespace Components::GSC
{
	class Field : public Component
	{
	public:
		Field();

		static int GetClientDowns(int clientNum);
		static int GetClientRevives(int clientNum);
		static bool IsClientDown(int clientNum);
		static void ResetClientScoreboardStats(int clientNum);

	private:
		struct ScoreboardStats
		{
			int downs = 0;
			int revives = 0;
			bool isDown = false;
		};

		static std::array<ScoreboardStats, Game::MAX_CLIENTS> clientScoreboardStats;

		static int GetClientNum(const Game::gclient_s* client);
		static bool IsValidClientNum(int clientNum);

		struct EntField
		{
			const char* name;
			int offset;
			void(*setter)(Game::gentity_s*, int);
			void(*getter)(Game::gentity_s*, int);
		};

		struct ClientFields
		{
			const char* name;
			int offset;
			void(*setter)(Game::client_s*, Game::gclient_s*, const ClientFields*);
			void(*getter)(Game::client_s*, Game::gclient_s*, const ClientFields*);
		};

		typedef void(*ScriptCallbackEnt)(Game::gentity_s*, int);
		typedef void(*ScriptCallbackClient)(Game::client_s*, Game::gclient_s*, const ClientFields*);

		static std::unordered_map<std::uint16_t, EntField> customEntityFields;
		static std::unordered_map<std::uint16_t, ClientFields> customClientFields;

		static void AddEntityField(const char* name, const ScriptCallbackEnt& setter, const ScriptCallbackEnt& getter);
		static void AddClientField(const char* name, const ScriptCallbackClient& setter, const ScriptCallbackClient& getter);

		static void GScr_AddFieldsForEntityStub();

		static int Scr_SetObjectFieldStub(unsigned int classnum, int entnum, int offset);
		static void Scr_SetClientFieldStub(Game::gclient_s* client, int offset);

		static void Scr_GetEntityFieldStub(int entnum, int offset);

		static void AddEntityFields();
		static void AddClientFields();
	};
}
