#include "STDInclude.hpp"

#include "Dvar.hpp"
#include "Dedicated.hpp"
#include "Flags.hpp"
#include "Friends.hpp"
#include "Logger.hpp"
#include "Scheduler.hpp"
#include "TextRenderer.hpp"

#include "Steam/Proxy.hpp"

namespace Components
{
	Dvar::Var Dvar::Name;

	constexpr std::uintptr_t CL_InitOnceForAllClients_NameRegisterCall = 0x1400FB605;

	static const std::uint8_t nameRegisterCall[] = { 0xE8, 0x16, 0xAF, 0x18, 0x00 };

	constexpr int dvarSourceInternal = 0;
	constexpr int dvarSourceExternal = 1;
	constexpr unsigned int dvarRom = 0x2000;
	constexpr unsigned int dvarInit = 0x800;

	constexpr std::uintptr_t Dvar_SetFromStringByNameFromSource = 0x140287560;

	struct FlagPatch
	{
		std::uintptr_t instruction;
		std::uint8_t bytes[8];
		std::size_t length;
		std::size_t flagsOffset;
		std::size_t flagsSize;
		std::uint32_t flipped;
	};

	static const FlagPatch flagPatches[] =
	{
		{ 0x14008B3FD, { 0xC7, 0x44, 0x24, 0x20, 0x04, 0x00, 0x00, 0x00 }, 8, 4, 4, Game::DVAR_CHEAT | Game::DVAR_ARCHIVE },
		{ 0x1400D7653, { 0xC7, 0x44, 0x24, 0x20, 0x44, 0x00, 0x00, 0x00 }, 8, 4, 4, Game::DVAR_CHEAT | Game::DVAR_ARCHIVE },
		{ 0x1400D769C, { 0xC7, 0x44, 0x24, 0x20, 0x04, 0x00, 0x00, 0x00 }, 8, 4, 4, Game::DVAR_CHEAT | Game::DVAR_ARCHIVE },
		{ 0x1400D76D3, { 0xC7, 0x44, 0x24, 0x20, 0x04, 0x00, 0x00, 0x00 }, 8, 4, 4, Game::DVAR_CHEAT | Game::DVAR_ARCHIVE },
		{ 0x1400D756A, { 0x41, 0xB8, 0x04, 0x00, 0x00, 0x00 }, 6, 2, 4, Game::DVAR_CHEAT },
		{ 0x1400D7720, { 0x41, 0xB8, 0x04, 0x00, 0x00, 0x00 }, 6, 2, 4, Game::DVAR_CHEAT },
		{ 0x1400D9304, { 0xC7, 0x44, 0x24, 0x20, 0x04, 0x00, 0x00, 0x00 }, 8, 4, 4, Game::DVAR_CHEAT },
		{ 0x1400D9339, { 0xC7, 0x44, 0x24, 0x20, 0x04, 0x00, 0x00, 0x00 }, 8, 4, 4, Game::DVAR_CHEAT },
		{ 0x1400D936E, { 0xC7, 0x44, 0x24, 0x20, 0x04, 0x00, 0x00, 0x00 }, 8, 4, 4, Game::DVAR_CHEAT },
		{ 0x1400D9392, { 0xC7, 0x44, 0x24, 0x20, 0x04, 0x00, 0x00, 0x00 }, 8, 4, 4, Game::DVAR_CHEAT },
		{ 0x1400D93D2, { 0xC7, 0x44, 0x24, 0x20, 0x04, 0x00, 0x00, 0x00 }, 8, 4, 4, Game::DVAR_CHEAT },
		{ 0x1400D93FF, { 0xC7, 0x44, 0x24, 0x28, 0x04, 0x00, 0x00, 0x00 }, 8, 4, 4, Game::DVAR_CHEAT },
		{ 0x1400D836C, { 0xC7, 0x44, 0x24, 0x28, 0x01, 0x00, 0x00, 0x00 }, 8, 4, 4, Game::DVAR_ARCHIVE },
		{ 0x14023A444, { 0x41, 0xB8, 0x01, 0x04, 0x00, 0x00 }, 6, 2, 4, Game::DVAR_ARCHIVE },
		{ 0x140277E90, { 0x41, 0xB8, 0x08, 0x0C, 0x00, 0x00 }, 6, 2, 4, dvarInit },
		{ 0x1402715B7, { 0x44, 0x8D, 0x43, 0x04 }, 4, 3, 1, Game::DVAR_CHEAT },
		{ 0x140271838, { 0x44, 0x8D, 0x43, 0x04 }, 4, 3, 1, Game::DVAR_CHEAT },
		{ 0x14008BDAB, { 0xC7, 0x44, 0x24, 0x20, 0x8C, 0x00, 0x00, 0x00 }, 8, 4, 4, Game::DVAR_CHEAT },
		{ 0x14008BDDB, { 0xC7, 0x44, 0x24, 0x20, 0x8C, 0x00, 0x00, 0x00 }, 8, 4, 4, Game::DVAR_CHEAT },
		{ 0x14008BE0D, { 0xC7, 0x44, 0x24, 0x20, 0x8C, 0x00, 0x00, 0x00 }, 8, 4, 4, Game::DVAR_CHEAT },
		{ 0x14008BE79, { 0xC7, 0x44, 0x24, 0x20, 0x8C, 0x00, 0x00, 0x00 }, 8, 4, 4, Game::DVAR_CHEAT },
		{ 0x1400F2C87, { 0xC7, 0x44, 0x24, 0x20, 0x00, 0x20, 0x00, 0x00 }, 8, 4, 4, dvarRom | Game::DVAR_ARCHIVE },
		{ 0x1400F2CB7, { 0xC7, 0x44, 0x24, 0x20, 0x00, 0x20, 0x00, 0x00 }, 8, 4, 4, dvarRom | Game::DVAR_ARCHIVE },
		{ 0x1400F2C22, { 0xC7, 0x44, 0x24, 0x20, 0x04, 0x00, 0x00, 0x00 }, 8, 4, 4, Game::DVAR_CHEAT | Game::DVAR_ARCHIVE },
		{ 0x1400F2C57, { 0xC7, 0x44, 0x24, 0x20, 0x04, 0x00, 0x00, 0x00 }, 8, 4, 4, Game::DVAR_CHEAT | Game::DVAR_ARCHIVE },
	};

