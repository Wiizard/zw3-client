#include "LobbyScene.hpp"
#include "LobbyTransition.hpp"
#include "LobbyCombat.hpp"

#include "D3D9Ex.hpp"
#include "Events.hpp"
#include "FastFiles.hpp"
#include "FileSystem.hpp"
#include "Materials.hpp"
#include "Renderer.hpp"
#include "Sound.hpp"

#include <sstream>
#include <random>
#include <zlib.h>

namespace Components
{
	namespace
	{
		struct Vertex
		{
			float x, y, z;
			DWORD color;
			float u, v;
		};
		struct RoomVertex
		{
			float x, y, z;
			DWORD color;
			float u, v;
			float lightU, lightV;
		};
		struct DrawGroup
		{
			UINT firstVertex;
			UINT vertexCount;
			std::string textureName;
			bool blend = false;
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
			bool alphaTest = true;
			DWORD srcBlend = D3DBLEND_SRCALPHA;
			DWORD dstBlend = D3DBLEND_INVSRCALPHA;
			float alphaThreshold = 0.05f;
			bool multiplicative = false;
			bool depthWrite = true;
			bool gammaWrite = false;
		};

		DWORD MaterialBlendFactor(const std::string& name)
		{
			if (name == "zero") return D3DBLEND_ZERO;
			if (name == "one") return D3DBLEND_ONE;
			if (name == "srccolor") return D3DBLEND_SRCCOLOR;
			if (name == "invsrccolor") return D3DBLEND_INVSRCCOLOR;
			if (name == "srcalpha") return D3DBLEND_SRCALPHA;
			if (name == "invsrcalpha") return D3DBLEND_INVSRCALPHA;
			if (name == "destalpha") return D3DBLEND_DESTALPHA;
			if (name == "invdestalpha") return D3DBLEND_INVDESTALPHA;
			if (name == "destcolor") return D3DBLEND_DESTCOLOR;
			if (name == "invdestcolor") return D3DBLEND_INVDESTCOLOR;
			throw std::runtime_error("Invalid lobby material blend factor");
		}
		struct SkyVertex
		{
			float x, y, z;
			DWORD color;
			float u, v, w;
		};
		struct ActorMesh
		{
			struct MuzzleAnchor
			{
				std::array<UINT, 3> vertices{};
				float u = 0, v = 0, w = 0;
				bool valid = false;
			};
			std::array<MuzzleAnchor, 4> muzzleAnchors{};
			std::vector<std::vector<Vertex>> frames;
			std::vector<Vertex> interpolated;
			std::vector<Vertex> blended;
			std::vector<DrawGroup> groups;
			UINT idleFirst = 0;
			UINT idleCount = 1;
			UINT walkFirst = 0;
			UINT walkCount = 1;
			UINT actionFirst = 0;
			UINT actionCount = 1;
			UINT deathFirst = 0;
			UINT deathCount = 1;
			UINT rifleIdleFirst = 0;
			UINT rifleIdleCount = 1;
			UINT rifleFireFirst = 0;
			UINT rifleFireCount = 1;
			UINT shotgunFireFirst = 0;
			UINT shotgunFireCount = 1;
			UINT pointFirst = 0;
			UINT pointCount = 0;
			UINT meleeFirst = 0;
			UINT meleeCount = 0;
			UINT attackFirst = 0;
			UINT attackCount = 0;
			std::array<unsigned, 10> clipDurationMs{};
		};

		static_assert(sizeof(Vertex) == 24);
		static_assert(sizeof(RoomVertex) == 32);

		unsigned textureWidth = 1920;
		unsigned textureHeight = 1080;
		constexpr auto VertexFormat = D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1;
		constexpr auto RoomVertexFormat = D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX2;

