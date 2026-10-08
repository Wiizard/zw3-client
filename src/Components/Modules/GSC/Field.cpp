#include "STDInclude.hpp"

#include "Field.hpp"
#include "../Logger.hpp"
#include "../Network.hpp"

namespace Components::GSC
{
	std::unordered_map<std::uint16_t, Field::EntField> Field::customEntityFields;
	std::unordered_map<std::uint16_t, Field::ClientFields> Field::customClientFields;
	std::array<Field::ScoreboardStats, Game::MAX_CLIENTS> Field::clientScoreboardStats;

	constexpr std::uintptr_t GScr_AddFieldsForEntity_ClientCall = 0x1401A8B55;
	constexpr std::uintptr_t GScr_AddFieldsForClient = 0x140163DF0;

	constexpr std::uintptr_t SetEntityFieldValue_SetObjectFieldCall = 0x14022B445;
	constexpr std::uintptr_t Scr_SetObjectField = 0x1401A9F30;
	constexpr std::uintptr_t Scr_SetObjectField_SetClientFieldCall = 0x1401AA15D;
	constexpr std::uintptr_t Scr_SetClientField = 0x140163E80;
	constexpr std::uintptr_t Scr_GetObjectField_GetEntityFieldJump = 0x1401A9DE9;
	constexpr std::uintptr_t Scr_GetEntityField = 0x1401A9A90;

	static Utils::Hook hooks[4];

	void Field::AddEntityField(const char* name, const ScriptCallbackEnt& setter, const ScriptCallbackEnt& getter)
	{
		static std::uint16_t fieldOffsetStart = 15;
		assert((fieldOffsetStart & Game::ENTFIELD_MASK) == Game::ENTFIELD_ENTITY);

		customEntityFields[fieldOffsetStart] = { name, fieldOffsetStart, setter, getter };
		++fieldOffsetStart;
	}

	void Field::AddClientField(const char* name, const ScriptCallbackClient& setter, const ScriptCallbackClient& getter)
	{
		static std::uint16_t fieldOffsetStart = 21;
		assert((fieldOffsetStart & Game::ENTFIELD_MASK) == Game::ENTFIELD_ENTITY);

		const auto offset = fieldOffsetStart | Game::ENTFIELD_CLIENT;

		customClientFields[fieldOffsetStart] = { name, offset, setter, getter };
		++fieldOffsetStart;
	}

	void Field::GScr_AddFieldsForEntityStub()
	{
		for (const auto& field : customEntityFields | std::views::values)
		{
			Game::Scr_AddClassField(Game::CLASS_NUM_ENTITY, field.name, field.offset);
		}

		Utils::Hook::Call<void()>(GScr_AddFieldsForClient)();

		for (const auto& field : customClientFields | std::views::values)
		{
			Game::Scr_AddClassField(Game::CLASS_NUM_ENTITY, field.name, field.offset);
		}
	}

	int Field::Scr_SetObjectFieldStub(const unsigned int classnum, const int entnum, const int offset)
	{
		if (classnum == Game::CLASS_NUM_ENTITY)
		{
			const auto entityOffset = static_cast<std::uint16_t>(offset);

			if (const auto itr = customEntityFields.find(entityOffset); itr != customEntityFields.end())
			{
				itr->second.setter(&Game::g_entities[entnum], offset);
				return 1;
			}
		}

		return Game::Scr_SetObjectField(classnum, entnum, offset);
	}

	void Field::Scr_SetClientFieldStub(Game::gclient_s* client, const int offset)
	{
		const auto clientOffset = static_cast<std::uint16_t>(offset);

		if (const auto itr = customClientFields.find(clientOffset); itr != customClientFields.end())
		{
			itr->second.setter(nullptr, client, &itr->second);
			return;
		}

		Game::Scr_SetClientField(client, offset);
	}

