#pragma once

namespace Components::GSC
{
	class Field : public Component
	{
	public:
		struct ScoreboardStats
		{
			int downs = 0;
			int revives = 0;
			int isDown = 0;
		};

		Field();

		static int GetClientDowns(int clientNum);
		static int GetClientRevives(int clientNum);
		static bool IsClientDown(int clientNum);
		static void ResetClientScoreboardStats(int clientNum);

	private:
		struct EntField
		{
			const char* name;
			int ofs;
			void(*setter)(Game::gentity_s*, int);
			void(*getter)(Game::gentity_s*, int);
		};

		struct ClientFields
		{
			const char* name;
			int ofs;
			void(*setter)(Game::client_s*, Game::gclient_s*, const ClientFields*);
			void(*getter)(Game::client_s*, Game::gclient_s*, const ClientFields*);
		};

		typedef void(*ScriptCallbackEnt)(Game::gentity_s*, int);
		typedef void(*ScriptCallbackClient)(Game::client_s*, Game::gclient_s*, const ClientFields*);

		static std::unordered_map<std::uint16_t, EntField> CustomEntityFields;
		static std::unordered_map<std::uint16_t, ClientFields> CustomClientFields;
		static std::array<ScoreboardStats, Game::MAX_CLIENTS> ClientScoreboardStats;

		static void AddEntityField(const char* name, const ScriptCallbackEnt& setter, const ScriptCallbackEnt& getter);
		static void AddClientField(const char* name, const ScriptCallbackClient& setter, const ScriptCallbackClient& getter);

		static void GScr_AddFieldsForEntityStub();

		// Two hooks because it makes our code cleaner (luckily functions were not inlined)
		static int Scr_SetObjectFieldStub(unsigned int classnum, int entnum, int offset);
		static void Scr_SetClientFieldStub(Game::gclient_s* client, int offset);

		// One hook because functions were inlined
		static void Scr_GetEntityFieldStub(int entnum, int offset);

		static void AddEntityFields();
		static void AddClientFields();

		static int GetClientNum(Game::gclient_s* client);
		static bool IsValidClientNum(int clientNum);
	};
}