	struct RegisterCall
	{
		std::uintptr_t call;
		std::uint8_t bytes[5];
		void* replacement;
	};

	constexpr std::uintptr_t UI_Dvar_SetFromStringByNameCalls[] =
	{
		0x14025F45A,
		0x14025F5B5,
		0x140263555,
		0x14026373D,
		0x140263ADE,
		0x140269374,
	};

	static const std::uint8_t uiSetCallBytes[][5] =
	{
		{ 0xE8, 0xA1, 0x80, 0x02, 0x00 },
		{ 0xE8, 0x46, 0x7F, 0x02, 0x00 },
		{ 0xE8, 0xA6, 0x3F, 0x02, 0x00 },
		{ 0xE8, 0xBE, 0x3D, 0x02, 0x00 },
		{ 0xE8, 0x1D, 0x3A, 0x02, 0x00 },
		{ 0xE8, 0x87, 0xE1, 0x01, 0x00 },
	};

	constexpr std::uintptr_t Script_SetDvar_Dvar_SetFromStringByNameCall = 0x14025DABA;

	static const std::uint8_t scriptSetCall[] = { 0xE8, 0x41, 0x9A, 0x02, 0x00 };

	constexpr std::uintptr_t CG_ServerCommand_Dvar_SetFromStringByNameCall = 0x1400E6EFC;

	static const std::uint8_t serverSetCall[] = { 0xE8, 0xFF, 0x05, 0x1A, 0x00 };

	constexpr std::uintptr_t CG_ServerCommand_DvarTableMiss = 0x1400E6EF2;

	static const std::uint8_t dvarTableMissJump[] = { 0xEB, 0x0D };

	constexpr std::uintptr_t Dvar_EnumToString = 0x140284F90;

	static const std::uint8_t enumToStringEntry[] = { 0x83, 0x79, 0x40, 0x00, 0x48, 0x8B, 0xC1 };

	static Utils::Hook nameHook;
	static Utils::Hook registerHooks[5];
	static Utils::Hook uiSetHooks[std::size(UI_Dvar_SetFromStringByNameCalls)];
	static Utils::Hook scriptSetHook;
	static Utils::Hook serverSetHook;
	static Utils::Hook enumToStringHook;

	Dvar::Var::Var(const std::string& name)
		: dvar(reinterpret_cast<Game::dvar_t*>(Game::Dvar_FindVar(name.data())))
	{
	}

	template <> bool Dvar::Var::Get() const
	{
		if (!this->dvar)
		{
			return false;
		}

		return this->dvar->current.enabled;
	}

	template <> int Dvar::Var::Get() const
	{
		if (!this->dvar)
		{
			return 0;
		}

		return this->dvar->current.integer;
	}

	template <> unsigned int Dvar::Var::Get() const
	{
		if (!this->dvar)
		{
			return 0;
		}

		return this->dvar->current.unsignedInt;
	}