	void Field::Scr_GetEntityFieldStub(const int entnum, int offset)
	{
		if ((offset & Game::ENTFIELD_MASK) == Game::ENTFIELD_CLIENT)
		{
			if (Game::g_entities[entnum].client != nullptr)
			{
				const auto clientOffset = static_cast<std::uint16_t>(offset & ~Game::ENTFIELD_MASK);

				if (const auto itr = customClientFields.find(clientOffset); itr != customClientFields.end())
				{
					itr->second.getter(&Game::svs_clients[entnum], Game::g_entities[entnum].client, &itr->second);
					return;
				}
			}
		}

		const auto entityOffset = static_cast<std::uint16_t>(offset);

		if (const auto itr = customEntityFields.find(entityOffset); itr != customEntityFields.end())
		{
			itr->second.getter(&Game::g_entities[entnum], offset);
			return;
		}

		Game::Scr_GetEntityField(entnum, offset);
	}

	void Field::AddEntityFields()
	{
		AddEntityField("entityflags",
			[](Game::gentity_s* ent, [[maybe_unused]] int offset)
			{
				ent->flags = Game::Scr_GetInt(0);
			},
			[](Game::gentity_s* ent, [[maybe_unused]] int offset)
			{
				Game::Scr_AddInt(ent->flags);
			}
		);
	}

	int Field::GetClientNum(const Game::gclient_s* client)
	{
		if (!client)
		{
			return -1;
		}

		for (int i = 0; i < static_cast<int>(Game::MAX_CLIENTS); ++i)
		{
			if (Game::g_entities[i].client == client)
			{
				return i;
			}
		}

		return client->sess.cs.clientIndex;
	}

	bool Field::IsValidClientNum(const int clientNum)
	{
		return clientNum >= 0 && clientNum < static_cast<int>(clientScoreboardStats.size());
	}

	int Field::GetClientDowns(const int clientNum)
	{
		if (!IsValidClientNum(clientNum))
		{
			return 0;
		}

		return clientScoreboardStats[clientNum].downs;
	}

	int Field::GetClientRevives(const int clientNum)
	{
		if (!IsValidClientNum(clientNum))
		{
			return 0;
		}

		return clientScoreboardStats[clientNum].revives;
	}

	bool Field::IsClientDown(const int clientNum)
	{
		if (!IsValidClientNum(clientNum))
		{
			return false;
		}

		return clientScoreboardStats[clientNum].isDown;
	}

	void Field::ResetClientScoreboardStats(const int clientNum)
	{
		if (!IsValidClientNum(clientNum))
		{
			return;
		}

		clientScoreboardStats[clientNum] = {};
	}

