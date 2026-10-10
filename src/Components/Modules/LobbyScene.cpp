#include "STDInclude.hpp"

#include <bit>
#include <d3dcommon.h>
#include <DirectXMath.h>
#include <future>
#include <zlib.h>

#include "LobbyScene.hpp"
#include "LobbyTransition.hpp"
#include "LobbyCombat.hpp"
#include "Command.hpp"
#include "D3D9Ex.hpp"
#include "Dedicated.hpp"
#include "Dvar.hpp"
#include "Events.hpp"
#include "FastFiles.hpp"
#include "FileSystem.hpp"
#include "Logger.hpp"
#include "Materials.hpp"
#include "Network.hpp"
#include "Party.hpp"
#include "Renderer.hpp"
#include "Scheduler.hpp"
#include "Sound.hpp"
#include "ZoneBuilder.hpp"

namespace Components
{
	using D3DXCompileShader_t = HRESULT(WINAPI*)(const char* source, UINT sourceLength, const void* defines, void* include, const char* functionName, const char* profile, DWORD flags, ID3DBlob** shader, ID3DBlob** errors, void** constantTable);
	using D3DXCreateTextureFromFileInMemory_t = HRESULT(WINAPI*)(IDirect3DDevice9* device, const void* source, UINT sourceSize, IDirect3DTexture9** texture);

	constexpr std::uintptr_t cinematicGlobBink = 0x1493C9BA8;
	constexpr std::uintptr_t Com_ErrorEntered = 0x1401F3F40;

	static const std::uint8_t comErrorEnteredBytes[] = { 0x83, 0x3D, 0xD5, 0x5B, 0x9E, 0x01, 0x00, 0x0F, 0x9F, 0xC0, 0xC3 };

	constexpr char assetNameCharacters[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_~$#&-.";

	constexpr unsigned int writableImageFlags = 0x1000003;
	constexpr std::int64_t maxTexturePackSize = 256 * 1024 * 1024;

	constexpr unsigned int clipIdle = 0;
	constexpr unsigned int clipAction = 1;
	constexpr unsigned int clipDeath = 2;
	constexpr unsigned int clipWalk = 3;
	constexpr unsigned int clipRifleIdle = 4;
	constexpr unsigned int clipRifleFire = 5;
	constexpr unsigned int clipShotgunFire = 6;
	constexpr unsigned int clipPoint = 7;
	constexpr unsigned int clipMelee = 8;
	constexpr unsigned int clipAttack = 9;
	constexpr std::size_t clipCount = 10;

	constexpr std::size_t zombieActor = 4;
	constexpr UINT zombieEyeFirstVertex = 18456;

	struct LobbyVertex
	{
		float x;
		float y;
		float z;
		DWORD color;
		float u;
		float v;
	};

	struct LobbyRoomVertex
	{
		float x;
		float y;
		float z;
		DWORD color;
		float u;
		float v;
		float lightU;
		float lightV;
	};

	static_assert(sizeof(LobbyVertex) == 24);
	static_assert(sizeof(LobbyRoomVertex) == 32);

	struct LobbyDrawGroup
	{
		UINT firstVertex = 0;
		UINT vertexCount = 0;
		std::string textureName;
		bool isBlended = false;
		bool hasCell = false;
		int cellX = 0;
		int cellY = 0;
		IDirect3DTexture9* texture = nullptr;
		int lightmapIndex = -1;
		int weaponIndex = -1;
		UINT firstIndex = 0;
		UINT indexCount = 0;
		std::string normalTextureName;
		IDirect3DTexture9* normalTexture = nullptr;
		bool isAlphaTested = true;
		DWORD srcBlend = D3DBLEND_SRCALPHA;
		DWORD dstBlend = D3DBLEND_INVSRCALPHA;
		float alphaThreshold = 0.05f;
		bool isMultiplicative = false;
		bool shouldWriteDepth = true;
		bool shouldWriteGamma = false;
	};

	struct LobbyMuzzleAnchor
	{
		std::array<UINT, 3> vertices{};
		float u = 0.0f;
		float v = 0.0f;
		float w = 0.0f;
		bool isValid = false;
	};

	struct LobbyActorMesh
	{
		std::array<LobbyMuzzleAnchor, 4> muzzleAnchors{};
		std::vector<std::vector<LobbyVertex>> frames;
		std::vector<LobbyVertex> interpolated;
		std::vector<LobbyVertex> blended;
		std::vector<LobbyDrawGroup> groups;
		std::array<UINT, clipCount> clipFirst{};
		std::array<UINT, clipCount> clipLength = { 1, 1, 1, 1, 1, 1, 1, 0, 0, 0 };
		std::array<unsigned int, clipCount> clipDurationMs{};
	};

	struct LobbyTexturePack
	{
		bool isIndexed = false;
		std::string bytes;
		std::unordered_map<std::string, std::pair<std::size_t, std::size_t>> entries;
	};

	struct LobbyPathPoint
	{
		float x;
		float y;
		float z;
	NLOHMANN_DEFINE_TYPE_INTRUSIVE(LobbyPathPoint, x, y, z)
	};

	constexpr DWORD vertexFormat = D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1;
	constexpr DWORD roomVertexFormat = D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX2;

				struct ZombieVisual
	{
		DWORD spawnTime = 0;
		DWORD deathTime = 0;
		DWORD nextSpawnTime = 0;
		float routeProgress = 0.0f;
		bool isAdmitted = false;
		DWORD admittedTime = 0;
		int approachPhase = 0;
		float assignedHomeX = 0.0f;
		float walkDuration = 22000.0f;
		std::vector<LobbyVertex> deathPose;
		LobbyPathPoint location{};
		float facing = 0.0f;
		bool isChasing = false;
		bool isAttacking = false;
		DWORD attackTime = 0;
		float animationTime = 0.0f;
		DWORD nextAttackTime = 0;
		int targetSurvivor = -1;
		float fallDirection = 1.0f;
		int variant = 0;
		bool isDying = false;
		bool isVisible = false;
		NLOHMANN_DEFINE_TYPE_INTRUSIVE(ZombieVisual, spawnTime, deathTime, nextSpawnTime, routeProgress, isAdmitted, admittedTime, approachPhase, assignedHomeX, walkDuration, location, facing, isChasing, isAttacking, attackTime, animationTime, nextAttackTime, targetSurvivor, fallDirection, variant, isDying, isVisible)
	};
	struct SurvivorState
				{
					float currentX = 0.0f;
					float homeX = 0.0f;
					int targetZombieIndex = -1;
					DWORD targetEngageTime = 0;
					DWORD lastFireTime = 0;
					DWORD meleeTime = 0;
					DWORD recoveryUntil = 0;
					float meleeFacing = 0.0f;
					int meleeTarget = -1;
					bool isMeleePending = false;
					int burstRemaining = 0;
					DWORD nextBurstShotTime = 0;
					float facing = -DirectX::XM_PI * 0.5f;
					bool isWalking = false;
					bool isFiring = false;
					bool isInitialized = false;
					bool isPresent = false;
					int modelIndex = -1;
					int weaponIndex = 0;
					unsigned int session = 0;
					NLOHMANN_DEFINE_TYPE_INTRUSIVE(SurvivorState, currentX, homeX, targetZombieIndex, targetEngageTime, lastFireTime, meleeTime, recoveryUntil, meleeFacing, meleeTarget, isMeleePending, burstRemaining, nextBurstShotTime, facing, isWalking, isFiring, isInitialized, isPresent, modelIndex, weaponIndex, session)
	};
				struct TeleportBurst
				{
					float x = 0.0f;
					DWORD started = 0;
					bool isActive = false;
					NLOHMANN_DEFINE_TYPE_INTRUSIVE(TeleportBurst, x, started, isActive)
	};

	struct SceneSnapshot
	{
		DWORD now = 0;
		DWORD phaseOrigin = 0;
		DWORD transitionElapsed = 0;
		bool transitioning = false;
		LobbyPathPoint baseEye{};
		LobbyPathPoint baseTarget{};
		std::array<ZombieVisual, 32> zombies;
		std::array<SurvivorState, 4> survivors;
		std::array<TeleportBurst, 4> teleports;
		NLOHMANN_DEFINE_TYPE_INTRUSIVE(SceneSnapshot, now, phaseOrigin, transitionElapsed, transitioning, baseEye, baseTarget, zombies, survivors, teleports)
	};
	static std::mutex sceneSyncMutex;
	static std::shared_ptr<const SceneSnapshot> receivedScene;
	static std::shared_ptr<const SceneSnapshot> previousScene;
	static std::atomic_uint64_t remoteSceneGeneration = 0;
	static std::atomic_bool hasSceneClock = false;
	static std::atomic_int sceneClockOffset = 0;
	static std::atomic_uint bestSceneRoundTrip = 1001;
	static DWORD receivedSceneAt = 0;
	static std::string publishedScene;
	static bool isDirectScene = false;
	struct LocalSceneBuffer
	{
		volatile LONG sequence = 0;
		DWORD size = 0;
		char bytes[8192]{};
	};
	struct LocalSceneMapping
	{
		HANDLE handle = nullptr;
		LocalSceneBuffer* view = nullptr;
		unsigned short port = 0;
		~LocalSceneMapping() { Close(); }
		void Close()
		{
			if (view) UnmapViewOfFile(view);
			if (handle) CloseHandle(handle);
			view = nullptr;
			handle = nullptr;
			port = 0;
		}
		bool Open(const unsigned short targetPort, const bool writer)
		{
			if (view && port == targetPort) return true;
			Close();
			const auto name = std::format(L"Local\\ZW3.LobbyScene.{}", targetPort);
			handle = writer ? CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(LocalSceneBuffer), name.c_str())
				: OpenFileMappingW(FILE_MAP_READ, FALSE, name.c_str());
			if (!handle) return false;
			view = static_cast<LocalSceneBuffer*>(MapViewOfFile(handle, writer ? FILE_MAP_ALL_ACCESS : FILE_MAP_READ, 0, 0, sizeof(LocalSceneBuffer)));
			if (!view) { Close(); return false; }
			port = targetPort;
			return true;
		}
	};
	static LocalSceneMapping localSceneWriter;
	static LocalSceneMapping localSceneReader;

	static const std::vector<LobbyPathPoint> leftZombiePath =
	{
		{ -900.0f, -1191.0f, 80.0f }, { -493.0f, -1191.0f, 80.0f },
		{ -340.0f, -1158.0f, 80.0f }, { -340.0f, -937.0f, 160.0f },
		{ -340.0f, -750.0f, 248.0f }, { -340.0f, -700.0f, 248.0f },
		{ -120.0f, -700.0f, 248.0f },
	};

	static const std::vector<LobbyPathPoint> rightZombiePath =
	{
		{ 900.0f, -1253.0f, 80.0f }, { 463.0f, -1253.0f, 80.0f },
		{ 327.0f, -1243.0f, 80.0f }, { 260.0f, -1198.0f, 80.0f },
		{ 260.0f, -991.0f, 135.0f }, { 260.0f, -750.0f, 248.0f },
		{ 260.0f, -700.0f, 248.0f }, { 120.0f, -700.0f, 248.0f },
	};

	static unsigned int textureWidth = 1920;
	static unsigned int textureHeight = 1080;

	static std::vector<LobbyRoomVertex> roomVertices;
	static std::vector<LobbyDrawGroup> roomGroups;
	static std::vector<LobbyRoomVertex> doorLeftVertices;
	static std::vector<LobbyRoomVertex> doorRightVertices;
	static std::vector<LobbyDrawGroup> doorLeftGroups;
	static std::vector<LobbyDrawGroup> doorRightGroups;
	static std::vector<LobbyVertex> propVertices;
	static std::vector<std::uint32_t> propIndices;
	static std::vector<LobbyDrawGroup> propGroups;
	static std::array<LobbyActorMesh, 5> actorMeshes;
	static std::unordered_map<std::string, IDirect3DTexture9*> loadedTextures;
	static std::vector<std::string> pendingTextureNames;
	static std::size_t nextTexture = 0;
	static bool hasRenderResources = false;

	static IDirect3DPixelShader9* filmShader = nullptr;
	static IDirect3DPixelShader9* lightmapFilmShader = nullptr;
	static IDirect3DPixelShader9* visionShader = nullptr;
	static IDirect3DTexture9* sceneTexture = nullptr;
	static std::array<IDirect3DTexture9*, 3> roomLightmaps{};
	static IDirect3DVertexBuffer9* roomVertexBuffer = nullptr;
	static IDirect3DVertexBuffer9* propVertexBuffer = nullptr;
	static IDirect3DIndexBuffer9* propIndexBuffer = nullptr;