	template <> float Dvar::Var::Get() const
	{
		if (!this->dvar)
		{
			return 0.0f;
		}

		return this->dvar->current.value;
	}

	template <> const char* Dvar::Var::Get() const
	{
		if (!this->dvar || this->dvar->type != Game::DVAR_TYPE_STRING)
		{
			return "";
		}

		const char* const value = this->dvar->current.string;

		return value ? value : "";
	}

	template <> std::string Dvar::Var::Get() const
	{
		return this->Get<const char*>();
	}

	void Dvar::Var::Set(bool value) const
	{
		if (this->dvar)
		{
			Game::Dvar_SetBool(this->dvar, value);
		}
	}

	void Dvar::Var::Set(int value) const
	{
		if (this->dvar)
		{
			Game::Dvar_SetInt(this->dvar, value);
		}
	}

	void Dvar::Var::Set(float value) const
	{
		if (this->dvar)
		{
			Game::Dvar_SetFloat(this->dvar, value);
		}
	}

	void Dvar::Var::Set(const char* value) const
	{
		if (this->dvar)
		{
			Game::Dvar_SetString(this->dvar, value);
		}
	}

	void Dvar::Var::Set(const std::string& value) const
	{
		this->Set(value.data());
	}

	Dvar::Var Dvar::Register(const char* name, bool value, unsigned int flags, const char* description)
	{
		return Var(Game::Dvar_RegisterBool(name, value, flags, description));
	}

	Dvar::Var Dvar::Register(const char* name, int value, int min, int max, unsigned int flags, const char* description)
	{
		return Var(Game::Dvar_RegisterInt(name, value, min, max, flags, description));
	}

	Dvar::Var Dvar::Register(const char* name, float value, float min, float max, unsigned int flags, const char* description)
	{
		return Var(Game::Dvar_RegisterFloat(name, value, min, max, flags, description));
	}

	Dvar::Var Dvar::Register(const char* name, const char* value, unsigned int flags, const char* description)
	{
		return Var(Game::Dvar_RegisterString(name, value, flags, description));
	}

	Dvar::Var Dvar::Find(const std::string& name)
	{
		return Var(name);
	}

	Game::dvar_t* Dvar::Dvar_RegisterName(const char* dvarName, [[maybe_unused]] const char* value, unsigned int flags, const char* description)
	{
		if (!Dedicated::IsEnabled())
		{
			Scheduler::Loop([]
			{
				static std::string lastValidName = "Unknown Soldier";
				auto name = Name.Get<std::string>();

				if (name == lastValidName)
				{
					return;
				}

				Utils::String::Trim(name);
				const auto saneName = TextRenderer::StripAllTextIcons(TextRenderer::StripColors(name));

				if (saneName.size() < 3 || (saneName[0] == '[' && saneName[1] == '{'))
				{
					Logger::Error("Username '{}' is invalid. It must at least be 3 characters long and not appear empty!\n", name);
					Name.Set(lastValidName);
				}
				else
				{
					lastValidName = name;
					Friends::UpdateName();
				}
			}, Scheduler::Pipeline::CLIENT, 3s);
		}

		std::string username = "Unknown Soldier";

		if (::Steam::Proxy::SteamFriends)
		{
			const char* const steamName = ::Steam::Proxy::SteamFriends->GetPersonaName();

			if (steamName && *steamName)
			{
				username = steamName;
			}
		}

		Name = Register(dvarName, username.data(), flags | Game::DVAR_ARCHIVE, description);
		return Name.Get();
	}

	Game::dvar_t* Dvar::Dvar_RegisterSVNetworkFps(const char* dvarName, int value, int min, [[maybe_unused]] int max, [[maybe_unused]] unsigned int flags, const char* description)
	{
		constexpr int networkFpsMax = 1000;

		int defaultValue = value;

		if (Dedicated::IsEnabled())
		{
			defaultValue = networkFpsMax;
		}

		return Game::Dvar_RegisterInt(dvarName, defaultValue, min, networkFpsMax, Game::DVAR_NONE, description);
	}

	Game::dvar_t* Dvar::Dvar_Register_cg_drawFPS(const char* dvarName, const char** valueList, int defaultIndex, unsigned int flags, const char* description)
	{
		return Game::Dvar_RegisterEnum(dvarName, valueList, defaultIndex, flags | Game::DVAR_ARCHIVE, description);
	}

	Game::dvar_t* Dvar::Dvar_Register_cg_fov(const char* dvarName, float value, float min, [[maybe_unused]] float max, unsigned int flags, const char* description)
	{
		return Game::Dvar_RegisterFloat(dvarName, value, min, 160.0f, flags, description);
	}

