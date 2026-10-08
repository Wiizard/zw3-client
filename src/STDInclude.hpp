#pragma once

#ifndef RC_INVOKED

#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _USE_MATH_DEFINES

#include <windows.h>
#include <winsock2.h>
#include <shlobj.h>
#include <timeapi.h>
#include <shellapi.h>
#include <wininet.h>
#include <d3d9.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <shlwapi.h>
#include <dbghelp.h>
#include <iphlpapi.h>
#include <XInput.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cctype>
#include <chrono>
#include <cinttypes>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <queue>
#include <random>
#include <ranges>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#pragma warning(push)
#pragma warning(disable: 4100)
#pragma warning(disable: 26812)

using namespace std::literals;

#ifdef max
	#undef max
#endif

#ifdef min
	#undef min
#endif

#ifdef GetObject
	#undef GetObject
#endif

#define AssertSize(x, size) \
	static_assert(sizeof(x) == (size), \
		"Structure has an invalid size. " #x " must be " #size " bytes")

#define AssertOffset(x, y, offset) \
	static_assert(offsetof(x, y) == (offset), \
		#x "::" #y " is not at the right offset. Must be at " #offset)

#define AssertIn(x, y) assert(static_cast<unsigned int>(x) < static_cast<unsigned int>(y))

#define AssertUnreachable assert(0 && "unreachable")

#include <json.hpp>

#pragma warning(pop)

#include <tomcrypt.h>

#include "Utils/Memory.hpp"

#include "Utils/Cache.hpp"
#include "Utils/Chain.hpp"
#include "Utils/Concurrency.hpp"
#include "Utils/CrashMarker.hpp"
#include "Utils/Cryptography.hpp"
#include "Utils/CSV.hpp"
#include "Utils/Entities.hpp"
#include "Utils/Hooking.hpp"
#include "Utils/Huffman.hpp"
#include "Utils/Utils.hpp"
#include "Utils/InfoString.hpp"
#include "Utils/IO.hpp"
#include "Utils/Library.hpp"
#include "Utils/Maths.hpp"
#include "Utils/NamedMutex.hpp"
#include "Utils/String.hpp"
#include "Utils/Thread.hpp"
#include "Utils/Time.hpp"
#include "Utils/WebIO.hpp"

#include "Game/Structs.hpp"
#include "Game/StructsX86.hpp"
#include "Game/Game.hpp"

#include "Game/Scripting/Function.hpp"
#include "Game/Scripting/StackIsolation.hpp"

#include "Utils/Stream.hpp"

#include "Components/Loader.hpp"

#pragma comment(lib, "Winmm.lib")
#pragma comment(lib, "Crypt32.lib")
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "Wininet.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "Urlmon.lib")
#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "rpcrt4.lib")
#pragma comment(lib, "dbghelp.lib")
#pragma comment(lib, "ntdll.lib")
#pragma comment(lib, "iphlpapi.lib")

#pragma comment(lib, "xinput.lib")
#pragma comment(lib, "Setupapi.lib")
#pragma comment(lib, "Hid.lib")

#endif

#define BASEGAME "iw4x64"
#define CLIENT_CONFIG "iw4x_config.cfg"

#define GAME_IMAGEBASE 0x140000000

#define XFILE_MAGIC_UNSIGNED 0x3030317566665749
#define XFILE_VERSION 276

#define XFILE_HEADER_IW4X 0x78345749
#define XFILE_VERSION_IW4X 3

#define XFILE_HEADER_ZW3 0x4633575A
#define XFILE_VERSION_ZW3 1