	static float filmContrast[4] = { 1.08f, 0.0f, 0.0f, 0.0f };
	static float filmLightTint[4] = { 1.15f, 1.10f, 1.00f, 0.0f };
	static float filmMediumTint[4] = { 1.02f, 1.00f, 0.97f, 0.0f };
	static float filmDarkTint[4] = { 1.00f, 0.98f, 0.95f, 0.0f };
	static const float filmExposure[4] = { 1.0f, 0.0f, 0.0f, 0.0f };
	static float filmBrightness[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	static float glowSettings[4] = { 0.76f, 0.32f, 0.0f, 0.0f };
	static DirectX::XMFLOAT3 cameraPan = { 4.0f, 3.0f, 0.0f };

	static Game::GfxImage* roomImage = nullptr;
	static Game::Material* roomMaterial = nullptr;
	static IDirect3DSurface9* roomDepth = nullptr;
	static IDirect3DStateBlock9* savedState = nullptr;

	static std::atomic_bool isLobbyVisible = false;
	static std::atomic_bool hasZombieWaveStarted = false;
	static std::atomic_bool shouldUseCloseCamera = false;
	static std::atomic_uint lobbySession = 0;
	static std::atomic_uint cameraResetSession = 0;
	static std::atomic_int lobbyCharacterCount = 1;
	static std::array<std::atomic_int, 4> lobbyCharacterModels = { 0, 1, 2, 3 };
	static std::atomic_bool hasAssets = false;
	static std::atomic_bool isFrameReady = false;
	static std::atomic_bool isStartupLoading = true;
	static DWORD startupWaitStart = 0;
	static bool shouldRefreshMaterial = true;
	static std::atomic_bool isTransitionActive = false;
	static std::atomic_bool didSeeConnecting = false;
	static std::atomic<DWORD> transitionStartTime = 0;
	static void(*CL_Live_PartyGo)() = nullptr;
	static bool isRunningDeferredLaunch = false;
	static bool didRunDeferredLaunch = false;
	static unsigned int launchCount = 0;
	static unsigned int pendingLaunchId = 0;
	static bool canReadErrorState = false;

	static LobbyTexturePack texturePack;
	static std::atomic_bool isTexturePackReady = false;

	static bool didTryD3DX = false;
	static D3DXCompileShader_t compileShader = nullptr;
	static D3DXCreateTextureFromFileInMemory_t createTexture = nullptr;

	template <typename T>
	static void ReleaseObject(T*& object)
	{
		if (!object)
		{
			return;
		}

		object->Release();
		object = nullptr;
	}

	static bool HasOnlyAssetNameCharacters(const std::string& name)
	{
		return name.find_first_not_of(assetNameCharacters) == std::string::npos;
	}

	static bool TryGetBlendFactor(const std::string& name, DWORD& factor)
	{
		static const std::pair<const char*, DWORD> factors[] =
		{
			{ "zero", D3DBLEND_ZERO },
			{ "one", D3DBLEND_ONE },
			{ "srccolor", D3DBLEND_SRCCOLOR },
			{ "invsrccolor", D3DBLEND_INVSRCCOLOR },
			{ "srcalpha", D3DBLEND_SRCALPHA },
			{ "invsrcalpha", D3DBLEND_INVSRCALPHA },
			{ "destalpha", D3DBLEND_DESTALPHA },
			{ "invdestalpha", D3DBLEND_INVDESTALPHA },
			{ "destcolor", D3DBLEND_DESTCOLOR },
			{ "invdestcolor", D3DBLEND_INVDESTCOLOR },
		};

		for (const auto& [factorName, value] : factors)
		{
			if (name == factorName)
			{
				factor = value;
				return true;
			}
		}

		return false;
	}

	static void SetTransform(IDirect3DDevice9* device, D3DTRANSFORMSTATETYPE state, const DirectX::XMMATRIX& matrix)
	{
		DirectX::XMFLOAT4X4 stored;
		DirectX::XMStoreFloat4x4(&stored, matrix);
		device->SetTransform(state, reinterpret_cast<const D3DMATRIX*>(&stored));
	}

	static bool TryLoadD3DX()
	{
		if (didTryD3DX)
		{
			return compileShader && createTexture;
		}

		didTryD3DX = true;

		const HMODULE d3dx = LoadLibraryA("d3dx9_43.dll");

		if (d3dx)
		{
			compileShader = reinterpret_cast<D3DXCompileShader_t>(GetProcAddress(d3dx, "D3DXCompileShader"));
			createTexture = reinterpret_cast<D3DXCreateTextureFromFileInMemory_t>(GetProcAddress(d3dx, "D3DXCreateTextureFromFileInMemory"));
		}

		if (!compileShader || !createTexture)
		{
			Logger::Print("d3dx9_43.dll is not installed, the lobby background keeps the menu image\n");
			return false;
		}

		return true;
	}

	static void IndexTexturePack(LobbyTexturePack& pack)
	{
		if (pack.isIndexed)
		{
			return;
		}

		pack.isIndexed = true;

		auto& bytes = pack.bytes;
		auto& entries = pack.entries;

		const bool hasHeader = bytes.size() >= 12 && std::memcmp(bytes.data(), "ZWTP", 4) == 0;

		if (hasHeader)
		{
			std::uint32_t version = 0;
			std::uint32_t directorySize = 0;

			std::memcpy(&version, bytes.data() + 4, sizeof(version));
			std::memcpy(&directorySize, bytes.data() + 8, sizeof(directorySize));

			if (version == 1 && directorySize <= bytes.size() - 12)
			{
				try
				{
					const auto directory = nlohmann::json::parse(bytes.begin() + 12, bytes.begin() + 12 + directorySize);
					const std::size_t payload = 12 + static_cast<std::size_t>(directorySize);

					for (const auto& item : directory.items())
					{
						const auto offset = item.value().at(0).get<std::size_t>();
						const auto length = item.value().at(1).get<std::size_t>();

						if (offset > bytes.size() - payload || length > bytes.size() - payload - offset)
						{
							entries.clear();
							break;
						}

						entries.emplace(item.key(), std::make_pair(payload + offset, length));
					}
				}
				catch (const nlohmann::json::exception&)
				{
					entries.clear();
				}
			}
		}

		if (entries.empty())
		{
			bytes.clear();
		}
	}

	static bool CanReadLobbyZone()
	{
		return Game::DB_IsZoneLoaded("zw3_lobby")
			&& !Game::CL_IsCgameInitialized(0)
			&& Game::CL_GetLocalClientConnectionState(0) < Game::CA_CONNECTING
			&& Game::Sys_IsDatabaseReady();
	}

	static void PrepareTexturePack()
	{
		if (isTexturePackReady.load(std::memory_order_acquire) || !CanReadLobbyZone())
		{
			return;
		}

		auto* const entry = Game::DB_FindXAssetEntry(Game::ASSET_TYPE_RAWFILE, "lobby/textures.pack");
		const Game::RawFile* raw = nullptr;

		if (entry)
		{
			raw = entry->asset.header.rawfile;
		}

		const bool isUsable = raw
			&& !Game::DB_IsXAssetDefault(Game::ASSET_TYPE_RAWFILE, "lobby/textures.pack")
			&& raw->buffer
			&& raw->len > 0
			&& raw->len <= maxTexturePackSize
			&& raw->compressedLen >= 0
			&& raw->compressedLen <= maxTexturePackSize;

		if (!isUsable)
		{
			isTexturePackReady.store(true, std::memory_order_release);
			return;
		}

		const auto expected = static_cast<uLong>(raw->len);
		if (raw->compressedLen == 0)
		{
			texturePack.bytes.assign(raw->buffer, static_cast<std::size_t>(raw->len));
		}
		else
		{
			texturePack.bytes.resize(expected);
			uLongf length = expected;
			const int result = uncompress(reinterpret_cast<Bytef*>(texturePack.bytes.data()), &length,
				reinterpret_cast<const Bytef*>(raw->buffer), static_cast<uLong>(raw->compressedLen));
			if (result != Z_OK || length != expected) texturePack.bytes.clear();
		}
		IndexTexturePack(texturePack);
		isTexturePackReady.store(true, std::memory_order_release);
	}

	static std::string_view FindPackedTexture(const std::string& assetName)
	{
		if (!isTexturePackReady.load(std::memory_order_acquire)) return {};
		const auto found = texturePack.entries.find(assetName.substr(15));

		if (found == texturePack.entries.end())
		{
			return {};
		}

		return std::string_view(texturePack.bytes).substr(found->second.first, found->second.second);
	}

	static std::string ReadLobbyAsset(const std::string& assetName, const std::string& diskPath)
	{
		const bool isTexture = assetName.starts_with("lobby/textures/");

		if (isTexture && isTexturePackReady.load(std::memory_order_acquire))
		{
			const auto packed = FindPackedTexture(assetName);

			if (!packed.empty())
			{
				return std::string(packed);
			}
		}

		if (CanReadLobbyZone())
		{
			if (isTexture && !isTexturePackReady.load(std::memory_order_acquire))
			{
				PrepareTexturePack();

			}

			FileSystem::RawFile asset(assetName);

			if (asset.Exists())
			{
				std::string bytes = std::move(asset.GetBuffer());

				if (assetName.ends_with(".json"))
				{
					while (!bytes.empty() && bytes.back() == '\0')
					{
						bytes.pop_back();
					}
				}

				return bytes;
			}
		}

		return Utils::IO::ReadFile(diskPath);
	}

	static void LoadTheaterVision()
	{
		const std::string vision = ReadLobbyAsset("lobby/vision/mp_zombie_theater.vision", "usermaps/mp_zombie_theater/vision/mp_zombie_theater.vision");

		if (vision.empty())
		{
			return;
		}

		std::istringstream stream(vision);
		std::string line;

		while (std::getline(stream, line))
		{
			const auto firstQuote = line.find('"');

			if (firstQuote == std::string::npos)
			{
				continue;
			}

			const auto endQuote = line.find('"', firstQuote + 1);

			if (endQuote == std::string::npos)
			{
				continue;
			}

			const std::string setting = line.substr(firstQuote + 1, endQuote - firstQuote - 1);

			if (line.starts_with("r_filmContrast"))
			{
				filmContrast[0] = std::clamp(std::strtof(setting.data(), nullptr), 0.5f, 2.0f);
			}
			else if (line.starts_with("r_filmBrightness"))
			{
				filmBrightness[0] = std::clamp(std::strtof(setting.data(), nullptr), -0.5f, 0.5f);
			}
			else if (line.starts_with("r_glowBloomCutoff"))
			{
				glowSettings[0] = std::clamp(std::strtof(setting.data(), nullptr), 0.0f, 1.0f);
			}
			else if (line.starts_with("r_glowBloomIntensity0"))
			{
				glowSettings[1] = std::clamp(std::strtof(setting.data(), nullptr), 0.0f, 2.0f);
			}
			else
			{
				float* tint = nullptr;

				if (line.starts_with("r_filmLightTint"))
				{
					tint = filmLightTint;
				}
				else if (line.starts_with("r_filmMediumTint"))
				{
					tint = filmMediumTint;
				}
				else if (line.starts_with("r_filmDarkTint"))
				{
					tint = filmDarkTint;
				}

				float red = 1.0f;
				float green = 1.0f;
				float blue = 1.0f;

				if (tint && std::sscanf(setting.data(), "%f %f %f", &red, &green, &blue) == 3)
				{
					tint[0] = std::clamp(red, 0.0f, 2.0f);
					tint[1] = std::clamp(green, 0.0f, 2.0f);
					tint[2] = std::clamp(blue, 0.0f, 2.0f);
				}
			}
		}
	}

	static bool IsLeftDoor(const LobbyRoomVertex& point)
	{
		return point.x >= -60.1f && point.x <= 0.05f && point.y >= -519.0f && point.y <= -509.5f && point.z >= 79.5f && point.z <= 184.5f;
	}

	static bool IsRightDoor(const LobbyRoomVertex& point)
	{
		return point.x >= -0.05f && point.x <= 60.1f && point.y >= -519.0f && point.y <= -509.5f && point.z >= 79.5f && point.z <= 184.5f;
	}

	static bool LoadRoomMesh()
	{
		const std::string mesh = ReadLobbyAsset("lobby/theater_room.zwlb", "zw3/core/lobby/theater_room.zwlb");

		if (mesh.size() < 12 || std::memcmp(mesh.data(), "ZWLB", 4) != 0)
		{
			return false;
		}

		std::uint32_t version = 0;
		std::uint32_t vertexCount = 0;
		std::uint32_t vertexStride = 0;

		std::memcpy(&version, mesh.data() + 4, sizeof(version));
		std::memcpy(&vertexCount, mesh.data() + 8, sizeof(vertexCount));

		if (mesh.size() >= 16)
		{
			std::memcpy(&vertexStride, mesh.data() + 12, sizeof(vertexStride));
		}

		std::size_t expectedStride = sizeof(LobbyVertex);

		if (version == 3)
		{
			expectedStride = sizeof(LobbyRoomVertex);
		}

		const std::size_t expectedSize = 16 + static_cast<std::size_t>(vertexCount) * expectedStride;
		const bool hasTrailingByte = mesh.size() == expectedSize + 1;

		const bool isValid = (version == 2 || version == 3)
			&& vertexStride == expectedStride
			&& vertexCount != 0
			&& vertexCount % 3 == 0
			&& vertexCount <= 1800000
			&& mesh.size() >= expectedSize
			&& mesh.size() <= expectedSize + 1
			&& (!hasTrailingByte || mesh.back() == '\0');

		if (!isValid)
		{
			return false;
		}

		const std::string manifestBytes = ReadLobbyAsset("lobby/theater_room.json", "zw3/core/lobby/theater_room.json");

		if (manifestBytes.empty())
		{
			return false;
		}

		try
		{
			const auto manifest = nlohmann::json::parse(manifestBytes);

			if (manifest.at("version").get<int>() != static_cast<int>(version) || !manifest.at("groups").is_array())
			{
				return false;
			}

			if (manifest.contains("camera"))
			{
				const auto& camera = manifest["camera"];

				if (camera.contains("pan") && camera["pan"].is_array() && camera["pan"].size() == 3)
				{
					cameraPan = { camera["pan"][0].get<float>(), camera["pan"][1].get<float>(), camera["pan"][2].get<float>() };
				}
			}
			else
			{
				cameraPan = { 4.0f, 3.0f, 0.0f };
			}

			roomGroups.clear();
			UINT expectedFirst = 0;

			for (const auto& group : manifest.at("groups"))
			{
				const auto first = group.at("firstVertex").get<UINT>();
				const auto count = group.at("vertexCount").get<UINT>();
				const std::string textureName = group.value("texture", "");
				const bool isBlended = group.value("blend", false);
				const int lightmapIndex = group.value("lightmapIndex", -1);

				const bool isRangeValid = first == expectedFirst && expectedFirst <= vertexCount && count != 0 && count % 3 == 0 && count <= vertexCount - expectedFirst;
				const bool isLightmapValid = lightmapIndex >= -1 && lightmapIndex < static_cast<int>(roomLightmaps.size());
				const bool isTextureValid = textureName.empty() || HasOnlyAssetNameCharacters(textureName);

				if (!isRangeValid || !isLightmapValid || !isTextureValid)
				{
					roomGroups.clear();
					return false;
				}

				LobbyDrawGroup drawGroup;
				drawGroup.firstVertex = first;
				drawGroup.vertexCount = count;
				drawGroup.textureName = textureName;
				drawGroup.isBlended = isBlended;
				drawGroup.lightmapIndex = lightmapIndex;

				const std::string normalName = group.value("normalTexture", "");

				if (HasOnlyAssetNameCharacters(normalName) && normalName.find("..") == std::string::npos)
				{
					drawGroup.normalTextureName = normalName;
				}

				drawGroup.isAlphaTested = group.value("alphaTest", isBlended);

				const bool hasBlendFactors = TryGetBlendFactor(group.value("srcBlend", "srcalpha"), drawGroup.srcBlend)
					&& TryGetBlendFactor(group.value("dstBlend", "invsrcalpha"), drawGroup.dstBlend);

				if (!hasBlendFactors)
				{
					roomGroups.clear();
					return false;
				}

				float defaultThreshold = -1.0f;

				if (drawGroup.isAlphaTested)
				{
					defaultThreshold = 0.05f;
				}

				drawGroup.alphaThreshold = std::clamp(group.value("alphaThreshold", defaultThreshold), -1.0f, 1.0f);
				drawGroup.isMultiplicative = group.value("multiplicative", false);
				drawGroup.shouldWriteDepth = group.value("depthWrite", !isBlended);
				drawGroup.shouldWriteGamma = group.value("gammaWrite", false);

				roomGroups.push_back(drawGroup);
				expectedFirst += count;
			}

			if (expectedFirst != vertexCount)
			{
				roomGroups.clear();
				return false;
			}
		}
		catch (const nlohmann::json::exception&)
		{
			roomGroups.clear();
			return false;
		}

		if (roomGroups.empty())
		{
			return false;
		}

		roomVertices.resize(vertexCount);

		if (version == 3)
		{
			std::memcpy(roomVertices.data(), mesh.data() + 16, vertexCount * sizeof(LobbyRoomVertex));
		}
		else
		{
			for (std::uint32_t i = 0; i < vertexCount; ++i)
			{
				LobbyVertex old{};
				std::memcpy(&old, mesh.data() + 16 + i * sizeof(LobbyVertex), sizeof(LobbyVertex));
				roomVertices[i] = { old.x, old.y, old.z, old.color, old.u, old.v, 0.0f, 0.0f };
			}
		}

		doorLeftVertices.clear();
		doorRightVertices.clear();
		doorLeftGroups.clear();
		doorRightGroups.clear();

		std::vector<LobbyRoomVertex> staticVertices;
		staticVertices.reserve(roomVertices.size());

		std::vector<LobbyDrawGroup> staticGroups;
		staticGroups.reserve(roomGroups.size());

		for (const auto& group : roomGroups)
		{
			LobbyDrawGroup staticGroup = group;
			staticGroup.firstVertex = static_cast<UINT>(staticVertices.size());
			staticGroup.vertexCount = 0;

			LobbyDrawGroup leftGroup = group;
			leftGroup.firstVertex = static_cast<UINT>(doorLeftVertices.size());
			leftGroup.vertexCount = 0;

			LobbyDrawGroup rightGroup = group;
			rightGroup.firstVertex = static_cast<UINT>(doorRightVertices.size());
			rightGroup.vertexCount = 0;

			for (UINT i = 0; i < group.vertexCount; i += 3)
			{
				const auto& first = roomVertices[group.firstVertex + i];
				const auto& second = roomVertices[group.firstVertex + i + 1];
				const auto& third = roomVertices[group.firstVertex + i + 2];

				if (IsLeftDoor(first) && IsLeftDoor(second) && IsLeftDoor(third))
				{
					doorLeftVertices.push_back(first);
					doorLeftVertices.push_back(second);
					doorLeftVertices.push_back(third);
					leftGroup.vertexCount += 3;
				}
				else if (IsRightDoor(first) && IsRightDoor(second) && IsRightDoor(third))
				{
					doorRightVertices.push_back(first);
					doorRightVertices.push_back(second);
					doorRightVertices.push_back(third);
					rightGroup.vertexCount += 3;
				}
				else
				{
					staticVertices.push_back(first);
					staticVertices.push_back(second);
					staticVertices.push_back(third);
					staticGroup.vertexCount += 3;
				}
			}

			if (staticGroup.vertexCount > 0)
			{
				staticGroups.push_back(staticGroup);
			}

			if (leftGroup.vertexCount > 0)
			{
				doorLeftGroups.push_back(leftGroup);
			}

			if (rightGroup.vertexCount > 0)
			{
				doorRightGroups.push_back(rightGroup);
			}
		}

		roomVertices = std::move(staticVertices);
		roomGroups = std::move(staticGroups);

		return true;
	}

	static void LoadPropMesh()
	{
		propVertices.clear();
		propIndices.clear();
		propGroups.clear();

		const std::string mesh = ReadLobbyAsset("lobby/theater_props.zwlb", "zw3/core/lobby/theater_props.zwlb");
		const std::string manifestBytes = ReadLobbyAsset("lobby/theater_props.json", "zw3/core/lobby/theater_props.json");

		if (mesh.size() < 16 || manifestBytes.empty() || std::memcmp(mesh.data(), "ZWLB", 4) != 0)
		{
			return;
		}

		std::uint32_t version = 0;
		std::uint32_t vertexCount = 0;
		std::uint32_t vertexStride = 0;
		std::uint32_t indexCount = 0;

		std::memcpy(&version, mesh.data() + 4, sizeof(version));
		std::memcpy(&vertexCount, mesh.data() + 8, sizeof(vertexCount));
		std::memcpy(&vertexStride, mesh.data() + 12, sizeof(vertexStride));

		if (version == 4 && mesh.size() >= 20)
		{
			std::memcpy(&indexCount, mesh.data() + 16, sizeof(indexCount));
		}

		std::size_t headerSize = 16;

		if (version == 4)
		{
			headerSize = 20;
		}

		const std::size_t expectedSize = headerSize + static_cast<std::size_t>(vertexCount) * sizeof(LobbyVertex) + 4 * static_cast<std::size_t>(indexCount);
		const bool hasTrailingByte = mesh.size() == expectedSize + 1;
		const bool areIndicesValid = version != 4 || (indexCount != 0 && indexCount % 3 == 0 && indexCount <= 6000000);

		const bool isValid = (version == 2 || version == 4)
			&& vertexStride == sizeof(LobbyVertex)
			&& vertexCount != 0
			&& (version != 2 || vertexCount % 3 == 0)
			&& vertexCount <= 3000000
			&& areIndicesValid
			&& mesh.size() >= expectedSize
			&& mesh.size() <= expectedSize + 1
			&& (!hasTrailingByte || mesh.back() == '\0');

		if (!isValid)
		{
			return;
		}

		try
		{
			const auto manifest = nlohmann::json::parse(manifestBytes);

			if (manifest.at("version").get<unsigned int>() != version || !manifest.at("groups").is_array())
			{
				return;
			}

			UINT expectedFirst = 0;
			UINT expectedIndex = 0;

			std::vector<std::uint32_t> indices(indexCount);

			if (indexCount)
			{
				std::memcpy(indices.data(), mesh.data() + headerSize + vertexCount * sizeof(LobbyVertex), indexCount * sizeof(std::uint32_t));
			}

			for (const auto& group : manifest.at("groups"))
			{
				const auto first = group.at("firstVertex").get<UINT>();
				const auto count = group.at("vertexCount").get<UINT>();
				const auto name = group.at("texture").get<std::string>();
				const auto& cell = group.at("cell");
				const auto firstIndex = group.value("firstIndex", 0u);
				const auto groupIndexCount = group.value("indexCount", 0u);

				const bool isRangeValid = first == expectedFirst && count != 0 && (version != 2 || count % 3 == 0) && count <= vertexCount - expectedFirst;
				const bool isIndexRangeValid = version != 4 || (firstIndex == expectedIndex && groupIndexCount != 0 && groupIndexCount % 3 == 0 && groupIndexCount <= indexCount - expectedIndex);
				const bool isNameValid = HasOnlyAssetNameCharacters(name) && name.find("..") == std::string::npos;
				const bool isCellValid = cell.is_array() && cell.size() == 2;

				if (!isRangeValid || !isIndexRangeValid || !isNameValid || !isCellValid)
				{
					propGroups.clear();
					return;
				}

				if (version == 4)
				{
					const auto groupFirst = indices.begin() + firstIndex;
					const bool isOutOfRange = std::any_of(groupFirst, groupFirst + groupIndexCount, [count](std::uint32_t index)
					{
						return index >= count;
					});

					if (isOutOfRange)
					{
						propGroups.clear();
						return;
					}
				}

				LobbyDrawGroup drawGroup;
				drawGroup.firstVertex = first;
				drawGroup.vertexCount = count;
				drawGroup.textureName = name;
				drawGroup.isBlended = group.value("blend", false);
				drawGroup.hasCell = true;
				drawGroup.cellX = cell[0].get<int>();
				drawGroup.cellY = cell[1].get<int>();
				drawGroup.firstIndex = firstIndex;
				drawGroup.indexCount = groupIndexCount;
				drawGroup.isAlphaTested = group.value("alphaTest", true);
				drawGroup.alphaThreshold = std::clamp(group.value("alphaThreshold", 0.05f), -1.0f, 1.0f);

				const bool hasBlendFactors = TryGetBlendFactor(group.value("srcBlend", "srcalpha"), drawGroup.srcBlend)
					&& TryGetBlendFactor(group.value("dstBlend", "invsrcalpha"), drawGroup.dstBlend);

				if (!hasBlendFactors)
				{
					propGroups.clear();
					return;
				}

				drawGroup.isMultiplicative = group.value("multiplicative", false);
				drawGroup.shouldWriteDepth = group.value("depthWrite", !drawGroup.isBlended);
				drawGroup.shouldWriteGamma = group.value("gammaWrite", false);

				propGroups.push_back(drawGroup);
				expectedFirst += count;
				expectedIndex += groupIndexCount;
			}

			if (expectedFirst != vertexCount || expectedIndex != indexCount)
			{
				propGroups.clear();
				return;
			}

			propIndices = std::move(indices);
		}
		catch (const nlohmann::json::exception&)
		{
			propGroups.clear();
			return;
		}

		propVertices.resize(vertexCount);
		std::memcpy(propVertices.data(), mesh.data() + headerSize, vertexCount * sizeof(LobbyVertex));
	}

	static unsigned int ClipFromName(const std::string& name, bool isZombie)
	{
		if (name == "attack")
		{
			return clipAttack;
		}

		if (name == "melee")
		{
			return clipMelee;
		}

		if (name == "point")
		{
			return clipPoint;
		}

		if (name == "shotgun_fire")
		{
			return clipShotgunFire;
		}

		if (name == "rifle_fire")
		{
			return clipRifleFire;
		}

		if (name == "rifle_idle")
		{
			return clipRifleIdle;
		}

		if (name == "walk" && !isZombie)
		{
			return clipWalk;
		}

		if (name == "death")
		{
			return clipDeath;
		}

		if (name == "fire" || name == "run")
		{
			return clipAction;
		}

		return clipIdle;
	}

	static void LoadActorMesh(const std::size_t index, const std::string& mesh, const std::string& manifestBytes)
	{
		if (mesh.size() < 16 || manifestBytes.empty() || std::memcmp(mesh.data(), "ZWLB", 4) != 0)
		{
			return;
		}

		std::uint32_t version = 0;
		std::uint32_t count = 0;
		std::uint32_t stride = 0;
		std::uint32_t frameCount = 1;

		std::memcpy(&version, mesh.data() + 4, sizeof(version));
		std::memcpy(&count, mesh.data() + 8, sizeof(count));
		std::memcpy(&stride, mesh.data() + 12, sizeof(stride));

		if ((version == 3 || version == 4) && mesh.size() >= 20)
		{
			std::memcpy(&frameCount, mesh.data() + 16, sizeof(frameCount));
		}

		std::uint64_t headerSize = 16;

		if (version >= 3)
		{
			headerSize = 20;
		}

		std::uint64_t expected = headerSize + static_cast<std::uint64_t>(frameCount) * count * sizeof(LobbyVertex);
		std::uint32_t expectedStride = sizeof(LobbyVertex);

		if (version == 4)
		{
			expected = headerSize + static_cast<std::uint64_t>(count) * 12 + static_cast<std::uint64_t>(frameCount) * count * 6;
			expectedStride = 6;
		}

		const bool isValid = (version == 2 || version == 3 || version == 4)
			&& stride == expectedStride
			&& count != 0
			&& count % 3 == 0
			&& count <= 100000
			&& frameCount != 0
			&& frameCount <= 128
			&& expected <= 100'000'000
			&& mesh.size() >= expected
			&& mesh.size() <= expected + 1;

		if (!isValid)
		{
			return;
		}

		auto& actor = actorMeshes[index];
		const bool isZombie = index == zombieActor;

		try
		{
			const auto manifest = nlohmann::json::parse(manifestBytes);

			if (manifest.at("version").get<std::uint32_t>() != version)
			{
				return;
			}

			actor.muzzleAnchors = {};

			if (manifest.contains("muzzleAnchors") && manifest.at("muzzleAnchors").size() == actor.muzzleAnchors.size())
			{
				for (std::size_t weapon = 0; weapon < actor.muzzleAnchors.size(); ++weapon)
				{
					const auto& source = manifest.at("muzzleAnchors").at(weapon);
					auto& anchor = actor.muzzleAnchors[weapon];

					anchor.vertices = source.at("vertices").get<std::array<UINT, 3>>();
					anchor.u = source.at("u").get<float>();
					anchor.v = source.at("v").get<float>();
					anchor.w = source.at("w").get<float>();

					const bool areVerticesInMesh = std::ranges::all_of(anchor.vertices, [count](const UINT vertex)
					{
						return vertex < count;
					});

					anchor.isValid = areVerticesInMesh && std::isfinite(anchor.u) && std::isfinite(anchor.v) && std::isfinite(anchor.w);
				}
			}

			actor.groups.clear();
			UINT expectedFirst = 0;

			for (const auto& group : manifest.at("groups"))
			{
				const auto first = group.at("firstVertex").get<UINT>();
				const auto length = group.at("vertexCount").get<UINT>();
				const auto texture = group.at("texture").get<std::string>();
				const int weapon = group.value("weapon", -1);

				const bool isRangeValid = first == expectedFirst && length != 0 && length % 3 == 0 && length <= count - expectedFirst;
				const bool isWeaponValid = weapon >= -1 && weapon <= 3;
				const bool isTextureValid = HasOnlyAssetNameCharacters(texture) && texture.find("..") == std::string::npos;

				if (!isRangeValid || !isWeaponValid || !isTextureValid)
				{
					actor.groups.clear();
					return;
				}

				LobbyDrawGroup drawGroup;
				drawGroup.firstVertex = first;
				drawGroup.vertexCount = length;
				drawGroup.textureName = texture;
				drawGroup.weaponIndex = weapon;

				if (weapon >= 0)
				{
					drawGroup.isAlphaTested = false;
					drawGroup.alphaThreshold = 0.0f;
				}

				actor.groups.push_back(drawGroup);
				expectedFirst += length;
			}

			if (expectedFirst != count)
			{
				actor.groups.clear();
				return;
			}

			if (version >= 3)
			{
				for (const auto& clip : manifest.at("clips"))
				{
					const auto first = clip.at("firstFrame").get<UINT>();
					const auto length = clip.at("frameCount").get<UINT>();

					if (!length || first >= frameCount || length > frameCount - first)
					{
						return;
					}

					const auto clipName = clip.at("name").get<std::string>();
					const unsigned int clipIndex = ClipFromName(clipName, isZombie);

					actor.clipDurationMs[clipIndex] = std::clamp(clip.value("durationMs", 0u), 0u, 10000u);

					const bool isKnownClip = clipName == "idle" || clipName == "walk" || clipName == "fire" || clipName == "run"
						|| clipName == "point" || clipName == "death" || clipName == "rifle_idle" || clipName == "rifle_fire"
						|| clipName == "shotgun_fire" || clipName == "melee" || clipName == "attack";

					if (isKnownClip)
					{
						actor.clipFirst[clipIndex] = first;
						actor.clipLength[clipIndex] = length;
					}
				}
			}
		}
		catch (const nlohmann::json::exception&)
		{
			actor.groups.clear();
			return;
		}

		actor.frames.resize(frameCount);

		for (std::uint32_t frame = 0; frame < frameCount; ++frame)
		{
			actor.frames[frame].resize(count);

			if (version == 4)
			{
				const auto* const attributes = reinterpret_cast<const unsigned char*>(mesh.data() + headerSize);
				const auto* const positions = reinterpret_cast<const unsigned char*>(mesh.data() + headerSize + static_cast<std::size_t>(count) * 12 + static_cast<std::size_t>(frame) * count * 6);

				for (std::uint32_t vertex = 0; vertex < count; ++vertex)
				{
					auto& output = actor.frames[frame][vertex];

					std::int16_t position[3];
					std::memcpy(position, positions + static_cast<std::size_t>(vertex) * 6, sizeof(position));

					output.x = static_cast<float>(position[0]) / 256.0f;
					output.y = static_cast<float>(position[1]) / 256.0f;
					output.z = static_cast<float>(position[2]) / 256.0f;

					const unsigned char* const attribute = attributes + static_cast<std::size_t>(vertex) * 12;
					std::memcpy(&output.color, attribute, sizeof(output.color));
					std::memcpy(&output.u, attribute + 4, sizeof(output.u));
					std::memcpy(&output.v, attribute + 8, sizeof(output.v));
				}
			}
			else
			{
				std::memcpy(actor.frames[frame].data(), mesh.data() + headerSize + static_cast<std::size_t>(frame) * count * sizeof(LobbyVertex), count * sizeof(LobbyVertex));
			}
		}

		actor.interpolated.resize(count);
		actor.blended.resize(count);
	}

	static IDirect3DVertexBuffer9* CreateFilledVertexBuffer(IDirect3DDevice9* device, const void* source, std::size_t size)
	{
		if (!size || size > std::numeric_limits<UINT>::max())
		{
			return nullptr;
		}

		IDirect3DVertexBuffer9* buffer = nullptr;

		if (FAILED(device->CreateVertexBuffer(static_cast<UINT>(size), D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT, &buffer, nullptr)))
		{
			return nullptr;
		}

		void* destination = nullptr;

		if (FAILED(buffer->Lock(0, 0, &destination, 0)))
		{
			buffer->Release();
			return nullptr;
		}

		std::memcpy(destination, source, size);
		buffer->Unlock();

		return buffer;
	}

	static void EnsureRoomVertexBuffer(IDirect3DDevice9* device)
	{
		if (roomVertexBuffer || roomVertices.empty() || !device)
		{
			return;
		}

		roomVertexBuffer = CreateFilledVertexBuffer(device, roomVertices.data(), roomVertices.size() * sizeof(LobbyRoomVertex));
	}

	static void RefreshLobbyMaterial()
	{
		const bool canRefresh = !Dedicated::IsEnabled()
			&& !ZoneBuilder::IsEnabled()
			&& FastFiles::Ready()
			&& *Game::dx_device
			&& Renderer::Width() > 0
			&& Renderer::Height() > 0
			&& !Renderer::IsDeviceRecoveryActive();

		if (!canRefresh)
		{
			return;
		}

		if (!roomImage)
		{
			textureWidth = static_cast<unsigned int>(std::clamp(Renderer::Width(), 640, 3840));
			textureHeight = static_cast<unsigned int>(std::clamp(Renderer::Height(), 360, 2160));
			roomImage = Materials::CreateImage("zw3_lobby_scene_image", textureWidth, textureHeight, 1, writableImageFlags, D3DFMT_A8R8G8B8);
		}

		auto* const entry = Game::DB_FindXAssetEntry(Game::ASSET_TYPE_MATERIAL, "white");

		if (!entry || !entry->asset.header.material)
		{
			return;
		}

		const Game::Material* const baseMaterial = entry->asset.header.material;

		if (!roomMaterial)
		{
			if (roomImage)
			{
				roomMaterial = Materials::Create("zw3_lobby_scene", roomImage);
			}
		}
		else if (shouldRefreshMaterial && baseMaterial->techniqueSet)
		{
			roomMaterial->techniqueSet = baseMaterial->techniqueSet;
			roomMaterial->constantTable = baseMaterial->constantTable;
			roomMaterial->constantCount = baseMaterial->constantCount;
			roomMaterial->stateBitsTable = baseMaterial->stateBitsTable;
			roomMaterial->stateBitsCount = baseMaterial->stateBitsCount;

			if (roomMaterial->textureTable)
			{
				roomMaterial->textureTable->u.image = roomImage;
			}
		}

		if (roomMaterial)
		{
			shouldRefreshMaterial = false;
		}
	}

	struct LobbyBackgroundAnimation
	{
		int expressionCount;
		float opacity;
	};

	static std::unordered_map<Game::itemDef_s*, LobbyBackgroundAnimation> backgroundAnimations;

	static void AttachMaterialToLobby(const char* menuName, const bool isSceneReady)
	{
		auto* const menu = Game::Menus_FindByName(Game::uiContext, menuName);

		if (!menu)
		{
			return;
		}

		for (int i = 0; i < menu->itemCount; ++i)
		{
			auto* const item = menu->items[i];

			if (!item)
			{
				continue;
			}

			const char* windowName = "";

			if (item->window.name)
			{
				windowName = item->window.name;
			}

			const char* backgroundName = "";

			if (item->window.background && item->window.background->info.name)
			{
				backgroundName = item->window.background->info.name;
			}

			const bool isBackground = !_stricmp(windowName, "zw3_lobby_theater_background")
				|| !_stricmp(windowName, "main_text_background")
				|| !_stricmp(backgroundName, "mw2_main_co_image")
				|| !_stricmp(backgroundName, "mw2_main_background");

			const bool isFullScreen = item->window.rectClient.w >= 640.0f && item->window.rectClient.h >= 480.0f;
			const bool isFullScreenGlow = isFullScreen && (!_stricmp(backgroundName, "black") || !_stricmp(backgroundName, "mockup_bg_glow"));

			const bool isCloud = !_stricmp(windowName, "zw3_lobby_fallback_cloud")
				|| !_stricmp(windowName, "main_text_cloud")
				|| std::strstr(windowName, "cloud")
				|| std::strstr(backgroundName, "cloud")
				|| isFullScreenGlow;

			if (isBackground)
			{
				if (isSceneReady && roomMaterial && roomImage && roomImage->texture.map)
				{
					item->window.background = roomMaterial;
					item->window.foreColor[3] = 1.0f;
				}
				else
				{
					Game::XAssetEntry* entry = nullptr;

					if (FastFiles::Ready())
					{
						entry = Game::DB_FindXAssetEntry(Game::ASSET_TYPE_MATERIAL, "mw2_main_co_image");
					}

					if (entry && entry->asset.header.material)
					{
						item->window.background = entry->asset.header.material;
					}

					item->window.foreColor[3] = 0.35f;
				}
			}
			else if (isCloud)
			{
				if (isSceneReady)
				{
					backgroundAnimations.try_emplace(item, LobbyBackgroundAnimation{ item->floatExpressionCount, item->window.foreColor[3] });
					item->floatExpressionCount = 0;
					item->window.foreColor[3] = 0.0f;
				}
				else
				{
					const auto original = backgroundAnimations.find(item);

					if (original != backgroundAnimations.end())
					{
						item->floatExpressionCount = original->second.expressionCount;
						item->window.foreColor[3] = original->second.opacity;
					}
				}
			}
		}
	}

	static void AttachMaterialToLobbies(const bool isSceneReady)
	{
		AttachMaterialToLobby("main_text", isSceneReady);
		AttachMaterialToLobby("menu_xboxlive_privatelobby", isSceneReady);
		AttachMaterialToLobby("zwnet_matchmaking", isSceneReady);
		AttachMaterialToLobby("pregame_loaderror", isSceneReady);
	}

	static bool IsLobbyMenuVisible(const char* menuName)
	{
		auto* const menu = Game::Menus_FindByName(Game::uiContext, menuName);
		return menu && Game::Menu_IsVisible(Game::uiContext, menu);
	}

	static void CL_Live_PartyGo_Hk()
	{
		const bool isDeferred = LobbyScene::DeferLaunch([]
		{
			Command::Execute("xpartygo");
		});

		if (!isDeferred)
		{
			CL_Live_PartyGo();
		}
	}

	static void PublishLobbyCharacters(const bool isPrivateVisible)
	{
		const char* countName = "zwnet_lobby_member_count";

		if (isPrivateVisible)
		{
			countName = "party_currentPlayers";
		}

		const auto* const countDvar = Game::Dvar_FindVar(countName);
		int count = 1;

		if (countDvar && countDvar->type == Game::DVAR_TYPE_INT)
		{
			count = countDvar->current.integer;
		}

		const int publishedCount = std::clamp(count, 0, 4);
		int occupiedCount = 0;
		std::array<int, 4> models = { -1, -1, -1, -1 };

		static constexpr const char* characterDvars[] = { "character_1", "character_2", "character_3", "character_4" };
		static constexpr const char* characterNames[] = { "Richtofen", "Dempsey", "Nikolai", "Takeo" };

		for (unsigned int slot = 0; slot < lobbyCharacterModels.size(); ++slot)
		{
			if (slot >= static_cast<unsigned int>(publishedCount))
			{
				break;
			}

			int modelIndex = static_cast<int>(slot);

			if (isPrivateVisible)
			{
				modelIndex = -1;
			}

			const auto* const owner = Game::Dvar_FindVar(Utils::String::VA("character_%u_player", slot + 1));
			const bool isOwnerString = owner && owner->type == Game::DVAR_TYPE_STRING;
			const bool isVacated = isPrivateVisible && isOwnerString && (!owner->current.string || !owner->current.string[0] || !_stricmp(owner->current.string, "None"));

			if (isVacated)
			{
				continue;
			}

			const auto* const character = Game::Dvar_FindVar(characterDvars[slot]);

			if (character && character->type == Game::DVAR_TYPE_STRING && character->current.string)
			{
				for (std::size_t candidate = 0; candidate < std::size(characterNames); ++candidate)
				{
					if (!_stricmp(character->current.string, characterNames[candidate]))
					{
						modelIndex = static_cast<int>(candidate);
						break;
					}
				}
			}

			if (modelIndex >= 0)
			{
				models[occupiedCount] = modelIndex;
				++occupiedCount;
			}
		}

		for (std::size_t slot = 0; slot < models.size(); ++slot)
		{
			lobbyCharacterModels[slot].store(models[slot], std::memory_order_release);
		}

		lobbyCharacterCount.store(occupiedCount, std::memory_order_release);
	}

	static void UpdateMenu()
	{
		if (!Party::IsLobbySceneClient()) LobbyScene::ClearRemoteScene();
		else LobbyScene::PollLocalScene();
		static bool wasStartupCinematic = false;
		static DWORD cinematicEndTime = 0;

		if (isStartupLoading.load(std::memory_order_acquire))
		{
			const bool isCinematic = LobbyScene::IsCinematicActive();

			if (isCinematic)
			{
				cinematicEndTime = 0;
			}
			else if (wasStartupCinematic)
			{
				cinematicEndTime = timeGetTime();
			}

			wasStartupCinematic = isCinematic;

			const bool hasCinematicSettled = !isCinematic && cinematicEndTime != 0 && timeGetTime() - cinematicEndTime >= 2000u;

			if (hasCinematicSettled)
			{
				isStartupLoading.store(false, std::memory_order_release);
			}
		}
		else
		{
			wasStartupCinematic = false;
			cinematicEndTime = 0;
		}

		if (!CL_Live_PartyGo)
		{
			auto* const command = Command::Find("xpartygo");

			if (command)
			{
				CL_Live_PartyGo = command->function;
				command->function = CL_Live_PartyGo_Hk;
			}
		}

		const auto state = Game::CL_GetLocalClientConnectionState(0);
		const bool hasTimedOut = LobbyScene::IsTransitionActive() && timeGetTime() - transitionStartTime.load() > 25000u;

		if (hasTimedOut)
		{
			LobbyScene::StopTransition();
		}

		if (state >= Game::CA_CONNECTING || Game::CL_IsCgameInitialized(0))
		{
			isStartupLoading.store(false, std::memory_order_release);

			if (LobbyScene::IsTransitionActive())
			{
				didSeeConnecting.store(true, std::memory_order_release);

				const bool hasSettled = didSeeConnecting.load() && (state == Game::CA_ACTIVE || state == Game::CA_DISCONNECTED);

				if (hasSettled)
				{
					LobbyScene::StopTransition();
				}
			}

			isLobbyVisible.store(false, std::memory_order_release);
			hasZombieWaveStarted.store(false, std::memory_order_release);
			return;
		}

		if (LobbyScene::IsTransitionActive())
		{
			return;
		}

		if (roomMaterial && shouldRefreshMaterial)
		{
			RefreshLobbyMaterial();
		}

		const bool isSceneReady = isFrameReady.load(std::memory_order_acquire) && roomMaterial && !shouldRefreshMaterial && !roomVertices.empty();

		AttachMaterialToLobby("menu_xboxlive_privatelobby", isSceneReady);
		AttachMaterialToLobby("zwnet_matchmaking", isSceneReady);
		AttachMaterialToLobby("pregame_loaderror", isSceneReady);
		AttachMaterialToLobby("main_text", isSceneReady);

		PrepareTexturePack();
		LobbyScene::PrepareStartup();

		const bool isPrivateVisible = IsLobbyMenuVisible("menu_xboxlive_privatelobby");
		const bool isMatchmakingVisible = IsLobbyMenuVisible("zwnet_matchmaking");
		const bool isInLobby = isPrivateVisible || isMatchmakingVisible;
		const bool isOnMainMenu = IsLobbyMenuVisible("main_text") || IsLobbyMenuVisible("pregame_loaderror");

		static bool wasInLobby = false;

		if (isInLobby)
		{
			wasInLobby = true;
		}
		else if (wasInLobby && isOnMainMenu)
		{
			hasZombieWaveStarted.store(false, std::memory_order_release);
			lobbySession.fetch_add(1, std::memory_order_release);
			wasInLobby = false;
		}

		const bool isVisible = isInLobby || isOnMainMenu;

		shouldUseCloseCamera.store(isInLobby, std::memory_order_release);
		isLobbyVisible.store(isVisible, std::memory_order_release);

		static bool wasLobbyVisible = false;

		if (isVisible && !wasLobbyVisible)
		{
			lobbySession.fetch_add(1, std::memory_order_release);
			cameraResetSession.fetch_add(1, std::memory_order_release);
		}

		wasLobbyVisible = isVisible;

		if (LobbyScene::IsCinematicActive())
		{
			startupWaitStart = 0;
		}

		if (LobbyScene::IsStartupLoading() && !Renderer::IsDeviceRecoveryActive())
		{
			if (!startupWaitStart)
			{
				startupWaitStart = timeGetTime();
			}

			if (isSceneReady || timeGetTime() - startupWaitStart >= 12000u)
			{
				isStartupLoading.store(false, std::memory_order_release);
			}
		}

		if (!isVisible)
		{
			return;
		}

		if (isSceneReady && !Renderer::IsDeviceRecoveryActive() && !isStartupLoading.load(std::memory_order_acquire))
		{
			hasZombieWaveStarted.store(true, std::memory_order_release);
		}

		if (isInLobby)
		{
			PublishLobbyCharacters(isPrivateVisible);
			return;
		}

		for (std::size_t slot = 1; slot < lobbyCharacterModels.size(); ++slot)
		{
			lobbyCharacterModels[slot].store(-1, std::memory_order_release);
		}

		if (lobbyCharacterModels[0].load(std::memory_order_acquire) < 0)
		{
			lobbyCharacterModels[0].store(0, std::memory_order_release);
		}

		lobbyCharacterCount.store(1, std::memory_order_release);
	}

	static void ReleaseDepth()
	{
		ReleaseObject(savedState);
		ReleaseObject(sceneTexture);
		ReleaseObject(roomDepth);

		if (roomImage)
		{
			ReleaseObject(roomImage->texture.map);
		}
	}

	static void ClearGroupTextures(std::vector<LobbyDrawGroup>& groups)
	{
		for (auto& group : groups)
		{
			group.texture = nullptr;
			group.normalTexture = nullptr;
		}
	}

	static void BindGroupTextures(std::vector<LobbyDrawGroup>& groups)
	{
		for (auto& group : groups)
		{
			const auto texture = loadedTextures.find(group.textureName);

			if (texture != loadedTextures.end())
			{
				group.texture = texture->second;
			}

			const auto normal = loadedTextures.find(group.normalTextureName);

			if (normal != loadedTextures.end())
			{
				group.normalTexture = normal->second;
			}
		}
	}

	static void QueueGroupTextures(const std::vector<LobbyDrawGroup>& groups, std::unordered_set<std::string>& seen)
	{
		for (const auto& group : groups)
		{
			if (!group.textureName.empty() && seen.emplace(group.textureName).second)
			{
				pendingTextureNames.push_back(group.textureName);
			}

			if (!group.normalTextureName.empty() && seen.emplace(group.normalTextureName).second)
			{
				pendingTextureNames.push_back(group.normalTextureName);
			}
		}
	}

	static void ReleaseTextures()
	{
		ReleaseObject(savedState);
		ReleaseObject(roomVertexBuffer);
		ReleaseObject(propVertexBuffer);
		ReleaseObject(propIndexBuffer);
		ReleaseObject(visionShader);
		ReleaseObject(lightmapFilmShader);
		ReleaseObject(filmShader);

		for (auto& [name, texture] : loadedTextures)
		{
			if (texture)
			{
				texture->Release();
			}
		}

		loadedTextures.clear();
		pendingTextureNames.clear();
		nextTexture = 0;
		hasRenderResources = false;

		for (auto& lightmap : roomLightmaps)
		{
			ReleaseObject(lightmap);
		}

		ClearGroupTextures(roomGroups);
		ClearGroupTextures(doorLeftGroups);
		ClearGroupTextures(doorRightGroups);
		ClearGroupTextures(propGroups);

		for (auto& actor : actorMeshes)
		{
			ClearGroupTextures(actor.groups);
		}
	}

	static IDirect3DPixelShader9* CompilePixelShader(IDirect3DDevice9* device, const std::string_view source)
	{
		static std::unordered_map<std::string, std::vector<DWORD>> bytecode;
		const std::string key(source);
		const auto cached = bytecode.find(key);
		IDirect3DPixelShader9* shader = nullptr;
		if (cached != bytecode.end())
		{
			device->CreatePixelShader(cached->second.data(), &shader);
			return shader;
		}

		ID3DBlob* code = nullptr;
		ID3DBlob* errors = nullptr;

		const HRESULT result = compileShader(source.data(), static_cast<UINT>(source.size()), nullptr, nullptr, "main", "ps_2_0", 0, &code, &errors, nullptr);

		if (SUCCEEDED(result) && code)
		{
			const auto* words = static_cast<const DWORD*>(code->GetBufferPointer());
			auto& cachedCode = bytecode[key];
			cachedCode.assign(words, words + code->GetBufferSize() / sizeof(DWORD));
			device->CreatePixelShader(cachedCode.data(), &shader);
		}

		ReleaseObject(errors);
		ReleaseObject(code);

		return shader;
	}

	static constexpr char filmSource[] = R"(
		sampler2D diffuseTexture : register(s0);
		float4 materialFlags : register(c0);
		float4 main(float2 uv : TEXCOORD0, float4 vertexColor : COLOR0) : COLOR0
		{
			float4 diffuse = tex2D(diffuseTexture, uv);
			if (materialFlags.z < -4.5f)
			{
				float intensity = max(diffuse.r, max(diffuse.g, diffuse.b));
				return float4(intensity * vertexColor.rgb * vertexColor.a * 2.5f, 1.0f);
			}
			if (materialFlags.z < -3.5f)
			{
				return float4(float3(3.5f, 0.25f, 0.08f) * diffuse.rgb, 1.0f);
			}
			if (materialFlags.z < -1.5f)
			{
				return float4(diffuse.rgb * vertexColor.rgb * vertexColor.a * 2.5f, 1.0f);
			}
			if (materialFlags.y > 0.5f)
			{
				clip(diffuse.a - materialFlags.z);
			}
			float3 shaded = diffuse.rgb * lerp(vertexColor.rgb, 1.0f, materialFlags.w);
			if (materialFlags.x > 0.5f)
			{
				shaded *= shaded;
			}
			return float4(shaded, lerp(1.0f, diffuse.a, materialFlags.y));
		}
	)";

