# Zombie Warfare 3 Client

A client running Zombie Warfare 3 based on the IW4x client.

## Building on Windows

The client DLL targets x64. Use Visual Studio with the v143 C++ build tools and a Windows SDK.

Run `generate.bat`, then open `zw3.sln` or `build/zw3.sln` and select `Debug | x64` or `Release | x64`.
The generator also updates the legacy `build/iw4x.sln` entry point to use the x64 projects.
If Visual Studio was open during the upgrade from Win32, close and reopen the solution to reload its projects.
Use `generate.bat` to prepare the solution; the old Premake configuration still targets Win32.

For a command-line build, run `powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build.ps1 -Configuration Release`.
The DLL is written to `build/bin/x64/Release/zw3.dll`.

At startup, the client loads `zw3/zw3.ff` followed by `zw3/zw3_assets.ff` with the same zone flags and reader checks.
Both fastfiles are required for a normal launch and are skipped when `-dev` is enabled.