		std::vector<RoomVertex> roomVertices;
		std::vector<DrawGroup> roomGroups;
		std::vector<RoomVertex> doorLeftVertices;
		std::vector<RoomVertex> doorRightVertices;
		std::vector<DrawGroup> doorLeftGroups;
		std::vector<DrawGroup> doorRightGroups;
		std::vector<Vertex> propVertices;
		std::vector<std::uint32_t> propIndices;
		std::vector<DrawGroup> propGroups;
		std::array<ActorMesh, 5> actorMeshes;
		std::unordered_map<std::string, IDirect3DTexture9*> loadedTextures;
		std::vector<std::string> pendingTextureNames;
		size_t nextTexture = 0;
		bool renderResourcesReady = false;
		std::string skyTextureName;
		IDirect3DCubeTexture9* skyTexture = nullptr;
		IDirect3DPixelShader9* filmShader = nullptr;
		IDirect3DPixelShader9* lightmapFilmShader = nullptr;
		IDirect3DPixelShader9* visionShader = nullptr;
		IDirect3DTexture9* sceneTexture = nullptr;
		std::array<IDirect3DTexture9*, 3> roomLightmaps{};
		IDirect3DVertexBuffer9* roomVertexBuffer = nullptr;
		IDirect3DVertexBuffer9* propVertexBuffer = nullptr;
		IDirect3DIndexBuffer9* propIndexBuffer = nullptr;
		float filmContrast[4] = { 1.08f, 0.0f, 0.0f, 0.0f };
		float filmLightTint[4] = { 1.15f, 1.10f, 1.00f, 0.0f };
		float filmMediumTint[4] = { 1.02f, 1.00f, 0.97f, 0.0f };
		float filmDarkTint[4] = { 1.00f, 0.98f, 0.95f, 0.0f };
		float filmExposure[4] = { 1.0f, 0.0f, 0.0f, 0.0f };
		float filmBrightness[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		float glowSettings[4] = { 0.76f, 0.32f, 0.0f, 0.0f };
		D3DXVECTOR3 currentCameraEye = { 0.0f, -1450.0f, 192.0f };
		D3DXVECTOR3 currentCameraTarget = { 0.0f, -600.0f, 182.0f };
		D3DXVECTOR3 currentCameraPan = { 4.0f, 3.0f, 0.0f };
		Game::GfxImage* roomImage = nullptr;
		Game::Material* roomMaterial = nullptr;
		IDirect3DSurface9* roomDepth = nullptr;
		IDirect3DStateBlock9* savedState = nullptr;
		std::atomic_bool lobbyVisible = false;
		std::atomic_bool zombieWaveStarted = false;
		std::atomic_bool closeCamera = false;
		std::atomic_uint lobbySession = 0;
		std::atomic_uint cameraResetSession = 0;
		std::atomic_int lobbyCharacterCount = 1;
		std::array<std::atomic_int, 4> lobbyCharacterModels = { 0, 1, 2, 3 };
		std::atomic_bool assetsReady = false;
		std::atomic_bool frameReady = false;
		std::atomic_bool startupLoading = true;
		unsigned startupWaitStart = 0;
		bool materialNeedsRefresh = true;
		std::atomic<bool> theaterDirectTransitionActive = false;
		std::atomic<bool> sawConnectingState = false;
		std::atomic<unsigned> transitionStartTime = 0;
		std::atomic_uint transitionGeneration = 0;
		std::atomic_bool fadePresented = false;
		void (*originalPartyGo)() = nullptr;
		bool executingDeferredLaunch = false;

		bool IsCinematicActive()
		{
			return LobbyScene::IsCinematicActive();
		}

		struct TexturePackCache
		{
			bool checked = false;
			std::string bytes;
			std::unordered_map<std::string, std::pair<size_t, size_t>> entries;
		};
		TexturePackCache texturePackCache;
		std::atomic_int texturePackStage = 0;

		void IndexTexturePack()
		{
			auto& checked = texturePackCache.checked;
			auto& pack = texturePackCache.bytes;
			auto& entries = texturePackCache.entries;
			if (checked) return;
			checked = true;
			std::uint32_t version = 0, directorySize = 0;
			if (pack.size() >= 12 && !std::memcmp(pack.data(), "ZWTP", 4))
			{
				std::memcpy(&version, pack.data() + 4, 4);
				std::memcpy(&directorySize, pack.data() + 8, 4);
				if (version == 1 && directorySize <= pack.size() - 12)
				{
					try
					{
						const auto directory = nlohmann::json::parse(pack.begin() + 12, pack.begin() + 12 + directorySize);
						const size_t payload = 12ull + directorySize;
						for (const auto& [name, range] : directory.items())
						{
							const auto offset = range.at(0).get<size_t>();
							const auto length = range.at(1).get<size_t>();
							if (offset > pack.size() - payload || length > pack.size() - payload - offset)
							{
								entries.clear();
								break;
							}
							entries.emplace(name, std::make_pair(payload + offset, length));
						}
					}
					catch (const nlohmann::json::exception&) { entries.clear(); }
				}
			}
			if (entries.empty()) pack.clear();
		}

		void PrepareTexturePack()
		{
			if (texturePackStage.load(std::memory_order_acquire) != 0 ||
				!Game::DB_IsZoneLoaded("zw3_lobby") ||
				!Game::Sys_IsDatabaseReady() ||
				Game::CL_IsCgameInitialized() ||
				*reinterpret_cast<Game::connstate_t*>(0xB2C540) >= Game::CA_CONNECTING) return;
			auto* entry = Game::DB_FindXAssetEntry(Game::ASSET_TYPE_RAWFILE, "lobby/textures.pack");
			auto* raw = entry ? entry->asset.header.rawfile : nullptr;
			if (!raw || Game::DB_IsXAssetDefault(Game::ASSET_TYPE_RAWFILE, "lobby/textures.pack") ||
				!raw->buffer || raw->len <= 0 || raw->len > 256 * 1024 * 1024 ||
				raw->compressedLen < 0 || raw->compressedLen > 256 * 1024 * 1024)
			{
				texturePackStage.store(2, std::memory_order_release);
				return;
			}
			const auto expected = static_cast<unsigned long>(raw->len);
			const bool compressed = raw->compressedLen != 0;
			if (!compressed)
			{
				texturePackCache.bytes.assign(raw->buffer, raw->len);
			}
			else
			{
				texturePackCache.bytes.resize(expected);
				auto length = expected;
				if (uncompress(reinterpret_cast<Bytef*>(texturePackCache.bytes.data()), &length,
					reinterpret_cast<const Bytef*>(raw->buffer), static_cast<uLong>(raw->compressedLen)) != Z_OK ||
					length != expected)
				{
					texturePackCache.bytes.clear();
				}
			}
			IndexTexturePack();
			texturePackStage.store(2, std::memory_order_release);
		}

		std::string ReadLobbyAsset(const std::string& assetName, const std::string& diskPath)
		{
			// The pack is owned by this component and survives database/device reloads.
			// Do not turn a temporary database unavailability into a missing texture.
			if (assetName.starts_with("lobby/textures/") && texturePackStage.load(std::memory_order_acquire) == 2)
			{
				const auto found = texturePackCache.entries.find(assetName.substr(15));
				if (found != texturePackCache.entries.end())
					return texturePackCache.bytes.substr(found->second.first, found->second.second);
			}
			if (Game::DB_IsZoneLoaded("zw3_lobby") && !Game::CL_IsCgameInitialized() &&
				*reinterpret_cast<Game::connstate_t*>(0xB2C540) < Game::CA_CONNECTING &&
				Game::Sys_IsDatabaseReady())
			{
				if (assetName.starts_with("lobby/textures/"))
				{
					IndexTexturePack();
					auto& pack = texturePackCache.bytes;
					auto& entries = texturePackCache.entries;
					if (const auto found = entries.find(assetName.substr(15)); found != entries.end())
						return pack.substr(found->second.first, found->second.second);
				}
				FileSystem::RawFile asset(assetName);
				if (asset)
				{
					auto bytes = std::move(asset.getBuffer());
					if (assetName.ends_with(".json"))
					{
						while (!bytes.empty() && bytes.back() == '\0') bytes.pop_back();
					}
					return bytes;
				}
			}
			return Utils::IO::ReadFile(diskPath);
		}

		void LoadTheaterVision()
		{
			const auto data = ReadLobbyAsset("lobby/vision/mp_zombie_theater.vision",
				"usermaps/mp_zombie_theater/vision/mp_zombie_theater.vision");
			if (data.empty()) return;
			std::istringstream stream(data);
			std::string line;
			while (std::getline(stream, line))
			{
				const auto firstQuote = line.find('"');
				if (firstQuote == std::string::npos) continue;
				const auto endQuote = line.find('"', firstQuote + 1);
				if (endQuote == std::string::npos) continue;
				const auto value = line.substr(firstQuote + 1, endQuote - firstQuote - 1);
				if (line.starts_with("r_filmContrast"))
				{
					filmContrast[0] = std::clamp(std::strtof(value.c_str(), nullptr), 0.5f, 2.0f);
				}
				else if (line.starts_with("r_filmBrightness"))
				{
					filmBrightness[0] = std::clamp(std::strtof(value.c_str(), nullptr), -0.5f, 0.5f);
				}
				else if (line.starts_with("r_glowBloomCutoff"))
				{
					glowSettings[0] = std::clamp(std::strtof(value.c_str(), nullptr), 0.0f, 1.0f);
				}
				else if (line.starts_with("r_glowBloomIntensity0"))
				{
					glowSettings[1] = std::clamp(std::strtof(value.c_str(), nullptr), 0.0f, 2.0f);
				}
				else
				{
					float* tint = nullptr;
					if (line.starts_with("r_filmLightTint")) tint = filmLightTint;
					else if (line.starts_with("r_filmMediumTint")) tint = filmMediumTint;
					else if (line.starts_with("r_filmDarkTint")) tint = filmDarkTint;
					if (tint)
					{
						float r = 1.0f, g = 1.0f, b = 1.0f;
						if (std::sscanf(value.c_str(), "%f %f %f", &r, &g, &b) == 3)
						{
							tint[0] = std::clamp(r, 0.0f, 2.0f);
							tint[1] = std::clamp(g, 0.0f, 2.0f);
							tint[2] = std::clamp(b, 0.0f, 2.0f);
						}
					}
				}
			}
		}

		bool LoadRoomMesh()
		{
			const auto data = ReadLobbyAsset("lobby/theater_room.zwlb", "zw3/core/lobby/theater_room.zwlb");
			if (data.size() < 12 || std::memcmp(data.data(), "ZWLB", 4) != 0)
			{
				return false;
			}

			std::uint32_t version = 0, vertexCount = 0, vertexStride = 0;
			std::memcpy(&version, data.data() + 4, sizeof(version));
			std::memcpy(&vertexCount, data.data() + 8, sizeof(vertexCount));
			if (data.size() >= 16) std::memcpy(&vertexStride, data.data() + 12, sizeof(vertexStride));
			const auto expectedStride = version == 3 ? sizeof(RoomVertex) : sizeof(Vertex);
			const auto expectedSize = 16ull + vertexCount * expectedStride;
			if ((version != 2 && version != 3) || vertexStride != expectedStride || vertexCount == 0 || vertexCount % 3 != 0 ||
				vertexCount > 1800000 || data.size() < expectedSize || data.size() > expectedSize + 1 ||
				(data.size() == expectedSize + 1 && data.back() != '\0'))
			{
				return false;
			}

			const auto manifestBytes = ReadLobbyAsset("lobby/theater_room.json", "zw3/core/lobby/theater_room.json");
			if (manifestBytes.empty()) return false;
			try
			{
				const auto manifest = nlohmann::json::parse(manifestBytes);
				if (manifest.at("version").get<int>() != static_cast<int>(version) || !manifest.at("groups").is_array()) return false;

				if (manifest.contains("camera"))
				{
					const auto& cam = manifest["camera"];
					if (cam.contains("eye") && cam["eye"].is_array() && cam["eye"].size() == 3)
						currentCameraEye = { cam["eye"][0].get<float>(), cam["eye"][1].get<float>(), cam["eye"][2].get<float>() };
					if (cam.contains("target") && cam["target"].is_array() && cam["target"].size() == 3)
						currentCameraTarget = { cam["target"][0].get<float>(), cam["target"][1].get<float>(), cam["target"][2].get<float>() };
					if (cam.contains("pan") && cam["pan"].is_array() && cam["pan"].size() == 3)
						currentCameraPan = { cam["pan"][0].get<float>(), cam["pan"][1].get<float>(), cam["pan"][2].get<float>() };
				}
				else
				{
					currentCameraEye = { 0.0f, -1450.0f, 192.0f };
					currentCameraTarget = { 0.0f, -600.0f, 182.0f };
					currentCameraPan = { 4.0f, 3.0f, 0.0f };
				}

				skyTextureName = manifest.value("skyTexture", "");
				if (skyTextureName.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_~$#&-.") != std::string::npos ||
					skyTextureName.find("..") != std::string::npos) skyTextureName.clear();

				roomGroups.clear();
				UINT expectedFirst = 0;
				for (const auto& group : manifest.at("groups"))
				{
					const auto first = group.at("firstVertex").get<UINT>();
					const auto count = group.at("vertexCount").get<UINT>();
					const auto textureName = group.value("texture", "");
					const auto blend = group.value("blend", false);
					const auto lightmapIndex = group.value("lightmapIndex", -1);
					if (first != expectedFirst || expectedFirst > vertexCount || count == 0 || count % 3 || count > vertexCount - expectedFirst ||
						lightmapIndex < -1 || lightmapIndex >= static_cast<int>(roomLightmaps.size()) ||
						(!textureName.empty() && textureName.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_~$#&-.") != std::string::npos))
					{
						roomGroups.clear();
						return false;
					}
					roomGroups.push_back({ first, count, textureName, blend, false, 0, 0, nullptr, lightmapIndex });
					const auto normalName = group.value("normalTexture", "");
					if (normalName.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_~$#&-.") == std::string::npos &&
						normalName.find("..") == std::string::npos) roomGroups.back().normalTextureName = normalName;
					roomGroups.back().alphaTest = group.value("alphaTest", blend);
					roomGroups.back().srcBlend = MaterialBlendFactor(group.value("srcBlend", "srcalpha"));
					roomGroups.back().dstBlend = MaterialBlendFactor(group.value("dstBlend", "invsrcalpha"));
					roomGroups.back().alphaThreshold = std::clamp(group.value("alphaThreshold",
						roomGroups.back().alphaTest ? 0.05f : -1.0f), -1.0f, 1.0f);
					roomGroups.back().multiplicative = group.value("multiplicative", false);
					roomGroups.back().depthWrite = group.value("depthWrite", !blend);
					roomGroups.back().gammaWrite = group.value("gammaWrite", false);
					expectedFirst += count;
				}
				if (expectedFirst != vertexCount)
				{
					roomGroups.clear();
					return false;
				}
			}
			catch (const std::exception&)
			{
				roomGroups.clear();
				return false;
			}
			if (roomGroups.empty()) return false;

			roomVertices.resize(vertexCount);
			if (version == 3)
			{
				std::memcpy(roomVertices.data(), data.data() + 16, vertexCount * sizeof(RoomVertex));
			}
			else
			{
				for (auto i = 0u; i < vertexCount; ++i)
				{
					Vertex old{};
					std::memcpy(&old, data.data() + 16 + i * sizeof(Vertex), sizeof(Vertex));
					roomVertices[i] = { old.x, old.y, old.z, old.color, old.u, old.v, 0.0f, 0.0f };
				}
			}

			doorLeftVertices.clear();
			doorRightVertices.clear();
			doorLeftGroups.clear();
			doorRightGroups.clear();

			std::vector<RoomVertex> filteredRoomVertices;
			filteredRoomVertices.reserve(roomVertices.size());
			std::vector<DrawGroup> filteredRoomGroups;
			filteredRoomGroups.reserve(roomGroups.size());

			const auto isDoorLeft = [](const RoomVertex& pt) {
				return pt.x >= -60.1f && pt.x <= 0.05f && pt.y >= -519.0f && pt.y <= -509.5f && pt.z >= 79.5f && pt.z <= 184.5f;
			};
			const auto isDoorRight = [](const RoomVertex& pt) {
				return pt.x >= -0.05f && pt.x <= 60.1f && pt.y >= -519.0f && pt.y <= -509.5f && pt.z >= 79.5f && pt.z <= 184.5f;
			};

			for (const auto& group : roomGroups)
			{
				const UINT groupFirst = group.firstVertex;
				const UINT groupCount = group.vertexCount;

				DrawGroup staticGroup = group;
				staticGroup.firstVertex = static_cast<UINT>(filteredRoomVertices.size());
				staticGroup.vertexCount = 0;

				DrawGroup leftGroup = group;
				leftGroup.firstVertex = static_cast<UINT>(doorLeftVertices.size());
				leftGroup.vertexCount = 0;

				DrawGroup rightGroup = group;
				rightGroup.firstVertex = static_cast<UINT>(doorRightVertices.size());
				rightGroup.vertexCount = 0;

				for (UINT i = 0; i < groupCount; i += 3)
				{
					const auto& v0 = roomVertices[groupFirst + i + 0];
					const auto& v1 = roomVertices[groupFirst + i + 1];
					const auto& v2 = roomVertices[groupFirst + i + 2];

					if (isDoorLeft(v0) && isDoorLeft(v1) && isDoorLeft(v2))
					{
						doorLeftVertices.push_back(v0);
						doorLeftVertices.push_back(v1);
						doorLeftVertices.push_back(v2);
						leftGroup.vertexCount += 3;
					}
					else if (isDoorRight(v0) && isDoorRight(v1) && isDoorRight(v2))
					{
						doorRightVertices.push_back(v0);
						doorRightVertices.push_back(v1);
						doorRightVertices.push_back(v2);
						rightGroup.vertexCount += 3;
					}
					else
					{
						auto v0_out = v0;
						auto v1_out = v1;
						auto v2_out = v2;
						filteredRoomVertices.push_back(v0_out);
						filteredRoomVertices.push_back(v1_out);
						filteredRoomVertices.push_back(v2_out);
						staticGroup.vertexCount += 3;
					}
				}

				if (staticGroup.vertexCount > 0)
				{
					filteredRoomGroups.push_back(staticGroup);
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

			roomVertices = std::move(filteredRoomVertices);
			roomGroups = std::move(filteredRoomGroups);


			return true;
		}

		void LoadPropMesh()
		{
			propVertices.clear();
			propIndices.clear();
			propGroups.clear();
			const auto data = ReadLobbyAsset("lobby/theater_props.zwlb", "zw3/core/lobby/theater_props.zwlb");
			const auto manifestBytes = ReadLobbyAsset("lobby/theater_props.json", "zw3/core/lobby/theater_props.json");
			if (data.size() < 16 || manifestBytes.empty() ||
				std::memcmp(data.data(), "ZWLB", 4) != 0) return;
			std::uint32_t version = 0, vertexCount = 0, vertexStride = 0;
			std::memcpy(&version, data.data() + 4, sizeof(version));
			std::memcpy(&vertexCount, data.data() + 8, sizeof(vertexCount));
			std::memcpy(&vertexStride, data.data() + 12, sizeof(vertexStride));
			std::uint32_t indexCount = 0;
			if (version == 4 && data.size() >= 20) std::memcpy(&indexCount, data.data() + 16, 4);
			const auto headerSize = version == 4 ? 20ull : 16ull;
			const auto expectedSize = headerSize + vertexCount * sizeof(Vertex) + 4ull * indexCount;
			if ((version != 2 && version != 4) || vertexStride != sizeof(Vertex) || vertexCount == 0 ||
				(version == 2 && vertexCount % 3) || vertexCount > 3000000 ||
				(version == 4 && (!indexCount || indexCount % 3 || indexCount > 6000000)) || data.size() < expectedSize ||
				data.size() > expectedSize + 1 ||
				(data.size() == expectedSize + 1 && data.back() != '\0')) return;
			try
			{
				const auto manifest = nlohmann::json::parse(manifestBytes);
				if (manifest.at("version").get<unsigned>() != version || !manifest.at("groups").is_array()) return;
				UINT expectedFirst = 0;
				UINT expectedIndex = 0;
				std::vector<std::uint32_t> indices(indexCount);
				if (indexCount) std::memcpy(indices.data(), data.data() + headerSize + vertexCount * sizeof(Vertex), indexCount * 4ull);
				for (const auto& group : manifest.at("groups"))
				{
					const auto first = group.at("firstVertex").get<UINT>();
					const auto count = group.at("vertexCount").get<UINT>();
					const auto name = group.at("texture").get<std::string>();
					const auto& cell = group.at("cell");
					const auto firstIndex = group.value("firstIndex", 0u);
					const auto numIndices = group.value("indexCount", 0u);
					if (first != expectedFirst || !count || (version == 2 && count % 3) || count > vertexCount - expectedFirst ||
						(version == 4 && (firstIndex != expectedIndex || !numIndices || numIndices % 3 || numIndices > indexCount - expectedIndex)) ||
						name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_~$#&-.") != std::string::npos ||
						name.find("..") != std::string::npos || !cell.is_array() || cell.size() != 2)
					{
						propGroups.clear();
						return;
					}
					if (version == 4 && std::any_of(indices.begin() + firstIndex, indices.begin() + firstIndex + numIndices,
						[count](auto index) { return index >= count; }))
					{
						propGroups.clear();
						return;
					}
					propGroups.push_back({ first, count, name, group.value("blend", false), true,
						cell[0].get<int>(), cell[1].get<int>() });
					propGroups.back().firstIndex = firstIndex;
					propGroups.back().indexCount = numIndices;
					propGroups.back().alphaTest = group.value("alphaTest", true);
					propGroups.back().alphaThreshold = std::clamp(group.value("alphaThreshold", 0.05f), -1.0f, 1.0f);
					propGroups.back().srcBlend = MaterialBlendFactor(group.value("srcBlend", "srcalpha"));
					propGroups.back().dstBlend = MaterialBlendFactor(group.value("dstBlend", "invsrcalpha"));
					propGroups.back().multiplicative = group.value("multiplicative", false);
					propGroups.back().depthWrite = group.value("depthWrite", !propGroups.back().blend);
					propGroups.back().gammaWrite = group.value("gammaWrite", false);
					expectedFirst += count;
					expectedIndex += numIndices;
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
			std::memcpy(propVertices.data(), data.data() + headerSize, vertexCount * sizeof(Vertex));
		}

		void LoadActorMesh(const size_t index, const char* name)
		{
			const auto stem = std::string("lobby/actors/") + name;
			const auto data = ReadLobbyAsset(stem + ".zwlb", "zw3/core/lobby/actors/" + std::string(name) + ".zwlb");
			const auto manifestBytes = ReadLobbyAsset(stem + ".json", "zw3/core/lobby/actors/" + std::string(name) + ".json");
			if (data.size() < 16 || manifestBytes.empty() || std::memcmp(data.data(), "ZWLB", 4)) return;
			std::uint32_t version = 0, count = 0, stride = 0;
			std::memcpy(&version, data.data() + 4, 4);
			std::memcpy(&count, data.data() + 8, 4);
			std::memcpy(&stride, data.data() + 12, 4);
			std::uint32_t frameCount = 1;
			if ((version == 3 || version == 4) && data.size() >= 20)
				std::memcpy(&frameCount, data.data() + 16, 4);
			const auto headerSize = version >= 3 ? 20ull : 16ull;
			const auto expected = version == 4 ? headerSize + static_cast<std::uint64_t>(count) * 12 +
				static_cast<std::uint64_t>(frameCount) * count * 6 :
				headerSize + static_cast<std::uint64_t>(frameCount) * count * sizeof(Vertex);
			if ((version != 2 && version != 3 && version != 4) ||
				stride != (version == 4 ? 6u : sizeof(Vertex)) || !count || count % 3 || count > 100000 ||
				!frameCount || frameCount > 128 || expected > 100'000'000 ||
				data.size() < expected || data.size() > expected + 1) return;
			auto& actor = actorMeshes[index];
			try
			{
				const auto manifest = nlohmann::json::parse(manifestBytes);
				if (manifest.at("version").get<std::uint32_t>() != version) return;
				actor.muzzleAnchors = {};
				if (manifest.contains("muzzleAnchors") && manifest["muzzleAnchors"].size() == 4)
				{
					for (size_t weapon = 0; weapon < 4; ++weapon)
					{
						const auto& source = manifest["muzzleAnchors"][weapon];
						auto& anchor = actor.muzzleAnchors[weapon];
						anchor.vertices = source.at("vertices").get<std::array<UINT, 3>>();
						anchor.u = source.at("u").get<float>();
						anchor.v = source.at("v").get<float>();
						anchor.w = source.at("w").get<float>();
						anchor.valid = std::all_of(anchor.vertices.begin(), anchor.vertices.end(),
							[count](UINT vertex) { return vertex < count; }) &&
							std::isfinite(anchor.u) && std::isfinite(anchor.v) && std::isfinite(anchor.w);
					}
				}
				UINT firstExpected = 0;
				for (const auto& group : manifest.at("groups"))
				{
					const auto first = group.at("firstVertex").get<UINT>();
					const auto length = group.at("vertexCount").get<UINT>();
					const auto texture = group.at("texture").get<std::string>();
					const auto weapon = group.value("weapon", -1);
					if (first != firstExpected || !length || length % 3 || length > count - firstExpected ||
						weapon < -1 || weapon > 3 ||
						texture.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_~$#&-.") != std::string::npos ||
						texture.find("..") != std::string::npos)
					{
						actor.groups.clear();
						return;
					}
					const auto isWeaponGroup = weapon >= 0;
					actor.groups.push_back({ first, length, texture, false, false, 0, 0, nullptr, -1, weapon });
					if (isWeaponGroup)
					{
						actor.groups.back().alphaTest = false;
						actor.groups.back().alphaThreshold = 0.0f;
					}
					firstExpected += length;
				}
				if (firstExpected != count)
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
						if (!length || first >= frameCount || length > frameCount - first) return;
						const auto clipName = clip.at("name").get<std::string>();
						const auto clipIndex = clipName == "attack" ? 9 : clipName == "melee" ? 8 : clipName == "point" ? 7 : clipName == "shotgun_fire" ? 6 :
							clipName == "rifle_fire" ? 5 : clipName == "rifle_idle" ? 4 :
							clipName == "walk" && index != 4 ? 3 : clipName == "death" ? 2 :
							clipName == "fire" || clipName == "run" ? 1 : 0;
						actor.clipDurationMs[clipIndex] = std::clamp(clip.value("durationMs", 0u), 0u, 10000u);
						if (clipName == "idle" || (clipName == "walk" && index == 4))
						{
							actor.idleFirst = first;
							actor.idleCount = length;
						}
						else if (clipName == "walk")
						{
							actor.walkFirst = first;
							actor.walkCount = length;
						}
						else if (clipName == "fire" || clipName == "run")
						{
							actor.actionFirst = first;
							actor.actionCount = length;
						}
						else if (clipName == "melee")
						{
							actor.meleeFirst = first;
							actor.meleeCount = length;
						}
						else if (clipName == "attack")
						{
							actor.attackFirst = first;
							actor.attackCount = length;
						}
						else if (clipName == "point")
						{
							actor.pointFirst = first;
							actor.pointCount = length;
						}
						else if (clipName == "death")
						{
							actor.deathFirst = first;
							actor.deathCount = length;
						}
						else if (clipName == "rifle_idle")
						{
							actor.rifleIdleFirst = first;
							actor.rifleIdleCount = length;
						}
						else if (clipName == "rifle_fire")
						{
							actor.rifleFireFirst = first;
							actor.rifleFireCount = length;
						}
						else if (clipName == "shotgun_fire")
						{
							actor.shotgunFireFirst = first;
							actor.shotgunFireCount = length;
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
			for (auto frame = 0u; frame < frameCount; ++frame)
			{
				actor.frames[frame].resize(count);
				if (version == 4)
				{
					const auto* attributes = reinterpret_cast<const unsigned char*>(data.data() + headerSize);
					const auto* positions = reinterpret_cast<const unsigned char*>(data.data() + headerSize +
						static_cast<size_t>(count) * 12 + static_cast<size_t>(frame) * count * 6);
					for (auto vertex = 0u; vertex < count; ++vertex)
					{
						auto& output = actor.frames[frame][vertex];
						std::int16_t xyz[3];
						std::memcpy(xyz, positions + static_cast<size_t>(vertex) * 6, sizeof(xyz));
						output.x = static_cast<float>(xyz[0]) / 256.0f;
						output.y = static_cast<float>(xyz[1]) / 256.0f;
						output.z = static_cast<float>(xyz[2]) / 256.0f;
						std::memcpy(&output.color, attributes + static_cast<size_t>(vertex) * 12, 12);
					}
				}
				else
				{
					std::memcpy(actor.frames[frame].data(), data.data() + headerSize +
						static_cast<size_t>(frame) * count * sizeof(Vertex), count * sizeof(Vertex));
				}
			}
			actor.interpolated.resize(count);
			actor.blended.resize(count);
		}

		void EnsureRoomVertexBuffer(IDirect3DDevice9* device)
		{
			if (roomVertexBuffer || roomVertices.empty() || !device) return;
			const auto bytes = roomVertices.size() * sizeof(RoomVertex);
			if (bytes && bytes <= std::numeric_limits<UINT>::max() &&
				SUCCEEDED(device->CreateVertexBuffer(static_cast<UINT>(bytes), D3DUSAGE_WRITEONLY,
					0, D3DPOOL_DEFAULT, &roomVertexBuffer, nullptr)))
			{
				void* destination = nullptr;
				if (SUCCEEDED(roomVertexBuffer->Lock(0, 0, &destination, 0)))
				{
					std::memcpy(destination, roomVertices.data(), bytes);
					roomVertexBuffer->Unlock();
				}
				else
				{
					roomVertexBuffer->Release();
					roomVertexBuffer = nullptr;
				}
			}
		}

		void RefreshLobbyMaterial()
		{
			if (Dedicated::IsEnabled() || ZoneBuilder::IsEnabled() ||
				!FastFiles::Ready() ||
				!*Game::dx_ptr || Renderer::Width() <= 0 || Renderer::Height() <= 0 ||
				Renderer::IsDeviceRecoveryActive()) return;

			if (!roomImage)
			{
				textureWidth = static_cast<unsigned>(std::clamp(Renderer::Width(), 640, 3840));
				textureHeight = static_cast<unsigned>(std::clamp(Renderer::Height(), 360, 2160));
				roomImage = Materials::CreateImage("zw3_lobby_scene_image", textureWidth, textureHeight,
					1, 0x1000003, D3DFMT_A8R8G8B8);
			}

			auto* entry = Game::DB_FindXAssetEntry(Game::ASSET_TYPE_MATERIAL, "white");
			auto* baseMaterial = entry ? entry->asset.header.material : nullptr;
			if (!baseMaterial) return;

			if (!roomMaterial)
			{
				if (roomImage) roomMaterial = Materials::Create("zw3_lobby_scene", roomImage);
			}
			else if (materialNeedsRefresh && baseMaterial->techniqueSet)
			{
				roomMaterial->techniqueSet = baseMaterial->techniqueSet;
				roomMaterial->stateBitsTable = baseMaterial->stateBitsTable;
				roomMaterial->stateBitsCount = baseMaterial->stateBitsCount;
				if (roomMaterial->textureTable)
				{
					roomMaterial->textureTable->u.image = roomImage;
				}
			}
			if (roomMaterial) materialNeedsRefresh = false;
		}

		// Retain fallback animation expressions so recovery/loading can restore the original background.
		struct BackgroundAnimation { int expressionCount; float opacity; };
		std::unordered_map<Game::itemDef_s*, BackgroundAnimation> backgroundExpressionCounts;

		void AttachMaterialToLobby(const char* name, const bool sceneReady = true)
		{
			auto* menu = Game::Menus_FindByName(Game::uiContext, name);
			if (!menu) return;
			for (auto i = 0; i < menu->itemCount; ++i)
			{
				auto* item = menu->items[i];
				if (!item) continue;
				const char* winName = item->window.name ? item->window.name : "";
				const char* bgName = (item->window.background && item->window.background->info.name)
					? item->window.background->info.name : "";

				if (!_stricmp(winName, "zw3_lobby_theater_background") ||
					!_stricmp(winName, "main_text_background") ||
					!_stricmp(bgName, "mw2_main_co_image") ||
					!_stricmp(bgName, "mw2_main_background"))
				{
					if (sceneReady && roomMaterial && roomImage && roomImage->texture.map)
					{
						item->window.background = roomMaterial;
						item->window.foreColor[3] = 1.0f;
					}
					else
					{
						auto* entry = FastFiles::Ready() ? Game::DB_FindXAssetEntry(Game::ASSET_TYPE_MATERIAL, "mw2_main_co_image") : nullptr;
						if (entry && entry->asset.header.material)
							item->window.background = entry->asset.header.material;
						item->window.foreColor[3] = 0.35f;
					}
				}
				else if (!_stricmp(winName, "zw3_lobby_fallback_cloud") ||
					!_stricmp(winName, "main_text_cloud") ||
					strstr(winName, "cloud") ||
					strstr(bgName, "cloud") ||
					(item->window.rectClient.w >= 640.0f && item->window.rectClient.h >= 480.0f &&
						(!_stricmp(bgName, "black") || !_stricmp(bgName, "mockup_bg_glow"))))
				{
					if (sceneReady)
					{
						backgroundExpressionCounts.try_emplace(item, BackgroundAnimation{item->floatExpressionCount, item->window.foreColor[3]});
						// An exp foreColor a otherwise restores the black pulse during UI painting.
						item->floatExpressionCount = 0;
						item->window.foreColor[3] = 0.0f;
					}
					else
					{
						if (const auto original = backgroundExpressionCounts.find(item); original != backgroundExpressionCounts.end())
						{
							item->floatExpressionCount = original->second.expressionCount;
							item->window.foreColor[3] = original->second.opacity;
						}
					}
				}
			}
		}

		bool IsLobbyVisible(const char* name)
		{
			auto* menu = Game::Menus_FindByName(Game::uiContext, name);
			return menu && Game::Menu_IsVisible(Game::uiContext, menu);
		}

		void UpdateMenu()
		{
			static bool wasStartupCinematic = false;
			static unsigned cinematicEndTime = 0;
			if (startupLoading.load(std::memory_order_acquire))
			{
				const bool cinematic = IsCinematicActive();
				if (cinematic) cinematicEndTime = 0;
				else if (wasStartupCinematic) cinematicEndTime = timeGetTime();
				wasStartupCinematic = cinematic;
				if (!cinematic && cinematicEndTime && timeGetTime() - cinematicEndTime >= 2000u)
					startupLoading.store(false, std::memory_order_release);
			}
			else
			{
				wasStartupCinematic = false;
				cinematicEndTime = 0;
			}
			// Wrap the existing engine command, retaining its normal launch semantics.
			if (!originalPartyGo)
			{
				if (auto* command = Command::Find("xpartygo"))
				{
					originalPartyGo = command->function;
					command->function = []
					{
						if (!LobbyScene::DeferLaunch([] { Command::Execute("xpartygo"); }))
							originalPartyGo();
					};
				}
			}

			const auto state = *reinterpret_cast<Game::connstate_t*>(0xB2C540);
			if (state >= Game::CA_CONNECTING || Game::CL_IsCgameInitialized())
			{
				startupLoading.store(false, std::memory_order_release);
				if (LobbyScene::IsTransitionActive())
				{
					sawConnectingState.store(true, std::memory_order_release);
					if (timeGetTime() - transitionStartTime.load() > 25000u ||
						(sawConnectingState.load() && (state == Game::CA_ACTIVE || state == Game::CA_DISCONNECTED)))
					{
						LobbyScene::StopTransition();
					}
				}
				lobbyVisible.store(false, std::memory_order_release);
				zombieWaveStarted.store(false, std::memory_order_release);
				return;
			}

			if (LobbyScene::IsTransitionActive())
			{
				return;
			}

			const auto sceneReady = frameReady.load(std::memory_order_acquire) && roomMaterial &&
				!roomVertices.empty();
			AttachMaterialToLobby("menu_xboxlive_privatelobby", sceneReady);
			AttachMaterialToLobby("zwnet_matchmaking", sceneReady);
			AttachMaterialToLobby("pregame_loaderror", sceneReady);
			AttachMaterialToLobby("main_text", sceneReady);

			PrepareTexturePack();
			LobbyScene::PrepareStartup();

			const auto privateVisible = IsLobbyVisible("menu_xboxlive_privatelobby");
			const auto matchmakingVisible = IsLobbyVisible("zwnet_matchmaking");
			const auto inLobby = privateVisible || matchmakingVisible;
			static bool hadLobby = false;
			if (inLobby) hadLobby = true;
			else if (hadLobby && (IsLobbyVisible("main_text") || IsLobbyVisible("pregame_loaderror")))
			{
				// Leaving a frontend lobby need not call CL_Disconnect. Reset on its return to the main menu.
				zombieWaveStarted.store(false, std::memory_order_release);
				lobbySession.fetch_add(1, std::memory_order_release);
				hadLobby = false;
			}
			const auto visible = inLobby || IsLobbyVisible("pregame_loaderror") || IsLobbyVisible("main_text");
			closeCamera.store(inLobby, std::memory_order_release);
			lobbyVisible.store(visible, std::memory_order_release);

			static bool wasLobbyVisible = false;
			if (visible && !wasLobbyVisible)
			{
				lobbySession.fetch_add(1, std::memory_order_release);
				cameraResetSession.fetch_add(1, std::memory_order_release);
			}
			wasLobbyVisible = visible;

			if (!visible)
			{
				return;
			}

			if (startupLoading.load(std::memory_order_acquire) && !Renderer::IsDeviceRecoveryActive())
			{
				if (!startupWaitStart) startupWaitStart = timeGetTime();
				if (sceneReady || timeGetTime() - startupWaitStart >= 12000u)
					startupLoading.store(false, std::memory_order_release);
			}

			// Begin the wave when the title / press-any-key scene is actually shown.
			if (sceneReady && !Renderer::IsDeviceRecoveryActive() && !startupLoading.load(std::memory_order_acquire) &&
				(IsLobbyVisible("main_text") || inLobby || IsLobbyVisible("pregame_loaderror")))
				zombieWaveStarted.store(true, std::memory_order_release);

			if (inLobby)
			{
				const auto* countDvar = Game::Dvar_FindVar(privateVisible ?
					"party_currentPlayers" : "zwnet_lobby_member_count");
				const auto count = countDvar && countDvar->type == Game::DVAR_TYPE_INT ?
					countDvar->current.integer : 1;
				const auto publishedCount = std::clamp(count, 0, 4);
				int occupiedCount = 0;
				std::array<int, 4> models{ -1, -1, -1, -1 };
				static constexpr const char* characterDvars[] =
				{
					"character_1", "character_2", "character_3", "character_4"
				};
				static constexpr const char* characterNames[] =
				{
					"Richtofen", "Dempsey", "Nikolai", "Takeo"
				};
				for (auto slot = 0u; slot < lobbyCharacterModels.size(); ++slot)
				{
					if (slot >= static_cast<unsigned>(publishedCount)) break;
					// Private-party snapshots explicitly publish None for vacated slots.
					// Never substitute a default actor for a departed player.
					auto modelIndex = privateVisible ? -1 : static_cast<int>(slot);
					const auto* owner = Game::Dvar_FindVar(Utils::String::VA("character_%u_player", slot + 1));
					if (privateVisible && owner && owner->type == Game::DVAR_TYPE_STRING &&
						(!owner->current.string || !owner->current.string[0] || !_stricmp(owner->current.string, "None"))) continue;
					const auto* character = Game::Dvar_FindVar(characterDvars[slot]);
					if (character && character->type == Game::DVAR_TYPE_STRING && character->current.string)
					{
						for (auto candidate = 0u; candidate < std::size(characterNames); ++candidate)
						{
							if (!_stricmp(character->current.string, characterNames[candidate]))
							{
								modelIndex = static_cast<int>(candidate);
								break;
							}
						}
					}
					if (modelIndex >= 0) models[occupiedCount++] = modelIndex;
				}
				for (auto slot = 0u; slot < models.size(); ++slot)
					lobbyCharacterModels[slot].store(models[slot], std::memory_order_release);
				lobbyCharacterCount.store(occupiedCount, std::memory_order_release);
			}
			else
			{
				// Leaving a lobby must not leave its remote players/bots in the menu scene.
				for (auto slot = 1u; slot < lobbyCharacterModels.size(); ++slot)
					lobbyCharacterModels[slot].store(-1, std::memory_order_release);
				if (lobbyCharacterModels[0].load(std::memory_order_acquire) < 0)
					lobbyCharacterModels[0].store(0, std::memory_order_release);
				lobbyCharacterCount.store(1, std::memory_order_release);
			}
		}

		void ReleaseDepth()
		{
			if (savedState)
			{
				savedState->Release();
				savedState = nullptr;
			}
			if (sceneTexture)
			{
				sceneTexture->Release();
				sceneTexture = nullptr;
			}
			if (roomDepth)
			{
				roomDepth->Release();
				roomDepth = nullptr;
			}
			if (roomImage && roomImage->texture.map)
			{
				roomImage->texture.map->Release();
				roomImage->texture.map = nullptr;
			}
		}

		void ReleaseTextures()
		{
			if (savedState)
			{
				savedState->Release();
				savedState = nullptr;
			}
			if (roomVertexBuffer) roomVertexBuffer->Release();
			if (propVertexBuffer) propVertexBuffer->Release();
			if (propIndexBuffer) propIndexBuffer->Release();
			propIndexBuffer = nullptr;
			roomVertexBuffer = propVertexBuffer = nullptr;
			if (visionShader)
			{
				visionShader->Release();
				visionShader = nullptr;
			}
			if (lightmapFilmShader)
			{
				lightmapFilmShader->Release();
				lightmapFilmShader = nullptr;
			}
			if (filmShader)
			{
				filmShader->Release();
				filmShader = nullptr;
			}
			if (skyTexture)
			{
				skyTexture->Release();
				skyTexture = nullptr;
			}
			for (auto& [name, texture] : loadedTextures)
			{
				if (texture) texture->Release();
			}
			loadedTextures.clear();
			pendingTextureNames.clear();
			nextTexture = 0;
			renderResourcesReady = false;
			for (auto& lightmap : roomLightmaps)
			{
				if (lightmap) lightmap->Release();
				lightmap = nullptr;
			}
			const auto clearGroups = [](auto& groups)
			{
				for (auto& group : groups) group.texture = group.normalTexture = nullptr;
			};
			clearGroups(roomGroups);
			clearGroups(doorLeftGroups);
			clearGroups(doorRightGroups);
			clearGroups(propGroups);
			for (auto& actor : actorMeshes)
			{
				clearGroups(actor.groups);
			}
		}

		bool EnsureRenderTarget(IDirect3DDevice9* device)
		{
			if (!Game::Sys_IsDatabaseReady() || !FastFiles::Ready() || !Game::DB_IsZoneLoaded("zw3_lobby")) return false;
			if (texturePackStage.load(std::memory_order_acquire) != 2) return false;
			if (!roomImage) return false;
			const auto width = static_cast<unsigned>(std::clamp(Renderer::Width(), 640, 3840));
			const auto height = static_cast<unsigned>(std::clamp(Renderer::Height(), 360, 2160));
			if (width != textureWidth || height != textureHeight)
			{
				frameReady.store(false, std::memory_order_release);
				ReleaseDepth();
				textureWidth = width;
				textureHeight = height;
				roomImage->width = static_cast<unsigned short>(width);
				roomImage->height = static_cast<unsigned short>(height);
			}
			if (!roomDepth)
			{
				IDirect3DTexture9* target = nullptr;
				if (FAILED(device->CreateTexture(textureWidth, textureHeight, 1,
					D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT,
					&target, nullptr))) return false;

				if (FAILED(device->CreateTexture(textureWidth, textureHeight, 1,
					D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT,
					&sceneTexture, nullptr)))
				{
					target->Release();
					return false;
				}

				HRESULT hrDepth = device->CreateDepthStencilSurface(textureWidth, textureHeight,
					D3DFMT_D24S8, D3DMULTISAMPLE_NONE, 0, TRUE, &roomDepth, nullptr);
				if (FAILED(hrDepth))
				{
					hrDepth = device->CreateDepthStencilSurface(textureWidth, textureHeight,
						D3DFMT_D24X8, D3DMULTISAMPLE_NONE, 0, TRUE, &roomDepth, nullptr);
				}
				if (FAILED(hrDepth))
				{
					hrDepth = device->CreateDepthStencilSurface(textureWidth, textureHeight,
						D3DFMT_D16, D3DMULTISAMPLE_NONE, 0, TRUE, &roomDepth, nullptr);
				}
				if (FAILED(hrDepth))
				{
					sceneTexture->Release();
					sceneTexture = nullptr;
					target->Release();
					return false;
				}
				if (roomImage->texture.map) roomImage->texture.map->Release();
				roomImage->texture.map = target;
				roomImage->cardMemory = {};
			}
			if (!savedState)
			{
				device->CreateStateBlock(D3DSBT_ALL, &savedState);
			}
			if (renderResourcesReady) return roomImage->texture.map != nullptr;
			if (pendingTextureNames.empty())
			{
				std::unordered_set<std::string> seen;
				const auto queueGroups = [&seen](const std::vector<DrawGroup>& groups)
				{
					for (const auto& group : groups)
					{
						if (!group.textureName.empty() && seen.emplace(group.textureName).second)
							pendingTextureNames.push_back(group.textureName);
						if (!group.normalTextureName.empty() && seen.emplace(group.normalTextureName).second)
							pendingTextureNames.push_back(group.normalTextureName);
					}
				};
				queueGroups(roomGroups);
				queueGroups(doorLeftGroups);
				queueGroups(doorRightGroups);
				queueGroups(propGroups);
				for (const auto& actor : actorMeshes) queueGroups(actor.groups);
				if (seen.emplace("zombie_eye_flare.dds").second)
					pendingTextureNames.push_back("zombie_eye_flare.dds");
			}
			const auto uploadStart = timeGetTime();
			unsigned uploaded = 0;
			const auto preparing = startupLoading.load(std::memory_order_acquire);
			while (nextTexture < pendingTextureNames.size() && uploaded < (preparing ? 16u : 2u) &&
				(uploaded == 0 || timeGetTime() - uploadStart < (preparing ? 4u : 2u)))
			{
				++uploaded;
				const auto& name = pendingTextureNames[nextTexture++];
				const auto data = ReadLobbyAsset("lobby/textures/" + name,
					"zw3/core/lobby/textures/" + name);
				IDirect3DTexture9* texture = nullptr;
				if (!data.empty()) D3DXCreateTextureFromFileInMemory(device,
					data.data(), static_cast<UINT>(data.size()), &texture);
				loadedTextures.emplace(name, texture);
			}
			if (nextTexture < pendingTextureNames.size()) return false;
			const auto bindGroups = [](std::vector<DrawGroup>& groups)
			{
				for (auto& group : groups)
				{
					if (const auto found = loadedTextures.find(group.textureName);
						found != loadedTextures.end()) group.texture = found->second;
					if (const auto found = loadedTextures.find(group.normalTextureName);
						found != loadedTextures.end()) group.normalTexture = found->second;
				}
			};
			bindGroups(roomGroups);
			bindGroups(doorLeftGroups);
			bindGroups(doorRightGroups);
			bindGroups(propGroups);
			for (auto& actor : actorMeshes) bindGroups(actor.groups);
			if (!roomVertexBuffer && !roomVertices.empty())
			{
				const auto bytes = roomVertices.size() * sizeof(RoomVertex);
				if (bytes && bytes <= std::numeric_limits<UINT>::max() &&
					SUCCEEDED(device->CreateVertexBuffer(static_cast<UINT>(bytes), D3DUSAGE_WRITEONLY,
						0, D3DPOOL_DEFAULT, &roomVertexBuffer, nullptr)))
				{
					void* destination = nullptr;
					if (SUCCEEDED(roomVertexBuffer->Lock(0, 0, &destination, 0)))
					{
						std::memcpy(destination, roomVertices.data(), bytes);
						roomVertexBuffer->Unlock();
					}
					else
					{
						roomVertexBuffer->Release();
						roomVertexBuffer = nullptr;
					}
				}
			}
			if (!propVertexBuffer && !propVertices.empty())
			{
				const auto bytes = propVertices.size() * sizeof(Vertex);
				if (bytes && bytes <= std::numeric_limits<UINT>::max() &&
					SUCCEEDED(device->CreateVertexBuffer(static_cast<UINT>(bytes), D3DUSAGE_WRITEONLY,
						0, D3DPOOL_DEFAULT, &propVertexBuffer, nullptr)))
				{
					void* destination = nullptr;
					if (SUCCEEDED(propVertexBuffer->Lock(0, 0, &destination, 0)))
					{
						std::memcpy(destination, propVertices.data(), bytes);
						propVertexBuffer->Unlock();
					}
					else
					{
						propVertexBuffer->Release();
						propVertexBuffer = nullptr;
					}
				}
			}
			if (!propIndexBuffer && !propIndices.empty())
			{
				const auto bytes = static_cast<UINT>(propIndices.size() * sizeof(std::uint32_t));
				if (SUCCEEDED(device->CreateIndexBuffer(bytes, D3DUSAGE_WRITEONLY, D3DFMT_INDEX32,
					D3DPOOL_DEFAULT, &propIndexBuffer, nullptr)))
				{
					void* destination = nullptr;
					if (SUCCEEDED(propIndexBuffer->Lock(0, 0, &destination, 0)))
					{
						std::memcpy(destination, propIndices.data(), bytes);
						propIndexBuffer->Unlock();
					}
					else
					{
						propIndexBuffer->Release();
						propIndexBuffer = nullptr;
					}
				}
			}
			if (!propIndices.empty() && (!propVertexBuffer || !propIndexBuffer)) return false;
			for (auto i = 0u; i < roomLightmaps.size(); ++i)
			{
				if (roomLightmaps[i]) continue;
				const auto name = std::format("_lightmap{}_secondary.dds", i);
				const auto data = ReadLobbyAsset("lobby/lightmaps/" + name,
					"zw3/core/lobby/lightmaps/" + name);
				if (!data.empty()) D3DXCreateTextureFromFileInMemory(device,
					data.data(), static_cast<UINT>(data.size()), &roomLightmaps[i]);
			}

			// Film shader for characters and props:
			static constexpr char filmSource[] = R"(
				sampler2D diffuseTexture : register(s0);
				float4 materialFlags : register(c0);
				float4 main(float2 uv : TEXCOORD0, float4 vertexColor : COLOR0) : COLOR0
				{
					float4 diffuse = tex2D(diffuseTexture, uv);
					// -1 is a normal material's disabled alpha cutoff, not an eye glow.
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
					if (materialFlags.x > 0.5f) shaded *= shaded;
					return float4(shaded, lerp(1.0f, diffuse.a, materialFlags.y));
				}
			)";
			ID3DXBuffer* shaderCode = nullptr;
			ID3DXBuffer* shaderErrors = nullptr;
			if (SUCCEEDED(D3DXCompileShader(filmSource, sizeof(filmSource) - 1,
				nullptr, nullptr, "main", "ps_2_0", 0, &shaderCode, &shaderErrors, nullptr)) && shaderCode)
			{
				device->CreatePixelShader(static_cast<const DWORD*>(shaderCode->GetBufferPointer()), &filmShader);
			}
			if (shaderErrors) shaderErrors->Release();
			if (shaderCode) shaderCode->Release();

			// IW4 ps_lm_fog_b0c0[n0]_sm3: secondary lightmaps are two stacked
			// lobes, not ordinary RGB irradiance. Alpha stores tangent-space slopes.
			// Match the native shader's linear output: both diffuse and light are
			// squared. IW4 postfx copies this value; it does not undo the square.
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
			shaderCode = nullptr;
			shaderErrors = nullptr;
			if (SUCCEEDED(D3DXCompileShader(lightmapFilmSource, sizeof(lightmapFilmSource) - 1,
				nullptr, nullptr, "main", "ps_2_0", 0, &shaderCode, &shaderErrors, nullptr)) && shaderCode)
			{
				device->CreatePixelShader(static_cast<const DWORD*>(shaderCode->GetBufferPointer()), &lightmapFilmShader);
			}
			if (shaderErrors) shaderErrors->Release();
			if (shaderCode) shaderCode->Release();

			// Vision shader applied in screen space from sceneTexture -> target:
			// Uses mp_zombie_theater.vision color grading constants and tight bloom
			static constexpr char visionSource[] = R"(
				sampler2D scene : register(s0);
				float4 contrast : register(c0);
				float4 lightTint : register(c1);
				float4 mediumTint : register(c2);
				float4 darkTint : register(c3);
				float4 exposure : register(c4);
				float4 brightness : register(c5);
				float4 glow : register(c6); // cutoff, intensity, dx, dy
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
			shaderCode = nullptr;
			shaderErrors = nullptr;
			if (SUCCEEDED(D3DXCompileShader(visionSource, sizeof(visionSource) - 1,
				nullptr, nullptr, "main", "ps_2_0", 0, &shaderCode, &shaderErrors, nullptr)) && shaderCode)
			{
				device->CreatePixelShader(static_cast<const DWORD*>(shaderCode->GetBufferPointer()), &visionShader);
			}
			if (shaderErrors) shaderErrors->Release();
			if (shaderCode) shaderCode->Release();

			if (!skyTextureName.empty())
			{
				const auto data = ReadLobbyAsset("lobby/textures/" + skyTextureName,
					"zw3/core/lobby/textures/" + skyTextureName);
				if (!data.empty())
				{
					D3DXCreateCubeTextureFromFileInMemory(device,
						data.data(), static_cast<UINT>(data.size()), &skyTexture);
				}
			}
			renderResourcesReady = true;
			return true;
		}

		D3DXVECTOR3 CameraPosition(float phase)
		{
			return {
				currentCameraEye.x + std::sin(phase) * currentCameraPan.x,
				currentCameraEye.y + std::cos(phase * 0.7f) * currentCameraPan.y,
				currentCameraEye.z,
			};
		}

		void DrawSky(IDirect3DDevice9* device, const D3DXVECTOR3& eye)
		{
			if (!skyTexture) return;
			constexpr float s = 1000.0f;
			const D3DXVECTOR3 corners[] =
			{
				{ -s, -s, -s }, { s, -s, -s }, { s, s, -s }, { -s, s, -s },
				{ -s, -s, s }, { s, -s, s }, { s, s, s }, { -s, s, s },
			};
			constexpr unsigned indices[] =
			{
				0, 1, 2, 0, 2, 3, 4, 6, 5, 4, 7, 6,
				0, 4, 5, 0, 5, 1, 1, 5, 6, 1, 6, 2,
				2, 6, 7, 2, 7, 3, 3, 7, 4, 3, 4, 0,
			};
			SkyVertex vertices[std::size(indices)]{};
			for (auto i = 0u; i < std::size(indices); ++i)
			{
				const auto& direction = corners[indices[i]];
				vertices[i] = { direction.x, direction.y, direction.z,
					0xff51545bu, direction.x / s, direction.y / s, direction.z / s };
			}
			D3DXMATRIX skyWorld;
			D3DXMatrixTranslation(&skyWorld, eye.x, eye.y, eye.z);
			device->SetTransform(D3DTS_WORLD, &skyWorld);
			device->SetRenderState(D3DRS_ZENABLE, FALSE);
			device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
			device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
			device->SetRenderState(D3DRS_LIGHTING, FALSE);
			device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
			device->SetRenderState(D3DRS_FOGENABLE, FALSE);
			device->SetTexture(0, skyTexture);
			device->SetTexture(1, nullptr);
			device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
			device->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1 | D3DFVF_TEXCOORDSIZE3(0));
			device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
			device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
			device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
			device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 12, vertices, sizeof(SkyVertex));
		}

		void RenderRoom(IDirect3DDevice9* device)
		{
			if (Renderer::IsDeviceRecoveryActive() || !device) return;

			const auto inTransition = theaterDirectTransitionActive.load(std::memory_order_acquire);
			const bool bootPreview = startupLoading.load(std::memory_order_acquire) &&
				Game::CL_GetLocalClientConnectionState(0) < Game::CA_CONNECTING;
			const auto transitionElapsed = timeGetTime() - transitionStartTime.load(std::memory_order_acquire);
			if (inTransition && transitionElapsed >= LobbyTransition::WhiteEndMs)
			{
				// The scene is obscured. Fade white to black without drawing the hidden world.
				const auto intensity = static_cast<unsigned>((1.0f - LobbyTransition::BlackOpacity(transitionElapsed)) * 255.0f);
				const auto color = D3DCOLOR_XRGB(intensity, intensity, intensity);
				if (SUCCEEDED(device->Clear(0, nullptr, D3DCLEAR_TARGET, color, 1.0f, 0)) &&
					transitionElapsed >= LobbyTransition::EndMs)
					fadePresented.store(true, std::memory_order_release);
				return;
			}

			if (!assetsReady.load(std::memory_order_acquire) || roomVertices.empty() || !roomImage) return;

			if (!EnsureRenderTarget(device)) return;
			// Warm resources and the first frame before a menu requests this material.
			if (!lobbyVisible.load(std::memory_order_acquire) && !inTransition &&
				!bootPreview && frameReady.load(std::memory_order_acquire)) return;

			IDirect3DSurface9* target = nullptr;
			IDirect3DSurface9* sceneTarget = nullptr;
			IDirect3DSurface9* oldTarget = nullptr;
			IDirect3DSurface9* oldDepth = nullptr;
			D3DVIEWPORT9 oldViewport{};
			const auto ready = SUCCEEDED(roomImage->texture.map->GetSurfaceLevel(0, &target)) &&
				sceneTexture && SUCCEEDED(sceneTexture->GetSurfaceLevel(0, &sceneTarget)) &&
				SUCCEEDED(device->GetRenderTarget(0, &oldTarget)) &&
				SUCCEEDED(device->GetDepthStencilSurface(&oldDepth)) &&
				SUCCEEDED(device->GetViewport(&oldViewport));
			if (!ready)
			{
				frameReady.store(false, std::memory_order_release);
				if (oldDepth) oldDepth->Release();
				if (oldTarget) oldTarget->Release();
				if (target) target->Release();
				if (sceneTarget) sceneTarget->Release();
				return;
			}

			if (!savedState) device->CreateStateBlock(D3DSBT_ALL, &savedState);
			if (!savedState || FAILED(savedState->Capture()))
			{
				oldDepth->Release(); oldTarget->Release(); target->Release(); sceneTarget->Release();
				frameReady.store(false, std::memory_order_release);
				return;
			}

			bool rendered = false;
			float whiteOpacity = 0.0f;
			if (SUCCEEDED(device->SetRenderTarget(0, sceneTarget)) &&
				SUCCEEDED(device->SetDepthStencilSurface(roomDepth)))
			{
				D3DVIEWPORT9 viewport{ 0, 0, textureWidth, textureHeight, 0.0f, 1.0f };
				device->SetViewport(&viewport);
				device->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
					D3DCOLOR_ARGB(255, 6, 7, 9), 1.0f, 0);

				D3DXMATRIX world, view, projection;
				D3DXMatrixIdentity(&world);
				static const auto startTime = timeGetTime();
				const auto now = timeGetTime();
				const auto phase = static_cast<float>(now - startTime) * 0.00012f;

				static D3DXVECTOR3 currentBaseEye = { 0.0f, -1450.0f, 192.0f };
				static D3DXVECTOR3 currentBaseTarget = { 0.0f, -600.0f, 182.0f };
				static unsigned lastCamTime = 0;
				if (!lastCamTime) lastCamTime = now;
				const float dt = std::clamp(static_cast<float>(now - lastCamTime) / 1000.0f, 0.0f, 0.1f);
				lastCamTime = now;

				// Menu entry, not selected map, controls the close framing.
				D3DXVECTOR3 desiredBaseEye = { 0.0f, -1450.0f, 192.0f };
				D3DXVECTOR3 desiredBaseTarget = { 0.0f, -600.0f, 182.0f };
				static unsigned cameraSession = 0;
				// Reset framing only when the scene opens, independently of combat wave resets.
				const auto session = cameraResetSession.load(std::memory_order_acquire);
				if (cameraSession != session)
				{
					currentBaseEye = desiredBaseEye;
					currentBaseTarget = desiredBaseTarget;
					cameraSession = session;
				}
				// Pull back during the taunt instead of retaining the private-lobby close shot.
				if (closeCamera.load(std::memory_order_acquire) && !inTransition)
				{
					desiredBaseEye = { 0.0f, -1180.0f, 188.0f };
					desiredBaseTarget = { 0.0f, -550.0f, 168.0f };
				}

				// Smooth menu zoom and the transition's initial pullback.
				const float lerpFactor = 1.0f - std::exp(-dt * 2.0f);
				currentBaseEye += (desiredBaseEye - currentBaseEye) * lerpFactor;
				currentBaseTarget += (desiredBaseTarget - currentBaseTarget) * lerpFactor;

				D3DXVECTOR3 eye;
				D3DXVECTOR3 at;
				whiteOpacity = 0.0f;
				float currentDoorAngle = 0.0f;

				if (inTransition)
				{
					const auto start = transitionStartTime.load(std::memory_order_acquire);
					const auto elapsed = now - start;

					// Give the balcony-facing taunt time to read before the door approach.
					const float doorT = LobbyTransition::Smooth(LobbyTransition::Progress(elapsed,
						LobbyTransition::DoorStartMs, LobbyTransition::DoorEndMs));
					currentDoorAngle = doorT * D3DXToRadian(102.0f);

					// Hold for the taunt, then preserve the existing flight duration.
					const float flyT = LobbyTransition::CameraProgress(elapsed);
					const float easeFly = LobbyTransition::Smooth(flyT);

					const D3DXVECTOR3 flyEndEye = { 0.0f, -400.0f, 140.0f };
					const D3DXVECTOR3 flyEndTarget = { 0.0f, -100.0f, 140.0f };

					// Fade out idle pan as camera flies through door
					const float panScale = std::max(0.0f, 1.0f - flyT * 2.0f);

					eye.x = currentBaseEye.x * (1.0f - easeFly) + flyEndEye.x * easeFly + std::sin(phase) * currentCameraPan.x * panScale;
					eye.y = currentBaseEye.y + easeFly * (flyEndEye.y - currentBaseEye.y);
					eye.z = currentBaseEye.z + easeFly * (flyEndEye.z - currentBaseEye.z);

					at.x = currentBaseTarget.x * (1.0f - easeFly) + flyEndTarget.x * easeFly;
					at.y = currentBaseTarget.y + easeFly * (flyEndTarget.y - currentBaseTarget.y);
					at.z = currentBaseTarget.z + easeFly * (flyEndTarget.z - currentBaseTarget.z);

					// Start as the doors open; the opaque phase then fades to black.
					whiteOpacity = LobbyTransition::WhiteOpacity(elapsed);
				}
				else
				{
					eye = {
						currentBaseEye.x + std::sin(phase) * currentCameraPan.x,
						currentBaseEye.y + std::cos(phase * 0.7f) * currentCameraPan.y,
						currentBaseEye.z,
					};
					at = currentBaseTarget;
				}

				const D3DXVECTOR3 up(0.0f, 0.0f, 1.0f);
				D3DXMatrixLookAtRH(&view, &eye, &at, &up);
				D3DXMatrixPerspectiveFovRH(&projection, D3DXToRadian(58.0f),
					static_cast<float>(textureWidth) / textureHeight, 16.0f, 5000.0f);
				device->SetTransform(D3DTS_VIEW, &view);
				device->SetTransform(D3DTS_PROJECTION, &projection);
				device->SetVertexShader(nullptr);
				device->SetPixelShader(nullptr);

				device->SetTransform(D3DTS_WORLD, &world);
				device->SetFVF(VertexFormat);
				device->SetRenderState(D3DRS_LIGHTING, FALSE);
				// Material gammaWrite is disabled in the original Theater assets.
				// Never inherit a UI pass's gamma conversion for this linear scene.
				device->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
				for (DWORD sampler = 0; sampler < 3; ++sampler)
					device->SetSamplerState(sampler, D3DSAMP_SRGBTEXTURE, FALSE);
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

				const auto drawGroups = [device, &eye](const std::vector<DrawGroup>& groups,
					const auto& vertices, const bool blendPass, const bool roomPass,
					IDirect3DVertexBuffer9* staticBuffer, const int selectedWeapon = -1)
				{
					const auto stride = sizeof(typename std::decay_t<decltype(vertices)>::value_type);
					const auto useBuffer = staticBuffer &&
						SUCCEEDED(device->SetStreamSource(0, staticBuffer, 0, stride));
					device->SetFVF(roomPass ? RoomVertexFormat : VertexFormat);
					for (const auto& group : groups)
					{
						if (group.blend != blendPass ||
							(group.weaponIndex >= 0 && group.weaponIndex != selectedWeapon)) continue;
						if (group.hasCell)
						{
							const auto dx = (group.cellX + 0.5f) * 512.0f - eye.x;
							const auto dy = (group.cellY + 0.5f) * 512.0f - eye.y;
							if (dx * dx + dy * dy > 1400.0f * 1400.0f) continue;
						}
						device->SetRenderState(D3DRS_SRGBWRITEENABLE, group.gammaWrite ? TRUE : FALSE);
						const bool isEyeGlow = !group.textureName.empty() &&
							(group.textureName.find("zombie_eye") != std::string::npos ||
							 group.textureName.find("eye_glow") != std::string::npos);
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
							device->SetRenderState(D3DRS_ALPHABLENDENABLE, group.blend);
							device->SetRenderState(D3DRS_SRCBLEND, group.srcBlend);
							device->SetRenderState(D3DRS_DESTBLEND, group.dstBlend);
							device->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
							device->SetRenderState(D3DRS_ZWRITEENABLE,
								(roomPass || group.hasCell) ? group.depthWrite : !group.blend);
						}
						if (group.blend)
						{
							float bias = -0.00003f;
							float slopeBias = -0.5f;
							device->SetRenderState(D3DRS_DEPTHBIAS, *reinterpret_cast<DWORD*>(&bias));
							device->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS, *reinterpret_cast<DWORD*>(&slopeBias));
						}
						else
						{
							DWORD zero = 0;
							device->SetRenderState(D3DRS_DEPTHBIAS, zero);
							device->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS, zero);
						}
						const auto lightmapped = roomPass && group.lightmapIndex >= 0 &&
							group.lightmapIndex < static_cast<int>(roomLightmaps.size()) &&
							roomLightmaps[group.lightmapIndex];
						device->SetTexture(1, lightmapped ? roomLightmaps[group.lightmapIndex] : nullptr);
						device->SetTexture(2, lightmapped ? group.normalTexture : nullptr);
						// Opaque world texture alpha participates in material shading; it is
						// not a universal cut-out mask. Preserve actor/prop cut-outs.
						const bool isWeapon = group.weaponIndex >= 0;
						const bool alphaCutout = !isWeapon && (group.alphaTest || group.blend);
						// Keep the native cutout test active for static props, including the
						// fixed-function fallback if shader creation is unavailable.
						const bool hardwareCutout = !isEyeGlow && alphaCutout && group.alphaThreshold >= 0.0f;
						device->SetRenderState(D3DRS_ALPHATESTENABLE, hardwareCutout);
						device->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL);
						device->SetRenderState(D3DRS_ALPHAREF,
							static_cast<DWORD>(std::clamp(group.alphaThreshold * 255.0f, 0.0f, 255.0f)));
						const float materialFlags[4] = { (lightmapped ? group.normalTexture != nullptr : group.gammaWrite) ? 1.0f : 0.0f,
							alphaCutout ? 1.0f : 0.0f, isEyeGlow ? -4.0f : group.alphaThreshold,
							group.multiplicative ? 1.0f : 0.0f };
						device->SetPixelShaderConstantF(0, materialFlags, 1);
						device->SetSamplerState(2, D3DSAMP_MINFILTER, D3DTEXF_ANISOTROPIC);
						device->SetSamplerState(2, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
						device->SetSamplerState(2, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);
						device->SetSamplerState(2, D3DSAMP_MAXANISOTROPY, 16);
						device->SetSamplerState(2, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
						device->SetSamplerState(2, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
						device->SetPixelShader(lightmapped ? lightmapFilmShader :
							(group.texture ? filmShader : nullptr));
						device->SetTextureStageState(1, D3DTSS_COLOROP,
							lightmapped && !lightmapFilmShader ? D3DTOP_MODULATE : D3DTOP_DISABLE);
						if (lightmapped && !lightmapFilmShader)
						{
							device->SetTextureStageState(1, D3DTSS_COLORARG1, D3DTA_TEXTURE);
							device->SetTextureStageState(1, D3DTSS_COLORARG2, D3DTA_CURRENT);
						}
						device->SetTextureStageState(0, D3DTSS_ALPHAARG1,
							group.texture ? D3DTA_TEXTURE : D3DTA_DIFFUSE);
						device->SetTexture(0, group.texture);
						if (!group.texture)
						{
							device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
							device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
						}
						HRESULT result;
						if (group.indexCount)
						{
							if (!useBuffer || !propIndexBuffer || FAILED(device->SetIndices(propIndexBuffer))) return false;
							result = device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, group.firstVertex,
								0, group.vertexCount, group.firstIndex, group.indexCount / 3);
						}
						else result = useBuffer ?
							device->DrawPrimitive(D3DPT_TRIANGLELIST, group.firstVertex, group.vertexCount / 3) :
							device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, group.vertexCount / 3,
								vertices.data() + group.firstVertex, stride);
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
				rendered = drawGroups(roomGroups, roomVertices, false, true, roomVertexBuffer);
				if (propVertexBuffer)
				{
					rendered = rendered && drawGroups(propGroups, propVertices, false, false, propVertexBuffer);
				}
				rendered = rendered && drawGroups(roomGroups, roomVertices, true, true, roomVertexBuffer);
				if (propVertexBuffer)
				{
					rendered = rendered && drawGroups(propGroups, propVertices, true, false, propVertexBuffer);
				}

				// Render animated back double doors
				D3DXMATRIX mTransToOrigin, mRot, mTransBack, mWorldDoor;

				// Left door pivots at (-60.0f, -510.0f)
				D3DXMatrixTranslation(&mTransToOrigin, 60.0f, 510.0f, 0.0f);
				D3DXMatrixRotationZ(&mRot, currentDoorAngle);
				D3DXMatrixTranslation(&mTransBack, -60.0f, -510.0f, 0.0f);
				mWorldDoor = mTransToOrigin * mRot * mTransBack;
				device->SetTransform(D3DTS_WORLD, &mWorldDoor);
				drawGroups(doorLeftGroups, doorLeftVertices, false, true, nullptr);
				drawGroups(doorLeftGroups, doorLeftVertices, true, true, nullptr);

				// Right door pivots at (60.0f, -510.0f)
				D3DXMatrixTranslation(&mTransToOrigin, -60.0f, 510.0f, 0.0f);
				D3DXMatrixRotationZ(&mRot, -currentDoorAngle);
				D3DXMatrixTranslation(&mTransBack, 60.0f, -510.0f, 0.0f);
				mWorldDoor = mTransToOrigin * mRot * mTransBack;
				device->SetTransform(D3DTS_WORLD, &mWorldDoor);
				drawGroups(doorRightGroups, doorRightVertices, false, true, nullptr);
				drawGroups(doorRightGroups, doorRightVertices, true, true, nullptr);

				// Reset world transform to Identity
				device->SetTransform(D3DTS_WORLD, &world);

				if (rendered)
				{
					const auto sampleActor = [now](ActorMesh& actor, const int clip, const unsigned phaseOffset)
						-> const std::vector<Vertex>&
					{
						if (actor.frames.size() < 2) return actor.frames.front();
						const auto first = clip == 9 ? actor.attackFirst : clip == 8 ? actor.meleeFirst : clip == 7 ? actor.pointFirst : clip == 6 ? actor.shotgunFireFirst :
							clip == 5 ? actor.rifleFireFirst : clip == 4 ? actor.rifleIdleFirst :
							clip == 3 ? actor.walkFirst : clip == 2 ? actor.deathFirst :
							clip == 1 ? actor.actionFirst : actor.idleFirst;
						const auto length = clip == 9 ? actor.attackCount : clip == 8 ? actor.meleeCount : clip == 7 ? actor.pointCount : clip == 6 ? actor.shotgunFireCount :
							clip == 5 ? actor.rifleFireCount : clip == 4 ? actor.rifleIdleCount :
							clip == 3 ? actor.walkCount : clip == 2 ? actor.deathCount :
							clip == 1 ? actor.actionCount : actor.idleCount;
						if (length < 2) return actor.frames[first];
						const auto zombie = &actor == &actorMeshes[4];
						const auto frameTime = clip == 2 || clip == 7 ? 140u : (clip == 1 || clip == 5 || clip == 6) ? (zombie ? 130u : 85u) :
							clip == 3 ? 250u : (zombie ? 190u : 240u);
						const bool oneShot = clip == 2 || clip >= 7 || (!zombie && (clip == 1 || clip == 5 || clip == 6));
						const auto duration = actor.clipDurationMs[clip] ? actor.clipDurationMs[clip] : frameTime * (length - 1);
						const unsigned elapsed = now + phaseOffset;
						const auto phase = static_cast<float>(oneShot ? std::min(elapsed, duration) : elapsed % duration) /
							duration * static_cast<float>(length - 1);
						const auto frameA = static_cast<unsigned>(phase) % length;
						const auto frameB = oneShot ? std::min(frameA + 1, length - 1) :
							(frameA + 1) % length;
						const auto blend = phase - static_cast<float>(static_cast<unsigned>(phase));
						const auto& a = actor.frames[first + frameA];
						const auto& b = actor.frames[first + frameB];
						for (auto vertex = 0u; vertex < a.size(); ++vertex)
						{
							auto& output = actor.interpolated[vertex];
							output = a[vertex];
							output.x += (b[vertex].x - a[vertex].x) * blend;
							output.y += (b[vertex].y - a[vertex].y) * blend;
							output.z += (b[vertex].z - a[vertex].z) * blend;
						}
						return actor.interpolated;
					};

					const auto count = lobbyCharacterCount.load(std::memory_order_acquire);
					struct PathPoint { float x, y, z; };
					static const std::vector<PathPoint> leftPath = {
						{ -900, -1191, 80 }, { -493, -1191, 80 },
						{ -340, -1158, 80 }, { -340, -937, 160 },
						{ -340, -750, 248 }, { -340, -700, 248 }, { -120, -700, 248 }
					};
					static const std::vector<PathPoint> rightPath = {
						{ 900, -1253, 80 }, { 463, -1253, 80 },
						{ 327, -1243, 80 }, { 260, -1198, 80 },
						{ 260, -991, 135 }, { 260, -750, 248 },
						{ 260, -700, 248 }, { 120, -700, 248 }
					};

					const auto sampleZombiePath = [](size_t index, float progress, PathPoint& outLoc, float& outFacing)
					{
						const auto fromLeft = index % 2 == 0;
						const auto& path = fromLeft ? leftPath : rightPath;
						float total = 0.0f;
						for (auto node = 1u; node < path.size(); ++node)
						{
							const auto dx = path[node].x - path[node - 1].x;
							const auto dy = path[node].y - path[node - 1].y;
							const auto dz = path[node].z - path[node - 1].z;
							total += std::sqrt(dx * dx + dy * dy + dz * dz);
						}
						outLoc = path.back();
						outFacing = fromLeft ? 0.0f : D3DX_PI;
						float remaining = progress * total;
						for (auto node = 1u; node < path.size(); ++node)
						{
							const auto& a = path[node - 1];
							const auto& b = path[node];
							const auto dx = b.x - a.x, dy = b.y - a.y, dz = b.z - a.z;
							const auto distance = std::sqrt(dx * dx + dy * dy + dz * dz);
							if (remaining > distance && node + 1 < path.size())
							{
								remaining -= distance;
								continue;
							}
							const auto t = std::clamp(remaining / distance, 0.0f, 1.0f);
							outLoc = { a.x + dx * t, a.y + dy * t, a.z + dz * t };
							break;
						}
						const auto pointAt = [&path, total](float distance)
						{
							distance = std::clamp(distance, 0.0f, total);
							for (auto node = 1u; node < path.size(); ++node)
							{
								const auto& a = path[node - 1];
								const auto& b = path[node];
								const auto dx = b.x - a.x, dy = b.y - a.y, dz = b.z - a.z;
								const auto length = std::sqrt(dx * dx + dy * dy + dz * dz);
								if (distance > length && node + 1 < path.size())
								{
									distance -= length;
									continue;
								}
								const auto t = std::clamp(distance / length, 0.0f, 1.0f);
								return PathPoint{ a.x + dx * t, a.y + dy * t, a.z + dz * t };
							}
							return path.back();
						};
						const auto before = pointAt(progress * total - 35.0f);
						const auto after = pointAt(progress * total + 35.0f);
						outFacing = std::atan2(after.y - before.y, after.x - before.x);

						// Lateral spreading across stairs & hallway so zombies don't stack directly inside each other
						const float lateral = ((static_cast<int>(index / 2) % 4) - 1.5f) * 4.0f;
						const float stairScale = progress > 0.86f ? std::max(0.0f, 1.0f - (progress - 0.86f) / 0.12f) : 1.0f;
						const float perpX = -std::sin(outFacing);
						const float perpY = std::cos(outFacing);
						outLoc.x += perpX * lateral * stairScale;
						outLoc.y += perpY * lateral * stairScale;


						// The theater treads rise 8 units per 16 units of forward travel.
						// Follow their continuous support ramp instead of snapping to treads/rubble.
						// Four units of clearance keep the feet above the next tread during a stride.
						outLoc.z = std::clamp(84.0f + (outLoc.y + 1098.0f) * 0.5f, 80.0f, 248.0f);
						const auto& landing = path[path.size() - 2];
						const auto& end = path.back();
						const auto landingLength = std::hypot(end.x - landing.x, end.y - landing.y);
						return progress * total >= total - landingLength;

					};

					struct ZombieVisual
					{
						unsigned spawnTime = 0;
						unsigned deathTime = 0;
						unsigned nextSpawnTime = 0;
						float routeProgress = 0.0f;
						bool admitted = false;
						unsigned admittedTime = 0;
						int approachPhase = 0;
						float assignedHomeX = 0.0f;
						float walkDuration = 22000.0f;
						std::vector<Vertex> deathPose;
						PathPoint location{};
						float facing = 0.0f;
						bool chasing = false;
						bool attacking = false;
						unsigned attackTime = 0;
						float animationTime = 0.0f;
						unsigned nextAttackTime = 0;
						int targetSurvivor = -1;
						float fallDirection = 1.0f;
						int variant = 0; // 0 = walk, 1 = run
						bool dying = false;
						bool visible = false;
					};
					constexpr unsigned zombieFallDuration = 650u;
					constexpr unsigned zombieCorpseLifetime = 3200u;
					const unsigned zombieAttackDuration = actorMeshes[4].clipDurationMs[9] ? actorMeshes[4].clipDurationMs[9] : 1800u;
					static std::array<ZombieVisual, 32> zombies{};
					static unsigned nextWaveTime = 0;
					static unsigned spawnCursor = 0;
					const auto zombieAnimationTime = [now](const ZombieVisual& visual)
					{
						// Movement advances the gait; attacks and waiting hold the last planted pose.
						return static_cast<unsigned>(visual.animationTime);
					};
					static bool initialized = false;
					static unsigned lastSession = 0;
					const auto session = lobbySession.load(std::memory_order_acquire);
					const auto desiredZombies = LobbyCombat::Population(count);
					if (!initialized || session != lastSession)
					{
						for (auto i = 0u; i < zombies.size(); ++i)
						{
							const bool isRunner = (i % 3 == 2);
							zombies[i].variant = isRunner ? 1 : 0;
							zombies[i].walkDuration = isRunner ? 16500.0f : (21000.0f + (i % 5) * 1100.0f);
							zombies[i].spawnTime = now;
							zombies[i].deathTime = 0;
							zombies[i].nextSpawnTime = now;
							zombies[i].routeProgress = 0.0f;
							zombies[i].animationTime = 0.0f;
							zombies[i].admitted = false;
							zombies[i].targetSurvivor = -1;
							zombies[i].deathPose.clear();
							zombies[i].chasing = false;
							zombies[i].attacking = false;
							zombies[i].nextAttackTime = 0;
							zombies[i].dying = false;
							zombies[i].visible = false;
						}
						initialized = true;
						lastSession = session;
						nextWaveTime = now;
						spawnCursor = 0;
					}

					for (auto index = 0u; index < zombies.size(); ++index)
					{
						auto& visual = zombies[index];
						if (visual.dying)
						{
							if (now - visual.deathTime >= zombieCorpseLifetime &&
								static_cast<int>(now - visual.nextSpawnTime) >= 0)
							{
								visual.dying = false;
								visual.visible = false;
								visual.deathPose.clear();
								visual.chasing = false;
								visual.routeProgress = 0.0f;
								visual.admitted = false;
								visual.targetSurvivor = -1;
								visual.attacking = false;
								visual.nextAttackTime = 0;
								visual.spawnTime = now;
								const bool isRunner = (index % 3 == 2);
								visual.variant = isRunner ? 1 : 0;
								visual.walkDuration = isRunner ? 16500.0f : (21000.0f + (index % 5) * 1100.0f);
							}
						}

					}

					// One off-screen arrival per interval; frame stalls never release a catch-up wave.
					const auto visibleCount = std::count_if(zombies.begin(), zombies.end(), [](const ZombieVisual& visual) { return visual.visible; });
					if (zombieWaveStarted.load(std::memory_order_acquire) && lobbyVisible.load(std::memory_order_acquire) &&
						!bootPreview && !inTransition && static_cast<unsigned>(visibleCount) < desiredZombies && static_cast<int>(now - nextWaveTime) >= 0)
					{
						for (auto offset = 0u; offset < zombies.size(); ++offset)
						{
							const auto index = (spawnCursor + offset) % zombies.size();
							auto& visual = zombies[index];
							if (visual.visible || visual.dying) continue;
							visual = ZombieVisual{};
							visual.variant = index % 3 == 2 ? 1 : 0;
							visual.walkDuration = visual.variant == 1 ? 16500.0f : 21000.0f + (index % 5) * 1100.0f;
							visual.spawnTime = now;
							visual.nextSpawnTime = now;
							// Skip the long off-screen entrance corridor, while still starting on the lower floor.
							visual.routeProgress = 0.28f;
							visual.visible = true;
							spawnCursor = static_cast<unsigned>((index + 1) % zombies.size());
							nextWaveTime = now + LobbyCombat::SpawnInterval(count);
							break;
						}
					}

					struct SurvivorState
					{
						float currentX = 0.0f;
						float homeX = 0.0f;
						int targetZombieIndex = -1;
						unsigned targetEngageTime = 0;
						unsigned lastFireTime = 0;
						unsigned meleeTime = 0;
						unsigned recoveryUntil = 0;
						float meleeFacing = 0.0f;
						int meleeTarget = -1;
						bool meleePending = false;
						int burstRemaining = 0;
						unsigned nextBurstShotTime = 0;
						float facing = -D3DX_PI * 0.5f;
						bool isWalking = false;
						bool initialized = false;
						bool present = false;
						int modelIndex = -1;
						int weaponIndex = 0;
						unsigned session = 0;
					};
					static std::array<SurvivorState, 4> survivors{};
					constexpr auto balconyY = -765.0f;
					static std::mt19937 weaponRandom(timeGetTime() ^ GetCurrentProcessId());
					static std::uniform_int_distribution<int> weaponChoice(0, 3);
					struct TeleportBurst
					{
						float x = 0.0f;
						unsigned started = 0;
						bool active = false;
					};
					static std::array<TeleportBurst, 4> teleportBursts{};
					static unsigned teleportSession = 0;
					if (teleportSession != session)
					{
						teleportBursts = {};
						for (auto& survivor : survivors) survivor.present = false;
						teleportSession = session;
					}
					// One arrival effect per occupied slot. Leaves and icon changes do not
					// create ghost effects, and never reset the zombie simulation.
					for (auto slot = 0u; slot < survivors.size(); ++slot)
					{
						auto& survivor = survivors[slot];
						const auto model = lobbyCharacterModels[slot].load(std::memory_order_acquire);
						const bool present = slot < static_cast<unsigned>(count) && model >= 0 && model < 4 && !actorMeshes[model].frames.empty();
						if (!present || inTransition) teleportBursts[slot].active = false;
						else if (!survivor.present)
							teleportBursts[slot] = { (static_cast<float>(slot) - (count - 1) * 0.5f) * 65.0f, now, true };
						else if (teleportBursts[slot].active)
							teleportBursts[slot].x = (static_cast<float>(slot) - (count - 1) * 0.5f) * 65.0f;
						if (!present) survivor.initialized = false;
						survivor.present = present;
						survivor.modelIndex = model;
					}

					// Changed occupied slots invalidate combat claims while preserving each
					// zombie's route and position. Camera changes alone do not interrupt encounters.
					std::array<int, 5> roster{};
					roster[0] = count;
					for (auto slot = 0u; slot < survivors.size(); ++slot)
						roster[slot + 1] = survivors[slot].present ? survivors[slot].modelIndex : -1;
					static std::array<int, 5> lastRoster{};
					static bool rosterReady = false;
					if (rosterReady && roster != lastRoster)
					{
						for (auto& survivor : survivors)
						{
							survivor.targetZombieIndex = -1;
							survivor.burstRemaining = 0;
							survivor.lastFireTime = 0;
							survivor.meleeTime = 0;
							survivor.meleePending = false;
							survivor.recoveryUntil = 0;
						}
						for (auto& visual : zombies)
						{
							if (!visual.visible || visual.dying) continue;
							LobbyCombat::ReleaseEncounter(visual);
						}
					}
					lastRoster = roster;
					rosterReady = true;

					const auto killZombie = [&](ZombieVisual& visual, bool meleeHit = false)
					{
						if (visual.dying) return;
						auto& mesh = actorMeshes[4];
						if (!mesh.frames.empty()) visual.deathPose = sampleActor(mesh,
							visual.attacking && mesh.attackCount > 1 ? 9 : visual.variant == 1 ? 1 : 0,
							visual.attacking && mesh.attackCount > 1 ? 0u - visual.attackTime : zombieAnimationTime(visual) - now);
						visual.fallDirection = meleeHit ? -1.0f : 1.0f;
						visual.dying = true;
						visual.deathTime = now;
						visual.nextSpawnTime = now + zombieCorpseLifetime;
						if (meleeHit && visual.targetSurvivor >= 0 && visual.targetSurvivor < count)
							survivors[visual.targetSurvivor].recoveryUntil = now + LobbyCombat::RecoveryMs;
					};

					// Ease movement and gait together into a slower pace during the camera transition.
					const auto transitionPace = inTransition ?
						1.0f - 0.45f * LobbyTransition::Smooth(std::clamp(transitionElapsed / 600.0f, 0.0f, 1.0f)) : 1.0f;
					const auto movementDt = dt * transitionPace;
					const auto advanceEncounter = [&](ZombieVisual& visual, float home, bool waiting)
					{
						const auto oldX = visual.location.x, oldY = visual.location.y;
						const auto moved = LobbyCombat::AdvanceApproach(visual.location.x, visual.location.y, visual.approachPhase,
							home, movementDt, visual.variant == 1 ? 80.0f : 55.0f, waiting);
						if (moved > 0.001f)
						{
							visual.animationTime += movementDt * 1000.0f;
							const auto yaw = std::atan2(visual.location.y - oldY, visual.location.x - oldX);
							auto difference = yaw - visual.facing;
							while (difference > D3DX_PI) difference -= 2.0f * D3DX_PI;
							while (difference < -D3DX_PI) difference += 2.0f * D3DX_PI;
							visual.facing += difference * (1.0f - std::exp(-dt * 8.0f));
						}
					};

					// Advance leaders first so followers leave space instead of stacking on the landing.
					std::array<unsigned, 32> movementOrder{};
					for (auto index = 0u; index < movementOrder.size(); ++index) movementOrder[index] = index;
					std::sort(movementOrder.begin(), movementOrder.end(), [&](unsigned a, unsigned b)
						{ return zombies[a].routeProgress > zombies[b].routeProgress; });
					for (const auto index : movementOrder)
					{
						auto& visual = zombies[index];
						if (!visual.visible || visual.dying) continue;
						if (visual.targetSurvivor >= count || (visual.targetSurvivor >= 0 && !survivors[visual.targetSurvivor].present))
						{
							LobbyCombat::ReleaseEncounter(visual);
						}
						if (!visual.chasing)
						{
							auto progress = std::min(visual.routeProgress + (movementDt * 1000.0f / visual.walkDuration), 1.0f);
							for (auto leader = 0u; leader < zombies.size(); ++leader)
							{
								const auto& other = zombies[leader];
								if (leader != index && leader % 2 == index % 2 && other.visible && !other.dying && !other.chasing &&
									other.routeProgress > visual.routeProgress)
									progress = std::max(visual.routeProgress, std::min(progress, other.routeProgress - LobbyCombat::RouteSpacing));
							}
							if (!visual.admitted && progress >= LobbyCombat::AdmissionProgress)
							{
								float best = std::numeric_limits<float>::max();
								visual.targetSurvivor = -1;
								PathPoint entry{}; float entryFacing = 0.0f;
								sampleZombiePath(index, progress, entry, entryFacing);
								for (auto slot = 0; slot < count; ++slot)
								{
									if (!survivors[slot].present) continue;
									const auto occupied = std::count_if(zombies.begin(), zombies.end(), [&](const ZombieVisual& other)
										{ return other.visible && !other.dying && other.admitted && other.targetSurvivor == slot; });
									if (occupied >= 2) continue;
									const auto x = (static_cast<float>(slot) - (count - 1) * 0.5f) * 65.0f;
									const auto score = std::hypot(x - entry.x, balconyY - entry.y) + occupied * 120.0f;
									if (score < best) { best = score; visual.targetSurvivor = slot; }
								}
								visual.admitted = visual.targetSurvivor >= 0;
								if (visual.admitted)
								{
									visual.admittedTime = now;
									visual.approachPhase = 0;
									visual.assignedHomeX = (static_cast<float>(visual.targetSurvivor) - (count - 1) * 0.5f) * 65.0f;
								}
								// Full encounter slots do not stop the stair route. Reassign at the landing.
							}
							if (progress > visual.routeProgress) visual.animationTime += movementDt * 1000.0f;
							visual.routeProgress = progress;
							sampleZombiePath(index, progress, visual.location, visual.facing);
							if (progress < 1.0f) continue;
							visual.chasing = true;
						}
						// A departing character releases its encounter without teleporting the zombie.
						if (!visual.admitted)
						{
							float best = std::numeric_limits<float>::max();
							for (auto slot = 0; slot < count; ++slot)
							{
								if (!survivors[slot].present) continue;
								const auto occupied = std::count_if(zombies.begin(), zombies.end(), [&](const ZombieVisual& other)
									{ return other.visible && !other.dying && other.admitted && other.targetSurvivor == slot; });
								const auto x = (static_cast<float>(slot) - (count - 1) * 0.5f) * 65.0f;
								const auto score = std::hypot(x - visual.location.x, balconyY - visual.location.y) + occupied * 120.0f;
								if (score < best) { best = score; visual.targetSurvivor = slot; }
							}
							visual.admitted = visual.targetSurvivor >= 0;
								if (visual.admitted)
								{
									visual.admittedTime = now;
									visual.approachPhase = 0;
									visual.assignedHomeX = (static_cast<float>(visual.targetSurvivor) - (count - 1) * 0.5f) * 65.0f;
								}
						}
						const auto target = visual.targetSurvivor;
						if (target < 0)
						{
							// Keep clear of the characters until an occupied slot becomes available.
							visual.attacking = false;
							advanceEncounter(visual, visual.location.x, true);
							continue;
						}
						const auto targetX = (static_cast<float>(target) - (count - 1) * 0.5f) * 65.0f;
						if (targetX != visual.assignedHomeX)
						{
							visual.assignedHomeX = targetX;
							visual.approachPhase = 0;
							visual.attacking = false;
						}
						const auto dx = targetX - visual.location.x, dy = balconyY - visual.location.y;
						const auto nearest = std::hypot(dx, dy);
						unsigned ahead = 0;
						bool waiting = survivors[target].recoveryUntil != 0 && static_cast<int>(now - survivors[target].recoveryUntil) < 0;
						for (auto otherIndex = 0u; otherIndex < zombies.size(); ++otherIndex)
						{
							const auto& other = zombies[otherIndex];
							if (otherIndex == index || !other.visible || other.dying || !other.admitted || other.targetSurvivor != target) continue;
							if (other.attacking || LobbyCombat::OlderEncounter(other.admittedTime, otherIndex, visual.admittedTime, index))
							{ waiting = true; ++ahead; }
						}
						if (visual.attacking && now - visual.attackTime >= zombieAttackDuration) visual.attacking = false;
						if (!visual.attacking)
						{
							// Spread a solo character's waiting crowd across the rear row, rather than stacking at one point.
							const auto queueOffset = count == 1 && waiting && ahead > 0 ?
								(ahead % 2 ? -1.0f : 1.0f) * std::min(112.0f, ((ahead + 1) / 2) * 28.0f) : 0.0f;
							advanceEncounter(visual, targetX + queueOffset, waiting);
						}

						if (!inTransition && !waiting && visual.approachPhase == 2 && nearest <= LobbyCombat::AttackDistance && !visual.attacking && static_cast<int>(now - visual.nextAttackTime) >= 0)
						{
							visual.attacking = true;
							visual.facing = std::atan2(dy, dx);
							visual.attackTime = now;
							visual.nextAttackTime = now + zombieAttackDuration + 450u;
						}
					}

					struct MuzzleFlareRecord
					{
						D3DXVECTOR3 worldPos;
						float radius;
						float alpha;
					};
					std::vector<MuzzleFlareRecord> muzzleFlares;

					for (auto i = 0; i < count; ++i)
					{
						const auto modelIndex = lobbyCharacterModels[i].load(std::memory_order_acquire);
						if (modelIndex < 0 || modelIndex >= 4) continue;
						auto& actor = actorMeshes[modelIndex];
						if (actor.frames.empty()) continue;

						const unsigned meleeDuration = actor.clipDurationMs[8] ? actor.clipDurationMs[8] : 1067u;
						const unsigned meleeImpact = meleeDuration * 45u / 100u;
						auto& survivor = survivors[i];
						const auto homeX = (static_cast<float>(i) - (count - 1) * 0.5f) * 65.0f;
						if (!survivor.initialized || survivor.session != session)
						{
							survivor.currentX = homeX;
							survivor.homeX = homeX;
							survivor.targetZombieIndex = -1;
							survivor.targetEngageTime = 0;
							survivor.lastFireTime = 0;
							survivor.meleeTime = 0;
							survivor.recoveryUntil = 0;
							survivor.meleeTarget = -1;
							survivor.meleePending = false;
							survivor.burstRemaining = 0;
							survivor.nextBurstShotTime = 0;
							survivor.facing = -D3DX_PI * 0.5f;
							survivor.weaponIndex = weaponChoice(weaponRandom);
							survivor.session = session;
							survivor.initialized = true;
						}
						else if (survivor.homeX != homeX)
						{
							survivor.currentX = homeX;
							survivor.homeX = homeX;
							survivor.meleeTime = 0;
							survivor.meleePending = false;
							survivor.lastFireTime = 0;
							survivor.burstRemaining = 0;
							survivor.targetZombieIndex = -1;
						}

						const auto y = balconyY;
						const auto z = 245.0f;

						int bestZombie = -1;
						float bestScore = 9999999.0f;
						PathPoint bestLoc{};

						for (auto zIdx = 0u; zIdx < zombies.size(); ++zIdx)
						{
							auto& zmb = zombies[zIdx];
							if (!zmb.visible || zmb.dying || static_cast<int>(now - zmb.nextSpawnTime) < 0) continue;
							const auto approach = zmb.routeProgress;
							if (approach < 0.55f) continue;
							const auto loc = zmb.location;
							const auto closeDistance = std::hypot(loc.x - survivor.currentX, loc.y - y);
							const bool meleeLocked = survivor.meleeTime != 0 && now - survivor.meleeTime < meleeDuration;
							if (meleeLocked && survivor.meleeTarget != static_cast<int>(zIdx)) continue;
							if (loc.z < 224.0f || closeDistance > 420.0f) continue;
							if (closeDistance > LobbyCombat::MeleeDistance)
							{
								if (!LobbyCombat::ClearShot(survivor.currentX, y, loc.x, loc.y)) continue;
								bool unsafe = false;
								for (auto slot = 0; slot < count; ++slot)
									if (survivors[slot].present && std::hypot(loc.x - (static_cast<float>(slot) - (count - 1) * 0.5f) * 65.0f,
										loc.y - y) <= LobbyCombat::GunSafetyDistance) unsafe = true;
								if (unsafe) continue;
							}

							// A close attacker interrupts a distant gun burst.
							if (closeDistance > 58.0f && survivor.burstRemaining > 0 && survivor.targetZombieIndex != static_cast<int>(zIdx)) continue;

							const auto sideBias = (homeX * loc.x >= 0.0f) ? 0.7f : 1.4f;
							const auto urgency = (1.05f - approach);
							const auto dx = loc.x - survivor.currentX;
							const auto dy = loc.y - y;
							float score = closeDistance <= 58.0f ? -100000.0f + closeDistance :
								(dx * dx + dy * dy) * sideBias * (urgency * urgency);

							for (auto s = 0; s < count; ++s)
							{
								if (closeDistance > LobbyCombat::MeleeDistance && s != i && survivors[s].targetZombieIndex == static_cast<int>(zIdx) && survivors[s].burstRemaining > 0)
								{
									score *= 2.8f;
									break;
								}
							}

							if (score < bestScore)
							{
								bestScore = score;
								bestZombie = static_cast<int>(zIdx);
								bestLoc = loc;
							}
						}

						float targetYaw = -D3DX_PI * 0.5f;
						if (bestZombie >= 0)
						{
							if (survivor.targetZombieIndex != bestZombie)
							{
								survivor.targetZombieIndex = bestZombie;
								survivor.targetEngageTime = now;
							}
							targetYaw = std::atan2(bestLoc.y - y, bestLoc.x - survivor.currentX);
						}
						else
						{
							survivor.targetZombieIndex = -1;
							const float scan = std::sin(static_cast<float>(now) * 0.0012f + static_cast<float>(i) * 1.7f) * 0.08f;
							targetYaw = -D3DX_PI * 0.5f + scan;
						}

						if (survivor.meleeTime != 0 && now - survivor.meleeTime < meleeDuration) targetYaw = survivor.meleeFacing;

						// Rotate the whole actor toward the actual target, including the stair landings.
						// Clamping the aim here previously allowed kills outside the gun's direction.
						float diffYaw = targetYaw - survivor.facing;
						while (diffYaw > D3DX_PI) diffYaw -= 2.0f * D3DX_PI;
						while (diffYaw < -D3DX_PI) diffYaw += 2.0f * D3DX_PI;
						survivor.facing += diffYaw * (1.0f - std::exp(-dt * 5.0f));

						// Survivors stand their ground at homeX on the balcony
						const auto diffX = survivor.homeX - survivor.currentX;
						if (std::abs(diffX) > 1.5f && bestZombie < 0)
						{
							const auto step = std::copysign(std::min(std::abs(diffX), 0.035f * 33.0f), diffX);
							survivor.currentX += step;
							survivor.isWalking = true;
						}
						else
						{
							survivor.currentX = survivor.homeX;
							survivor.isWalking = false;
						}

						if (bestZombie < 0) survivor.burstRemaining = 0;
						const bool aimAligned = bestZombie >= 0 && std::abs(diffYaw) <= 0.12f;

						const auto bestDistance = bestZombie >= 0 ? std::hypot(bestLoc.x - survivor.currentX, bestLoc.y - y) : 99999.0f;
						bool closeToAnySurvivor = false;
						if (bestZombie >= 0)
							for (auto slot = 0; slot < count; ++slot)
								if (survivors[slot].present && std::hypot(bestLoc.x -
									(static_cast<float>(slot) - (count - 1) * 0.5f) * 65.0f, bestLoc.y - y) <= 72.0f)
									closeToAnySurvivor = true;
						const bool canShootBest = bestZombie >= 0 && bestLoc.z >= 224.0f && bestDistance <= 420.0f &&
							LobbyCombat::ClearShot(survivor.currentX, y, bestLoc.x, bestLoc.y);
						if (closeToAnySurvivor || !canShootBest) survivor.burstRemaining = 0;
						const auto nearbyThreats = static_cast<unsigned>(std::count_if(zombies.begin(), zombies.end(), [&](const ZombieVisual& zombie)
							{ return zombie.visible && !zombie.dying && zombie.location.z >= 224.0f &&
								std::hypot(zombie.location.x - survivor.currentX, zombie.location.y - y) <= 220.0f; }));
						if (!inTransition && survivor.meleePending && now - survivor.meleeTime >= meleeImpact)
						{
							auto& target = zombies[survivor.meleeTarget];
							if (target.visible && !target.dying && LobbyCombat::InMeleeSweep(
								target.location.x - survivor.currentX, target.location.y - y, survivor.meleeFacing))
							{
								killZombie(target, true);
								// The same visible knife swing can catch two additional tightly packed attackers.
								unsigned hits = 1;
								for (auto& nearby : zombies)
								{
									if (hits >= 3) break;
									if (!nearby.visible || nearby.dying || nearby.location.z < 224.0f) continue;
									if (LobbyCombat::InMeleeSweep(nearby.location.x - survivor.currentX,
										nearby.location.y - y, survivor.meleeFacing)) { killZombie(nearby, true); ++hits; }
								}
							}
							survivor.meleePending = false;
						}
						if (!inTransition && bestZombie >= 0 && !zombies[bestZombie].dying && bestDistance <= 58.0f && aimAligned &&
							!survivor.meleePending && zombies[bestZombie].attacking && now - zombies[bestZombie].attackTime >= 600u && (survivor.meleeTime == 0 || now - survivor.meleeTime >= meleeDuration + 200u))
						{
							survivor.meleeTime = now;
							survivor.meleeFacing = survivor.facing;
							survivor.meleeTarget = bestZombie;
							survivor.meleePending = true;
							survivor.lastFireTime = 0;
						}
						const bool isMeleeing = survivor.meleeTime != 0 && now - survivor.meleeTime < meleeDuration;

						// Upper stairs and the far balcony remain valid ranged targets; recover between bursts.
						if (!inTransition && bestZombie >= 0 && canShootBest && aimAligned && !closeToAnySurvivor && !isMeleeing && bestDistance <= 420.0f)
						{
							if (now - survivor.targetEngageTime >= 350u &&
								survivor.burstRemaining == 0 &&
								now - survivor.lastFireTime >= LobbyCombat::PressureCooldown(count, nearbyThreats) + i * 120u)
							{
								survivor.burstRemaining = (survivor.weaponIndex == 3 ? 1 : survivor.weaponIndex == 0 ? 2 : 3);
								survivor.nextBurstShotTime = now;
							}
						}

						// Handle active burst shots
						if (!inTransition && canShootBest && !closeToAnySurvivor && !isMeleeing && aimAligned && survivor.burstRemaining > 0 && static_cast<int>(now - survivor.nextBurstShotTime) >= 0)
						{
							survivor.burstRemaining--;
							survivor.lastFireTime = now;
							const unsigned shotDelay = (survivor.weaponIndex == 0 ? 160u :
								survivor.weaponIndex == 3 ? 400u :
								survivor.weaponIndex == 2 ? 100u : 115u);
							survivor.nextBurstShotTime = now + shotDelay;

							// A completed, aimed gun burst kills its target at range.
							if (survivor.burstRemaining == 0 && bestZombie >= 0)
							{
								auto& zmb = zombies[bestZombie];
								killZombie(zmb);
								survivor.targetZombieIndex = -1;
							}
						}

						const auto fireClip = survivor.weaponIndex == 3 ? 6 : survivor.weaponIndex == 0 ? 1 : 5;
						const auto fireDuration = actor.clipDurationMs[fireClip] ? actor.clipDurationMs[fireClip] : 450u;
						const bool isFiring = !inTransition && !isMeleeing && !closeToAnySurvivor && survivor.lastFireTime != 0 && now - survivor.lastFireTime < fireDuration;

						D3DXMATRIX rotation, translation, placement;
						// Melee is pose animation only; keep the character planted at its home.
						D3DXMatrixRotationZ(&rotation, survivor.facing);
						D3DXMatrixTranslation(&translation, survivor.homeX, y, z);
						placement = rotation * translation;
						device->SetTransform(D3DTS_WORLD, &placement);

						const auto clip = isMeleeing ? (actor.meleeCount > 1 ? 8 : survivor.weaponIndex == 0 ? 0 : 4) : isFiring ? (survivor.weaponIndex == 3 ? 6 :
							survivor.weaponIndex == 0 ? 1 : 5) :
							(survivor.isWalking ? 3 : survivor.weaponIndex == 0 ? 0 : 4);
						const auto phaseOffset = isMeleeing ? (0u - survivor.meleeTime) : isFiring ? (0u - survivor.lastFireTime) :
							(survivor.isWalking ? (i * 200u) : (i * 540u));
						const std::vector<Vertex>* pose = &sampleActor(actor, clip, phaseOffset);
						const auto shotAge = now - survivor.lastFireTime;
						if (isFiring && fireDuration > 150u && shotAge > fireDuration - 150u)
						{
							actor.blended = *pose;
							const auto& idle = sampleActor(actor, survivor.weaponIndex == 0 ? 0 : 4, i * 540u);
							const auto t = LobbyTransition::Smooth(LobbyTransition::Progress(shotAge, fireDuration - 150u, fireDuration));
							for (auto v = 0u; v < idle.size(); ++v)
							{
								actor.blended[v].x += (idle[v].x - actor.blended[v].x) * t;
								actor.blended[v].y += (idle[v].y - actor.blended[v].y) * t;
								actor.blended[v].z += (idle[v].z - actor.blended[v].z) * t;
							}
							pose = &actor.blended;
						}

						const auto flashElapsed = now - survivor.lastFireTime;
						const auto& anchor = actor.muzzleAnchors[survivor.weaponIndex];
						if (!inTransition && isFiring && flashElapsed <= 75u && anchor.valid)
						{
							const auto position = [&](UINT index)
							{
								const auto& vertex = (*pose)[index];
								return D3DXVECTOR3(vertex.x, vertex.y, vertex.z);
							};
							const auto a = position(anchor.vertices[0]);
							const auto ab = position(anchor.vertices[1]) - a;
							const auto ac = position(anchor.vertices[2]) - a;
							D3DXVECTOR3 normal, worldMuzzle;
							D3DXVec3Cross(&normal, &ab, &ac);
							D3DXVec3Normalize(&normal, &normal);
							const auto localMuzzle = a + ab * anchor.u + ac * anchor.v + normal * anchor.w;
							D3DXVec3TransformCoord(&worldMuzzle, &localMuzzle, &placement);
							const float radius = survivor.weaponIndex == 0 ? 4.2f : survivor.weaponIndex == 3 ? 8.5f : 6.0f;
							muzzleFlares.push_back({ worldMuzzle, radius, 1.0f - flashElapsed / 75.0f });
						}
						if (!drawGroups(actor.groups, *pose, false, false, nullptr, survivor.weaponIndex))
						{
							rendered = false;
							break;
						}
					}

					if (rendered)
					{
						auto& zombie = actorMeshes[4];
						std::vector<Vertex> eyeFlares, muzzleQuads;
						auto* flareBatch = &eyeFlares;
						const D3DXVECTOR3 camRight(view._11, view._21, view._31);
						const D3DXVECTOR3 camUp(view._12, view._22, view._32);

						const auto addFlareQuad = [&](const D3DXVECTOR3& center, float r, DWORD c)
						{
							const D3DXVECTOR3 p0 = center - camRight * r - camUp * r;
							const D3DXVECTOR3 p1 = center + camRight * r - camUp * r;
							const D3DXVECTOR3 p2 = center + camRight * r + camUp * r;
							const D3DXVECTOR3 p3 = center - camRight * r + camUp * r;
							flareBatch->push_back({ p0.x, p0.y, p0.z, c, 0.0f, 0.0f });
							flareBatch->push_back({ p1.x, p1.y, p1.z, c, 1.0f, 0.0f });
							flareBatch->push_back({ p2.x, p2.y, p2.z, c, 1.0f, 1.0f });
							flareBatch->push_back({ p0.x, p0.y, p0.z, c, 0.0f, 0.0f });
							flareBatch->push_back({ p2.x, p2.y, p2.z, c, 1.0f, 1.0f });
							flareBatch->push_back({ p3.x, p3.y, p3.z, c, 0.0f, 1.0f });
						};

						for (auto i = 0u; i < zombies.size() && !zombie.frames.empty(); ++i)
						{
							auto& visual = zombies[i];
							if (!visual.visible) continue;
							const auto location = visual.location;
							const auto facing = visual.facing;

							// Baked meshes have no bones: blending a walk pose into a distant death
							// pose collapses limbs. Fall from the hit pose with one rigid transform.
							if (visual.dying && !visual.deathPose.empty()) zombie.blended = visual.deathPose;
							else zombie.blended = sampleActor(zombie, visual.attacking && zombie.attackCount > 1 ? 9 : visual.variant == 1 ? 1 : 0,
								visual.attacking && zombie.attackCount > 1 ? 0u - visual.attackTime : zombieAnimationTime(visual) - now);
							auto& pose = zombie.blended;
							float lowest = std::numeric_limits<float>::max();
							for (const auto& vertex : pose) lowest = std::min(lowest, vertex.z);
							for (auto& vertex : pose) vertex.z -= lowest;

							D3DXMATRIX fall, rotation, translation, placement;
							const auto fallProgress = visual.dying ?
								LobbyTransition::Progress(now - visual.deathTime, 0u, zombieFallDuration) : 0.0f;
							const auto fallAngle = visual.dying ? visual.fallDirection * fallProgress * fallProgress * D3DX_PI * 0.5f : 0.0f;
							D3DXMatrixRotationY(&fall, fallAngle);
							// Keep the rotating body supported throughout the fall and corpse hold.
							float supportZ = std::numeric_limits<float>::max();
							for (const auto& vertex : pose)
								supportZ = std::min(supportZ, vertex.z * std::cos(fallAngle) - vertex.x * std::sin(fallAngle));
							D3DXMatrixRotationZ(&rotation, facing);
							D3DXMatrixTranslation(&translation, location.x, location.y, location.z - supportZ);
							placement = fall * rotation * translation;
							device->SetTransform(D3DTS_WORLD, &placement);

							if (!drawGroups(zombie.groups, pose, false, false, nullptr))
							{
								rendered = false;
								break;
							}

							float flareAlpha = 1.0f;
							if (visual.dying)
							{
								const auto deathElapsed = static_cast<float>(now - visual.deathTime) * 0.001f;
								flareAlpha = std::clamp(1.0f - deathElapsed * 1.5f, 0.0f, 1.0f);
							}
							if (flareAlpha > 0.01f && pose.size() >= 18456 + 540)
							{
								D3DXVECTOR3 localLeft(0.0f, 0.0f, 0.0f), localRight(0.0f, 0.0f, 0.0f);
								for (UINT v = 18456; v < 18456 + 270; v += 9)
								{
									localLeft.x += pose[v].x; localLeft.y += pose[v].y; localLeft.z += pose[v].z;
								}
								localLeft /= 30.0f;
								for (UINT v = 18456 + 270; v < 18456 + 540; v += 9)
								{
									localRight.x += pose[v].x; localRight.y += pose[v].y; localRight.z += pose[v].z;
								}
								localRight /= 30.0f;

								D3DXVECTOR3 worldLeft, worldRight;
								D3DXVec3TransformCoord(&worldLeft, &localLeft, &placement);
								D3DXVec3TransformCoord(&worldRight, &localRight, &placement);

								const auto toCamOffset = [&eye](D3DXVECTOR3& pt)
								{
									D3DXVECTOR3 dir = eye - pt;
									D3DXVec3Normalize(&dir, &dir);
									pt += dir * 0.8f;
								};
								toCamOffset(worldLeft);
								toCamOffset(worldRight);

								addFlareQuad(worldLeft, 2.4f, D3DCOLOR_ARGB(static_cast<DWORD>(255 * flareAlpha), 255, 255, 255));
								addFlareQuad(worldRight, 2.4f, D3DCOLOR_ARGB(static_cast<DWORD>(255 * flareAlpha), 255, 255, 255));
							}
						}

						// Menu-only teleport energy: reuse the existing soft flare texture,
						// with bounded particles and no gameplay FX entities or new assets.
						for (auto& burst : teleportBursts)
						{
							if (!burst.active) continue;
							const auto age = now - burst.started;
							if (age >= 800u) { burst.active = false; continue; }
							const float t = static_cast<float>(age) / 800.0f;
							const float intensity = std::sin(t * D3DX_PI) * 0.55f;
							const auto color = D3DCOLOR_ARGB(static_cast<DWORD>(255 * intensity), 255, 32, 16);
							for (auto particle = 0u; particle < 8u; ++particle)
							{
								const float angle = particle * 2.399963f + t * 4.0f;
								const float radius = 8.0f + (1.0f - t) * 10.0f;
								const float height = std::fmod(particle * 5.7f + t * 90.0f, 72.0f);
								addFlareQuad({ burst.x + std::cos(angle) * radius,
									balconyY + std::sin(angle) * radius, 248.0f + height }, 2.5f, color);
							}
							addFlareQuad({ burst.x, balconyY, 278.0f }, 10.0f, color);
						}
						flareBatch = &muzzleQuads;
						for (const auto& mf : muzzleFlares)
						{
							// Warm orange/yellow outer muzzle flare
							addFlareQuad(mf.worldPos, mf.radius,
								D3DCOLOR_ARGB(static_cast<DWORD>(255 * mf.alpha), 255, 205, 80));
							// White-hot blast core
							addFlareQuad(mf.worldPos, mf.radius * 0.42f,
								D3DCOLOR_ARGB(static_cast<DWORD>(255 * mf.alpha), 255, 255, 230));
						}

						if (rendered && (!eyeFlares.empty() || !muzzleQuads.empty()))
						{
							device->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
							IDirect3DTexture9* flareTex = nullptr;
							const auto found = loadedTextures.find("zombie_eye_flare.dds");
							if (found != loadedTextures.end()) flareTex = found->second;

							device->SetTransform(D3DTS_WORLD, &world);
							device->SetFVF(VertexFormat);
							device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
							device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
							device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
							device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
							device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
							device->SetTexture(0, flareTex);
							device->SetTexture(1, nullptr);
							device->SetTexture(2, nullptr);

							const float flareFlags[4] = { 0.0f, 0.0f, -2.0f, 0.0f };
							device->SetPixelShaderConstantF(0, flareFlags, 1);
							device->SetPixelShader(filmShader);
							if (!eyeFlares.empty()) device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, static_cast<UINT>(eyeFlares.size() / 3),
								eyeFlares.data(), sizeof(Vertex));
							if (!muzzleQuads.empty())
							{
								const float muzzleFlags[4] = { 0.0f, 0.0f, -5.0f, 0.0f };
								device->SetPixelShaderConstantF(0, muzzleFlags, 1);
								device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, static_cast<UINT>(muzzleQuads.size() / 3), muzzleQuads.data(), sizeof(Vertex));
							}
						}
					}
					device->SetTransform(D3DTS_WORLD, &world);
				}
			}

			// Screen-space vision grading pass: sceneTexture -> target (roomImage->texture.map)
			struct ScreenVertex { float x, y, z, rhw, u, v; };
			device->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
			if (rendered && SUCCEEDED(device->SetRenderTarget(0, target)) &&
				SUCCEEDED(device->SetDepthStencilSurface(nullptr)))
			{
				if (visionShader)
				{
					const auto w = static_cast<float>(textureWidth);
					const auto h = static_cast<float>(textureHeight);
					const ScreenVertex quad[] = {
						{ -0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f },
						{ w - 0.5f, -0.5f, 0.0f, 1.0f, 1.0f, 0.0f },
						{ -0.5f, h - 0.5f, 0.0f, 1.0f, 0.0f, 1.0f },
						{ w - 0.5f, -0.5f, 0.0f, 1.0f, 1.0f, 0.0f },
						{ w - 0.5f, h - 0.5f, 0.0f, 1.0f, 1.0f, 1.0f },
						{ -0.5f, h - 0.5f, 0.0f, 1.0f, 0.0f, 1.0f },
					};
					float glow[4] = { glowSettings[0], glowSettings[1], 1.5f / w, 1.5f / h };
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
					rendered = SUCCEEDED(device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 2, quad, sizeof(ScreenVertex)));

				}
				else
				{
					rendered = SUCCEEDED(device->StretchRect(sceneTarget, nullptr, target, nullptr, D3DTEXF_NONE));
				}

				if ((inTransition || bootPreview) && oldTarget)
				{
					device->SetRenderTarget(0, oldTarget);
					device->SetDepthStencilSurface(nullptr);
					D3DVIEWPORT9 backVp = oldViewport;
					device->SetViewport(&backVp);
					const auto bw = static_cast<float>(backVp.Width);
					const auto bh = static_cast<float>(backVp.Height);
					const ScreenVertex backQuad[] = {
						{ -0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f },
						{ bw - 0.5f, -0.5f, 0.0f, 1.0f, 1.0f, 0.0f },
						{ -0.5f, bh - 0.5f, 0.0f, 1.0f, 0.0f, 1.0f },
						{ bw - 0.5f, -0.5f, 0.0f, 1.0f, 1.0f, 0.0f },
						{ bw - 0.5f, bh - 0.5f, 0.0f, 1.0f, 1.0f, 1.0f },
						{ -0.5f, bh - 0.5f, 0.0f, 1.0f, 0.0f, 1.0f },
					};
					// Composite the already graded image exactly once, with explicit state.
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
					if (rendered)
					{
						device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 2, backQuad, sizeof(ScreenVertex));
					}
					else
					{
						device->StretchRect(sceneTarget, nullptr, oldTarget, nullptr, D3DTEXF_NONE);
					}

					if (whiteOpacity > 0.0f)
					{
						device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
						device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
						device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
						device->SetRenderState(D3DRS_ZENABLE, FALSE);
						device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
						device->SetVertexShader(nullptr);
						device->SetPixelShader(nullptr);
						device->SetTexture(0, nullptr);
						device->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);

						const DWORD darkAlpha = static_cast<DWORD>(std::clamp(whiteOpacity * 255.0f, 0.0f, 255.0f));
						const DWORD darkColor = (darkAlpha << 24) | 0xFFFFFF;
						device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
						device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
						device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);

						struct FadeVertex { float x, y, z, rhw; DWORD color; };
						const FadeVertex fadeQuad[] = {
							{ -0.5f, -0.5f, 0.0f, 1.0f, darkColor },
							{ bw - 0.5f, -0.5f, 0.0f, 1.0f, darkColor },
							{ -0.5f, bh - 0.5f, 0.0f, 1.0f, darkColor },
							{ bw - 0.5f, -0.5f, 0.0f, 1.0f, darkColor },
							{ bw - 0.5f, bh - 0.5f, 0.0f, 1.0f, darkColor },
							{ -0.5f, bh - 0.5f, 0.0f, 1.0f, darkColor },
						};
						device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 2, fadeQuad, sizeof(FadeVertex));
					}
				}
			}
			else rendered = false;

			device->SetRenderTarget(0, oldTarget);
			device->SetDepthStencilSurface(oldDepth);
			device->SetViewport(&oldViewport);
			if (savedState) savedState->Apply();
			oldDepth->Release();
			oldTarget->Release();
			target->Release();
			sceneTarget->Release();
			frameReady.store(rendered, std::memory_order_release);
		}
	}

	LobbyScene::LobbyScene()
	{
		if (Dedicated::IsEnabled() || ZoneBuilder::IsEnabled()) return;
		Scheduler::Loop(UpdateMenu, Scheduler::Pipeline::MAIN);

		Events::AfterUIInit([]
		{
			backgroundExpressionCounts.clear();
			materialNeedsRefresh = true;
			RefreshLobbyMaterial();
			PrepareStartup();
			const auto ready = IsSceneReady();
			AttachMaterialToLobby("main_text", ready);
			AttachMaterialToLobby("menu_xboxlive_privatelobby", ready);
			AttachMaterialToLobby("zwnet_matchmaking", ready);
			AttachMaterialToLobby("pregame_loaderror", ready);
		});
		Renderer::OnBackendFrame([](IDirect3DDevice9* device)
		{
			if (IsCinematicActive())
			{
				if (!frameReady.load(std::memory_order_acquire)) RenderRoom(device);
				return;
			}

			RenderRoom(device);
			if (IsStartupLoading() && !frameReady.load(std::memory_order_acquire))
				device->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
		});
		Renderer::OnDeviceRecoveryBegin([]
		{
			StopTransition();
			// Recreate the empty startup wave before warming the recovered scene texture.
			zombieWaveStarted.store(false, std::memory_order_release);
			lobbySession.fetch_add(1, std::memory_order_release);
			cameraResetSession.fetch_add(1, std::memory_order_release);
			frameReady.store(false, std::memory_order_release);
			assetsReady.store(false, std::memory_order_release);
			startupLoading.store(true, std::memory_order_release);
			startupWaitStart = 0;
			materialNeedsRefresh = true;
			AttachMaterialToLobby("main_text", false);
			AttachMaterialToLobby("menu_xboxlive_privatelobby", false);
			AttachMaterialToLobby("zwnet_matchmaking", false);
			AttachMaterialToLobby("pregame_loaderror", false);
			ReleaseDepth();
			ReleaseTextures();
			// Keep texturePackCache populated — only GPU resources are lost on device reset.
			// Set texturePackStage back to 2 if cache is still valid so we skip re-decompression.
		});
		Renderer::OnDeviceRecoveryEnd([]
		{
			frameReady.store(false, std::memory_order_release);

			// Refresh material technique pointers (fastfiles were reloaded during vid_restart)
			RefreshLobbyMaterial();

			// CPU-side mesh data (roomVertices, propVertices, actorMeshes, etc.) survives
			// device reset. If it's still loaded, re-enable assetsReady so the normal
			// OnBackendFrame → RenderRoom → EnsureRenderTarget path can recreate GPU resources
			// and render the first frame.
			if (!roomVertices.empty())
			{
				assetsReady.store(true, std::memory_order_release);
			}

			// texturePackCache (CPU bytes) is preserved across vid_restart.
			// If it's still valid, keep texturePackStage=2 so we skip re-decompression.
			// If it was cleared (e.g. by ReleaseResources for map load), reset to 0.
			if (texturePackCache.bytes.empty())
			{
				texturePackStage.store(0, std::memory_order_release);
			}

			// UpdateMenu attaches the replacement texture after the first complete frame.
		});
		Events::OnCLDisconnected([](bool)
		{
			zombieWaveStarted.store(false, std::memory_order_release);
			lobbySession.fetch_add(1, std::memory_order_release);
			if (sawConnectingState.load(std::memory_order_acquire))
			{
				StopTransition();
			}
		});
	}

	bool LobbyScene::IsTransitionActive()
	{
		return theaterDirectTransitionActive.load(std::memory_order_acquire);
	}

	bool LobbyScene::IsSceneReady()
	{
		return assetsReady.load(std::memory_order_acquire) && frameReady.load(std::memory_order_acquire);
	}

	bool LobbyScene::IsStartupLoading()
	{
		return !Dedicated::IsEnabled() && !ZoneBuilder::IsEnabled() &&
			startupLoading.load(std::memory_order_acquire) && !IsCinematicActive();
	}

	bool LobbyScene::IsCinematicActive()
	{
		if (*reinterpret_cast<void**>(0x069F980C) != nullptr) return true;
		const auto state = Game::CL_GetLocalClientConnectionState(0);
		return state == Game::CA_CINEMATIC || state == Game::CA_LOGO;
	}

	void LobbyScene::ReleaseResources()
	{
		static std::mutex releaseMutex;
		std::lock_guard lock(releaseMutex);

		if (!assetsReady.load(std::memory_order_acquire) && !frameReady.load(std::memory_order_acquire))
		{
			return;
		}

		frameReady.store(false, std::memory_order_release);
		assetsReady.store(false, std::memory_order_release);

		ReleaseDepth();
		ReleaseTextures();

		texturePackCache = {};
		texturePackStage.store(0, std::memory_order_release);

		roomVertices.clear();
		roomGroups.clear();
		propVertices.clear();
		propIndices.clear();
		propGroups.clear();
		doorLeftVertices.clear();
		doorRightVertices.clear();
		doorLeftGroups.clear();
		doorRightGroups.clear();
		for (auto& actor : actorMeshes)
		{
			actor.frames.clear();
			actor.interpolated.clear();
			actor.blended.clear();
			actor.groups.clear();
		}
	}

	void LobbyScene::PrepareStartup()
	{
		if (Dedicated::IsEnabled() || ZoneBuilder::IsEnabled() ||
			!Game::Sys_IsDatabaseReady() ||
			Game::CL_IsCgameInitialized() ||
			*reinterpret_cast<Game::connstate_t*>(0xB2C540) >= Game::CA_CONNECTING ||
			!*Game::dx_ptr || Renderer::Width() <= 0 || Renderer::Height() <= 0) return;
		if (!Game::DB_IsZoneLoaded("zw3_common") && !Game::DB_IsZoneLoaded("common_mp")) return;
		if (!Game::DB_IsZoneLoaded("zw3_lobby"))
		{
			if (FastFiles::Exists("zw3_lobby") && !Game::CL_IsCgameInitialized() &&
				*reinterpret_cast<Game::connstate_t*>(0xB2C540) < Game::CA_CONNECTING)
			{
				Game::XZoneInfo zone{ "zw3_lobby", 1, 0 };
				Game::DB_LoadXAssets(&zone, 1, true);
			}
			else
			{
				return;
			}
		}
		if (!Game::DB_IsZoneLoaded("zw3_lobby")) return;
		RefreshLobbyMaterial();
		if (assetsReady.load(std::memory_order_acquire))
		{
			return;
		}
		// No nested DB loads or renderer calls while the engine is loading its startup batch.
		PrepareTexturePack();
		LoadRoomMesh();
		if (!roomVertices.empty()) LoadPropMesh();
		LoadTheaterVision();
		static constexpr const char* names[] = { "richtofen", "dempsey", "nikolai", "takeo", "zombie" };
		for (auto i = 0u; i < std::size(names); ++i) LoadActorMesh(i, names[i]);
		// Cache the round-start PCM clip while the frontend database is ready. Playback
		// never performs an asset lookup or enters the game's sound-alias dispatcher.
		if (auto* entry = Game::DB_FindXAssetEntry(Game::ASSET_TYPE_RAWFILE, "lobby/audio/round_start.wav"))
		{
			auto* raw = entry->asset.header.rawfile;
			if (raw && raw->len > 0 && raw->len < 4 * 1024 * 1024)
			{
				std::string wav(raw->len + 1, '\0');
				Game::DB_GetRawBuffer(raw, wav.data(), static_cast<int>(wav.size()));
				wav.resize(raw->len);
				Sound::PrepareLobbyRoundStart(wav);
			}
		}
		assetsReady.store(true, std::memory_order_release);

	}

	bool LobbyScene::DeferLaunch(const std::function<void()>& launch)
	{
		if (executingDeferredLaunch) return false;
		if (IsTransitionActive()) return true; // Ignore duplicate clicks while queued/loading.
		if (!IsSceneReady() || (!IsLobbyVisible("menu_xboxlive_privatelobby") &&
			!IsLobbyVisible("zwnet_matchmaking"))) return false;
		StartTransition();
		const auto generation = transitionGeneration.load();
		Scheduler::Schedule([launch, generation]
		{
			if (generation != transitionGeneration.load() || !IsTransitionActive()) return true;
			const auto elapsed = timeGetTime() - transitionStartTime.load();
			if (elapsed < LobbyTransition::WhiteEndMs) return false;
			executingDeferredLaunch = true;
			const auto reset = gsl::finally([] { executingDeferredLaunch = false; });
			launch();
			return true;
		}, Scheduler::Pipeline::MAIN);
		return true;
	}

	void LobbyScene::StartTransition()
	{
		if (!IsSceneReady()) return;
		if (theaterDirectTransitionActive.load(std::memory_order_acquire)) return;
		transitionStartTime.store(timeGetTime(), std::memory_order_release);
		transitionGeneration.fetch_add(1);
		fadePresented.store(false, std::memory_order_release);
		theaterDirectTransitionActive.store(true, std::memory_order_release);
		sawConnectingState.store(false, std::memory_order_release);

		Game::Key_RemoveCatcher(0, ~Game::KEYCATCH_UI);
		Sound::PlayLobbyRoundStart();
		Game::Key_ClearStates(0);
		if (Game::uiContext)
		{
			Game::uiContext->cursor.x = -1000.0f;
			Game::uiContext->cursor.y = -1000.0f;
			Game::uiContext->isCursorVisible = 0;
		}

		if (Game::ui_mapname && *Game::ui_mapname && (*Game::ui_mapname)->current.string)
		{
			const std::string mapName = (*Game::ui_mapname)->current.string;
			if (!mapName.empty())
			{
				D3D9Ex::BeginMapLoading(mapName);
				FastFiles::PrefetchZone(mapName);
				FastFiles::PrefetchZone(mapName + "_load");
				FastFiles::PrefetchZone("patch_" + mapName);
				FastFiles::PrefetchZone("localized_" + mapName);
			}
		}
	}

	void LobbyScene::StopTransition()
	{
		if (!theaterDirectTransitionActive.load(std::memory_order_acquire)) return;
		theaterDirectTransitionActive.store(false, std::memory_order_release);
		transitionGeneration.fetch_add(1);
		sawConnectingState.store(false, std::memory_order_release);
		transitionStartTime.store(0, std::memory_order_release);
		if (Game::uiContext && Game::uiContext->openMenuCount > 0)
		{
			Game::Key_SetCatcher(0, Game::KEYCATCH_UI);
		}
	}

	LobbyScene::~LobbyScene()
	{
		StopTransition();
		ReleaseResources();
	}
}