	static constexpr char lightmapFilmSource[] = R"(
		sampler2D diffuseTexture : register(s0);
		sampler2D lightTexture : register(s1);
		sampler2D normalTexture : register(s2);
		float4 materialFlags : register(c0);
		float4 main(float2 uv : TEXCOORD0, float2 lightUv : TEXCOORD1,
			float4 vertexColor : COLOR0) : COLOR0
		{
			float4 diffuse = tex2D(diffuseTexture, uv) * vertexColor;
			if (materialFlags.y > 0.5f)
			{
				clip(diffuse.a - materialFlags.z);
			}
			float2 packedUv = float2(lightUv.x, lightUv.y * 0.5f);
			float4 lobe0 = tex2D(lightTexture, packedUv);
			float4 lobe1 = tex2D(lightTexture, packedUv + float2(0.0f, 0.5f));
			float2 lightSlope = float2(lobe0.a, lobe1.a) * float2(4.08f, 4.064516f) - float2(2.08f, 2.064516f);
			float2 normalSlope = (tex2D(normalTexture, uv).ag * float2(4.08f, 4.064516f) -
				float2(2.08f, 2.064516f)) * materialFlags.x * diffuse.a;
			float normalScale = rsqrt(dot(normalSlope, normalSlope) + 1.0f);
			float directionScale = saturate((dot(lightSlope, normalSlope) + 1.0f) *
				rsqrt(dot(lightSlope, lightSlope) + 1.0f) * normalScale);
			float3 light = lobe0.rgb * normalScale + lobe1.rgb * directionScale;
			float3 shaded = diffuse.rgb * lerp(light, 1.0f, materialFlags.w);
			return float4(shaded * shaded, lerp(1.0f, diffuse.a, materialFlags.y));
		}
	)";

	static constexpr char visionSource[] = R"(
		sampler2D scene : register(s0);
		float4 contrast : register(c0);
		float4 lightTint : register(c1);
		float4 mediumTint : register(c2);
		float4 darkTint : register(c3);
		float4 exposure : register(c4);
		float4 brightness : register(c5);
		float4 glow : register(c6);
		float3 highlight(float3 color)
		{
			return max(color - glow.x, 0.0);
		}
		float4 main(float2 uv : TEXCOORD0) : COLOR0
		{
			float4 pixel = tex2D(scene, uv);
			float2 x = float2(glow.z, 0.0);
			float2 y = float2(0.0, glow.w);
			float3 bloom = (highlight(tex2D(scene, uv + x).rgb) +
				highlight(tex2D(scene, uv - x).rgb) +
				highlight(tex2D(scene, uv + y).rgb) +
				highlight(tex2D(scene, uv - y).rgb)) * 0.25f;
			pixel.rgb = pixel.rgb * exposure.x + bloom * (glow.y * 0.25f);
			float luminance = dot(pixel.rgb, float3(0.2126f, 0.7152f, 0.0722f));
			float3 tint = lerp(darkTint.rgb, mediumTint.rgb, saturate(luminance * 2.0f));
			tint = lerp(tint, lightTint.rgb, saturate((luminance - 0.40f) * 2.2f));
			pixel.rgb = saturate((pixel.rgb - 0.5f) * contrast.x + 0.5f + brightness.x) * tint;
			return float4(pixel.rgb, 1.0f);
		}
	)";

	static bool EnsureRenderTarget(IDirect3DDevice9* device)
	{
		if (!Game::Sys_IsDatabaseReady() || !FastFiles::Ready() || !Game::DB_IsZoneLoaded("zw3_lobby"))
		{
			return false;
		}

		if (!isTexturePackReady.load(std::memory_order_acquire) || !roomImage)
		{
			return false;
		}

		if (!TryLoadD3DX())
		{
			isStartupLoading.store(false, std::memory_order_release);
			return false;
		}

		const auto width = static_cast<unsigned int>(std::clamp(Renderer::Width(), 640, 3840));
		const auto height = static_cast<unsigned int>(std::clamp(Renderer::Height(), 360, 2160));

		if (width != textureWidth || height != textureHeight)
		{
			isFrameReady.store(false, std::memory_order_release);
			ReleaseDepth();

			textureWidth = width;
			textureHeight = height;

			roomImage->width = static_cast<unsigned short>(width);
			roomImage->height = static_cast<unsigned short>(height);
		}

		if (!roomDepth)
		{
			IDirect3DTexture9* target = nullptr;

			if (FAILED(device->CreateTexture(textureWidth, textureHeight, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &target, nullptr)))
			{
				return false;
			}

			if (FAILED(device->CreateTexture(textureWidth, textureHeight, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &sceneTexture, nullptr)))
			{
				target->Release();
				return false;
			}

			bool hasDepth = false;

			for (const D3DFORMAT format : { D3DFMT_D24S8, D3DFMT_D24X8, D3DFMT_D16 })
			{
				if (SUCCEEDED(device->CreateDepthStencilSurface(textureWidth, textureHeight, format, D3DMULTISAMPLE_NONE, 0, TRUE, &roomDepth, nullptr)))
				{
					hasDepth = true;
					break;
				}
			}

			if (!hasDepth)
			{
				ReleaseObject(sceneTexture);
				target->Release();
				return false;
			}

			ReleaseObject(roomImage->texture.map);
			roomImage->texture.map = target;
			roomImage->cardMemory = {};
		}

		if (!savedState)
		{
			device->CreateStateBlock(D3DSBT_ALL, &savedState);
		}

		if (hasRenderResources)
		{
			return roomImage->texture.map != nullptr;
		}

		if (pendingTextureNames.empty())
		{
			std::unordered_set<std::string> seen;

			QueueGroupTextures(roomGroups, seen);
			QueueGroupTextures(doorLeftGroups, seen);
			QueueGroupTextures(doorRightGroups, seen);
			QueueGroupTextures(propGroups, seen);

			for (const auto& actor : actorMeshes)
			{
				QueueGroupTextures(actor.groups, seen);
			}

			if (seen.emplace("zombie_eye_flare.dds").second)
			{
				pendingTextureNames.push_back("zombie_eye_flare.dds");
			}
		}

		const DWORD uploadStart = timeGetTime();
		const bool isPreparing = isStartupLoading.load(std::memory_order_acquire);

		unsigned int uploadLimit = 2;
		DWORD uploadBudgetMs = 2;

		if (isPreparing)
		{
			uploadLimit = 64;
			uploadBudgetMs = 12;
		}

		unsigned int uploaded = 0;

		while (nextTexture < pendingTextureNames.size() && uploaded < uploadLimit && (uploaded == 0 || timeGetTime() - uploadStart < uploadBudgetMs))
		{
			++uploaded;

			const std::string& name = pendingTextureNames[nextTexture];
			++nextTexture;

			const std::string assetName = "lobby/textures/" + name;
			std::string fallback;
			auto bytes = FindPackedTexture(assetName);
			if (bytes.empty())
			{
				fallback = ReadLobbyAsset(assetName, "zw3/core/lobby/textures/" + name);
				bytes = fallback;
			}
			IDirect3DTexture9* texture = nullptr;

			if (!bytes.empty())
			{
				createTexture(device, bytes.data(), static_cast<UINT>(bytes.size()), &texture);
			}

			loadedTextures.emplace(name, texture);
		}

		if (nextTexture < pendingTextureNames.size())
		{
			return false;
		}

		BindGroupTextures(roomGroups);
		BindGroupTextures(doorLeftGroups);
		BindGroupTextures(doorRightGroups);
		BindGroupTextures(propGroups);

		for (auto& actor : actorMeshes)
		{
			BindGroupTextures(actor.groups);
		}

		EnsureRoomVertexBuffer(device);

		if (!propVertexBuffer && !propVertices.empty())
		{
			propVertexBuffer = CreateFilledVertexBuffer(device, propVertices.data(), propVertices.size() * sizeof(LobbyVertex));
		}

		if (!propIndexBuffer && !propIndices.empty())
		{
			const auto bytes = static_cast<UINT>(propIndices.size() * sizeof(std::uint32_t));

			if (SUCCEEDED(device->CreateIndexBuffer(bytes, D3DUSAGE_WRITEONLY, D3DFMT_INDEX32, D3DPOOL_DEFAULT, &propIndexBuffer, nullptr)))
			{
				void* destination = nullptr;

				if (SUCCEEDED(propIndexBuffer->Lock(0, 0, &destination, 0)))
				{
					std::memcpy(destination, propIndices.data(), bytes);
					propIndexBuffer->Unlock();
				}
				else
				{
					ReleaseObject(propIndexBuffer);
				}
			}
		}

		if (!propIndices.empty() && (!propVertexBuffer || !propIndexBuffer))
		{
			return false;
		}

		for (std::size_t i = 0; i < roomLightmaps.size(); ++i)
		{
			if (roomLightmaps[i])
			{
				continue;
			}

			const std::string name = std::format("_lightmap{}_secondary.dds", i);
			const std::string bytes = ReadLobbyAsset("lobby/lightmaps/" + name, "zw3/core/lobby/lightmaps/" + name);

			if (!bytes.empty())
			{
				createTexture(device, bytes.data(), static_cast<UINT>(bytes.size()), &roomLightmaps[i]);
			}
		}

		filmShader = CompilePixelShader(device, filmSource);
		lightmapFilmShader = CompilePixelShader(device, lightmapFilmSource);
		visionShader = CompilePixelShader(device, visionSource);

		hasRenderResources = true;
		return true;
	}

	static const std::vector<LobbyVertex>& SampleActor(LobbyActorMesh& actor, const unsigned int clip, const DWORD now, const DWORD phaseOffset)
	{
		if (actor.frames.size() < 2)
		{
			return actor.frames.front();
		}

		const UINT first = actor.clipFirst[clip];
		const UINT length = actor.clipLength[clip];

		if (length < 2)
		{
			return actor.frames[first];
		}

		const bool isZombie = &actor == &actorMeshes[zombieActor];
		const bool isFireClip = clip == clipAction || clip == clipRifleFire || clip == clipShotgunFire;

		unsigned int frameTime = 240;

		if (clip == clipDeath || clip == clipPoint)
		{
			frameTime = 140;
		}
		else if (isFireClip && isZombie)
		{
			frameTime = 130;
		}
		else if (isFireClip)
		{
			frameTime = 85;
		}
		else if (clip == clipWalk)
		{
			frameTime = 250;
		}
		else if (isZombie)
		{
			frameTime = 190;
		}

		const bool isOneShot = clip == clipDeath || clip >= clipPoint || (!isZombie && isFireClip);

		unsigned int duration = actor.clipDurationMs[clip];

		if (!duration)
		{
			duration = frameTime * (length - 1);
		}

		const unsigned int elapsed = now + phaseOffset;

		unsigned int clipTime = elapsed % duration;

		if (isOneShot)
		{
			clipTime = std::min(elapsed, duration);
		}

		const float phase = static_cast<float>(clipTime) / static_cast<float>(duration) * static_cast<float>(length - 1);
		const unsigned int frameA = static_cast<unsigned int>(phase) % length;

		unsigned int frameB = (frameA + 1) % length;

		if (isOneShot)
		{
			frameB = std::min(frameA + 1, length - 1);
		}

		const float blend = phase - static_cast<float>(static_cast<unsigned int>(phase));
		const auto& a = actor.frames[first + frameA];
		const auto& b = actor.frames[first + frameB];

		for (std::size_t vertex = 0; vertex < a.size(); ++vertex)
		{
			auto& output = actor.interpolated[vertex];
			output = a[vertex];
			output.x += (b[vertex].x - a[vertex].x) * blend;
			output.y += (b[vertex].y - a[vertex].y) * blend;
			output.z += (b[vertex].z - a[vertex].z) * blend;
		}

		return actor.interpolated;
	}

	static float PathLength(const std::vector<LobbyPathPoint>& path)
	{
		float total = 0.0f;

		for (std::size_t node = 1; node < path.size(); ++node)
		{
			const float dx = path[node].x - path[node - 1].x;
			const float dy = path[node].y - path[node - 1].y;
			const float dz = path[node].z - path[node - 1].z;
			total += std::sqrt(dx * dx + dy * dy + dz * dz);
		}

		return total;
	}

	static LobbyPathPoint PointAlongPath(const std::vector<LobbyPathPoint>& path, float distance, const float total)
	{
		distance = std::clamp(distance, 0.0f, total);

		for (std::size_t node = 1; node < path.size(); ++node)
		{
			const auto& a = path[node - 1];
			const auto& b = path[node];

			const float dx = b.x - a.x;
			const float dy = b.y - a.y;
			const float dz = b.z - a.z;
			const float length = std::sqrt(dx * dx + dy * dy + dz * dz);

			if (distance > length && node + 1 < path.size())
			{
				distance -= length;
				continue;
			}

			const float t = std::clamp(distance / length, 0.0f, 1.0f);
			return { a.x + dx * t, a.y + dy * t, a.z + dz * t };
		}

		return path.back();
	}

	static void SampleZombiePath(const std::size_t index, const float progress, LobbyPathPoint& location, float& facing)
	{
		const bool isFromLeft = index % 2 == 0;
		const std::vector<LobbyPathPoint>* path = &rightZombiePath;

		if (isFromLeft)
		{
			path = &leftZombiePath;
		}

		const float total = PathLength(*path);

		location = PointAlongPath(*path, progress * total, total);

		const LobbyPathPoint before = PointAlongPath(*path, progress * total - 35.0f, total);
		const LobbyPathPoint after = PointAlongPath(*path, progress * total + 35.0f, total);

		facing = std::atan2(after.y - before.y, after.x - before.x);

		const float lateral = (static_cast<float>(static_cast<int>(index / 2) % 4) - 1.5f) * 4.0f;

		float stairScale = 1.0f;

		if (progress > 0.86f)
		{
			stairScale = std::max(0.0f, 1.0f - (progress - 0.86f) / 0.12f);
		}

		const float perpendicularX = -std::sin(facing);
		const float perpendicularY = std::cos(facing);

		location.x += perpendicularX * lateral * stairScale;
		location.y += perpendicularY * lateral * stairScale;

		location.z = std::clamp(84.0f + (location.y + 1098.0f) * 0.5f, 80.0f, 248.0f);
	}

	static void RenderRoom(IDirect3DDevice9* device)
	{
		if (Renderer::IsDeviceRecoveryActive() || !device)
		{
			return;
		}

		std::shared_ptr<const SceneSnapshot> remoteScene;
		std::shared_ptr<const SceneSnapshot> priorScene;
		DWORD remoteAge = 0;
		bool directScene = false;
		if (Party::IsLobbySceneClient())
		{
			std::lock_guard lock(sceneSyncMutex);
			remoteScene = receivedScene;
			directScene = isDirectScene;
			priorScene = previousScene;
			const DWORD hostNow = timeGetTime() + static_cast<DWORD>(sceneClockOffset.load());
			remoteAge = receivedScene && hasSceneClock
				? static_cast<DWORD>(std::clamp(static_cast<int>(hostNow - receivedScene->now), 0, 100))
				: std::min<DWORD>(timeGetTime() - receivedSceneAt, 100u);
		}
		const bool isInTransition = directScene && remoteScene ? remoteScene->transitioning : isTransitionActive.load(std::memory_order_acquire);
		const bool isBootPreview = isStartupLoading.load(std::memory_order_acquire) && Game::CL_GetLocalClientConnectionState(0) < Game::CA_CONNECTING;
		const DWORD transitionElapsed = directScene && remoteScene ? remoteScene->transitionElapsed : timeGetTime() - transitionStartTime.load(std::memory_order_acquire);

		if (isInTransition && transitionElapsed >= LobbyTransition::whiteEndMs)
		{
			const auto intensity = static_cast<unsigned int>((1.0f - LobbyTransition::BlackOpacity(transitionElapsed)) * 255.0f);
			device->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(intensity, intensity, intensity), 1.0f, 0);
			return;
		}

		if (!hasAssets.load(std::memory_order_acquire) || roomVertices.empty() || !roomImage)
		{
			return;
		}

		if (!EnsureRenderTarget(device))
		{
			return;
		}

		if (!isLobbyVisible.load(std::memory_order_acquire) && !isInTransition && !isBootPreview && isFrameReady.load(std::memory_order_acquire))
		{
			return;
		}

		IDirect3DSurface9* target = nullptr;
		IDirect3DSurface9* sceneTarget = nullptr;
		IDirect3DSurface9* oldTarget = nullptr;
		IDirect3DSurface9* oldDepth = nullptr;
		D3DVIEWPORT9 oldViewport{};

		const bool isReady = SUCCEEDED(roomImage->texture.map->GetSurfaceLevel(0, &target))
			&& sceneTexture
			&& SUCCEEDED(sceneTexture->GetSurfaceLevel(0, &sceneTarget))
			&& SUCCEEDED(device->GetRenderTarget(0, &oldTarget))
			&& SUCCEEDED(device->GetDepthStencilSurface(&oldDepth))
			&& SUCCEEDED(device->GetViewport(&oldViewport));

		if (!isReady)
		{
			isFrameReady.store(false, std::memory_order_release);
			ReleaseObject(oldDepth);
			ReleaseObject(oldTarget);
			ReleaseObject(target);
			ReleaseObject(sceneTarget);
			return;
		}

		if (!savedState)
		{
			device->CreateStateBlock(D3DSBT_ALL, &savedState);
		}

		if (!savedState || FAILED(savedState->Capture()))
		{
			ReleaseObject(oldDepth);
			ReleaseObject(oldTarget);
			ReleaseObject(target);
			ReleaseObject(sceneTarget);
			isFrameReady.store(false, std::memory_order_release);
			return;
		}

		bool isRendered = false;
		float whiteOpacity = 0.0f;

		if (SUCCEEDED(device->SetRenderTarget(0, sceneTarget)) && SUCCEEDED(device->SetDepthStencilSurface(roomDepth)))
		{
			D3DVIEWPORT9 viewport{ 0, 0, textureWidth, textureHeight, 0.0f, 1.0f };
			device->SetViewport(&viewport);
			device->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_ARGB(255, 6, 7, 9), 1.0f, 0);

			const DirectX::XMMATRIX world = DirectX::XMMatrixIdentity();

			static const DWORD startTime = timeGetTime();
			const DWORD now = directScene && remoteScene ? remoteScene->now : remoteScene ? (hasSceneClock ? timeGetTime() + static_cast<DWORD>(sceneClockOffset.load())
				: remoteScene->now + remoteAge) : timeGetTime();
			const float phase = static_cast<float>(now - (remoteScene ? remoteScene->phaseOrigin : startTime)) * 0.00012f;

			static DirectX::XMFLOAT3 currentBaseEye = { 0.0f, -1450.0f, 192.0f };
			static DirectX::XMFLOAT3 currentBaseTarget = { 0.0f, -600.0f, 182.0f };
			static DWORD lastCameraTime = 0;

			if (!lastCameraTime)
			{
				lastCameraTime = now;
			}

			const float deltaSeconds = std::clamp(static_cast<float>(now - lastCameraTime) / 1000.0f, 0.0f, 0.1f);
			lastCameraTime = now;

			DirectX::XMFLOAT3 desiredBaseEye = { 0.0f, -1450.0f, 192.0f };
			DirectX::XMFLOAT3 desiredBaseTarget = { 0.0f, -600.0f, 182.0f };

			static unsigned int cameraSession = 0;
			const unsigned int session = lobbySession.load(std::memory_order_acquire);

			const unsigned int cameraReset = cameraResetSession.load(std::memory_order_acquire);

			if (cameraSession != cameraReset)
			{
				currentBaseEye = desiredBaseEye;
				currentBaseTarget = desiredBaseTarget;
				cameraSession = cameraReset;
			}

			if (shouldUseCloseCamera.load(std::memory_order_acquire) && !isInTransition)
			{
				desiredBaseEye = { 0.0f, -1180.0f, 188.0f };
				desiredBaseTarget = { 0.0f, -550.0f, 168.0f };
			}

			const float lerpFactor = 1.0f - std::exp(-deltaSeconds * 2.0f);

			currentBaseEye.x += (desiredBaseEye.x - currentBaseEye.x) * lerpFactor;
			currentBaseEye.y += (desiredBaseEye.y - currentBaseEye.y) * lerpFactor;
			currentBaseEye.z += (desiredBaseEye.z - currentBaseEye.z) * lerpFactor;
			currentBaseTarget.x += (desiredBaseTarget.x - currentBaseTarget.x) * lerpFactor;
			currentBaseTarget.y += (desiredBaseTarget.y - currentBaseTarget.y) * lerpFactor;
			currentBaseTarget.z += (desiredBaseTarget.z - currentBaseTarget.z) * lerpFactor;
			if (remoteScene)
			{
				currentBaseEye = { remoteScene->baseEye.x, remoteScene->baseEye.y, remoteScene->baseEye.z };
				currentBaseTarget = { remoteScene->baseTarget.x, remoteScene->baseTarget.y, remoteScene->baseTarget.z };
			}

			DirectX::XMFLOAT3 eye{};
			DirectX::XMFLOAT3 at{};
			float doorAngle = 0.0f;

			if (isInTransition)
			{
				const DWORD elapsed = transitionElapsed;

				const float doorProgress = LobbyTransition::Smooth(LobbyTransition::Progress(elapsed, LobbyTransition::doorStartMs, LobbyTransition::doorEndMs));
				doorAngle = doorProgress * DirectX::XMConvertToRadians(102.0f);

				const float flyProgress = LobbyTransition::CameraProgress(elapsed);
				const float easeFly = LobbyTransition::Smooth(flyProgress);

				const DirectX::XMFLOAT3 flyEndEye = { 0.0f, -400.0f, 140.0f };
				const DirectX::XMFLOAT3 flyEndTarget = { 0.0f, -100.0f, 140.0f };

				const float panScale = std::max(0.0f, 1.0f - flyProgress * 2.0f);

				eye.x = currentBaseEye.x * (1.0f - easeFly) + flyEndEye.x * easeFly + std::sin(phase) * cameraPan.x * panScale;
				eye.y = currentBaseEye.y + easeFly * (flyEndEye.y - currentBaseEye.y);
				eye.z = currentBaseEye.z + easeFly * (flyEndEye.z - currentBaseEye.z);

				at.x = currentBaseTarget.x * (1.0f - easeFly) + flyEndTarget.x * easeFly;
				at.y = currentBaseTarget.y + easeFly * (flyEndTarget.y - currentBaseTarget.y);
				at.z = currentBaseTarget.z + easeFly * (flyEndTarget.z - currentBaseTarget.z);

				whiteOpacity = LobbyTransition::WhiteOpacity(elapsed);
			}
			else
			{
				eye = { currentBaseEye.x + std::sin(phase) * cameraPan.x, currentBaseEye.y + std::cos(phase * 0.7f) * cameraPan.y, currentBaseEye.z };
				at = currentBaseTarget;
			}

			const DirectX::XMMATRIX view = DirectX::XMMatrixLookAtRH(DirectX::XMLoadFloat3(&eye), DirectX::XMLoadFloat3(&at), DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f));
			const DirectX::XMMATRIX projection = DirectX::XMMatrixPerspectiveFovRH(DirectX::XMConvertToRadians(58.0f), static_cast<float>(textureWidth) / static_cast<float>(textureHeight), 16.0f, 5000.0f);

			DirectX::XMFLOAT4X4 viewFloats;
			DirectX::XMStoreFloat4x4(&viewFloats, view);

			SetTransform(device, D3DTS_VIEW, view);
			SetTransform(device, D3DTS_PROJECTION, projection);
			device->SetVertexShader(nullptr);
			device->SetPixelShader(nullptr);

			SetTransform(device, D3DTS_WORLD, world);
			device->SetFVF(vertexFormat);
			device->SetRenderState(D3DRS_LIGHTING, FALSE);
			device->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);

			for (DWORD sampler = 0; sampler < 3; ++sampler)
			{
				device->SetSamplerState(sampler, D3DSAMP_SRGBTEXTURE, FALSE);
			}

			device->SetRenderState(D3DRS_ZENABLE, TRUE);
			device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
			device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
			device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
			device->SetRenderState(D3DRS_FOGENABLE, FALSE);
			device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
			device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
			device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_ANISOTROPIC);
			device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
			device->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);
			device->SetSamplerState(0, D3DSAMP_MAXANISOTROPY, 16);
			device->SetSamplerState(1, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
			device->SetSamplerState(1, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
			device->SetSamplerState(1, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
			device->SetSamplerState(1, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
			device->SetSamplerState(1, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);
			device->SetTextureStageState(1, D3DTSS_TEXCOORDINDEX, 1);
			device->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
			device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
			device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
			device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
			device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
			device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
			device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
			device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
			device->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATER);
			device->SetRenderState(D3DRS_ALPHAREF, 24);

			const auto drawGroups = [device, &eye](const std::vector<LobbyDrawGroup>& groups, const auto& vertices, const bool isBlendPass, const bool isRoomPass, IDirect3DVertexBuffer9* staticBuffer, const int selectedWeapon) -> bool
			{
				using VertexType = typename std::decay_t<decltype(vertices)>::value_type;
				constexpr UINT stride = sizeof(VertexType);

				bool isUsingBuffer = false;

				if (staticBuffer)
				{
					isUsingBuffer = SUCCEEDED(device->SetStreamSource(0, staticBuffer, 0, stride));
				}

				if (isRoomPass)
				{
					device->SetFVF(roomVertexFormat);
				}
				else
				{
					device->SetFVF(vertexFormat);
				}

				for (const auto& group : groups)
				{
					const bool isOtherPass = group.isBlended != isBlendPass;
					const bool isOtherWeapon = group.weaponIndex >= 0 && group.weaponIndex != selectedWeapon;

					if (isOtherPass || isOtherWeapon)
					{
						continue;
					}

					if (group.hasCell)
					{
						const float dx = (static_cast<float>(group.cellX) + 0.5f) * 512.0f - eye.x;
						const float dy = (static_cast<float>(group.cellY) + 0.5f) * 512.0f - eye.y;

						if (dx * dx + dy * dy > 1400.0f * 1400.0f)
						{
							continue;
						}
					}

					device->SetRenderState(D3DRS_SRGBWRITEENABLE, static_cast<DWORD>(group.shouldWriteGamma));

					const bool isEyeGlow = !group.textureName.empty()
						&& (group.textureName.find("zombie_eye") != std::string::npos || group.textureName.find("eye_glow") != std::string::npos);

					if (isEyeGlow)
					{
						device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
						device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
						device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
						device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
						device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
					}
					else
					{
						bool shouldWriteDepth = !group.isBlended;

						if (isRoomPass || group.hasCell)
						{
							shouldWriteDepth = group.shouldWriteDepth;
						}

						device->SetRenderState(D3DRS_ALPHABLENDENABLE, static_cast<DWORD>(group.isBlended));
						device->SetRenderState(D3DRS_SRCBLEND, group.srcBlend);
						device->SetRenderState(D3DRS_DESTBLEND, group.dstBlend);
						device->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
						device->SetRenderState(D3DRS_ZWRITEENABLE, static_cast<DWORD>(shouldWriteDepth));
					}

					if (group.isBlended)
					{
						const float bias = -0.00003f;
						const float slopeBias = -0.5f;
						device->SetRenderState(D3DRS_DEPTHBIAS, std::bit_cast<DWORD>(bias));
						device->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS, std::bit_cast<DWORD>(slopeBias));
					}
					else
					{
						device->SetRenderState(D3DRS_DEPTHBIAS, 0);
						device->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS, 0);
					}

					const bool isLightmapped = isRoomPass
						&& group.lightmapIndex >= 0
						&& group.lightmapIndex < static_cast<int>(roomLightmaps.size())
						&& roomLightmaps[group.lightmapIndex];

					IDirect3DTexture9* lightmap = nullptr;
					IDirect3DTexture9* normal = nullptr;

					if (isLightmapped)
					{
						lightmap = roomLightmaps[group.lightmapIndex];
						normal = group.normalTexture;
					}

					device->SetTexture(1, lightmap);
					device->SetTexture(2, normal);

					const bool isWeapon = group.weaponIndex >= 0;
					const bool isAlphaCutout = !isWeapon && (group.isAlphaTested || group.isBlended);
					const bool isHardwareCutout = !isEyeGlow && isAlphaCutout && group.alphaThreshold >= 0.0f;

					device->SetRenderState(D3DRS_ALPHATESTENABLE, static_cast<DWORD>(isHardwareCutout));
					device->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL);
					device->SetRenderState(D3DRS_ALPHAREF, static_cast<DWORD>(std::clamp(group.alphaThreshold * 255.0f, 0.0f, 255.0f)));

					bool isSquared = group.shouldWriteGamma;

					if (isLightmapped)
					{
						isSquared = group.normalTexture != nullptr;
					}

					float threshold = group.alphaThreshold;

					if (isEyeGlow)
					{
						threshold = -4.0f;
					}

					const float materialFlags[4] = { static_cast<float>(isSquared), static_cast<float>(isAlphaCutout), threshold, static_cast<float>(group.isMultiplicative) };
					device->SetPixelShaderConstantF(0, materialFlags, 1);

					device->SetSamplerState(2, D3DSAMP_MINFILTER, D3DTEXF_ANISOTROPIC);
					device->SetSamplerState(2, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
					device->SetSamplerState(2, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);
					device->SetSamplerState(2, D3DSAMP_MAXANISOTROPY, 16);
					device->SetSamplerState(2, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
					device->SetSamplerState(2, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);

					IDirect3DPixelShader9* shader = nullptr;

					if (isLightmapped)
					{
						shader = lightmapFilmShader;
					}
					else if (group.texture)
					{
						shader = filmShader;
					}

					device->SetPixelShader(shader);

					const bool isFixedLightmap = isLightmapped && !lightmapFilmShader;

					if (isFixedLightmap)
					{
						device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_MODULATE);
						device->SetTextureStageState(1, D3DTSS_COLORARG1, D3DTA_TEXTURE);
						device->SetTextureStageState(1, D3DTSS_COLORARG2, D3DTA_CURRENT);
					}
					else
					{
						device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
					}

					if (group.texture)
					{
						device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
					}
					else
					{
						device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
					}

					device->SetTexture(0, group.texture);

					if (!group.texture)
					{
						device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
						device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
					}

					HRESULT result = S_OK;

					if (group.indexCount)
					{
						if (!isUsingBuffer || !propIndexBuffer || FAILED(device->SetIndices(propIndexBuffer)))
						{
							return false;
						}

						result = device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, group.firstVertex, 0, group.vertexCount, group.firstIndex, group.indexCount / 3);
					}
					else if (isUsingBuffer)
					{
						result = device->DrawPrimitive(D3DPT_TRIANGLELIST, group.firstVertex, group.vertexCount / 3);
					}
					else
					{
						result = device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, group.vertexCount / 3, vertices.data() + group.firstVertex, stride);
					}

					if (!group.texture)
					{
						device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
						device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
					}

					if (FAILED(result))
					{
						return false;
					}
				}

				return true;
			};

			EnsureRoomVertexBuffer(device);

			isRendered = drawGroups(roomGroups, roomVertices, false, true, roomVertexBuffer, -1);

			if (propVertexBuffer)
			{
				isRendered = isRendered && drawGroups(propGroups, propVertices, false, false, propVertexBuffer, -1);
			}

			isRendered = isRendered && drawGroups(roomGroups, roomVertices, true, true, roomVertexBuffer, -1);

			if (propVertexBuffer)
			{
				isRendered = isRendered && drawGroups(propGroups, propVertices, true, false, propVertexBuffer, -1);
			}

			const DirectX::XMMATRIX leftDoor = DirectX::XMMatrixMultiply(DirectX::XMMatrixMultiply(DirectX::XMMatrixTranslation(60.0f, 510.0f, 0.0f), DirectX::XMMatrixRotationZ(doorAngle)), DirectX::XMMatrixTranslation(-60.0f, -510.0f, 0.0f));
			SetTransform(device, D3DTS_WORLD, leftDoor);
			drawGroups(doorLeftGroups, doorLeftVertices, false, true, nullptr, -1);
			drawGroups(doorLeftGroups, doorLeftVertices, true, true, nullptr, -1);

			const DirectX::XMMATRIX rightDoor = DirectX::XMMatrixMultiply(DirectX::XMMatrixMultiply(DirectX::XMMatrixTranslation(-60.0f, 510.0f, 0.0f), DirectX::XMMatrixRotationZ(-doorAngle)), DirectX::XMMatrixTranslation(60.0f, -510.0f, 0.0f));
			SetTransform(device, D3DTS_WORLD, rightDoor);
			drawGroups(doorRightGroups, doorRightVertices, false, true, nullptr, -1);
			drawGroups(doorRightGroups, doorRightVertices, true, true, nullptr, -1);

			SetTransform(device, D3DTS_WORLD, world);

			if (isRendered)
			{
				const int count = remoteScene ? static_cast<int>(std::ranges::count_if(remoteScene->survivors, [](const SurvivorState& s) { return s.isPresent; })) : lobbyCharacterCount.load(std::memory_order_acquire);

				auto& zombieMesh = actorMeshes[zombieActor];




				constexpr std::size_t zombieSlots = 32;
				constexpr DWORD zombieFallDuration = 650;
				constexpr DWORD zombieCorpseLifetime = 3200;

				DWORD zombieAttackDuration = zombieMesh.clipDurationMs[clipAttack];

				if (!zombieAttackDuration)
				{
					zombieAttackDuration = 1800;
				}

				static std::array<ZombieVisual, zombieSlots> zombies{};
				static DWORD nextWaveTime = 0;
				static std::size_t spawnCursor = 0;
				static bool areZombiesInitialized = false;
				static unsigned int zombieSession = 0;

				const unsigned int desiredZombies = LobbyCombat::Population(count);

				const auto walkDurationOf = [](const std::size_t index)
				{
					if (index % 3 == 2)
					{
						return 16500.0f;
					}

					return 21000.0f + static_cast<float>(index % 5) * 1100.0f;
				};

				if (!remoteScene && (!areZombiesInitialized || session != zombieSession))
				{
					for (std::size_t i = 0; i < zombies.size(); ++i)
					{
						zombies[i] = ZombieVisual{};
						zombies[i].variant = static_cast<int>(i % 3 == 2);
						zombies[i].walkDuration = walkDurationOf(i);
						zombies[i].spawnTime = now;
						zombies[i].nextSpawnTime = now;
					}

					areZombiesInitialized = true;
					zombieSession = session;
					nextWaveTime = now;
					spawnCursor = 0;
				}

				if (remoteScene)
				{
					zombies = remoteScene->zombies;
					if (priorScene && !directScene)
					{
						const auto span = std::max<DWORD>(1u, remoteScene->now - priorScene->now);
						const float blend = 1.0f + std::min(1.0f, static_cast<float>(remoteAge) / static_cast<float>(span));
						for (std::size_t i = 0; i < zombies.size(); ++i)
						{
							auto& visual = zombies[i];
							const auto& prior = priorScene->zombies[i];
							if (!visual.isVisible || visual.isDying || !prior.isVisible || prior.isDying || visual.spawnTime != prior.spawnTime) continue;
							if (span > 150u) continue;
							visual.location.x = std::lerp(prior.location.x, visual.location.x, blend);
							visual.location.y = std::lerp(prior.location.y, visual.location.y, blend);
							visual.location.z = std::lerp(prior.location.z, visual.location.z, blend);
						}
					}
				}
				for (std::size_t index = 0; !remoteScene && index < zombies.size(); ++index)
				{
					auto& visual = zombies[index];

					if (!visual.isDying)
					{
						continue;
					}

					if (now - visual.deathTime >= zombieCorpseLifetime && static_cast<int>(now - visual.nextSpawnTime) >= 0)
					{
						visual.isDying = false;
						visual.isVisible = false;
						visual.deathPose.clear();
						visual.isChasing = false;
						visual.routeProgress = 0.0f;
						visual.isAdmitted = false;
						visual.targetSurvivor = -1;
						visual.isAttacking = false;
						visual.nextAttackTime = 0;
						visual.spawnTime = now;
						visual.variant = static_cast<int>(index % 3 == 2);
						visual.walkDuration = walkDurationOf(index);
					}
				}

				const auto visibleZombies = static_cast<unsigned int>(std::ranges::count_if(zombies, [](const ZombieVisual& visual)
				{
					return visual.isVisible;
				}));

				const bool canSpawnZombie = !remoteScene && hasZombieWaveStarted.load(std::memory_order_acquire)
					&& isLobbyVisible.load(std::memory_order_acquire)
					&& !isBootPreview
					&& !isInTransition
					&& visibleZombies < desiredZombies
					&& static_cast<int>(now - nextWaveTime) >= 0;

				if (canSpawnZombie)
				{
					for (std::size_t offset = 0; offset < zombies.size(); ++offset)
					{
						const std::size_t index = (spawnCursor + offset) % zombies.size();
						auto& visual = zombies[index];

						if (visual.isVisible || visual.isDying)
						{
							continue;
						}

						visual = ZombieVisual{};
						visual.variant = static_cast<int>(index % 3 == 2);
						visual.walkDuration = walkDurationOf(index);
						visual.spawnTime = now;
						visual.nextSpawnTime = now;

						visual.routeProgress = 0.28f;
						visual.isVisible = true;

						spawnCursor = (index + 1) % zombies.size();
						nextWaveTime = now + LobbyCombat::SpawnInterval(count);
						break;
					}
				}

				const auto sampleZombie = [&zombieMesh, now](const ZombieVisual& visual) -> const std::vector<LobbyVertex>&
				{
					if (visual.isAttacking && zombieMesh.clipLength[clipAttack] > 1)
					{
						return SampleActor(zombieMesh, clipAttack, now, 0u - visual.attackTime);
					}

					unsigned int clip = clipIdle;

					if (visual.variant == 1)
					{
						clip = clipAction;
					}

					return SampleActor(zombieMesh, clip, now, static_cast<DWORD>(visual.animationTime) - now);
				};





				struct MuzzleFlare
				{
					DirectX::XMFLOAT3 position;
					float radius;
					float alpha;
				};

				static std::array<SurvivorState, 4> survivors{};
				constexpr float balconyY = -765.0f;
				static std::mt19937 weaponRandom(timeGetTime() ^ GetCurrentProcessId());
				static std::uniform_int_distribution<int> weaponChoice(0, 3);
				static std::array<TeleportBurst, 4> teleportBursts{};
				static unsigned int teleportSession = 0;

				if (remoteScene)
				{
					survivors = remoteScene->survivors;
					teleportBursts = remoteScene->teleports;
				}
				if (!remoteScene && teleportSession != session)
				{
					teleportBursts = {};

					for (auto& survivor : survivors)
					{
						survivor.isPresent = false;
					}

					teleportSession = session;
				}

				for (std::size_t slot = 0; !remoteScene && slot < survivors.size(); ++slot)
				{
					auto& survivor = survivors[slot];
					const int model = lobbyCharacterModels[slot].load(std::memory_order_acquire);
					const bool isPresent = slot < static_cast<std::size_t>(std::max(count, 0)) && model >= 0 && model < 4 && !actorMeshes[model].frames.empty();
					const float burstX = (static_cast<float>(slot) - static_cast<float>(count - 1) * 0.5f) * 65.0f;

					if (!isPresent || isInTransition)
					{
						teleportBursts[slot].isActive = false;
					}
					else if (!survivor.isPresent)
					{
						teleportBursts[slot] = { burstX, now, true };
					}
					else if (teleportBursts[slot].isActive)
					{
						teleportBursts[slot].x = burstX;
					}

					if (!isPresent)
					{
						survivor.isInitialized = false;
					}

					survivor.isPresent = isPresent;
					survivor.modelIndex = model;
				}

				const auto slotX = [count](const int slot)
				{
					return (static_cast<float>(slot) - static_cast<float>(count - 1) * 0.5f) * 65.0f;
				};

				std::array<int, 5> roster{};
				roster[0] = count;

				for (std::size_t slot = 0; !remoteScene && slot < survivors.size(); ++slot)
				{
					roster[slot + 1] = -1;

					if (survivors[slot].isPresent)
					{
						roster[slot + 1] = survivors[slot].modelIndex;
					}
				}

				static std::array<int, 5> lastRoster{};
				static bool isRosterReady = false;

				if (!remoteScene && isRosterReady && roster != lastRoster)
				{
					for (auto& survivor : survivors)
					{
						survivor.targetZombieIndex = -1;
						survivor.burstRemaining = 0;
						survivor.lastFireTime = 0;
						survivor.meleeTime = 0;
						survivor.isMeleePending = false;
						survivor.recoveryUntil = 0;
					}

					for (auto& visual : zombies)
					{
						if (visual.isVisible && !visual.isDying)
						{
							LobbyCombat::ReleaseEncounter(visual);
						}
					}
				}

				lastRoster = roster;
				isRosterReady = true;

				const auto killZombie = [&](ZombieVisual& visual, const bool isMeleeHit)
				{
					if (visual.isDying)
					{
						return;
					}

					if (!zombieMesh.frames.empty())
					{
						visual.deathPose = sampleZombie(visual);
					}

					visual.fallDirection = 1.0f;

					if (isMeleeHit)
					{
						visual.fallDirection = -1.0f;
					}

					visual.isDying = true;
					visual.deathTime = now;
					visual.nextSpawnTime = now + zombieCorpseLifetime;

					if (isMeleeHit && visual.targetSurvivor >= 0 && visual.targetSurvivor < count)
					{
						survivors[visual.targetSurvivor].recoveryUntil = now + LobbyCombat::recoveryMs;
					}
				};

				float transitionPace = 1.0f;

				if (isInTransition)
				{
					transitionPace = 1.0f - 0.45f * LobbyTransition::Smooth(std::clamp(static_cast<float>(transitionElapsed) / 600.0f, 0.0f, 1.0f));
				}

				const float movementSeconds = deltaSeconds * transitionPace;

				const auto advanceEncounter = [&](ZombieVisual& visual, const float home, const bool isWaiting)
				{
					const float oldX = visual.location.x;
					const float oldY = visual.location.y;

					float speed = 55.0f;

					if (visual.variant == 1)
					{
						speed = 80.0f;
					}

					const float moved = LobbyCombat::AdvanceApproach(visual, home, movementSeconds * speed, isWaiting);

					if (moved <= 0.001f)
					{
						return;
					}

					visual.animationTime += movementSeconds * 1000.0f;

					const float yaw = std::atan2(visual.location.y - oldY, visual.location.x - oldX);
					float difference = yaw - visual.facing;

					while (difference > DirectX::XM_PI)
					{
						difference -= 2.0f * DirectX::XM_PI;
					}

					while (difference < -DirectX::XM_PI)
					{
						difference += 2.0f * DirectX::XM_PI;
					}

					visual.facing += difference * (1.0f - std::exp(-deltaSeconds * 8.0f));
				};

				const auto claimsOn = [](const int slot)
				{
					return std::ranges::count_if(zombies, [slot](const ZombieVisual& other)
					{
						return other.isVisible && !other.isDying && other.isAdmitted && other.targetSurvivor == slot;
					});
				};

				const auto pickSurvivor = [&](const LobbyPathPoint& from, const bool isCapped)
				{
					float best = std::numeric_limits<float>::max();
					int target = -1;

					for (int slot = 0; slot < count; ++slot)
					{
						if (!survivors[slot].isPresent)
						{
							continue;
						}

						const auto claims = claimsOn(slot);

						if (isCapped && claims >= 2)
						{
							continue;
						}

						const float score = std::hypot(slotX(slot) - from.x, balconyY - from.y) + static_cast<float>(claims) * 120.0f;

						if (score < best)
						{
							best = score;
							target = slot;
						}
					}

					return target;
				};

				const auto admit = [&](ZombieVisual& visual, const int target)
				{
					visual.targetSurvivor = target;
					visual.isAdmitted = target >= 0;

					if (visual.isAdmitted)
					{
						visual.admittedTime = now;
						visual.approachPhase = 0;
						visual.assignedHomeX = slotX(target);
					}
				};

				std::array<std::size_t, zombieSlots> movementOrder{};

				for (std::size_t index = 0; index < movementOrder.size(); ++index)
				{
					movementOrder[index] = index;
				}

				std::ranges::sort(movementOrder, [](const std::size_t a, const std::size_t b)
				{
					return zombies[a].routeProgress > zombies[b].routeProgress;
				});

				for (const std::size_t index : movementOrder)
				{
					if (remoteScene) break;
					auto& visual = zombies[index];

					if (!visual.isVisible || visual.isDying)
					{
						continue;
					}

					const bool hasLostTarget = visual.targetSurvivor >= count || (visual.targetSurvivor >= 0 && !survivors[visual.targetSurvivor].isPresent);

					if (hasLostTarget)
					{
						LobbyCombat::ReleaseEncounter(visual);
					}

					if (!visual.isChasing)
					{
						float progress = std::min(visual.routeProgress + movementSeconds * 1000.0f / visual.walkDuration, 1.0f);

						for (std::size_t leader = 0; leader < zombies.size(); ++leader)
						{
							const auto& other = zombies[leader];

							const bool isLeaderAhead = leader != index
								&& leader % 2 == index % 2
								&& other.isVisible
								&& !other.isDying
								&& !other.isChasing
								&& other.routeProgress > visual.routeProgress;

							if (isLeaderAhead)
							{
								progress = std::max(visual.routeProgress, std::min(progress, other.routeProgress - LobbyCombat::routeSpacing));
							}
						}

						if (!visual.isAdmitted && progress >= LobbyCombat::admissionProgress)
						{
							LobbyPathPoint entry{};
							float entryFacing = 0.0f;
							SampleZombiePath(index, progress, entry, entryFacing);

							admit(visual, pickSurvivor(entry, true));
						}

						if (progress > visual.routeProgress)
						{
							visual.animationTime += movementSeconds * 1000.0f;
						}

						visual.routeProgress = progress;
						SampleZombiePath(index, progress, visual.location, visual.facing);

						if (progress < 1.0f)
						{
							continue;
						}

						visual.isChasing = true;
					}

					if (!visual.isAdmitted)
					{
						admit(visual, pickSurvivor(visual.location, false));
					}

					const int targetSlot = visual.targetSurvivor;

					if (targetSlot < 0)
					{
						visual.isAttacking = false;
						advanceEncounter(visual, visual.location.x, true);
						continue;
					}

					const float targetX = slotX(targetSlot);

					if (targetX != visual.assignedHomeX)
					{
						visual.assignedHomeX = targetX;
						visual.approachPhase = 0;
						visual.isAttacking = false;
					}

					const float dx = targetX - visual.location.x;
					const float dy = balconyY - visual.location.y;
					const float nearest = std::hypot(dx, dy);

					unsigned int ahead = 0;
					bool isWaiting = survivors[targetSlot].recoveryUntil != 0 && static_cast<int>(now - survivors[targetSlot].recoveryUntil) < 0;

					for (std::size_t otherIndex = 0; otherIndex < zombies.size(); ++otherIndex)
					{
						const auto& other = zombies[otherIndex];

						const bool isRival = otherIndex != index
							&& other.isVisible
							&& !other.isDying
							&& other.isAdmitted
							&& other.targetSurvivor == targetSlot;

						if (!isRival)
						{
							continue;
						}

						const bool isFirst = other.isAttacking
							|| LobbyCombat::IsOlderEncounter(other.admittedTime, static_cast<unsigned int>(otherIndex), visual.admittedTime, static_cast<unsigned int>(index));

						if (isFirst)
						{
							isWaiting = true;
							++ahead;
						}
					}

					if (visual.isAttacking && now - visual.attackTime >= zombieAttackDuration)
					{
						visual.isAttacking = false;
					}

					if (!visual.isAttacking)
					{
						float queueOffset = 0.0f;

						if (count == 1 && isWaiting && ahead > 0)
						{
							queueOffset = std::min(112.0f, static_cast<float>((ahead + 1) / 2) * 28.0f);

							if (ahead % 2 == 1)
							{
								queueOffset = -queueOffset;
							}
						}

						advanceEncounter(visual, targetX + queueOffset, isWaiting);
					}

					const bool canAttack = !isInTransition
						&& !isWaiting
						&& visual.approachPhase == 2
						&& nearest <= LobbyCombat::attackDistance
						&& !visual.isAttacking
						&& static_cast<int>(now - visual.nextAttackTime) >= 0;

					if (canAttack)
					{
						visual.isAttacking = true;
						visual.facing = std::atan2(dy, dx);
						visual.attackTime = now;
						visual.nextAttackTime = now + zombieAttackDuration + 450u;
					}
				}

				std::vector<MuzzleFlare> muzzleFlares;

				for (int i = 0; i < count; ++i)
				{
					const int modelIndex = remoteScene ? survivors[i].modelIndex : lobbyCharacterModels[i].load(std::memory_order_acquire);

					if (modelIndex < 0 || modelIndex >= 4)
					{
						continue;
					}

					auto& actor = actorMeshes[modelIndex];

					if (actor.frames.empty())
					{
						continue;
					}

					DWORD meleeDuration = actor.clipDurationMs[clipMelee];

					if (!meleeDuration)
					{
						meleeDuration = 1067;
					}

					const DWORD meleeImpact = meleeDuration * 45u / 100u;

					auto& survivor = survivors[i];
					const float homeX = (static_cast<float>(i) - static_cast<float>(count - 1) * 0.5f) * 65.0f;

					const float y = balconyY;
					const float z = 245.0f;
					bool isBestNearSurvivor = false;
					if (!remoteScene)
					{
					if (!survivor.isInitialized || survivor.session != session)
					{
						survivor.currentX = homeX;
						survivor.homeX = homeX;
						survivor.targetZombieIndex = -1;
						survivor.targetEngageTime = 0;
						survivor.lastFireTime = 0;
						survivor.meleeTime = 0;
						survivor.recoveryUntil = 0;
						survivor.meleeTarget = -1;
						survivor.isMeleePending = false;
						survivor.burstRemaining = 0;
						survivor.nextBurstShotTime = 0;
						survivor.facing = -DirectX::XM_PI * 0.5f;
						survivor.weaponIndex = weaponChoice(weaponRandom);
						survivor.session = session;
						survivor.isInitialized = true;
					}
					else if (survivor.homeX != homeX)
					{
						survivor.currentX = homeX;
						survivor.homeX = homeX;
						survivor.meleeTime = 0;
						survivor.isMeleePending = false;
						survivor.lastFireTime = 0;
						survivor.burstRemaining = 0;
						survivor.targetZombieIndex = -1;
					}



					int bestZombie = -1;
					float bestScore = 9999999.0f;
					LobbyPathPoint bestLocation{};

					const auto isNearAnySurvivor = [&](const LobbyPathPoint& location)
					{
						for (int slot = 0; slot < count; ++slot)
						{
							if (survivors[slot].isPresent && std::hypot(location.x - slotX(slot), location.y - y) <= LobbyCombat::gunSafetyDistance)
							{
								return true;
							}
						}

						return false;
					};

					for (std::size_t zombieIndex = 0; zombieIndex < zombies.size(); ++zombieIndex)
					{
						const auto& zombie = zombies[zombieIndex];

						if (!zombie.isVisible || zombie.isDying || static_cast<int>(now - zombie.nextSpawnTime) < 0)
						{
							continue;
						}

						const float approach = zombie.routeProgress;

						if (approach < 0.55f)
						{
							continue;
						}

						const LobbyPathPoint location = zombie.location;
						const float closeDistance = std::hypot(location.x - survivor.currentX, location.y - y);
						const bool isMeleeLocked = survivor.meleeTime != 0 && now - survivor.meleeTime < meleeDuration;

						if (isMeleeLocked && survivor.meleeTarget != static_cast<int>(zombieIndex))
						{
							continue;
						}

						if (location.z < 224.0f || closeDistance > 420.0f)
						{
							continue;
						}

						if (closeDistance > LobbyCombat::meleeDistance)
						{
							if (!LobbyCombat::IsShotClear(survivor.currentX, y, location.x, location.y) || isNearAnySurvivor(location))
							{
								continue;
							}
						}

						if (closeDistance > LobbyCombat::meleeDistance && survivor.burstRemaining > 0 && survivor.targetZombieIndex != static_cast<int>(zombieIndex))
						{
							continue;
						}

						float sideBias = 1.4f;

						if (homeX * location.x >= 0.0f)
						{
							sideBias = 0.7f;
						}

						const float urgency = 1.05f - approach;
						const float dx = location.x - survivor.currentX;
						const float dy = location.y - y;

						float score = (dx * dx + dy * dy) * sideBias * (urgency * urgency);

						if (closeDistance <= LobbyCombat::meleeDistance)
						{
							score = -100000.0f + closeDistance;
						}

						for (int other = 0; other < count; ++other)
						{
							const bool isOtherShooting = other != i
								&& survivors[other].targetZombieIndex == static_cast<int>(zombieIndex)
								&& survivors[other].burstRemaining > 0;

							if (closeDistance > LobbyCombat::meleeDistance && isOtherShooting)
							{
								score *= 2.8f;
								break;
							}
						}

						if (score < bestScore)
						{
							bestScore = score;
							bestZombie = static_cast<int>(zombieIndex);
							bestLocation = location;
						}
					}

					float targetYaw = -DirectX::XM_PI * 0.5f;

					if (bestZombie >= 0)
					{
						if (survivor.targetZombieIndex != bestZombie)
						{
							survivor.targetZombieIndex = bestZombie;
							survivor.targetEngageTime = now;
						}

						targetYaw = std::atan2(bestLocation.y - y, bestLocation.x - survivor.currentX);
					}
					else
					{
						survivor.targetZombieIndex = -1;

						const float scan = std::sin(static_cast<float>(now) * 0.0012f + static_cast<float>(i) * 1.7f) * 0.08f;
						targetYaw = -DirectX::XM_PI * 0.5f + scan;
					}

					if (survivor.meleeTime != 0 && now - survivor.meleeTime < meleeDuration)
					{
						targetYaw = survivor.meleeFacing;
					}

					float yawDifference = targetYaw - survivor.facing;

					while (yawDifference > DirectX::XM_PI)
					{
						yawDifference -= 2.0f * DirectX::XM_PI;
					}

					while (yawDifference < -DirectX::XM_PI)
					{
						yawDifference += 2.0f * DirectX::XM_PI;
					}

					survivor.facing += yawDifference * (1.0f - std::exp(-deltaSeconds * 5.0f));

					const float homeDifference = survivor.homeX - survivor.currentX;

					if (std::abs(homeDifference) > 1.5f && bestZombie < 0)
					{
						const float step = std::copysign(std::min(std::abs(homeDifference), 0.035f * 33.0f), homeDifference);
						survivor.currentX += step;
						survivor.isWalking = true;
					}
					else
					{
						survivor.currentX = survivor.homeX;
						survivor.isWalking = false;
					}

					if (bestZombie < 0)
					{
						survivor.burstRemaining = 0;
					}

					const bool isAimAligned = bestZombie >= 0 && std::abs(yawDifference) <= 0.12f;

					float bestDistance = 99999.0f;

					if (bestZombie >= 0)
					{
						bestDistance = std::hypot(bestLocation.x - survivor.currentX, bestLocation.y - y);
					}

					isBestNearSurvivor = bestZombie >= 0 && isNearAnySurvivor(bestLocation);

					const bool canShootBest = bestZombie >= 0
						&& bestLocation.z >= 224.0f
						&& bestDistance <= 420.0f
						&& LobbyCombat::IsShotClear(survivor.currentX, y, bestLocation.x, bestLocation.y);

					if (isBestNearSurvivor || !canShootBest)
					{
						survivor.burstRemaining = 0;
					}

					const auto nearbyThreats = static_cast<unsigned int>(std::ranges::count_if(zombies, [&survivor, y](const ZombieVisual& zombie)
					{
						return zombie.isVisible
							&& !zombie.isDying
							&& zombie.location.z >= 224.0f
							&& std::hypot(zombie.location.x - survivor.currentX, zombie.location.y - y) <= 220.0f;
					}));

					if (!isInTransition && survivor.isMeleePending && now - survivor.meleeTime >= meleeImpact)
					{
						auto& victim = zombies[survivor.meleeTarget];

						const bool isHit = victim.isVisible
							&& !victim.isDying
							&& LobbyCombat::InMeleeSweep(victim.location.x - survivor.currentX, victim.location.y - y, survivor.meleeFacing);

						if (isHit)
						{
							killZombie(victim, true);

							unsigned int hits = 1;

							for (auto& nearby : zombies)
							{
								if (hits >= 3)
								{
									break;
								}

								if (!nearby.isVisible || nearby.isDying || nearby.location.z < 224.0f)
								{
									continue;
								}

								if (LobbyCombat::InMeleeSweep(nearby.location.x - survivor.currentX, nearby.location.y - y, survivor.meleeFacing))
								{
									killZombie(nearby, true);
									++hits;
								}
							}
						}

						survivor.isMeleePending = false;
					}

					const bool canMelee = !isInTransition
						&& bestZombie >= 0
						&& !zombies[bestZombie].isDying
						&& bestDistance <= LobbyCombat::meleeDistance
						&& isAimAligned
						&& !survivor.isMeleePending
						&& zombies[bestZombie].isAttacking
						&& now - zombies[bestZombie].attackTime >= 600u
						&& (survivor.meleeTime == 0 || now - survivor.meleeTime >= meleeDuration + 200u);

					if (canMelee)
					{
						survivor.meleeTime = now;
						survivor.meleeFacing = survivor.facing;
						survivor.meleeTarget = bestZombie;
						survivor.isMeleePending = true;
						survivor.lastFireTime = 0;
					}

					const bool isMeleeing = survivor.meleeTime != 0 && now - survivor.meleeTime < meleeDuration;

					const bool canShoot = !isInTransition
						&& canShootBest
						&& isAimAligned
						&& !isBestNearSurvivor
						&& !isMeleeing;

					if (canShoot)
					{
						const bool canStartBurst = now - survivor.targetEngageTime >= 350u
							&& survivor.burstRemaining == 0
							&& now - survivor.lastFireTime >= LobbyCombat::PressureCooldown(count, nearbyThreats) + static_cast<DWORD>(i) * 120u;

						if (canStartBurst)
						{
							survivor.burstRemaining = 3;

							if (survivor.weaponIndex == 3)
							{
								survivor.burstRemaining = 1;
							}
							else if (survivor.weaponIndex == 0)
							{
								survivor.burstRemaining = 2;
							}

							survivor.nextBurstShotTime = now;
						}
					}

					if (canShoot && survivor.burstRemaining > 0 && static_cast<int>(now - survivor.nextBurstShotTime) >= 0)
					{
						--survivor.burstRemaining;
						survivor.lastFireTime = now;

						DWORD shotDelay = 115;

						if (survivor.weaponIndex == 0)
						{
							shotDelay = 160;
						}
						else if (survivor.weaponIndex == 3)
						{
							shotDelay = 400;
						}
						else if (survivor.weaponIndex == 2)
						{
							shotDelay = 100;
						}

						survivor.nextBurstShotTime = now + shotDelay;

						if (survivor.burstRemaining == 0 && bestZombie >= 0)
						{
							killZombie(zombies[bestZombie], false);
							survivor.targetZombieIndex = -1;
						}
					}

					}
					const bool isMeleeing = survivor.meleeTime != 0 && now - survivor.meleeTime < meleeDuration;
					unsigned int fireClip = clipRifleFire;
					unsigned int idleClip = clipRifleIdle;

					if (survivor.weaponIndex == 3)
					{
						fireClip = clipShotgunFire;
					}
					else if (survivor.weaponIndex == 0)
					{
						fireClip = clipAction;
						idleClip = clipIdle;
					}

					unsigned int fireDuration = actor.clipDurationMs[fireClip];

					if (!fireDuration)
					{
						fireDuration = 450;
					}

					const bool computedFiring = !isInTransition
						&& !isMeleeing
						&& !isBestNearSurvivor
						&& survivor.lastFireTime != 0
						&& now - survivor.lastFireTime < fireDuration;
					if (!remoteScene) survivor.isFiring = computedFiring;
					const bool isFiring = survivor.isFiring && !isInTransition && now - survivor.lastFireTime < fireDuration;

					const DirectX::XMMATRIX placement = DirectX::XMMatrixMultiply(DirectX::XMMatrixRotationZ(survivor.facing), DirectX::XMMatrixTranslation(survivor.homeX, y, z));
					SetTransform(device, D3DTS_WORLD, placement);

					unsigned int clip = idleClip;
					DWORD phaseOffset = static_cast<DWORD>(i) * 540u;

					if (isMeleeing)
					{
						if (actor.clipLength[clipMelee] > 1)
						{
							clip = clipMelee;
						}

						phaseOffset = 0u - survivor.meleeTime;
					}
					else if (isFiring)
					{
						clip = fireClip;
						phaseOffset = 0u - survivor.lastFireTime;
					}
					else if (survivor.isWalking)
					{
						clip = clipWalk;
						phaseOffset = static_cast<DWORD>(i) * 200u;
					}

					const std::vector<LobbyVertex>* pose = &SampleActor(actor, clip, now, phaseOffset);
					const DWORD shotAge = now - survivor.lastFireTime;

					if (isFiring && fireDuration > 150u && shotAge > fireDuration - 150u)
					{
						actor.blended = *pose;

						const auto& idle = SampleActor(actor, idleClip, now, static_cast<DWORD>(i) * 540u);
						const float settle = LobbyTransition::Smooth(LobbyTransition::Progress(shotAge, fireDuration - 150u, fireDuration));

						for (std::size_t vertex = 0; vertex < idle.size(); ++vertex)
						{
							actor.blended[vertex].x += (idle[vertex].x - actor.blended[vertex].x) * settle;
							actor.blended[vertex].y += (idle[vertex].y - actor.blended[vertex].y) * settle;
							actor.blended[vertex].z += (idle[vertex].z - actor.blended[vertex].z) * settle;
						}

						pose = &actor.blended;
					}

					const DWORD flashElapsed = now - survivor.lastFireTime;
					const auto& anchor = actor.muzzleAnchors[survivor.weaponIndex];
					const bool isFlashing = !isInTransition && isFiring && flashElapsed <= 75u && anchor.isValid;

					if (isFlashing)
					{
						const auto position = [pose](const UINT index)
						{
							const auto& vertex = (*pose)[index];
							return DirectX::XMVectorSet(vertex.x, vertex.y, vertex.z, 0.0f);
						};

						const DirectX::XMVECTOR origin = position(anchor.vertices[0]);
						const DirectX::XMVECTOR edgeU = DirectX::XMVectorSubtract(position(anchor.vertices[1]), origin);
						const DirectX::XMVECTOR edgeV = DirectX::XMVectorSubtract(position(anchor.vertices[2]), origin);
						const DirectX::XMVECTOR normal = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(edgeU, edgeV));

						DirectX::XMVECTOR localMuzzle = DirectX::XMVectorAdd(origin, DirectX::XMVectorScale(edgeU, anchor.u));
						localMuzzle = DirectX::XMVectorAdd(localMuzzle, DirectX::XMVectorScale(edgeV, anchor.v));
						localMuzzle = DirectX::XMVectorAdd(localMuzzle, DirectX::XMVectorScale(normal, anchor.w));

						DirectX::XMFLOAT3 worldMuzzle;
						DirectX::XMStoreFloat3(&worldMuzzle, DirectX::XMVector3TransformCoord(localMuzzle, placement));

						float flashRadius = 6.0f;

						if (survivor.weaponIndex == 0)
						{
							flashRadius = 4.2f;
						}
						else if (survivor.weaponIndex == 3)
						{
							flashRadius = 8.5f;
						}

						const float alpha = 1.0f - static_cast<float>(flashElapsed) / 75.0f;
						muzzleFlares.push_back({ worldMuzzle, flashRadius, alpha });
					}

					if (!drawGroups(actor.groups, *pose, false, false, nullptr, survivor.weaponIndex))
					{
						isRendered = false;
						break;
					}
				}

				if (!remoteScene && Party::IsHostingParty())
				{
					static DWORD lastPublish = 0;
					if (now != lastPublish)
					{
						lastPublish = now;
						const auto raw = nlohmann::json::to_msgpack(nlohmann::json{
							{ "now", now }, { "phaseOrigin", startTime }, { "transitionElapsed", transitionElapsed },
							{ "transitioning", isInTransition }, { "baseEye", LobbyPathPoint{currentBaseEye.x, currentBaseEye.y, currentBaseEye.z} },
							{ "baseTarget", LobbyPathPoint{currentBaseTarget.x, currentBaseTarget.y, currentBaseTarget.z} },
							{ "zombies", zombies }, { "survivors", survivors }, { "teleports", teleportBursts } });
						std::string packet(compressBound(static_cast<uLong>(raw.size())), '\0');
						uLongf length = static_cast<uLongf>(packet.size());
						if (compress2(reinterpret_cast<Bytef*>(packet.data()), &length, raw.data(), static_cast<uLong>(raw.size()), Z_BEST_SPEED) == Z_OK && length <= 8192)
						{
							packet.resize(length);
							if (localSceneWriter.Open(Network::GetPort(), true))
							{
								auto* const buffer = localSceneWriter.view;
								const LONG sequence = (buffer->sequence + 1) | 1;
								InterlockedExchange(&buffer->sequence, sequence);
								buffer->size = static_cast<DWORD>(packet.size());
								std::memcpy(buffer->bytes, packet.data(), packet.size());
								InterlockedExchange(&buffer->sequence, sequence + 1);
							}
							std::lock_guard lock(sceneSyncMutex);
							publishedScene = std::move(packet);
						}
					}
				}
				if (isRendered)
				{
					std::vector<LobbyVertex> eyeFlares;
					std::vector<LobbyVertex> muzzleQuads;
					std::vector<LobbyVertex>* flareBatch = &eyeFlares;

					const DirectX::XMVECTOR cameraRight = DirectX::XMVectorSet(viewFloats._11, viewFloats._21, viewFloats._31, 0.0f);
					const DirectX::XMVECTOR cameraUp = DirectX::XMVectorSet(viewFloats._12, viewFloats._22, viewFloats._32, 0.0f);

					const auto addFlareQuad = [&flareBatch, &cameraRight, &cameraUp](const DirectX::XMVECTOR& center, const float radius, const DWORD color)
					{
						const DirectX::XMVECTOR right = DirectX::XMVectorScale(cameraRight, radius);
						const DirectX::XMVECTOR up = DirectX::XMVectorScale(cameraUp, radius);

						DirectX::XMFLOAT3 corners[4];
						DirectX::XMStoreFloat3(&corners[0], DirectX::XMVectorSubtract(DirectX::XMVectorSubtract(center, right), up));
						DirectX::XMStoreFloat3(&corners[1], DirectX::XMVectorSubtract(DirectX::XMVectorAdd(center, right), up));
						DirectX::XMStoreFloat3(&corners[2], DirectX::XMVectorAdd(DirectX::XMVectorAdd(center, right), up));
						DirectX::XMStoreFloat3(&corners[3], DirectX::XMVectorAdd(DirectX::XMVectorSubtract(center, right), up));

						flareBatch->push_back({ corners[0].x, corners[0].y, corners[0].z, color, 0.0f, 0.0f });
						flareBatch->push_back({ corners[1].x, corners[1].y, corners[1].z, color, 1.0f, 0.0f });
						flareBatch->push_back({ corners[2].x, corners[2].y, corners[2].z, color, 1.0f, 1.0f });
						flareBatch->push_back({ corners[0].x, corners[0].y, corners[0].z, color, 0.0f, 0.0f });
						flareBatch->push_back({ corners[2].x, corners[2].y, corners[2].z, color, 1.0f, 1.0f });
						flareBatch->push_back({ corners[3].x, corners[3].y, corners[3].z, color, 0.0f, 1.0f });
					};

					const DirectX::XMVECTOR eyePosition = DirectX::XMLoadFloat3(&eye);

					for (std::size_t i = 0; i < zombies.size() && !zombieMesh.frames.empty(); ++i)
					{
						const auto& visual = zombies[i];

						if (!visual.isVisible)
						{
							continue;
						}

						if (remoteScene && visual.isDying)
						{
							const unsigned int deathClip = visual.isAttacking ? clipAttack : (visual.variant == 1 ? clipAction : clipIdle);
							const DWORD deathPhase = visual.isAttacking ? visual.deathTime - visual.attackTime : static_cast<DWORD>(visual.animationTime);
							zombieMesh.blended = SampleActor(zombieMesh, deathClip, deathPhase, 0);
						}
						else if (visual.isDying && !visual.deathPose.empty())
						{
							zombieMesh.blended = visual.deathPose;
						}
						else
						{
							zombieMesh.blended = sampleZombie(visual);
						}

						auto& pose = zombieMesh.blended;
						float lowest = std::numeric_limits<float>::max();

						for (const auto& vertex : pose)
						{
							lowest = std::min(lowest, vertex.z);
						}

						for (auto& vertex : pose)
						{
							vertex.z -= lowest;
						}

						float fallAngle = 0.0f;

						if (visual.isDying)
						{
							const float fallProgress = LobbyTransition::Progress(static_cast<unsigned int>(now - visual.deathTime), 0u, zombieFallDuration);
							fallAngle = visual.fallDirection * fallProgress * fallProgress * DirectX::XM_PI * 0.5f;
						}

						float supportZ = std::numeric_limits<float>::max();

						for (const auto& vertex : pose)
						{
							supportZ = std::min(supportZ, vertex.z * std::cos(fallAngle) - vertex.x * std::sin(fallAngle));
						}

						const DirectX::XMMATRIX turn = DirectX::XMMatrixMultiply(DirectX::XMMatrixRotationY(fallAngle), DirectX::XMMatrixRotationZ(visual.facing));
						const DirectX::XMMATRIX placement = DirectX::XMMatrixMultiply(turn, DirectX::XMMatrixTranslation(visual.location.x, visual.location.y, visual.location.z - supportZ));
						SetTransform(device, D3DTS_WORLD, placement);

						if (!drawGroups(zombieMesh.groups, pose, false, false, nullptr, -1))
						{
							isRendered = false;
							break;
						}

						float flareAlpha = 1.0f;

						if (visual.isDying)
						{
							const float deathElapsed = static_cast<float>(now - visual.deathTime) * 0.001f;
							flareAlpha = std::clamp(1.0f - deathElapsed * 1.5f, 0.0f, 1.0f);
						}

						if (flareAlpha > 0.01f && pose.size() >= zombieEyeFirstVertex + 540)
						{
							DirectX::XMFLOAT3 localLeft = { 0.0f, 0.0f, 0.0f };
							DirectX::XMFLOAT3 localRight = { 0.0f, 0.0f, 0.0f };

							for (UINT vertex = zombieEyeFirstVertex; vertex < zombieEyeFirstVertex + 270; vertex += 9)
							{
								localLeft.x += pose[vertex].x;
								localLeft.y += pose[vertex].y;
								localLeft.z += pose[vertex].z;
							}

							for (UINT vertex = zombieEyeFirstVertex + 270; vertex < zombieEyeFirstVertex + 540; vertex += 9)
							{
								localRight.x += pose[vertex].x;
								localRight.y += pose[vertex].y;
								localRight.z += pose[vertex].z;
							}

							DirectX::XMVECTOR worldLeft = DirectX::XMVector3TransformCoord(DirectX::XMVectorScale(DirectX::XMLoadFloat3(&localLeft), 1.0f / 30.0f), placement);
							DirectX::XMVECTOR worldRight = DirectX::XMVector3TransformCoord(DirectX::XMVectorScale(DirectX::XMLoadFloat3(&localRight), 1.0f / 30.0f), placement);

							worldLeft = DirectX::XMVectorAdd(worldLeft, DirectX::XMVectorScale(DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(eyePosition, worldLeft)), 0.8f));
							worldRight = DirectX::XMVectorAdd(worldRight, DirectX::XMVectorScale(DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(eyePosition, worldRight)), 0.8f));

							const DWORD flareColor = D3DCOLOR_ARGB(static_cast<DWORD>(255 * flareAlpha), 255, 255, 255);
							addFlareQuad(worldLeft, 2.4f, flareColor);
							addFlareQuad(worldRight, 2.4f, flareColor);
						}
					}

					for (auto& burst : teleportBursts)
					{
						if (!burst.isActive)
						{
							continue;
						}

						const DWORD age = now - burst.started;

						if (age >= 800u)
						{
							burst.isActive = false;
							continue;
						}

						const float t = static_cast<float>(age) / 800.0f;
						const float intensity = std::sin(t * DirectX::XM_PI) * 0.55f;
						const DWORD color = D3DCOLOR_ARGB(static_cast<DWORD>(255 * intensity), 255, 32, 16);

						for (unsigned int particle = 0; particle < 8; ++particle)
						{
							const float angle = static_cast<float>(particle) * 2.399963f + t * 4.0f;
							const float radius = 8.0f + (1.0f - t) * 10.0f;
							const float height = std::fmod(static_cast<float>(particle) * 5.7f + t * 90.0f, 72.0f);

							addFlareQuad(DirectX::XMVectorSet(burst.x + std::cos(angle) * radius, balconyY + std::sin(angle) * radius, 248.0f + height, 0.0f), 2.5f, color);
						}

						addFlareQuad(DirectX::XMVectorSet(burst.x, balconyY, 278.0f, 0.0f), 10.0f, color);
					}

					flareBatch = &muzzleQuads;

					for (const auto& flare : muzzleFlares)
					{
						const DirectX::XMVECTOR position = DirectX::XMLoadFloat3(&flare.position);
						addFlareQuad(position, flare.radius, D3DCOLOR_ARGB(static_cast<DWORD>(255 * flare.alpha), 255, 205, 80));
						addFlareQuad(position, flare.radius * 0.42f, D3DCOLOR_ARGB(static_cast<DWORD>(255 * flare.alpha), 255, 255, 230));
					}

					if (isRendered && (!eyeFlares.empty() || !muzzleQuads.empty()))
					{
						device->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);

						IDirect3DTexture9* flareTexture = nullptr;
						const auto found = loadedTextures.find("zombie_eye_flare.dds");

						if (found != loadedTextures.end())
						{
							flareTexture = found->second;
						}

						SetTransform(device, D3DTS_WORLD, world);
						device->SetFVF(vertexFormat);
						device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
						device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
						device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
						device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
						device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
						device->SetTexture(0, flareTexture);
						device->SetTexture(1, nullptr);
						device->SetTexture(2, nullptr);

						const float flareFlags[4] = { 0.0f, 0.0f, -2.0f, 0.0f };
						device->SetPixelShaderConstantF(0, flareFlags, 1);
						device->SetPixelShader(filmShader);

						if (!eyeFlares.empty())
						{
							device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, static_cast<UINT>(eyeFlares.size() / 3), eyeFlares.data(), sizeof(LobbyVertex));
						}

						if (!muzzleQuads.empty())
						{
							const float muzzleFlags[4] = { 0.0f, 0.0f, -5.0f, 0.0f };
							device->SetPixelShaderConstantF(0, muzzleFlags, 1);
							device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, static_cast<UINT>(muzzleQuads.size() / 3), muzzleQuads.data(), sizeof(LobbyVertex));
						}
					}
				}

				SetTransform(device, D3DTS_WORLD, world);
			}
		}

		struct ScreenVertex
		{
			float x;
			float y;
			float z;
			float rhw;
			float u;
			float v;
		};

		device->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);

		if (isRendered && SUCCEEDED(device->SetRenderTarget(0, target)) && SUCCEEDED(device->SetDepthStencilSurface(nullptr)))
		{
			if (visionShader)
			{
				const auto width = static_cast<float>(textureWidth);
				const auto height = static_cast<float>(textureHeight);

				const ScreenVertex quad[] =
				{
					{ -0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f },
					{ width - 0.5f, -0.5f, 0.0f, 1.0f, 1.0f, 0.0f },
					{ -0.5f, height - 0.5f, 0.0f, 1.0f, 0.0f, 1.0f },
					{ width - 0.5f, -0.5f, 0.0f, 1.0f, 1.0f, 0.0f },
					{ width - 0.5f, height - 0.5f, 0.0f, 1.0f, 1.0f, 1.0f },
					{ -0.5f, height - 0.5f, 0.0f, 1.0f, 0.0f, 1.0f },
				};

				const float glow[4] = { glowSettings[0], glowSettings[1], 1.5f / width, 1.5f / height };

				device->SetRenderState(D3DRS_ZENABLE, FALSE);
				device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
				device->SetRenderState(D3DRS_FOGENABLE, FALSE);
				device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
				device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
				device->SetVertexShader(nullptr);
				device->SetPixelShader(visionShader);
				device->SetPixelShaderConstantF(0, filmContrast, 1);
				device->SetPixelShaderConstantF(1, filmLightTint, 1);
				device->SetPixelShaderConstantF(2, filmMediumTint, 1);
				device->SetPixelShaderConstantF(3, filmDarkTint, 1);
				device->SetPixelShaderConstantF(4, filmExposure, 1);
				device->SetPixelShaderConstantF(5, filmBrightness, 1);
				device->SetPixelShaderConstantF(6, glow, 1);
				device->SetTexture(0, sceneTexture);
				device->SetTexture(1, nullptr);
				device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
				device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
				device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
				device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
				device->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
				device->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1);

				isRendered = SUCCEEDED(device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 2, quad, sizeof(ScreenVertex)));
			}
			else
			{
				isRendered = SUCCEEDED(device->StretchRect(sceneTarget, nullptr, target, nullptr, D3DTEXF_NONE));
			}

			if ((isInTransition || isBootPreview) && oldTarget)
			{
				device->SetRenderTarget(0, oldTarget);
				device->SetDepthStencilSurface(nullptr);

				D3DVIEWPORT9 backViewport = oldViewport;
				device->SetViewport(&backViewport);

				const auto backWidth = static_cast<float>(backViewport.Width);
				const auto backHeight = static_cast<float>(backViewport.Height);

				const ScreenVertex backQuad[] =
				{
					{ -0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f },
					{ backWidth - 0.5f, -0.5f, 0.0f, 1.0f, 1.0f, 0.0f },
					{ -0.5f, backHeight - 0.5f, 0.0f, 1.0f, 0.0f, 1.0f },
					{ backWidth - 0.5f, -0.5f, 0.0f, 1.0f, 1.0f, 0.0f },
					{ backWidth - 0.5f, backHeight - 0.5f, 0.0f, 1.0f, 1.0f, 1.0f },
					{ -0.5f, backHeight - 0.5f, 0.0f, 1.0f, 0.0f, 1.0f },
				};

				device->SetVertexShader(nullptr);
				device->SetPixelShader(nullptr);
				device->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1);
				device->SetTexture(0, roomImage->texture.map);
				device->SetRenderState(D3DRS_ZENABLE, FALSE);
				device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
				device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
				device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
				device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
				device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
				device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
				device->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
				device->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
				device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
				device->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);

				if (isRendered)
				{
					device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 2, backQuad, sizeof(ScreenVertex));
				}
				else
				{
					device->StretchRect(sceneTarget, nullptr, oldTarget, nullptr, D3DTEXF_NONE);
				}

				if (whiteOpacity > 0.0f)
				{
					struct FadeVertex
					{
						float x;
						float y;
						float z;
						float rhw;
						DWORD color;
					};

					device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
					device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
					device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
					device->SetRenderState(D3DRS_ZENABLE, FALSE);
					device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
					device->SetVertexShader(nullptr);
					device->SetPixelShader(nullptr);
					device->SetTexture(0, nullptr);
					device->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);

					const auto fadeAlpha = static_cast<DWORD>(std::clamp(whiteOpacity * 255.0f, 0.0f, 255.0f));
					const DWORD fadeColor = (fadeAlpha << 24) | 0xFFFFFF;

					device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
					device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
					device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);

					const FadeVertex fadeQuad[] =
					{
						{ -0.5f, -0.5f, 0.0f, 1.0f, fadeColor },
						{ backWidth - 0.5f, -0.5f, 0.0f, 1.0f, fadeColor },
						{ -0.5f, backHeight - 0.5f, 0.0f, 1.0f, fadeColor },
						{ backWidth - 0.5f, -0.5f, 0.0f, 1.0f, fadeColor },
						{ backWidth - 0.5f, backHeight - 0.5f, 0.0f, 1.0f, fadeColor },
						{ -0.5f, backHeight - 0.5f, 0.0f, 1.0f, fadeColor },
					};

					device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 2, fadeQuad, sizeof(FadeVertex));
				}
			}
		}
		else
		{
			isRendered = false;
		}

		device->SetRenderTarget(0, oldTarget);
		device->SetDepthStencilSurface(oldDepth);
		device->SetViewport(&oldViewport);

		if (savedState)
		{
			savedState->Apply();
		}

		oldDepth->Release();
		oldTarget->Release();
		target->Release();
		sceneTarget->Release();

		isFrameReady.store(isRendered, std::memory_order_release);
	}

	static void EndTransition()
	{
		if (!isTransitionActive.load(std::memory_order_acquire))
		{
			return;
		}

		isTransitionActive.store(false, std::memory_order_release);
		didSeeConnecting.store(false, std::memory_order_release);
		didRunDeferredLaunch = false;
		transitionStartTime.store(0, std::memory_order_release);

		if (Game::uiContext->openMenuCount > 0)
		{
			Game::Key_SetCatcher(0, Game::KEYCATCH_UI);
		}
	}

	LobbyScene::LobbyScene()
	{
		if (Dedicated::IsEnabled() || ZoneBuilder::IsEnabled())
		{
			return;
		}

		Scheduler::Loop(UpdateMenu, Scheduler::Pipeline::MAIN);

		Events::AfterUIInit([]
		{
			backgroundAnimations.clear();
			shouldRefreshMaterial = true;
			RefreshLobbyMaterial();
			PrepareStartup();
			AttachMaterialToLobbies(IsSceneReady() && !shouldRefreshMaterial);
		});

		Renderer::OnBackendFrame([](IDirect3DDevice9* device)
		{
			if (IsCinematicActive())
			{
				if (!isFrameReady.load(std::memory_order_acquire))
				{
					RenderRoom(device);
				}

				return;
			}

			RenderRoom(device);

			if (IsStartupLoading() && !isFrameReady.load(std::memory_order_acquire))
			{
				device->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
			}
		});

		Renderer::OnDeviceRecoveryBegin([]
		{
			EndTransition();

			hasZombieWaveStarted.store(false, std::memory_order_release);
			lobbySession.fetch_add(1, std::memory_order_release);
			cameraResetSession.fetch_add(1, std::memory_order_release);

			const bool isAtMainMenu = Game::CL_GetLocalClientConnectionState(0) < Game::CA_CONNECTING && !Game::CL_IsCgameInitialized(0);

			isFrameReady.store(false, std::memory_order_release);
			hasAssets.store(false, std::memory_order_release);
			shouldRefreshMaterial = true;

			if (isAtMainMenu)
			{
				isStartupLoading.store(true, std::memory_order_release);
				startupWaitStart = 0;
			}

			AttachMaterialToLobbies(false);
			ReleaseDepth();
			ReleaseTextures();
		});

		Renderer::OnDeviceRecoveryEnd([]
		{
			isFrameReady.store(false, std::memory_order_release);

			RefreshLobbyMaterial();

			if (!roomVertices.empty())
			{
				hasAssets.store(true, std::memory_order_release);
			}

			if (texturePack.bytes.empty())
			{
				isTexturePackReady.store(false, std::memory_order_release);
			}
		});

		canReadErrorState = Utils::Hook::MatchesBytes(Com_ErrorEntered, comErrorEnteredBytes, sizeof(comErrorEnteredBytes));

		if (!canReadErrorState)
		{
			Logger::Error("lobby: Com_ErrorEntered does not read as expected, an error ends the start transition only by its timeout\n");
		}

		Events::OnCLDisconnected([](bool)
		{
			hasZombieWaveStarted.store(false, std::memory_order_release);
			lobbySession.fetch_add(1, std::memory_order_release);

			const bool isError = canReadErrorState && reinterpret_cast<bool(*)()>(Utils::Hook::Rebase(Com_ErrorEntered))();

			if (isError)
			{
				isRunningDeferredLaunch = false;
			}

			if (isError || didRunDeferredLaunch || didSeeConnecting.load(std::memory_order_acquire))
			{
				StopTransition();
			}
		});
	}

	std::string LobbyScene::GetSceneSnapshot()
	{
		std::lock_guard lock(sceneSyncMutex);
		return publishedScene;
	}

	void LobbyScene::ClearRemoteScene()
	{
		std::lock_guard lock(sceneSyncMutex);
		++remoteSceneGeneration;
		receivedScene.reset();
		previousScene.reset();
		isDirectScene = false;
		localSceneReader.Close();
		hasSceneClock = false;
		bestSceneRoundTrip = 1001;
		receivedSceneAt = 0;
	}

	void LobbyScene::ReceiveSceneClock(const unsigned int clientSent, const unsigned int hostTime)
	{
		const DWORD received = timeGetTime();
		const DWORD roundTrip = received - clientSent;
		if (roundTrip > 1000u || roundTrip > bestSceneRoundTrip.load() + 10u) return;
		bestSceneRoundTrip = std::min<DWORD>(bestSceneRoundTrip.load(), roundTrip);
		sceneClockOffset = static_cast<int>(hostTime + roundTrip / 2u - received);
		hasSceneClock = true;
	}

	void LobbyScene::PollLocalScene()
	{
		if (!Party::IsLobbySceneClient()) return;
		const auto target = Party::Target();
		bool sameComputer = target.IsLoopback();
		for (int i = 0; !sameComputer && i < *Game::numIP; ++i) sameComputer = target.GetIP() == Game::localIP[i].full;
		if (!sameComputer || !localSceneReader.Open(target.GetPort(), false)) return;
		const auto* const view = localSceneReader.view;
		const LONG sequence = view->sequence;
		MemoryBarrier();
		if (sequence == 0 || (sequence & 1) || view->size == 0 || view->size > sizeof(view->bytes)) return;
		const std::string packet(view->bytes, view->size);
		MemoryBarrier();
		if (sequence != view->sequence) return;
		ReceiveSceneSnapshot(packet, true);
	}

	void LobbyScene::ReceiveSceneSnapshot(const std::string& packet, const bool immediate)
	{
		if (!immediate && localSceneReader.view) return;
		const auto generation = remoteSceneGeneration.load();
		if (packet.empty() || packet.size() > 8192) return;
		std::string bytes(65536, '\0');
		uLongf size = static_cast<uLongf>(bytes.size());
		if (uncompress(reinterpret_cast<Bytef*>(bytes.data()), &size, reinterpret_cast<const Bytef*>(packet.data()), static_cast<uLong>(packet.size())) != Z_OK) return;
		bytes.resize(size);
		try
		{
			const auto json = nlohmann::json::from_msgpack(bytes);
			if (json.at("zombies").size() != 32 || json.at("survivors").size() != 4 || json.at("teleports").size() != 4) return;
			const auto scene = std::make_shared<SceneSnapshot>(json.get<SceneSnapshot>());
			const auto validPoint = [](const LobbyPathPoint& point)
			{
				return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z)
					&& std::abs(point.x) < 100000.0f && std::abs(point.y) < 100000.0f && std::abs(point.z) < 100000.0f;
			};
			if (!validPoint(scene->baseEye) || !validPoint(scene->baseTarget) || scene->transitionElapsed > 25000u) return;
			for (const auto& survivor : scene->survivors)
			{
				if (survivor.modelIndex < -1 || survivor.modelIndex >= 4 || survivor.weaponIndex < 0 || survivor.weaponIndex >= 4
					|| !std::isfinite(survivor.facing) || !std::isfinite(survivor.homeX)) return;
			}
			for (const auto& zombie : scene->zombies)
			{
				if (!validPoint(zombie.location)
					|| !std::isfinite(zombie.facing) || !std::isfinite(zombie.animationTime)) return;
			}
			const auto apply = [scene, generation, immediate]
			{
				if (!Party::IsLobbySceneClient() || generation != remoteSceneGeneration) return;
				{
					std::lock_guard lock(sceneSyncMutex);
					if (receivedScene && static_cast<int>(scene->now - receivedScene->now) <= 0)
					{
						if (immediate && scene->now == receivedScene->now) isDirectScene = true;
						return;
					}
					previousScene = receivedScene;
					receivedScene = scene;
					isDirectScene = immediate;
					receivedSceneAt = timeGetTime();
				}
				if (scene->transitioning)
				{
					StartTransition();
					const DWORD age = hasSceneClock ? static_cast<DWORD>(std::max(0, static_cast<int>(timeGetTime()
						+ static_cast<DWORD>(sceneClockOffset.load()) - scene->now))) : 0u;
					transitionStartTime.store(timeGetTime() - scene->transitionElapsed - age, std::memory_order_release);
				}
			};
			if (immediate) apply();
			else Scheduler::Once(apply, Scheduler::Pipeline::MAIN);
		}
		catch (const nlohmann::json::exception&) {}
	}

	bool LobbyScene::IsTransitionActive()
	{
		return isTransitionActive.load(std::memory_order_acquire);
	}

	bool LobbyScene::IsSceneReady()
	{
		return hasAssets.load(std::memory_order_acquire) && isFrameReady.load(std::memory_order_acquire);
	}

	bool LobbyScene::IsStartupLoading()
	{
		return !Dedicated::IsEnabled()
			&& !ZoneBuilder::IsEnabled()
			&& isStartupLoading.load(std::memory_order_acquire)
			&& !IsCinematicActive();
	}

	bool LobbyScene::IsCinematicActive()
	{
		if (Utils::Hook::Get<void*>(cinematicGlobBink))
		{
			return true;
		}

		const auto state = Game::CL_GetLocalClientConnectionState(0);
		return state == Game::CA_CINEMATIC || state == Game::CA_LOGO;
	}

	void LobbyScene::PrepareStartup()
	{
		const bool canPrepare = !Dedicated::IsEnabled()
			&& !ZoneBuilder::IsEnabled()
			&& Game::Sys_IsDatabaseReady()
			&& !Game::CL_IsCgameInitialized(0)
			&& Game::CL_GetLocalClientConnectionState(0) < Game::CA_CONNECTING
			&& *Game::dx_device
			&& Renderer::Width() > 0
			&& Renderer::Height() > 0;

		if (!canPrepare)
		{
			return;
		}

		if (!Game::DB_IsZoneLoaded("zw3_common") && !Game::DB_IsZoneLoaded("common_mp"))
		{
			return;
		}

		if (!Game::DB_IsZoneLoaded("zw3_lobby"))
		{
			if (!FastFiles::Exists("zw3_lobby"))
			{
				isStartupLoading.store(false, std::memory_order_release);
				return;
			}

			Game::XZoneInfo zone{ "zw3_lobby", 1, 0 };
			Game::DB_LoadXAssets(&zone, 1, 1);
		}

		if (!Game::DB_IsZoneLoaded("zw3_lobby"))
		{
			return;
		}

		RefreshLobbyMaterial();

		if (hasAssets.load(std::memory_order_acquire))
		{
			return;
		}

		PrepareTexturePack();
		LoadRoomMesh();
		if (!roomVertices.empty()) LoadPropMesh();
		LoadTheaterVision();
		static constexpr const char* actorNames[] = { "richtofen", "dempsey", "nikolai", "takeo", "zombie" };
		std::array<std::future<void>, std::size(actorNames)> actorLoads;
		for (std::size_t i = 0; i < std::size(actorNames); ++i)
		{
			const std::string stem = std::string("lobby/actors/") + actorNames[i];
			const std::string diskStem = std::string("zw3/core/lobby/actors/") + actorNames[i];
			auto mesh = ReadLobbyAsset(stem + ".zwlb", diskStem + ".zwlb");
			auto manifest = ReadLobbyAsset(stem + ".json", diskStem + ".json");
			actorLoads[i] = std::async(std::launch::async, [i, mesh = std::move(mesh), manifest = std::move(manifest)]
			{
				LoadActorMesh(i, mesh, manifest);
			});
		}
		for (auto& load : actorLoads)
		{
			load.get();
		}
		auto* const entry = Game::DB_FindXAssetEntry(Game::ASSET_TYPE_RAWFILE, "lobby/audio/round_start.wav");

		if (entry)
		{
			const Game::RawFile* const raw = entry->asset.header.rawfile;

			if (raw && raw->len > 0 && raw->len < 4 * 1024 * 1024)
			{
				std::string wav(static_cast<std::size_t>(raw->len) + 1, '\0');
				Game::DB_GetRawBuffer(raw, wav.data(), static_cast<int>(wav.size()));
				wav.resize(static_cast<std::size_t>(raw->len));
				Sound::PrepareLobbyRoundStart(wav);
			}
		}

		hasAssets.store(true, std::memory_order_release);
	}

	bool LobbyScene::DeferLaunch(const std::function<void()>& launch)
	{
		if (isRunningDeferredLaunch)
		{
			return false;
		}

		if (pendingLaunchId)
		{
			return true;
		}

		const bool isInLobby = IsLobbyMenuVisible("menu_xboxlive_privatelobby") || IsLobbyMenuVisible("zwnet_matchmaking");

		if (!IsSceneReady() || (!isInLobby && !IsTransitionActive() && !Party::IsPrivateMatchClient()))
		{
			return false;
		}

		const bool wasTransitionActive = IsTransitionActive();
		StartTransition();
		if (!wasTransitionActive && Party::IsHostingParty()) Party::BroadcastLobbyTransition();

		++launchCount;
		pendingLaunchId = launchCount;

		const unsigned int launchId = pendingLaunchId;

		Scheduler::Schedule([launch, launchId]
		{
			if (launchId != pendingLaunchId)
			{
				return true;
			}

			const DWORD elapsed = timeGetTime() - transitionStartTime.load();
			const bool isWaitingForWhite = IsTransitionActive() && elapsed < LobbyTransition::whiteEndMs;

			if (isWaitingForWhite || Renderer::IsDeviceRecoveryActive())
			{
				return false;
			}

			pendingLaunchId = 0;
			isRunningDeferredLaunch = true;
			launch();
			isRunningDeferredLaunch = false;
			didRunDeferredLaunch = true;

			return true;
		}, Scheduler::Pipeline::MAIN);

		return true;
	}

	void LobbyScene::StartTransition()
	{
		if (!IsSceneReady() || isTransitionActive.load(std::memory_order_acquire))
		{
			return;
		}

		transitionStartTime.store(timeGetTime(), std::memory_order_release);
		isTransitionActive.store(true, std::memory_order_release);
		didSeeConnecting.store(false, std::memory_order_release);
		didRunDeferredLaunch = false;

		Game::Key_RemoveCatcher(0, ~Game::KEYCATCH_UI);
		Sound::PlayLobbyRoundStart();
		Game::Key_ClearStates(0);

		Game::uiContext->cursor.x = -1000.0f;
		Game::uiContext->cursor.y = -1000.0f;
		Game::uiContext->isCursorVisible = 0;

		const std::string mapName = Dvar::Var("ui_mapname").Get<std::string>();

		if (!mapName.empty())
		{
			D3D9Ex::BeginMapLoading(mapName);
			FastFiles::PrefetchZone(mapName + "_load");
			FastFiles::PrefetchZone(mapName);
			FastFiles::PrefetchZone("patch_" + mapName);
			FastFiles::PrefetchZone("localized_" + mapName);
		}
	}

	void LobbyScene::StopTransition()
	{
		if (pendingLaunchId)
		{
			Logger::Warning("lobby: match launch {} is dropped, the start transition was stopped before it ran\n", pendingLaunchId);
			pendingLaunchId = 0;
		}

		EndTransition();
	}
}