	void Field::AddClientFields()
	{
		AddClientField("clientflags",
			[]([[maybe_unused]] Game::client_s* client, Game::gclient_s* self, [[maybe_unused]] const ClientFields* field)
			{
				self->flags = Game::Scr_GetInt(0);
			},
			[]([[maybe_unused]] Game::client_s* client, Game::gclient_s* self, [[maybe_unused]] const ClientFields* field)
			{
				Game::Scr_AddInt(self->flags);
			}
		);

		AddClientField("ping",
			[]([[maybe_unused]] Game::client_s* client, [[maybe_unused]] Game::gclient_s* self, [[maybe_unused]] const ClientFields* field)
			{
			},
			[](Game::client_s* client, [[maybe_unused]] Game::gclient_s* self, [[maybe_unused]] const ClientFields* field)
			{
				Game::Scr_AddInt(client->ping);
			}
		);

		AddClientField("address",
			[]([[maybe_unused]] Game::client_s* client, [[maybe_unused]] Game::gclient_s* self, [[maybe_unused]] const ClientFields* field)
			{
			},
			[](Game::client_s* client, [[maybe_unused]] Game::gclient_s* self, [[maybe_unused]] const ClientFields* field)
			{
				Game::Scr_AddString(Network::AdrToString(Network::Address(&client->header.netchan.remoteAddress)));
			}
		);

		AddClientField("downs",
			[]([[maybe_unused]] Game::client_s* client, Game::gclient_s* self, [[maybe_unused]] const ClientFields* field)
			{
				const int clientNum = GetClientNum(self);

				if (IsValidClientNum(clientNum))
				{
					clientScoreboardStats[clientNum].downs = Game::Scr_GetInt(0);
				}
			},
			[]([[maybe_unused]] Game::client_s* client, Game::gclient_s* self, [[maybe_unused]] const ClientFields* field)
			{
				Game::Scr_AddInt(GetClientDowns(GetClientNum(self)));
			}
		);

		AddClientField("revives",
			[]([[maybe_unused]] Game::client_s* client, Game::gclient_s* self, [[maybe_unused]] const ClientFields* field)
			{
				const int clientNum = GetClientNum(self);

				if (IsValidClientNum(clientNum))
				{
					clientScoreboardStats[clientNum].revives = Game::Scr_GetInt(0);
				}
			},
			[]([[maybe_unused]] Game::client_s* client, Game::gclient_s* self, [[maybe_unused]] const ClientFields* field)
			{
				Game::Scr_AddInt(GetClientRevives(GetClientNum(self)));
			}
		);

		AddClientField("isDown",
			[]([[maybe_unused]] Game::client_s* client, Game::gclient_s* self, [[maybe_unused]] const ClientFields* field)
			{
				const int clientNum = GetClientNum(self);

				if (IsValidClientNum(clientNum))
				{
					clientScoreboardStats[clientNum].isDown = Game::Scr_GetInt(0) != 0;
				}
			},
			[]([[maybe_unused]] Game::client_s* client, Game::gclient_s* self, [[maybe_unused]] const ClientFields* field)
			{
				int isDown = 0;

				if (IsClientDown(GetClientNum(self)))
				{
					isDown = 1;
				}

				Game::Scr_AddInt(isDown);
			}
		);
	}

	Field::Field()
	{
		struct HookSite
		{
			std::uintptr_t site;
			std::uintptr_t target;
			void* stub;
			bool isJump;
		};

		const HookSite sites[] =
		{
			{ GScr_AddFieldsForEntity_ClientCall, GScr_AddFieldsForClient, reinterpret_cast<void*>(GScr_AddFieldsForEntityStub), HOOK_CALL },
			{ SetEntityFieldValue_SetObjectFieldCall, Scr_SetObjectField, reinterpret_cast<void*>(Scr_SetObjectFieldStub), HOOK_CALL },
			{ Scr_SetObjectField_SetClientFieldCall, Scr_SetClientField, reinterpret_cast<void*>(Scr_SetClientFieldStub), HOOK_CALL },
			{ Scr_GetObjectField_GetEntityFieldJump, Scr_GetEntityField, reinterpret_cast<void*>(Scr_GetEntityFieldStub), HOOK_JUMP },
		};

		static_assert(std::size(sites) == std::size(hooks));

		for (const auto& hookSite : sites)
		{
			if (!Utils::Hook::BranchesTo(hookSite.site, hookSite.target, hookSite.isJump))
			{
				Logger::Error("field: 0x{:X} no longer reaches 0x{:X}, no field of ours will exist\n", hookSite.site, hookSite.target);
				return;
			}
		}

		AddEntityFields();
		AddClientFields();

		bool isSeated = true;

		for (std::size_t i = 0; i < std::size(sites); ++i)
		{
			isSeated = hooks[i].Initialize(sites[i].site, sites[i].stub, sites[i].isJump)->Install()->IsInstalled() && isSeated;
		}

		if (!isSeated)
		{
			for (auto& hook : hooks)
			{
				hook.Uninstall();
			}

			customEntityFields.clear();
			customClientFields.clear();

			Logger::Error("field: could not seat every hook, no field of ours will exist\n");
			return;
		}

		for (auto& hook : hooks)
		{
			hook.Quick();
		}
	}
}