	Game::dvar_t* Dvar::Dvar_Register_com_maxfps(const char* dvarName, int value, int min, [[maybe_unused]] int max, unsigned int flags, const char* description)
	{
		return Game::Dvar_RegisterInt(dvarName, value, min, 1000, flags | Game::DVAR_ARCHIVE, description);
	}

	Game::dvar_t* Dvar::Dvar_Register_profileMenuOption_volume(const char* dvarName, [[maybe_unused]] float value, float min, [[maybe_unused]] float max, unsigned int flags, const char* description)
	{
		return Game::Dvar_RegisterFloat(dvarName, 1.0f, min, 1.0f, flags, description);
	}

	void Dvar::SetFromStringByNameSafeExternal(const char* dvarName, const char* string)
	{
		static const char* const exceptions[] =
		{
			"ui_showEndOfGame",
			"systemlink",
			"splitscreen",
			"onlinegame",
			"party_maxplayers",
			"xblive_privateserver",
			"xblive_rankedmatch",
			"ui_mptype",
		};

		for (const auto* const entry : exceptions)
		{
			if (_stricmp(dvarName, entry) == 0)
			{
				reinterpret_cast<Game::dvar_t*(*)(const char*, const char*, int)>(Utils::Hook::Rebase(Dvar_SetFromStringByNameFromSource))(
					dvarName, string, dvarSourceInternal);
				return;
			}
		}

		SetFromStringByNameExternal(dvarName, string);
	}

	void Dvar::SetFromStringByNameExternal(const char* dvarName, const char* string)
	{
		reinterpret_cast<Game::dvar_t*(*)(const char*, const char*, int)>(Utils::Hook::Rebase(Dvar_SetFromStringByNameFromSource))(
			dvarName, string, dvarSourceExternal);
	}

	bool Dvar::AreArchiveDvarsUnprotected()
	{
		static const bool isUnprotected = Flags::HasFlag("unprotect-dvars");

		return isUnprotected;
	}

	bool Dvar::IsSettingDvarsDisabled()
	{
		static const bool isDisabled = Flags::HasFlag("protect-dvars");

		return isDisabled;
	}

	void Dvar::DvarSetFromStringByName_Stub(const char* dvarName, const char* value)
	{
		if (IsSettingDvarsDisabled())
		{
			Logger::Debug("not allowing the server to set {}\n", dvarName);
			return;
		}

		const auto* const dvar = Game::Dvar_FindVar(dvarName);

		if (dvar && (dvar->flags & Game::DVAR_ARCHIVE))
		{
			if (!AreArchiveDvarsUnprotected())
			{
				Logger::Print("not allowing the server to override saved dvar {}\n", dvar->name);
				return;
			}

			Logger::Print("the server is overriding saved dvar {}\n", dvarName);
		}

		if (dvar && std::strcmp(dvar->name, "com_errorResolveCommand") == 0)
		{
			Logger::Print("not allowing the server to set {}\n", dvar->name);
			return;
		}

		reinterpret_cast<void(*)(const char*, const char*)>(serverSetHook.GetOriginal())(dvarName, value);
	}

	const char* Dvar::Dvar_EnumToString_Stub(const Game::dvar_t* dvar)
	{
		if (!dvar || dvar->domain.enumeration.stringCount == 0)
		{
			return "";
		}

		return dvar->domain.enumeration.strings[dvar->current.integer];
	}

