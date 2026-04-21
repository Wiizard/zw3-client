# Discord Social SDK setup (ZW3 client)

If you're asking “where do I get `discord_partner_sdk.dll`?”, the short answer is:

1. Go to the **Discord Developer Portal**: <https://discord.com/developers/applications>
2. Open your game application.
3. In the left sidebar, open the **Discord Social SDK** section.
4. Open **Downloads** and download the latest **C++ SDK** zip.

Discord’s official C++ getting-started docs also call this out and note that on Windows the runtime DLL must be next to your executable.

---

## Where to place files in this repo

Extract the SDK zip so you end up with this structure:

```text
deps/
  discord-social-sdk/
    include/
      discordpp.h
      ...
    bin/
      release/
        discord_partner_sdk.dll
    lib/               (may exist depending on SDK package version)
```

For this project, the important parts are:

- `deps/discord-social-sdk/include` (headers)
- `deps/discord-social-sdk/bin/release/discord_partner_sdk.dll` (runtime DLL)

The premake script automatically:

- Enables `HAS_DISCORD_SOCIAL_SDK=1` when `deps/discord-social-sdk` exists.
- Copies `discord_partner_sdk.dll` into your build output directory on Windows.

---

## Verify it is detected

After placing files:

1. Regenerate project files.
2. Build.
3. Check build logs for the post-build copy of `discord_partner_sdk.dll`.

You can also quickly verify files exist:

```bash
test -f deps/discord-social-sdk/include/discordpp.h && echo "headers ok"
test -f deps/discord-social-sdk/bin/release/discord_partner_sdk.dll && echo "dll ok"
```

---

## Visual Studio integration (external dependency)

You generally **should not manually add include/lib paths in VS** for this repo.
This project uses Premake, and dependency wiring is done in:

- `deps/premake/discord-social-sdk.lua`

### Recommended flow

1. Put SDK files in `deps/discord-social-sdk` (layout above).
2. Regenerate solution/project files (run the repo generate script).
3. Open the regenerated `.sln` in Visual Studio.
4. Build as usual.

Premake will then inject:

- the include path (`deps/discord-social-sdk/include`)
- `HAS_DISCORD_SOCIAL_SDK=1`
- post-build copy of `discord_partner_sdk.dll` to the target output directory

### If you still want to set it manually in VS (not recommended)

Project → Properties:

- **C/C++ → General → Additional Include Directories**
  - add `$(SolutionDir)..\\deps\\discord-social-sdk\\include`
- **Build Events → Post-Build Event**
  - copy `$(SolutionDir)..\\deps\\discord-social-sdk\\bin\\release\\discord_partner_sdk.dll` to `$(OutDir)`

Do this only as a temporary local override; it will be overwritten next time project files are regenerated.

---

## Common mistakes

- Putting the DLL in `System32` instead of next to the game binary.
- Extracting into `deps/discord-social-sdk/DiscordSocialSdk-<version>/...` (one folder too deep).
- Forgetting to regenerate premake project files after adding a new dependency directory.
