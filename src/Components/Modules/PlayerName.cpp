#include "STDInclude.hpp"

#include "PlayerName.hpp"
#include "ClanTags.hpp"
#include "Events.hpp"
#include "Logger.hpp"
#include "Network.hpp"
#include "TextRenderer.hpp"

namespace Components
{
	Dvar::Var PlayerName::sv_allowColoredNames;

	constexpr std::uintptr_t SV_UpdateUserinfo_f = 0x140236A10;
	static const std::uint8_t updateUserinfoEntry[] = { 0x40, 0x53, 0x48, 0x83, 0xEC, 0x20 };

	constexpr std::uintptr_t ClientCleanName = 0x140195DF0;
	constexpr std::uintptr_t ClientCleanNameCalls[] = { 0x140196051, 0x140196099, 0x140196993, 0x1401969DB };

	constexpr std::uintptr_t CG_DrawOverheadNames_ClientNameCall = 0x1400D1CBA;
	constexpr std::uintptr_t CL_GetClientName = 0x140101D80;

	constexpr std::uintptr_t I_CleanStr = 0x14028BFC0;
	static const std::uint8_t cleanStrEntry[] = { 0x0F, 0xB6, 0x01, 0x4C, 0x8B, 0xD1 };

	constexpr std::uintptr_t SV_UserinfoChanged_NameCall = 0x1402398E2;
	constexpr std::uintptr_t I_strncpyz = 0x14028C390;

	static Utils::Hook hooks[std::size(ClientCleanNameCalls) + 3];

	void PlayerName::UserInfoCopy(char* buffer, const char* name, const int size)
	{
		if (!sv_allowColoredNames.Get<bool>())
		{
			char nameBuffer[64]{};
			TextRenderer::StripColors(name, nameBuffer, sizeof(nameBuffer));
			TextRenderer::StripAllTextIcons(nameBuffer, buffer, size);
		}
		else
		{
			TextRenderer::StripAllTextIcons(name, buffer, size);
		}

		std::string readablePlayerName(buffer);
		Utils::String::Trim(readablePlayerName);

		if (readablePlayerName.size() < 3)
		{
			strncpy(buffer, "Unknown Soldier", size);
		}
	}

	void PlayerName::ClientCleanName_Hk(const char* name, char* buffer, const int size)
	{
		UserInfoCopy(buffer, name, size);
	}

	int PlayerName::GetClientName(int localClientNum, int index, char* buf, int size)
	{
		const auto result = Game::CL_GetClientName(localClientNum, index, buf, size);

		strncpy_s(buf, size, TextRenderer::StripColors(ClanTags::GetClanTagWithName(index, buf)).data(), size);

		return result;
	}

	char* PlayerName::CleanStrStub(char* string)
	{
		TextRenderer::StripColors(string, string, std::strlen(string) + 1);
		return string;
	}

	bool PlayerName::IsBadChar(int c)
	{
		if (c == '%')
		{
			return true;
		}

		if (c == '~')
		{
			return true;
		}

		if (c < 32 || c > 126)
		{
			return true;
		}

		return false;
	}

	bool PlayerName::CopyClientNameCheck(char* dest, const char* source, int size)
	{
		Game::I_strncpyz(dest, source, size);

		auto i = 0;

		while (i < size - 1 && dest[i] != '\0')
		{
			const auto c = static_cast<unsigned char>(dest[i]);

			if (IsBadChar(c))
			{
				return false;
			}

			++i;
		}

		return true;
	}

	void PlayerName::DropClient(Game::client_s* drop)
	{
		const auto* reason = "Invalid name detected";
		Network::SendCommand(drop->header.netchan.remoteAddress, "error", reason);
		Game::SV_DropClient(drop, reason, false);
	}

	void PlayerName::SV_UserinfoChanged_Hk(char* dest, const char* source, int size)
	{
		if (!CopyClientNameCheck(dest, source, size))
		{
			DropClient(reinterpret_cast<Game::client_s*>(dest - offsetof(Game::client_s, name)));
		}
	}

	PlayerName::PlayerName()
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
			{ ClientCleanNameCalls[0], ClientCleanName, reinterpret_cast<void*>(ClientCleanName_Hk), HOOK_CALL },
			{ ClientCleanNameCalls[1], ClientCleanName, reinterpret_cast<void*>(ClientCleanName_Hk), HOOK_CALL },
			{ ClientCleanNameCalls[2], ClientCleanName, reinterpret_cast<void*>(ClientCleanName_Hk), HOOK_CALL },
			{ ClientCleanNameCalls[3], ClientCleanName, reinterpret_cast<void*>(ClientCleanName_Hk), HOOK_CALL },
			{ CG_DrawOverheadNames_ClientNameCall, CL_GetClientName, reinterpret_cast<void*>(GetClientName), HOOK_CALL },
			{ SV_UserinfoChanged_NameCall, I_strncpyz, reinterpret_cast<void*>(SV_UserinfoChanged_Hk), HOOK_CALL },
		};

		static_assert(std::size(sites) + 1 == std::size(hooks));

		Events::OnDvarInit([]
		{
			sv_allowColoredNames = Dvar::Register("sv_allowColoredNames", true, Game::DVAR_NONE, "Allow colored names on the server");
		});

		for (const auto& hookSite : sites)
		{
			if (!Utils::Hook::BranchesTo(hookSite.site, hookSite.target, hookSite.isJump))
			{
				Logger::Error("playername: 0x{:X} no longer reaches 0x{:X}, names are the engine's\n", hookSite.site, hookSite.target);
				return;
			}
		}

		if (!Utils::Hook::MatchesBytes(SV_UpdateUserinfo_f, updateUserinfoEntry, sizeof(updateUserinfoEntry))
			|| !Utils::Hook::MatchesBytes(I_CleanStr, cleanStrEntry, sizeof(cleanStrEntry)))
		{
			Logger::Error("playername: SV_UpdateUserinfo_f or I_CleanStr does not read as expected, names are the engine's\n");
			return;
		}

		bool isSeated = true;

		for (std::size_t i = 0; i < std::size(sites); ++i)
		{
			isSeated = hooks[i].Initialize(sites[i].site, sites[i].stub, sites[i].isJump)->Install()->IsInstalled() && isSeated;
		}

		isSeated = hooks[std::size(sites)].Initialize(I_CleanStr, reinterpret_cast<void*>(CleanStrStub), HOOK_JUMP)->Install()->IsInstalled() && isSeated;

		if (!isSeated)
		{
			for (auto& hook : hooks)
			{
				hook.Uninstall();
			}

			Logger::Error("playername: could not seat every hook, names are the engine's\n");
			return;
		}

		for (auto& hook : hooks)
		{
			hook.Quick();
		}

		Utils::Hook::Set<std::uint8_t>(SV_UpdateUserinfo_f, 0xC3);
	}
}
