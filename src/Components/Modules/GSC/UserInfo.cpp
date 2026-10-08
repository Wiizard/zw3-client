#include "STDInclude.hpp"

#include "UserInfo.hpp"
#include "Script.hpp"
#include "../Events.hpp"
#include "../Logger.hpp"

namespace Components::GSC
{
	std::unordered_map<int, UserInfo::userInfoMap> UserInfo::userInfoOverrides;

	constexpr std::uintptr_t SV_GetUserinfo = 0x14023A210;

	constexpr std::uintptr_t SV_GetUserinfoCalls[] = { 0x140195FB3, 0x1401960EA, 0x1401968F6 };

	static Utils::Hook hooks[std::size(SV_GetUserinfoCalls)];

	void UserInfo::SV_GetUserInfo_Stub(int index, char* buffer, int bufferSize)
	{
		Utils::Hook::Call<void(int, char*, int)>(SV_GetUserinfo)(index, buffer, bufferSize);

		Utils::InfoString map(buffer);

		if (!userInfoOverrides.contains(index))
		{
			userInfoOverrides[index] = {};
		}

		for (const auto& [key, value] : userInfoOverrides[index])
		{
			if (value.empty())
			{
				map.Remove(key);
			}
			else
			{
				map.Set(key, value);
			}
		}

		const auto userInfo = map.Build();
		strncpy_s(buffer, bufferSize, userInfo.data(), _TRUNCATE);
	}

	void UserInfo::ClearClientOverrides(const int clientNum)
	{
		userInfoOverrides[clientNum].clear();
	}

	void UserInfo::ClearAllOverrides()
	{
		userInfoOverrides.clear();
	}

	void UserInfo::AddScriptMethods()
	{
		Script::AddMethod("SetName", [](Game::scr_entref_t entref)
		{
			const auto* ent = Script::Scr_GetPlayerEntity(entref);
			const auto* name = Game::Scr_GetString(0);

			if (!name)
			{
				Script::Scr_ParamError(0, "SetName: Illegal parameter!");
				return;
			}

			Logger::Debug("Setting name of {} to {}", ent->s.number, name);
			userInfoOverrides[ent->s.number]["name"] = name;
			Game::ClientUserinfoChanged(ent->s.number);
		});

		Script::AddMethod("ResetName", [](Game::scr_entref_t entref)
		{
			const auto* ent = Script::Scr_GetPlayerEntity(entref);

			Logger::Debug("Resetting name of {}", ent->s.number);
			userInfoOverrides[ent->s.number].erase("name");
			Game::ClientUserinfoChanged(ent->s.number);
		});

		Script::AddMethod("SetClanTag", [](Game::scr_entref_t entref)
		{
			const auto* ent = Script::Scr_GetPlayerEntity(entref);
			const auto* clanName = Game::Scr_GetString(0);

			if (!clanName)
			{
				Script::Scr_ParamError(0, "SetClanTag: Illegal parameter!");
				return;
			}

			Logger::Debug("Setting clanName of {} to {}", ent->s.number, clanName);
			userInfoOverrides[ent->s.number]["clanAbbrev"] = clanName;
			Game::ClientUserinfoChanged(ent->s.number);
		});

		Script::AddMethod("ResetClanTag", [](Game::scr_entref_t entref)
		{
			const auto* ent = Script::Scr_GetPlayerEntity(entref);

			Logger::Debug("Resetting clanName of {}", ent->s.number);
			userInfoOverrides[ent->s.number].erase("clanAbbrev");
			Game::ClientUserinfoChanged(ent->s.number);
		});
	}

	UserInfo::UserInfo()
	{
		for (const std::uintptr_t site : SV_GetUserinfoCalls)
		{
			if (!Utils::Hook::BranchesTo(site, SV_GetUserinfo, HOOK_CALL))
			{
				Logger::Error("userinfo: 0x{:X} no longer calls SV_GetUserinfo, SetName and SetClanTag will not exist\n", site);
				return;
			}
		}

		bool isSeated = true;

		for (std::size_t i = 0; i < std::size(SV_GetUserinfoCalls); ++i)
		{
			isSeated = hooks[i].Initialize(SV_GetUserinfoCalls[i], reinterpret_cast<void*>(SV_GetUserInfo_Stub), HOOK_CALL)->Install()->IsInstalled() && isSeated;
		}

		if (!isSeated)
		{
			for (auto& hook : hooks)
			{
				hook.Uninstall();
			}

			Logger::Error("userinfo: could not seat every hook, SetName and SetClanTag will not exist\n");
			return;
		}

		for (auto& hook : hooks)
		{
			hook.Quick();
		}

		AddScriptMethods();

		Events::OnVMShutdown(ClearAllOverrides);
		Events::OnClientDisconnect(ClearClientOverrides);
	}
}
