discordsocialsdk = {
	source = path.join(dependencies.basePath, "discord-social-sdk"),
	include = path.join(dependencies.basePath, "discord-social-sdk/include"),
	lib = path.join(dependencies.basePath, "discord-social-sdk/lib/release"),
	bin = path.join(dependencies.basePath, "discord-social-sdk/bin/release"),
}

function discordsocialsdk.import()
	if os.isdir(discordsocialsdk.source) then
		links { "discord_partner_sdk" }
		defines { "HAS_DISCORD_SOCIAL_SDK=1" }
		libdirs { discordsocialsdk.lib }
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
end

table.insert(dependencies, discordsocialsdk)