	Dvar::Dvar()
	{
		const RegisterCall registerCalls[] =
		{
			{ 0x1400D77E3, { 0xE8, 0x68, 0xE7, 0x1A, 0x00 }, reinterpret_cast<void*>(Dvar_Register_cg_drawFPS) },
			{ 0x1400D765B, { 0xE8, 0xF0, 0xE9, 0x1A, 0x00 }, reinterpret_cast<void*>(Dvar_Register_cg_fov) },
			{ 0x14023A853, { 0xE8, 0x28, 0xB9, 0x04, 0x00 }, reinterpret_cast<void*>(Dvar_RegisterSVNetworkFps) },
			{ 0x1400F112D, { 0xE8, 0x1E, 0x4F, 0x19, 0x00 }, reinterpret_cast<void*>(Dvar_Register_profileMenuOption_volume) },
			{ 0x1401F4CA3, { 0xE8, 0xD8, 0x14, 0x09, 0x00 }, reinterpret_cast<void*>(Dvar_Register_com_maxfps) },
		};

		static_assert(std::extent_v<decltype(registerCalls)> == std::extent_v<decltype(registerHooks)>);

		bool isExpected = Utils::Hook::MatchesBytes(CL_InitOnceForAllClients_NameRegisterCall, nameRegisterCall, sizeof(nameRegisterCall))
			&& Utils::Hook::MatchesBytes(Script_SetDvar_Dvar_SetFromStringByNameCall, scriptSetCall, sizeof(scriptSetCall))
			&& Utils::Hook::MatchesBytes(CG_ServerCommand_Dvar_SetFromStringByNameCall, serverSetCall, sizeof(serverSetCall))
			&& Utils::Hook::MatchesBytes(CG_ServerCommand_DvarTableMiss, dvarTableMissJump, sizeof(dvarTableMissJump))
			&& Utils::Hook::MatchesBytes(Dvar_EnumToString, enumToStringEntry, sizeof(enumToStringEntry));

		for (const auto& site : registerCalls)
		{
			isExpected = isExpected && Utils::Hook::MatchesBytes(site.call, site.bytes, sizeof(site.bytes));
		}

		for (std::size_t i = 0; i < std::size(UI_Dvar_SetFromStringByNameCalls); ++i)
		{
			isExpected = isExpected && Utils::Hook::MatchesBytes(UI_Dvar_SetFromStringByNameCalls[i], uiSetCallBytes[i], sizeof(uiSetCallBytes[i]));
		}

		for (const auto& patch : flagPatches)
		{
			isExpected = isExpected && Utils::Hook::MatchesBytes(patch.instruction, patch.bytes, patch.length);
		}

		if (!isExpected)
		{
			Logger::Error("dvar: the dvar registrations do not read as expected, the dvars keep the engine's defaults\n");
			return;
		}

		bool isSeated = nameHook.Initialize(CL_InitOnceForAllClients_NameRegisterCall, reinterpret_cast<void*>(Dvar_RegisterName), HOOK_CALL)->Install()->IsInstalled();
		isSeated = scriptSetHook.Initialize(Script_SetDvar_Dvar_SetFromStringByNameCall, reinterpret_cast<void*>(SetFromStringByNameSafeExternal), HOOK_CALL)->Install()->IsInstalled() && isSeated;
		isSeated = serverSetHook.Initialize(CG_ServerCommand_Dvar_SetFromStringByNameCall, reinterpret_cast<void*>(DvarSetFromStringByName_Stub), HOOK_CALL)->Install()->IsInstalled() && isSeated;
		isSeated = enumToStringHook.Initialize(Dvar_EnumToString, reinterpret_cast<void*>(Dvar_EnumToString_Stub), HOOK_JUMP)->Install()->IsInstalled() && isSeated;

		for (std::size_t i = 0; i < std::size(registerCalls); ++i)
		{
			isSeated = registerHooks[i].Initialize(registerCalls[i].call, registerCalls[i].replacement, HOOK_CALL)->Install()->IsInstalled() && isSeated;
		}

		for (std::size_t i = 0; i < std::size(UI_Dvar_SetFromStringByNameCalls); ++i)
		{
			isSeated = uiSetHooks[i].Initialize(UI_Dvar_SetFromStringByNameCalls[i], reinterpret_cast<void*>(SetFromStringByNameExternal), HOOK_CALL)
				->Install()->IsInstalled() && isSeated;
		}

		if (!isSeated)
		{
			nameHook.Uninstall();
			scriptSetHook.Uninstall();
			serverSetHook.Uninstall();
			enumToStringHook.Uninstall();

			for (auto& hook : registerHooks)
			{
				hook.Uninstall();
			}

			for (auto& hook : uiSetHooks)
			{
				hook.Uninstall();
			}

			Logger::Error("dvar: could not seat the dvar hooks, the dvars keep the engine's defaults\n");
			return;
		}

		nameHook.Quick();
		scriptSetHook.Quick();
		serverSetHook.Quick();
		enumToStringHook.Quick();

		Utils::Hook::Nop(CG_ServerCommand_DvarTableMiss, sizeof(dvarTableMissJump));

		for (auto& hook : registerHooks)
		{
			hook.Quick();
		}

		for (auto& hook : uiSetHooks)
		{
			hook.Quick();
		}

		for (const auto& patch : flagPatches)
		{
			const auto flags = patch.instruction + patch.flagsOffset;

			if (patch.flagsSize == sizeof(std::uint32_t))
			{
				Utils::Hook::Xor<std::uint32_t>(flags, patch.flipped);
			}
			else
			{
				Utils::Hook::Xor<std::uint8_t>(flags, static_cast<std::uint8_t>(patch.flipped));
			}
		}
	}
}
