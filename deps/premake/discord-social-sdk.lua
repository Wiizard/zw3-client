discordsocialsdk = {
	source = path.join(dependencies.basePath, "discord-social-sdk"),
	include = path.join(dependencies.basePath, "discord-social-sdk/include"),
	bin = path.join(dependencies.basePath, "discord-social-sdk/bin/release"),
}

function discordsocialsdk.import()
	if os.isdir(discordsocialsdk.source) then
		defines { "HAS_DISCORD_SOCIAL_SDK=1" }
		discordsocialsdk.includes()

		filter "system:windows"
			postbuildcommands {
				"{COPY} \"" .. path.join(discordsocialsdk.bin, "discord_partner_sdk.dll") .. "\" \"%{cfg.targetdir}\""
			}
		filter {}
	else
		defines { "HAS_DISCORD_SOCIAL_SDK=0" }
	end
end

function discordsocialsdk.includes()
	includedirs {
		discordsocialsdk.include,
	}
end

function discordsocialsdk.project()
	-- The official Social SDK is prebuilt and distributed by Discord.
	-- Place the extracted SDK at deps/discord-social-sdk.
	-- We intentionally do not hard-link against discord_partner_sdk.lib so the game can still boot
	-- if the SDK runtime DLL is absent; discordpp implementation handles runtime loading.
end

table.insert(dependencies, discordsocialsdk)
