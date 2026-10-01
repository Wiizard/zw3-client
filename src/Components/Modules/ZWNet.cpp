#include <Utils/WebIO.hpp>
#include <Utils/CSV.hpp>
#include <wincrypt.h>

#include "ZWNet.hpp"
#include "Auth.hpp"
#include "Command.hpp"
#include "Events.hpp"
#include "FileSystem.hpp"
#include "Friends.hpp"
#include "Localization.hpp"
#include "Maps.hpp"
#include "Party.hpp"
#include "Scheduler.hpp"
#include "TextRenderer.hpp"
#include "UIScript.hpp"

namespace Components
{
	namespace
	{
		constexpr auto ZWNET_API_BASE = "https://backend.zw3.eu";
		constexpr auto ZWNET_CLIENT_VERSION = "3.0.3";
		constexpr auto ZWNET_MOD_VERSION = "3.0.3";
		constexpr std::size_t ZWNET_MATERIAL_ENUM_CAPACITY = 16384;
		constexpr std::size_t ZWNET_PLAYLIST_PAGE_SIZE = 5;
		constexpr std::size_t ZWNET_PLAYLIST_MAX_MAPS = 512;
		constexpr auto ZWNET_CONTENT_MANIFEST = "zw3/core/zwnet-content-manifest.json";
		constexpr std::uintmax_t ZWNET_CONTENT_MANIFEST_MAX_BYTES = 256 * 1024;
		constexpr std::size_t ZWNET_CONTENT_MANIFEST_MAX_ENTRIES = 256;
		constexpr std::size_t ZWNET_CONTENT_ENTRY_MAX_FILES = 32;
		constexpr std::size_t ZWNET_CONTENT_MANIFEST_MAX_FILES = 1024;
		constexpr std::uintmax_t ZWNET_CONTENT_MANIFEST_MAX_DECLARED_BYTES = 50ULL * 1024 * 1024 * 1024;
		constexpr std::size_t ZWNET_CONTENT_HASH_BUFFER_SIZE = 1024 * 1024;

		struct ClientContentFile
		{
			std::filesystem::path relativePath;
			std::uintmax_t size = 0;
			std::string sha256;
		};

		struct ClientContentDefinition
		{
			std::string id;
			std::string version;
			std::vector<ClientContentFile> files;
		};

		struct ClientContentManifest
		{
			std::string version;
			std::unordered_map<std::string, ClientContentDefinition> entries;
			std::string error;

			[[nodiscard]] bool valid() const noexcept
			{
				return error.empty();
			}
		};

		struct ClientContentFileStamp
		{
			std::uintmax_t size = 0;
			std::filesystem::file_time_type modified{};

			bool operator==(const ClientContentFileStamp&) const = default;
		};

		struct ClientContentHash
		{
			ClientContentFileStamp stamp;
			std::string sha256;
		};

		std::mutex ClientContentHashMutex;
		std::unordered_map<std::string, ClientContentHash> ClientContentHashCache;

		struct ClientPlaylist
		{
			std::string id;
			std::string name;
			std::string description;
			std::string audience;
			std::string availability;
			std::string preview;
			std::string mapId;
			std::string mapImage;
			std::string rotationSummary;
			std::string zombieSettingsSummary;
			int minPlayers = 1;
			int maxPlayers = 4;
			std::string availabilityDetail;
			std::vector<std::string> requiredContent;
			std::vector<std::string> verifiedContent;
			std::int64_t revision = 0;
		};

		struct ClientPlaylistCatalog
		{
			std::mutex mutex;
			std::vector<ClientPlaylist> entries;
			std::vector<ClientPlaylist> pendingEntries;
			std::string accountId;
			std::string selectedId;
			std::string partySelectedId;
			std::int64_t revision = 0;
			std::int64_t pendingRevision = 0;
			std::int64_t partySelectedRevision = 0;
			std::uint64_t generation = 0;
			std::size_t page = 0;
			bool loaded = false;
			bool stale = false;
			bool inFlight = false;
			bool hasPending = false;
			bool notice = false;
			std::chrono::steady_clock::time_point lastAttempt{};
		};

		ClientPlaylistCatalog& PlaylistCatalogState()
		{
			static ClientPlaylistCatalog value;
			return value;
		}

		std::atomic_bool& PlaylistSearchStarting()
		{
			static std::atomic_bool value = false;
			return value;
		}

		std::atomic_bool& PlaylistSelectionStarting()
		{
			static std::atomic_bool value = false;
			return value;
		}

		std::atomic_bool& PlaylistSelectorOpen()
		{
			static std::atomic_bool value = false;
			return value;
		}

		std::atomic_uint64_t& PlaylistSelectorGeneration()
		{
			static std::atomic_uint64_t value = 0;
			return value;
		}

		std::atomic_int64_t& PlaylistSelectorOpenedAt()
		{
			static std::atomic_int64_t value = 0;
			return value;
		}

		std::int64_t PlaylistClockMilliseconds()
		{
			return std::chrono::duration_cast<std::chrono::milliseconds>(
				std::chrono::steady_clock::now().time_since_epoch()).count();
		}

		bool IsCurrentPlaylistActivation(const std::uint64_t generation)
		{
			return PlaylistSelectorOpen() && PlaylistSelectorGeneration() == generation;
		}

		std::string SafePlaylistText(const std::string& value, const std::size_t limit)
		{
			std::string safe;
			safe.reserve(std::min(value.size(), limit));
			for (const auto c : value)
			{
				if (safe.size() >= limit) break;
				if (static_cast<unsigned char>(c) < 32 || c == 127) continue;
				safe.push_back(c);
			}
			return safe;
		}

		bool IsContentId(const std::string& value)
		{
			return !value.empty() && value.size() <= 120 &&
				std::ranges::all_of(value, [](const unsigned char c)
				{
					return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
						(c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-';
				});
		}

		bool IsLowerSha256(const std::string& value)
		{
			return value.size() == 64 && std::ranges::all_of(value, [](const unsigned char c)
			{
				return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
			});
		}

		std::optional<std::filesystem::path> SafeContentRelativePath(const std::string& raw)
		{
			if (raw.empty() || raw.size() > 240 || raw.find('\\') != std::string::npos ||
				raw.find(':') != std::string::npos ||
				raw.front() == '/' || raw.back() == '/') return std::nullopt;
			const std::filesystem::path relative{raw};
			if (relative.is_absolute() || relative.has_root_path()) return std::nullopt;
			for (const auto& part : relative)
			{
				if (part == "." || part == ".." || part.empty()) return std::nullopt;
			}
			const auto normalized = relative.generic_string();
			if (normalized != raw) return std::nullopt;
			static constexpr std::array allowedPrefixes
			{
				"zw3/", "main/", "zone/", "usermaps/", "userraw/"
			};
			if (normalized != "zw3.dll" && normalized != "zw3.exe" &&
				!std::ranges::any_of(allowedPrefixes,
					[&](const std::string_view prefix) { return normalized.starts_with(prefix); }))
				return std::nullopt;
			return relative;
		}

		std::optional<ClientContentFileStamp> ContentFileStamp(const std::filesystem::path& path)
		{
			std::error_code error;
			if (!std::filesystem::is_regular_file(path, error) || error) return std::nullopt;
			const auto size = std::filesystem::file_size(path, error);
			if (error) return std::nullopt;
			const auto modified = std::filesystem::last_write_time(path, error);
			if (error) return std::nullopt;
			return ClientContentFileStamp{size, modified};
		}

		std::optional<std::filesystem::path> ResolveContentFile(const std::filesystem::path& base,
			const std::filesystem::path& relative)
		{
			std::error_code error;
			const auto resolved = std::filesystem::canonical(base / relative, error);
			if (error) return std::nullopt;
			const auto within = resolved.lexically_relative(base);
			if (within.empty() || within.is_absolute() || *within.begin() == "..") return std::nullopt;
			return resolved;
		}

		std::optional<std::string> HashContentFile(const std::filesystem::path& path,
			const ClientContentFileStamp& expectedStamp)
		{
			const auto cacheKey = path.lexically_normal().generic_string();
			{
				std::lock_guard lock(ClientContentHashMutex);
				if (const auto cached = ClientContentHashCache.find(cacheKey);
					cached != ClientContentHashCache.end() && cached->second.stamp == expectedStamp)
					return cached->second.sha256;
			}

			const auto file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
				nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
			if (file == INVALID_HANDLE_VALUE) return std::nullopt;
			const auto closeFile = gsl::finally([file] { CloseHandle(file); });
			hash_state state{};
			if (sha256_init(&state) != CRYPT_OK) return std::nullopt;
			std::vector<unsigned char> buffer(ZWNET_CONTENT_HASH_BUFFER_SIZE);
			for (;;)
			{
				DWORD bytesRead = 0;
				if (!ReadFile(file, buffer.data(), static_cast<DWORD>(buffer.size()), &bytesRead, nullptr))
					return std::nullopt;
				if (bytesRead == 0) break;
				if (sha256_process(&state, buffer.data(), bytesRead) != CRYPT_OK) return std::nullopt;
			}
			std::array<unsigned char, 32> digest{};
			if (sha256_done(&state, digest.data()) != CRYPT_OK) return std::nullopt;
			const auto finalStamp = ContentFileStamp(path);
			if (!finalStamp || *finalStamp != expectedStamp) return std::nullopt;
			static constexpr char hex[] = "0123456789abcdef";
			std::string result;
			result.resize(digest.size() * 2);
			for (std::size_t i = 0; i < digest.size(); ++i)
			{
				result[i * 2] = hex[digest[i] >> 4];
				result[i * 2 + 1] = hex[digest[i] & 0x0F];
			}
			{
				std::lock_guard lock(ClientContentHashMutex);
				ClientContentHashCache[cacheKey] = {*finalStamp, result};
			}
			return result;
		}

		ClientContentManifest LoadClientContentManifest()
		{
			ClientContentManifest manifest;
			std::error_code error;
			const auto basePath = std::filesystem::canonical(
				std::filesystem::path{(*Game::fs_basepath)->current.string}, error);
			const auto path = error ? std::nullopt :
				ResolveContentFile(basePath, std::filesystem::path{ZWNET_CONTENT_MANIFEST});
			const auto size = path ? std::filesystem::file_size(*path, error) : 0;
			if (error || size == 0 || size > ZWNET_CONTENT_MANIFEST_MAX_BYTES)
			{
				manifest.error = "The local ZW3 content manifest is missing or too large.";
				return manifest;
			}
			std::string serialized;
			if (!path || !Utils::IO::ReadFile(path->string(), &serialized) || serialized.size() != size)
			{
				manifest.error = "The local ZW3 content manifest could not be read.";
				return manifest;
			}
			const auto document = nlohmann::json::parse(serialized, nullptr, false);
			if (!document.is_object() || document.value("schema_version", 0) != 1 ||
				!document.contains("manifest_version") || !document.at("manifest_version").is_string() ||
				!document.contains("content") || !document.at("content").is_array() ||
				document.at("content").size() > ZWNET_CONTENT_MANIFEST_MAX_ENTRIES)
			{
				manifest.error = "The local ZW3 content manifest is invalid.";
				return manifest;
			}
			manifest.version = document.at("manifest_version").get<std::string>();
			if (manifest.version.empty() || manifest.version.size() > 64)
			{
				manifest.error = "The local ZW3 content manifest version is invalid.";
				return manifest;
			}
			std::size_t totalFiles = 0;
			std::uintmax_t totalBytes = 0;
			for (const auto& item : document.at("content"))
			{
				if (!item.is_object() || !item.contains("id") || !item.at("id").is_string() ||
					!item.contains("version") || !item.at("version").is_string() ||
					!item.contains("files") || !item.at("files").is_array() ||
					item.at("files").empty() || item.at("files").size() > ZWNET_CONTENT_ENTRY_MAX_FILES)
				{
					manifest.error = "A local ZW3 content definition is invalid.";
					return manifest;
				}
				ClientContentDefinition definition;
				definition.id = item.at("id").get<std::string>();
				definition.version = item.at("version").get<std::string>();
				if (!IsContentId(definition.id) || definition.version.empty() ||
					definition.version.size() > 64 || manifest.entries.contains(definition.id))
				{
					manifest.error = "A local ZW3 content identity is invalid or duplicated.";
					return manifest;
				}
				totalFiles += item.at("files").size();
				if (totalFiles > ZWNET_CONTENT_MANIFEST_MAX_FILES)
				{
					manifest.error = "The local ZW3 content manifest defines too many files.";
					return manifest;
				}
				std::unordered_set<std::string> paths;
				for (const auto& file : item.at("files"))
				{
					if (!file.is_object() || !file.contains("path") || !file.at("path").is_string() ||
						!file.contains("size") || !file.at("size").is_number_unsigned() ||
						!file.contains("sha256") || !file.at("sha256").is_string())
					{
						manifest.error = "A local ZW3 content file definition is invalid.";
						return manifest;
					}
					const auto rawPath = file.at("path").get<std::string>();
					const auto relativePath = SafeContentRelativePath(rawPath);
					const auto sha256 = file.at("sha256").get<std::string>();
					const auto fileSize = file.at("size").get<std::uintmax_t>();
					if (!relativePath || !paths.insert(Utils::String::ToLower(rawPath)).second ||
						!IsLowerSha256(sha256) ||
						fileSize > ZWNET_CONTENT_MANIFEST_MAX_DECLARED_BYTES - totalBytes)
					{
						manifest.error = "A local ZW3 content file path or hash is invalid.";
						return manifest;
					}
					totalBytes += fileSize;
					definition.files.push_back({*relativePath, fileSize, sha256});
				}
				manifest.entries.emplace(definition.id, std::move(definition));
			}
			return manifest;
		}

		void VerifyPlaylistContent(std::vector<ClientPlaylist>& entries)
		{
			if (!std::ranges::any_of(entries,
				[](const ClientPlaylist& entry) { return !entry.requiredContent.empty(); }))
			{
				for (auto& entry : entries)
					entry.availabilityDetail = entry.availability == "AVAILABLE"
						? "Ready to search." : "No compatible server capacity is available.";
				return;
			}

			const auto manifest = LoadClientContentManifest();
			std::error_code baseError;
			const auto basePath = std::filesystem::canonical(
				std::filesystem::path{(*Game::fs_basepath)->current.string}, baseError);
			for (auto& entry : entries)
			{
				entry.verifiedContent.clear();
				if (entry.availability != "AVAILABLE")
				{
					entry.availabilityDetail = "No compatible server capacity is available.";
					continue;
				}
				if (entry.requiredContent.empty())
				{
					entry.availabilityDetail = "Ready to search.";
					continue;
				}
				if (!manifest.valid() || baseError)
				{
					entry.availability = "CONTENT_MANIFEST_INVALID";
					entry.availabilityDetail = baseError ?
						"The local ZW3 game root could not be verified." : manifest.error;
					continue;
				}
				bool failed = false;
				for (const auto& contentId : entry.requiredContent)
				{
					const auto definition = manifest.entries.find(contentId);
					if (definition == manifest.entries.end())
					{
						entry.availability = "CONTENT_UNKNOWN";
						entry.availabilityDetail = SafePlaylistText(
							"Update required: content " + contentId + " is not defined locally.", 180);
						failed = true;
						break;
					}
					for (const auto& file : definition->second.files)
					{
						const auto absolutePath = ResolveContentFile(basePath, file.relativePath);
						const auto stamp = absolutePath ? ContentFileStamp(*absolutePath) : std::nullopt;
						if (!stamp || stamp->size != file.size)
						{
							entry.availability = "CONTENT_MISSING";
							entry.availabilityDetail = SafePlaylistText(
								"Update required: " + contentId + " is missing " + file.relativePath.generic_string() + ".", 180);
							failed = true;
							break;
						}
						const auto hash = HashContentFile(*absolutePath, *stamp);
						if (!hash || *hash != file.sha256)
						{
							entry.availability = "CONTENT_CORRUPT";
							entry.availabilityDetail = SafePlaylistText(
								"Repair required: " + contentId + " failed its integrity check.", 180);
							failed = true;
							break;
						}
					}
					if (failed) break;
					entry.verifiedContent.push_back(contentId);
				}
				if (!failed)
				{
					entry.availabilityDetail = std::format("Content verified (manifest {}).", manifest.version);
				}
			}
		}

		bool ParseClientPlaylistCatalog(const nlohmann::json& data,
			std::int64_t& revision, std::vector<ClientPlaylist>& entries)
		{
			if (!data.is_object() || !data.contains("schema_version") ||
				!data.at("schema_version").is_number_integer() ||
				data.at("schema_version").get<int>() != 1 ||
				!data.contains("catalog_revision") ||
				!data.at("catalog_revision").is_number_integer() ||
				!data.contains("playlists") || !data.at("playlists").is_array() ||
				data.at("playlists").size() > 128) return false;
			revision = data.at("catalog_revision").get<std::int64_t>();
			if (revision < 0) return false;
			std::unordered_set<std::string> ids;
			for (const auto& row : data.at("playlists"))
			{
				if (!row.is_object() || !row.contains("id") || !row.at("id").is_string() ||
					!row.contains("name") || !row.at("name").is_string() ||
					(row.contains("description") && !row.at("description").is_string()) ||
					!row.contains("revision") || !row.at("revision").is_number_integer() ||
					!row.contains("audience") || !row.at("audience").is_string() ||
					!row.contains("availability") || !row.at("availability").is_string())
					return false;
				if (!row.contains("required_content") || !row.at("required_content").is_array() ||
					row.at("required_content").size() > 64) return false;
				ClientPlaylist entry;
				entry.id = row.at("id").get<std::string>();
				entry.name = SafePlaylistText(row.at("name").get<std::string>(), 80);
				entry.description = row.value("description", std::string{});
				entry.description = SafePlaylistText(entry.description, 180);
				entry.audience = row.at("audience").get<std::string>();
				entry.availability = row.at("availability").get<std::string>();
				entry.revision = row.at("revision").get<std::int64_t>();
				if (row.contains("min_players") && row.at("min_players").is_number_integer())
					entry.minPlayers = row.at("min_players").get<int>();
				if (row.contains("max_players") && row.at("max_players").is_number_integer())
					entry.maxPlayers = row.at("max_players").get<int>();
				std::string zombieMode = "NORMAL";
				bool hitmarkers = true, zombieCounter = false, damageNumbers = true, dayNightCycle = true, omnimovement = true;
				if (row.contains("zombie_settings"))
				{
					const auto& settings = row.at("zombie_settings");
					if (!settings.is_object() || !settings.contains("mode") || !settings.at("mode").is_string() ||
						!settings.contains("hitmarkers") || !settings.at("hitmarkers").is_boolean() ||
						!settings.contains("zombie_counter") || !settings.at("zombie_counter").is_boolean() ||
						!settings.contains("damage_numbers") || !settings.at("damage_numbers").is_boolean() ||
						!settings.contains("day_night_cycle") || !settings.at("day_night_cycle").is_boolean() ||
						!settings.contains("omnimovement") || !settings.at("omnimovement").is_boolean()) return false;
					zombieMode = settings.at("mode").get<std::string>();
					hitmarkers = settings.at("hitmarkers").get<bool>();
					zombieCounter = settings.at("zombie_counter").get<bool>();
					damageNumbers = settings.at("damage_numbers").get<bool>();
					dayNightCycle = settings.at("day_night_cycle").get<bool>();
					omnimovement = settings.at("omnimovement").get<bool>();
				}
				if (zombieMode != "NORMAL" && zombieMode != "CLASSIC" && zombieMode != "HARDCORE") return false;
				entry.zombieSettingsSummary = std::format("{} / HITMARKERS {} / COUNTER {} / DAMAGE {} / DAY-NIGHT {} / OMNI {}",
					zombieMode, hitmarkers ? "ON" : "OFF", zombieCounter ? "ON" : "OFF", damageNumbers ? "ON" : "OFF",
					dayNightCycle ? "ON" : "OFF", omnimovement ? "ON" : "OFF");
				if (entry.id.empty() || entry.id.size() > 96 || entry.name.empty() ||
					entry.revision < 1 || !ids.insert(entry.id).second ||
					entry.minPlayers < 1 || entry.maxPlayers < entry.minPlayers || entry.maxPlayers > 18 ||
					(entry.audience != "PUBLIC" && entry.audience != "RESTRICTED") ||
					(entry.availability != "AVAILABLE" && entry.availability != "NO_CAPACITY") ||
					!std::ranges::all_of(entry.id, [](const unsigned char c)
					{
						return std::isalnum(c) || c == '-' || c == '_';
					})) return false;
				for (const auto& item : row.at("required_content"))
				{
					if (!item.is_string()) return false;
					const auto contentId = item.get<std::string>();
					if (!IsContentId(contentId)) return false;
					entry.requiredContent.push_back(contentId);
				}
				if (row.contains("maps"))
				{
					if (!row.at("maps").is_array() || row.at("maps").empty() ||
						row.at("maps").size() > ZWNET_PLAYLIST_MAX_MAPS) return false;
					std::vector<std::string> mapNames;
					for (const auto& map : row.at("maps"))
					{
						if (!map.is_object() || !map.contains("id") || !map.at("id").is_string() ||
							!map.contains("name") || !map.at("name").is_string()) return false;
						const auto mapId = map.at("id").get<std::string>();
						const auto mapName = SafePlaylistText(map.at("name").get<std::string>(), 60);
						if (mapId.empty() || mapId.size() > 80 || mapName.empty() ||
							!std::ranges::all_of(mapId, [](const unsigned char c)
							{
								return std::isalnum(c) || c == '_';
							})) return false;
						if (mapNames.empty())
						{
							entry.mapId = mapId;
							if (map.contains("image") && map.at("image").is_string())
								entry.mapImage = map.at("image").get<std::string>();
							if (entry.mapImage.size() > 80) return false;
						}
						mapNames.push_back(mapName);
					}
					if (mapNames.size() == 1)
					{
						entry.preview = mapNames.front();
						entry.rotationSummary = "FIXED MAP  /  " + mapNames.front();
					}
					else
					{
						entry.preview = std::format("{} MAP ROTATION", mapNames.size());
						entry.rotationSummary = std::format("{} maps: {}", mapNames.size(), mapNames[0]);
						if (mapNames.size() > 1) entry.rotationSummary += ", " + mapNames[1];
						if (mapNames.size() > 2) entry.rotationSummary +=
							std::format(" + {} more", mapNames.size() - 2);
						entry.rotationSummary = SafePlaylistText(entry.rotationSummary, 180);
					}
				}
				entries.push_back(std::move(entry));
			}
			return true;
		}

		std::array<Game::XAssetHeader, ZWNET_MATERIAL_ENUM_CAPACITY> MaterialEnumerationAssets{};
		std::uint32_t MaterialEnumerationCount{};
		std::string FormatPublicGuid();

		std::string ResolveVoteMapImage(const std::string& mapId, const std::string& serverImage)
		{
			if (!mapId.empty())
			{
				const auto* arenaImage = Localization::GetMapImageName(mapId.c_str());
				if (arenaImage && arenaImage[0]) return arenaImage;
			}

			if (!serverImage.empty() && serverImage != mapId &&
				!serverImage.starts_with("preview_mp_mp_") && serverImage.starts_with("preview_"))
			{
				return serverImage;
			}

			return mapId.empty() ? serverImage : "preview_" + mapId;
		}

		std::string ResolveVoteMapDisplayName(const std::string& mapId, const std::string& serverName)
		{
			if (!serverName.empty() && serverName != mapId) return serverName;
			if (!mapId.empty())
			{
				const auto* localized = Localization::LocalizeMapName(mapId.c_str());
				if (localized && localized[0] && localized != mapId) return localized;
			}
			return serverName.empty() ? mapId : serverName;
		}

		struct SharedLobbyRank
		{
			int level = 1;
			int prestige = 0;
		};

		struct LocalBarracksRank
		{
			int level = 1;
			int prestige = 0;
			int experience = 0;
			int experienceTarget = 50;
		};

		struct LocalChallengeProgress
		{
			int progress = 0;
			int tier = 0;
		};

		struct LocalBarracksChallenges
		{
			std::unordered_map<std::string, LocalChallengeProgress> entries;
			int zombieKills = 0;
			int zombieDeaths = 0;
			int zombieRevives = 0;
		};

		struct ChallengeDefinition
		{
			std::string id;
			std::array<int, 4> targets{};
			std::array<int, 4> rewards{};
		};

		constexpr std::array ChallengeSlotProgressDvars
		{
			"zw3_ch_slot_0_progress", "zw3_ch_slot_1_progress", "zw3_ch_slot_2_progress",
			"zw3_ch_slot_3_progress", "zw3_ch_slot_4_progress"
		};
		constexpr std::array ChallengeSlotTargetDvars
		{
			"zw3_ch_slot_0_target", "zw3_ch_slot_1_target", "zw3_ch_slot_2_target",
			"zw3_ch_slot_3_target", "zw3_ch_slot_4_target"
		};
		constexpr std::array ChallengeSlotTierDvars
		{
			"zw3_ch_slot_0_tier", "zw3_ch_slot_1_tier", "zw3_ch_slot_2_tier",
			"zw3_ch_slot_3_tier", "zw3_ch_slot_4_tier"
		};
		constexpr std::array ChallengeSlotTierCountDvars
		{
			"zw3_ch_slot_0_tier_count", "zw3_ch_slot_1_tier_count", "zw3_ch_slot_2_tier_count",
			"zw3_ch_slot_3_tier_count", "zw3_ch_slot_4_tier_count"
		};
		constexpr std::array ChallengeSlotRewardDvars
		{
			"zw3_ch_slot_0_reward", "zw3_ch_slot_1_reward", "zw3_ch_slot_2_reward",
			"zw3_ch_slot_3_reward", "zw3_ch_slot_4_reward"
		};
		constexpr std::array ChallengeSlotPercentDvars
		{
			"zw3_ch_slot_0_percent", "zw3_ch_slot_1_percent", "zw3_ch_slot_2_percent",
			"zw3_ch_slot_3_percent", "zw3_ch_slot_4_percent"
		};
		constexpr std::array ChallengeSlotCompleteDvars
		{
			"zw3_ch_slot_0_complete", "zw3_ch_slot_1_complete", "zw3_ch_slot_2_complete",
			"zw3_ch_slot_3_complete", "zw3_ch_slot_4_complete"
		};

		using SharedLobbyRankMap =
			std::unordered_map<std::string, SharedLobbyRank>;

		std::mutex SharedLobbyRankMutex;
		SharedLobbyRankMap SharedLobbyRanks;
		std::string SharedLobbyRankPartyId;
		std::chrono::steady_clock::time_point NextRankPublishAttempt{};
		std::string LastRankPublishPartyId;
		int LastRankPublishLevel = -1;
		int LastRankPublishPrestige = -1;

		std::atomic_int& DesiredPartyPrivacy()
		{
			static std::atomic_int value{0};
			return value;
		}

		std::atomic_bool& LocalPartyLeader()
		{
			static std::atomic_bool value{false};
			return value;
		}

		std::atomic_bool& VisibilitySyncPending()
		{
			static std::atomic_bool value{false};
			return value;
		}

		std::atomic_bool& EndpointJoinInFlight()
		{
			static std::atomic_bool value{false};
			return value;
		}

		std::atomic_bool& ManagedReconnectInFlight()
		{
			static std::atomic_bool value{false};
			return value;
		}

		std::atomic_uint64_t& JoinTransitionGeneration()
		{
			static std::atomic_uint64_t value{0};
			return value;
		}

		struct JoinPreviewState
		{
			std::mutex mutex;
			std::uint64_t generation{};
			std::string matchId;
			std::string completedMatchId;
			bool active{};
		};

		JoinPreviewState& JoinPreview()
		{
			static JoinPreviewState value;
			return value;
		}

		std::atomic_bool& JoinInProgressConnectionSoundPending()
		{
			static std::atomic_bool value{false};
			return value;
		}

		std::atomic_bool& NetworkMetricsEnabled()
		{
			static std::atomic_bool value{false};
			return value;
		}

		void MarkJoinInProgressConnectionStarted(const std::string& matchId)
		{
			auto& preview = JoinPreview();
			{
				std::lock_guard lock(preview.mutex);
				if (preview.completedMatchId != matchId) return;
			}
			JoinInProgressConnectionSoundPending() = true;
			Scheduler::Once([] { JoinInProgressConnectionSoundPending() = false; },
				Scheduler::Pipeline::MAIN, 15s);
		}

		struct ManagedRouteAttempt
		{
			std::mutex mutex;
			std::string matchId;
			std::string playerId;
			std::string sessionId;
			std::string serverIdentity;
			std::string instanceId;
			Network::Address directTarget;
			Network::Address assignedTarget;
			bool directWaiting = false;
			bool relayUsed = false;
			bool routeIsRelay = false;
			bool reconnectAttempt = false;
		};

		ManagedRouteAttempt& RouteAttempt()
		{
			static ManagedRouteAttempt value;
			return value;
		}

		struct RelayHandshake
		{
			std::mutex mutex;
			std::uint64_t generation = 0;
			bool pending = false;
			bool ready = false;
			Network::Address target;
			std::string matchId;
			std::string playerId;
			std::string nonce;
			std::string hello;
			std::chrono::steady_clock::time_point deadline{};
		};

		RelayHandshake& RelayState()
		{
			static RelayHandshake value;
			return value;
		}

		void CancelRelayHandshake()
		{
			auto& relay = RelayState();
			std::lock_guard lock(relay.mutex);
			++relay.generation;
			relay.pending = false;
			relay.ready = false;
			std::ranges::fill(relay.hello, '\0');
			relay.hello.clear();
			relay.nonce.clear();
			relay.matchId.clear();
			relay.playerId.clear();
		}

		void ClearManagedRouteAttempt()
		{
			auto& route = RouteAttempt();
			std::lock_guard lock(route.mutex);
			route.matchId.clear();
			route.playerId.clear();
			route.sessionId.clear();
			route.serverIdentity.clear();
			route.instanceId.clear();
			route.directWaiting = false;
			route.relayUsed = false;
			route.routeIsRelay = false;
			route.reconnectAttempt = false;
		}

		void MarkManagedRouteConnected()
		{
			auto& route = RouteAttempt();
			std::lock_guard lock(route.mutex);
			route.directWaiting = false;
		}

		bool RelayAlreadyUsedForMatch(const std::string& matchId)
		{
			auto& route = RouteAttempt();
			std::lock_guard lock(route.mutex);
			return route.matchId == matchId && route.relayUsed;
		}

		bool IsRelayOpaque(const std::string& value)
		{
			return !value.empty() && value.size() <= 128 &&
				std::ranges::all_of(value, [](const unsigned char character)
				{
					return std::isalnum(character) != 0 || character == '-' || character == '_';
				});
		}

		std::atomic_bool& TerminalDisconnectRequested()
		{
			static std::atomic_bool value{false};
			return value;
		}

		const char* PartyVisibilityName(const int privacy)
		{
			switch (privacy)
			{
			case 1: return "INVITE_ONLY";
			case 2: return "CLOSED";
			default: return "OPEN";
			}
		}

		std::string NormalizePartyVisibility(std::string visibility)
		{
			std::ranges::transform(visibility, visibility.begin(), [](const unsigned char character)
			{
				return static_cast<char>(std::toupper(character));
			});
			if (visibility == "INVITE" || visibility == "FRIENDS") return "INVITE_ONLY";
			if (visibility != "OPEN" && visibility != "INVITE_ONLY" && visibility != "CLOSED") return "OPEN";
			return visibility;
		}

		std::atomic_int& CachedPartyMemberCount()
		{
			static std::atomic_int value{0};
			return value;
		}

		std::atomic_int& CachedPartyVisibility()
		{
			static std::atomic_int value{2};
			return value;
		}

		std::atomic_bool& CachedPartyJoinStateSupported()
		{
			static std::atomic_bool value{false};
			return value;
		}

		int PartyVisibilityValue(const std::string& visibility)
		{
			if (visibility == "CLOSED") return 2;
			if (visibility == "INVITE_ONLY") return 1;
			return 0;
		}

		bool IsPartyJoinStateSupported(const std::string& state)
		{
			return state == "IDLE" || state == "IN_PARTY" ||
				state == "SEARCHING" || state == "MATCH_FOUND" ||
				state == "MAP_VOTE" || state == "READY_CHECK" ||
				state == "WAITING_FOR_READY" || state == "RESERVING_SERVER" ||
				state == "STARTING_SERVER" || state == "SERVER_STARTING" ||
				state == "CONNECTING" || state == "IN_MATCH";
		}

		bool IsOpaquePartyId(const std::string& value)
		{
			return value.starts_with("pty_") && value.size() <= 80 &&
				std::ranges::all_of(value, [](const unsigned char character)
				{
					return std::isalnum(character) != 0 || character == '-' || character == '_';
				});
		}

		bool IsOpaqueMatchId(const std::string& value)
		{
			return value.starts_with("mat_") && value.size() <= 80 &&
				std::ranges::all_of(value, [](const unsigned char character)
				{
					return std::isalnum(character) != 0 || character == '-' || character == '_';
				});
		}

		bool IsOpaqueJoinCapability(const std::string& value)
		{
			return value.size() >= 16 && value.size() <= 256 &&
				std::ranges::all_of(value, [](const unsigned char character)
				{
					return std::isalnum(character) != 0 || character == '-' || character == '_';
				});
		}

		struct MatchLobbySoundDelta
		{
			bool joined{};
			bool left{};
		};

		std::mutex MatchLobbySoundMutex;
		std::string MatchLobbySoundMatchId;
		std::unordered_set<std::string> MatchLobbySoundMembers;
		bool MatchLobbySoundInitialized{};

		void ResetMatchLobbySoundSnapshot()
		{
			std::lock_guard lock(MatchLobbySoundMutex);
			MatchLobbySoundMatchId.clear();
			MatchLobbySoundMembers.clear();
			MatchLobbySoundInitialized = false;
		}

		MatchLobbySoundDelta ObserveMatchLobbyMembers(const std::string& matchId,
			const std::unordered_set<std::string>& members)
		{
			std::lock_guard lock(MatchLobbySoundMutex);
			if (matchId.empty())
			{
				MatchLobbySoundMatchId.clear();
				MatchLobbySoundMembers.clear();
				MatchLobbySoundInitialized = false;
				return {};
			}

			// Entering a lobby or switching matches establishes a silent baseline.
			if (!MatchLobbySoundInitialized || MatchLobbySoundMatchId != matchId)
			{
				MatchLobbySoundMatchId = matchId;
				MatchLobbySoundMembers = members;
				MatchLobbySoundInitialized = true;
				return {};
			}

			MatchLobbySoundDelta delta;
			for (const auto& member : members)
			{
				if (!MatchLobbySoundMembers.contains(member)) delta.joined = true;
			}
			for (const auto& member : MatchLobbySoundMembers)
			{
				if (!members.contains(member)) delta.left = true;
			}
			MatchLobbySoundMembers = members;
			return delta;
		}

		bool TryParseRankValue(const std::string& data,
			const std::string_view field, int& value)
		{
			const auto fieldPosition = data.find(field);
			if (fieldPosition == std::string::npos)
			{
				return false;
			}

			auto valuePosition = fieldPosition + field.size();
			while (valuePosition < data.size() &&
				(data[valuePosition] == ' ' || data[valuePosition] == '\t'))
			{
				++valuePosition;
			}
			if (valuePosition >= data.size() || data[valuePosition] != ':')
			{
				return false;
			}
			++valuePosition;
			while (valuePosition < data.size() &&
				(data[valuePosition] == ' ' || data[valuePosition] == '\t'))
			{
				++valuePosition;
			}

			if (valuePosition >= data.size())
			{
				return false;
			}

			char* end = nullptr;
			const auto parsed = std::strtol(
				data.c_str() + valuePosition, &end, 10);
			if (end == data.c_str() + valuePosition)
			{
				return false;
			}
			while (end < data.c_str() + data.size() &&
				(*end == ' ' || *end == '\t'))
			{
				++end;
			}
			if (end < data.c_str() + data.size() && *end != ';')
			{
				return false;
			}
			if (parsed < std::numeric_limits<int>::min() ||
				parsed > std::numeric_limits<int>::max())
			{
				return false;
			}

			value = static_cast<int>(parsed);
			return true;
		}

		std::optional<SharedLobbyRank> ReadLocalLobbyRank()
		{
			const auto rankPath = std::filesystem::path("zw3") /
				"core" / "scriptdata" /
				("rank_" + FormatPublicGuid());
			std::string data;
			if (!Utils::IO::ReadFile(rankPath.string(), &data))
			{
				return std::nullopt;
			}

			int storedLevel = 0;
			int storedPrestige = 0;
			if (!TryParseRankValue(data, "level", storedLevel) ||
				!TryParseRankValue(data, "prestige", storedPrestige) ||
				storedLevel < 0 || storedLevel > 53 ||
				storedPrestige < 0 || storedPrestige > 20)
			{
				return std::nullopt;
			}

			return SharedLobbyRank{storedLevel + 1, storedPrestige};
		}

		std::optional<LocalBarracksRank> ReadLocalBarracksRank()
		{
			const auto rankPath = std::filesystem::path("zw3") /
				"core" / "scriptdata" /
				("rank_" + FormatPublicGuid());
			std::string data;
			if (!Utils::IO::ReadFile(rankPath.string(), &data))
			{
				return std::nullopt;
			}

			int storedLevel = 0;
			int storedPrestige = 0;
			int storedExperience = 0;
			if (!TryParseRankValue(data, "level", storedLevel) ||
				!TryParseRankValue(data, "prestige", storedPrestige) ||
				!TryParseRankValue(data, "experience", storedExperience) ||
				storedLevel < 0 || storedLevel > 53 ||
				storedPrestige < 0 || storedPrestige > 255 ||
				storedExperience < 0)
			{
				return std::nullopt;
			}

			int experienceTarget = 50;
			switch (storedLevel)
			{
			case 0: experienceTarget = 50; break;
			case 1: experienceTarget = 125; break;
			case 2: experienceTarget = 200; break;
			case 3: experienceTarget = 300; break;
			case 4: experienceTarget = 450; break;
			case 5: experienceTarget = 650; break;
			default: experienceTarget = 650 + ((storedLevel - 5) * 250); break;
			}

			return LocalBarracksRank{storedLevel + 1, storedPrestige, storedExperience, experienceTarget};
		}

		std::string ZombiePrestigeIcon(const int prestige)
		{
			const auto iconLevel = prestige + 1;
			return iconLevel > 8 ? "skullicon" : std::format("prestige_{}", iconLevel);
		}

		bool TryParseChallengeEntry(const std::string& data,
			const std::string_view id, LocalChallengeProgress& entry)
		{
			const auto needle = std::string(id) + ":";
			auto position = data.find(needle);
			while (position != std::string::npos && position > 0 && data[position - 1] != ';')
			{
				position = data.find(needle, position + 1);
			}
			if (position == std::string::npos)
			{
				return false;
			}

			const auto* progressStart = data.c_str() + position + needle.size();
			char* progressEnd = nullptr;
			const auto progress = std::strtol(progressStart, &progressEnd, 10);
			if (progressEnd == progressStart || *progressEnd != ':')
			{
				return false;
			}

			const auto* tierStart = progressEnd + 1;
			char* tierEnd = nullptr;
			const auto tier = std::strtol(tierStart, &tierEnd, 10);
			if (tierEnd == tierStart || *tierEnd != ';' ||
				progress < 0 || progress > std::numeric_limits<int>::max() ||
				tier < 0 || tier > std::numeric_limits<int>::max())
			{
				return false;
			}

			entry.progress = static_cast<int>(progress);
			entry.tier = static_cast<int>(tier);
			return true;
		}

		std::vector<ChallengeDefinition> ReadChallengeDefinitions()
		{
			Utils::CSV table("zw3/core/mp/zw3_challenge_ui.csv", true, false);
			std::vector<ChallengeDefinition> definitions;
			if (!table.isValid())
			{
				return definitions;
			}

			definitions.reserve(table.getRows());
			for (std::size_t row = 0; row < table.getRows(); ++row)
			{
				ChallengeDefinition definition;
				definition.id = table.getElementAt(row, 0);
				if (definition.id.empty())
				{
					continue;
				}

				for (std::size_t tier = 0; tier < definition.targets.size(); ++tier)
				{
					const auto targetText = table.getElementAt(row, 4 + tier * 2);
					const auto rewardText = table.getElementAt(row, 5 + tier * 2);
					if (targetText.empty())
					{
						break;
					}

					char* targetEnd = nullptr;
					char* rewardEnd = nullptr;
					const auto target = std::strtol(targetText.c_str(), &targetEnd, 10);
					const auto reward = std::strtol(rewardText.c_str(), &rewardEnd, 10);
					if (targetEnd == targetText.c_str() || *targetEnd != '\0' || target <= 0 ||
						target > std::numeric_limits<int>::max() ||
						rewardEnd == rewardText.c_str() || *rewardEnd != '\0' || reward < 0 ||
						reward > std::numeric_limits<int>::max())
					{
						break;
					}

					definition.targets[tier] = static_cast<int>(target);
					definition.rewards[tier] = static_cast<int>(reward);
				}

				if (definition.targets[0] > 0)
				{
					definitions.emplace_back(std::move(definition));
				}
			}

			return definitions;
		}

		LocalBarracksChallenges ReadLocalBarracksChallenges(
			const std::vector<ChallengeDefinition>& definitions)
		{
			LocalBarracksChallenges challenges;
			const auto challengePath = std::filesystem::path("zw3") /
				"core" / "scriptdata" /
				("challenges_" + FormatPublicGuid());
			std::string data;
			if (!Utils::IO::ReadFile(challengePath.string(), &data))
			{
				return challenges;
			}

			for (const auto& definition : definitions)
			{
				LocalChallengeProgress entry;
				if (TryParseChallengeEntry(data, definition.id, entry))
				{
					challenges.entries.emplace(definition.id, entry);
				}
			}

			LocalChallengeProgress value;
			const auto hasKills = TryParseChallengeEntry(data, "stat_zw3_zombie_kills", value);
			if (hasKills) challenges.zombieKills = value.progress;
			const auto hasDeaths = TryParseChallengeEntry(data, "stat_zw3_zombie_deaths", value);
			if (hasDeaths) challenges.zombieDeaths = value.progress;
			const auto hasRevives = TryParseChallengeEntry(data, "stat_zw3_zombie_revives", value);
			if (hasRevives) challenges.zombieRevives = value.progress;

			// Older or briefly out-of-sync challenge files retain the largest known
			// value. Lifetime counters can exceed the final challenge milestone.
			if (const auto it = challenges.entries.find("ch_zw3_zombie_killer"); it != challenges.entries.end())
			{
				challenges.zombieKills = std::max(challenges.zombieKills, it->second.progress);
			}
			if (const auto it = challenges.entries.find("ch_zw3_reviver"); it != challenges.entries.end())
			{
				challenges.zombieRevives = std::max(challenges.zombieRevives, it->second.progress);
			}

			return challenges;
		}

		void RefreshChallengeCategory(const int startIndex)
		{
			const auto definitions = ReadChallengeDefinitions();
			const auto challenges = ReadLocalBarracksChallenges(definitions);
			for (std::size_t slot = 0; slot < ChallengeSlotProgressDvars.size(); ++slot)
			{
				int progress = 0;
				int target = 0;
				int tier = 0;
				int tierCount = 0;
				int reward = 0;
				int percent = 0;
				bool complete = false;

				const auto definitionIndex = startIndex + static_cast<int>(slot);
				if (definitionIndex >= 0 && definitionIndex < static_cast<int>(definitions.size()))
				{
					const auto& definition = definitions[definitionIndex];
					tierCount = static_cast<int>(std::ranges::count_if(definition.targets,
						[](const int value) { return value > 0; }));
					if (const auto it = challenges.entries.find(definition.id); it != challenges.entries.end())
					{
						progress = std::max(it->second.progress, 0);
						tier = std::clamp(it->second.tier, 0, tierCount);
					}

					complete = tierCount > 0 && tier >= tierCount;
					const auto targetTier = complete ? tierCount - 1 : tier;
					if (targetTier >= 0)
					{
						target = definition.targets[targetTier];
						reward = complete ? 0 : definition.rewards[targetTier];
					}
					if (complete)
					{
						percent = 100;
					}
					else if (target > 0)
					{
						percent = std::clamp(static_cast<int>(
							(static_cast<std::int64_t>(progress) * 100) / target), 0, 100);
					}
				}

				Dvar::Var(ChallengeSlotProgressDvars[slot]).set(progress);
				Dvar::Var(ChallengeSlotTargetDvars[slot]).set(target);
				Dvar::Var(ChallengeSlotTierDvars[slot]).set(tier);
				Dvar::Var(ChallengeSlotTierCountDvars[slot]).set(tierCount);
				Dvar::Var(ChallengeSlotRewardDvars[slot]).set(reward);
				Dvar::Var(ChallengeSlotPercentDvars[slot]).set(percent);
				Dvar::Var(ChallengeSlotCompleteDvars[slot]).set(complete);
			}
		}

		void RefreshBarracksProfile()
		{
			const auto rank = ReadLocalBarracksRank();
			Dvar::Var("zw3_barracks_rank_known").set(rank.has_value());
			if (!rank)
			{
				Dvar::Var("zw3_barracks_rank_level").set(1);
				Dvar::Var("zw3_barracks_rank_prestige").set(0);
				Dvar::Var("zw3_barracks_rank_experience").set(0);
				Dvar::Var("zw3_barracks_rank_experience_target").set(50);
				Dvar::Var("zw3_barracks_rank_experience_percent").set(0);
				Dvar::Var("zw3_barracks_rank_icon").set("prestige_1");
			}
			else
			{
				Dvar::Var("zw3_barracks_rank_level").set(rank->level);
				Dvar::Var("zw3_barracks_rank_prestige").set(rank->prestige);
				Dvar::Var("zw3_barracks_rank_experience").set(rank->experience);
				Dvar::Var("zw3_barracks_rank_experience_target").set(rank->experienceTarget);
				Dvar::Var("zw3_barracks_rank_experience_percent").set(std::clamp(static_cast<int>(
					(static_cast<std::int64_t>(rank->experience) * 100) / rank->experienceTarget), 0, 100));
				Dvar::Var("zw3_barracks_rank_icon").set(ZombiePrestigeIcon(rank->prestige));
			}

			const auto definitions = ReadChallengeDefinitions();
			const auto challenges = ReadLocalBarracksChallenges(definitions);
			Dvar::Var("zw3_barracks_zombie_kills").set(challenges.zombieKills);
			Dvar::Var("zw3_barracks_zombie_deaths").set(challenges.zombieDeaths);
			Dvar::Var("zw3_barracks_zombie_revives").set(challenges.zombieRevives);
		}

		SharedLobbyRankMap ParseSharedLobbyRanks(
			const nlohmann::json& party)
		{
			SharedLobbyRankMap ranks;
			const auto settingsIt = party.find("zombie_settings");
			if (settingsIt != party.end() && settingsIt->is_object())
			{
				const auto ranksIt = settingsIt->find("zwnet_player_ranks");
				if (ranksIt != settingsIt->end() && ranksIt->is_object())
				{
					for (const auto& [guid, value] : ranksIt->items())
					{
						if (guid.size() != 16 || !value.is_object()) continue;
						SharedLobbyRank rank;
						rank.level = std::clamp(value.value("level", 1), 1, 54);
						rank.prestige = std::max(value.value("prestige", 0), 0);
						ranks[guid] = rank;
					}
				}
			}

			const auto membersIt = party.find("members");
			if (membersIt != party.end() && membersIt->is_array())
			{
				for (const auto& member : *membersIt)
				{
					if (!member.is_object()) continue;
					const auto playerId = member.find("player_id");
					if (playerId == member.end() || !playerId->is_string()) continue;
					const auto guid = playerId->get<std::string>();
					if (guid.size() != 16 || !std::ranges::all_of(guid,
						[](const unsigned char character) { return std::isxdigit(character) != 0; })) continue;
					SharedLobbyRank rank;
					bool rankKnown = false;
					const auto rankIt = member.find("rank");
					if (rankIt != member.end() && rankIt->is_object() &&
						rankIt->contains("level") && rankIt->at("level").is_number_integer())
					{
						rank.level = std::clamp(rankIt->at("level").get<int>(), 1, 54);
						rank.prestige = rankIt->contains("prestige") &&
							rankIt->at("prestige").is_number_integer()
							? std::max(rankIt->at("prestige").get<int>(), 0)
							: 0;
						rankKnown = true;
					}
					else if (member.contains("level") && member.at("level").is_number_integer())
					{
						rank.level = std::clamp(member.at("level").get<int>(), 1, 54);
						rank.prestige = member.contains("prestige") &&
							member.at("prestige").is_number_integer()
							? std::max(member.at("prestige").get<int>(), 0)
							: 0;
						rankKnown = true;
					}
					if (!rankKnown) continue;
					ranks[guid] = rank;
				}
			}

			return ranks;
		}

		SharedLobbyRankMap CacheSharedLobbyRanks(
			const nlohmann::json& party)
		{
			auto ranks = ParseSharedLobbyRanks(party);
			const auto localGuid = FormatPublicGuid();
			if (const auto localRank = ReadLocalLobbyRank())
			{
				ranks[localGuid] = *localRank;
			}
			else
			{
				ranks.erase(localGuid);
			}
			const auto partyId = party.value("id", std::string{});
			{
				std::lock_guard lock(SharedLobbyRankMutex);
				SharedLobbyRankPartyId = partyId;
				SharedLobbyRanks = ranks;
			}
			return ranks;
		}

		SharedLobbyRankMap GetCachedSharedLobbyRanks()
		{
			std::lock_guard lock(SharedLobbyRankMutex);
			return SharedLobbyRanks;
		}

		void PatchMaterialEnumerationScratch()
		{
			static_assert(sizeof(Game::XAssetHeader) == sizeof(void*));
			constexpr std::array<DWORD, 12> assetArrayOperands
			{
				0x507AB9, 0x50D8BB, 0x50DBEE, 0x50DC41, 0x50DC95, 0x50DCED,
				0x518C22, 0x5239D5, 0x523A0C, 0x523A47, 0x553EE5, 0x5545E6
			};
			constexpr std::array<DWORD, 7> assetCountOperands
			{
				0x518C0C, 0x518C17, 0x5239CA, 0x523A01, 0x523A12, 0x523A2E, 0x523A3C
			};

			for (const auto operand : assetArrayOperands)
			{
				Utils::Hook::Set<Game::XAssetHeader*>(operand, MaterialEnumerationAssets.data());
			}
			for (const auto operand : assetCountOperands)
			{
				Utils::Hook::Set<std::uint32_t*>(operand, &MaterialEnumerationCount);
			}

			// The ZW3 material pool is already expanded to 16384 entries, while the
			// stock renderer scratch list only holds 4096. A successful online join
			// can enumerate more than 4096 map materials and overwrite the adjacent
			// counter before the first frame. Keep the renderer scratch capacity in
			// lockstep with the existing material pool.
			Logger::Print("ZWNET material enumeration scratch: {} entries\n", ZWNET_MATERIAL_ENUM_CAPACITY);
		}

		std::string EncodeLowerHex(const std::string& bytes)
		{
			static constexpr char digits[] = "0123456789abcdef";
			std::string result;
			result.reserve(bytes.size() * 2);
			for (const auto byte : bytes)
			{
				const auto value = static_cast<unsigned char>(byte);
				result.push_back(digits[value >> 4]);
				result.push_back(digits[value & 0x0F]);
			}
			return result;
		}

		const char* PublicGuidText()
		{
			static constexpr char digits[] = "0123456789abcdef";
			static const auto result = []
			{
				std::array<char, 17> buffer{};
				auto value = Auth::GetKeyHash();
				for (std::size_t index = 0; index < 16; ++index)
				{
					buffer[15 - index] = digits[value & 0x0F];
					value >>= 4;
				}
				return buffer;
			}();
			return result.data();
		}

		std::string FormatPublicGuid()
		{
			return PublicGuidText();
		}

		std::string FriendlyStateText(const std::string& state)
		{
			if (state == "SIGNING_IN") return "SIGNING IN";
			if (state == "LOGIN_REQUIRED") return "LOGIN REQUIRED";
			if (state == "SEARCH_STARTING") return "STARTING SEARCH";
			if (state == "IN_PARTY") return "IN PARTY";
			if (state == "MATCH_FOUND") return "MATCH FOUND";
			if (state == "MAP_VOTE") return "MAP VOTE";
			if (state == "READY_CHECK" || state == "WAITING_FOR_READY") return "WAITING FOR READY";
			if (state == "RESERVING_SERVER") return "RESERVING SERVER";
			if (state == "STARTING_SERVER" || state == "SERVER_STARTING") return "SERVER STARTING";
			if (state == "COUNTDOWN") return "JOIN COUNTDOWN";
			if (state == "JOIN_PREVIEW") return "JOINING GAME IN PROGRESS";
			if (state == "DIRECT_CONNECTION") return "DIRECT CONNECTION";
			if (state == "RELAY_CONNECTION") return "RELAY CONNECTION";
			if (state == "IN_MATCH") return "IN MATCH";
			return state;
		}

		std::string FriendlyErrorText(const std::string& error)
		{
			if (error.empty()) return {};
			if (error == "ZWNET_LOGIN_REQUIRED") return "Sign in on the ZW3 Stats page.";
			if (error == "ZWNET_SESSION_EXPIRED") return "Your ZW3 session expired. Please sign in again.";
			if (error == "ZWNET_SEARCH_FAILED") return "The selected playlist could not enter matchmaking. Please try again.";
			if (error == "ZWNET_CATALOG_UNAVAILABLE") return "Playlists are unavailable. Please refresh the list.";
			if (error == "ZWNET_PLAYLIST_REFRESH_REQUIRED") return "This playlist changed or access ended. Refresh the list and choose again.";
			if (error == "ZWNET_PLAYLIST_SELECTION_FAILED") return "The party playlist could not be changed. Refresh and try again.";
			if (error == "ZWNET_VERSION_MISMATCH") return "Your ZW3 client version does not match the online service.";
			if (error == "ZWNET_LEADER_REQUIRED") return "Only the party leader can start matchmaking.";
			if (error == "ZWNET_PARTY_TOO_LARGE") return "This party has too many players for the selected playlist.";
			if (error == "ZWNET_CONTENT_MISSING") return "Required ZW3 content is missing.";
			if (error == "ZWNET_ROUTE_UNAVAILABLE") return "No direct or relay route is available.";
			if (error == "ZWNET_RELAY_TIMEOUT") return "The relay did not confirm the connection. Return to the lobby and try again.";
			if (error == "ZWNET_SERVER_NOT_READY") return "The assigned ZW3 server is no longer available. Return to the lobby and search again.";
			if (error == "ZWNET_DESCRIPTOR_INVALID") return "The assigned server address is invalid.";
			if (error == "ZWNET_ACCOUNT_LINK_REQUIRED") return "Link this GUID in ZW3 Stats Settings.";
			if (error == "ZWNET_SESSION_STORAGE_FAILED") return "The secure ZW3 session could not be stored.";
			if (error == "ZWNET_REGISTRATION_UNAVAILABLE") return "The ZW3 account page is unavailable.";
			if (error == "ZWNET_PARTY_FAILED") return "The ZW3 party could not be created or loaded.";
			if (error == "ZWNET_PRIVATE_MATCH_FAILED") return "The private ZW3 server could not be reserved.";
			if (error == "ZWNET_MAP_VOTE_FAILED") return "Your map vote could not be submitted.";
			if (error == "ZWNET_MATCH_FAILED") return "Server start failed. Return to the lobby and try again.";
			if (error == "ZWNET_GUID_COPY_FAILED") return "The ZW3 GUID could not be copied.";
			if (error == "ZWNET_MANUAL_JOIN_DENIED") return "Manual test access is unavailable. Check your invitation and sign in again.";
			return "The ZW3 online service could not complete this request.";
		}

		std::string JsonString(const nlohmann::json& object, const char* key, const char* fallback = "")
		{
			if (!object.is_object()) return fallback;
			const auto it = object.find(key);
			return it != object.end() && it->is_string() ? it->get<std::string>() : std::string{fallback};
		}

		std::string SafeDisplayName(std::string value)
		{
			constexpr std::size_t maxLength = 48;
			value = TextRenderer::EncodeUtf8ForGame(value, maxLength);
			Utils::String::Trim(value);
			return value.empty() ? "ZW3 Player" : value;
		}

		std::string ResponseErrorCode(const nlohmann::json& response)
		{
			if (!response.contains("error") || !response.at("error").is_object()) return {};
			return JsonString(response.at("error"), "code");
		}

		bool IsActiveMatchmakingState(const std::string& state)
		{
			return state == "SEARCHING"
				|| state == "MATCH_FOUND"
				|| state == "MAP_VOTE"
				|| state == "READY_CHECK"
				|| state == "WAITING_FOR_READY"
				|| state == "RESERVING_SERVER"
				|| state == "STARTING_SERVER"
				|| state == "SERVER_STARTING"
				|| state == "CONNECTING"
				|| state == "IN_MATCH";
		}

		std::uint64_t NextPresenceSequence()
		{
			static std::atomic_uint64_t sequence{1};
			return sequence.fetch_add(1, std::memory_order_relaxed);
		}

		bool CopyPublicGuidToClipboard()
		{
			if (!OpenClipboard(GetDesktopWindow())) return false;
			const auto closeClipboard = gsl::finally([] { CloseClipboard(); });
			if (!EmptyClipboard()) return false;

			const auto guid = FormatPublicGuid();
			auto memory = GlobalAlloc(GMEM_MOVEABLE, guid.size() + 1);
			if (!memory) return false;
			bool clipboardOwnsMemory = false;
			const auto releaseMemory = gsl::finally([&]
			{
				if (!clipboardOwnsMemory) GlobalFree(memory);
			});

			auto* destination = static_cast<char*>(GlobalLock(memory));
			if (!destination) return false;
			std::memcpy(destination, guid.c_str(), guid.size() + 1);
			GlobalUnlock(memory);
			if (!SetClipboardData(CF_TEXT, memory)) return false;
			clipboardOwnsMemory = true;
			return true;
		}

	}

	std::atomic_bool& ZWNet::ActiveState() { static std::atomic_bool value = false; return value; }
	std::atomic_bool& ZWNet::SearchingState() { static std::atomic_bool value = false; return value; }
	std::atomic_bool& ZWNet::ClosingOnlineSessionState() { static std::atomic_bool value = false; return value; }
	std::atomic_bool& ZWNet::ServerJoinTransitionState() { static std::atomic_bool value = false; return value; }
	std::atomic_bool& ZWNet::OnlineEntryPendingState() { static std::atomic_bool value = false; return value; }
	std::atomic_bool& ZWNet::InGameState() { static std::atomic_bool value = false; return value; }
	bool& ZWNet::LoginInFlightState() { static bool value = false; return value; }
	std::mutex& ZWNet::StateMutex() { static std::mutex value; return value; }
	std::string& ZWNet::AccessTokenState() { static std::string value; return value; }
	std::string& ZWNet::RefreshTokenState() { static std::string value; return value; }
	std::string& ZWNet::CurrentPartyIdState() { static std::string value; return value; }
	std::string& ZWNet::CurrentPlayerIdState() { static std::string value; return value; }
	std::string& ZWNet::CurrentProposalIdState() { static std::string value; return value; }
	std::string& ZWNet::CurrentMatchIdState() { static std::string value; return value; }
	std::mutex& ZWNet::AsyncTaskMutex() { static std::mutex value; return value; }
	std::deque<std::function<void()>>& ZWNet::AsyncTasks() { static std::deque<std::function<void()>> value; return value; }

	bool ZWNet::TryGetSharedLobbyRank(const std::string& guid, int& level, int& prestige)
	{
		auto normalizedGuid = guid;
		std::ranges::transform(normalizedGuid, normalizedGuid.begin(), [](const unsigned char character)
		{
			return static_cast<char>(std::tolower(character));
		});

		std::lock_guard lock(SharedLobbyRankMutex);
		const auto rank = SharedLobbyRanks.find(normalizedGuid);
		if (rank == SharedLobbyRanks.end()) return false;
		level = rank->second.level;
		prestige = rank->second.prestige;
		return true;
	}

	void ZWNet::EnqueueAsync(std::function<void()> task)
	{
		if (!ActiveState()) return;
		std::lock_guard lock(AsyncTaskMutex());
		if (!ActiveState()) return;
		AsyncTasks().emplace_back(std::move(task));
	}

	void ZWNet::ProcessAsyncTasks()
	{
		if (!ActiveState()) return;
		std::function<void()> task;
		{
			std::lock_guard lock(AsyncTaskMutex());
			if (!ActiveState() || AsyncTasks().empty()) return;
			task = std::move(AsyncTasks().front());
			AsyncTasks().pop_front();
		}
		if (!ActiveState()) return;
		try
		{
			task();
		}
		catch (const std::exception&)
		{
			SetState("ERROR", "ZWNET_REQUEST_FAILED");
		}
		catch (...)
		{
			SetState("ERROR", "ZWNET_REQUEST_FAILED");
		}
	}

	std::string ZWNet::SessionPath()
	{
		return (FileSystem::GetAppdataPath() / "zwnet.session").string();
	}

	bool ZWNet::StoreSession(const std::string& accessToken, const std::string& refreshToken)
	{
		if (accessToken.empty() || refreshToken.empty()) return false;
		const auto plain = nlohmann::json{{"access_token", accessToken}, {"refresh_token", refreshToken}}.dump();
		DATA_BLOB input{static_cast<DWORD>(plain.size()), reinterpret_cast<BYTE*>(const_cast<char*>(plain.data()))};
		DATA_BLOB output{};
		if (!CryptProtectData(&input, L"ZW3 ZWNET session", nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &output)) return false;
		const std::string encrypted{reinterpret_cast<char*>(output.pbData), output.cbData};
		LocalFree(output.pbData);
		if (!Utils::IO::WriteFile(SessionPath(), encrypted)) return false;
		std::lock_guard lock(StateMutex());
		AccessTokenState() = accessToken;
		RefreshTokenState() = refreshToken;
		return true;
	}

	bool ZWNet::LoadSession()
	{
		const auto encrypted = Utils::IO::ReadFile(SessionPath());
		if (encrypted.empty()) return false;
		DATA_BLOB input{static_cast<DWORD>(encrypted.size()), reinterpret_cast<BYTE*>(const_cast<char*>(encrypted.data()))};
		DATA_BLOB output{};
		if (!CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &output)) return false;
		const std::string plain{reinterpret_cast<char*>(output.pbData), output.cbData};
		SecureZeroMemory(output.pbData, output.cbData);
		LocalFree(output.pbData);
		try
		{
			const auto data = nlohmann::json::parse(plain);
			std::lock_guard lock(StateMutex());
			AccessTokenState() = data.at("access_token").get<std::string>();
			RefreshTokenState() = data.at("refresh_token").get<std::string>();
			return !AccessTokenState().empty() && !RefreshTokenState().empty();
		}
		catch (const nlohmann::json::exception&)
		{
			return false;
		}
	}

	void ZWNet::ClearSession()
	{
		ManagedReconnectInFlight() = false;
		PlaylistSelectionStarting() = false;
		CancelRelayHandshake();
		ClearManagedRouteAttempt();
		Auth::ClearManagedConnectTicket();
		{
			std::lock_guard lock(StateMutex());
			std::ranges::fill(AccessTokenState(), '\0');
			std::ranges::fill(RefreshTokenState(), '\0');
			AccessTokenState().clear(); RefreshTokenState().clear();
			CurrentPlayerIdState().clear();
			Utils::IO::RemoveFile(SessionPath());
		}
		ClearPlaylistCatalog();
	}

	std::optional<nlohmann::json> ZWNet::Request(const std::string& method, const std::string& path, const nlohmann::json& body)
	{
		if (!ActiveState()) return std::nullopt;
		try
		{
			std::string token;
			{
				std::lock_guard lock(StateMutex());
				token = AccessTokenState();
			}
			Utils::WebIO::params headers{{"Accept", "application/json"}, {"Content-Type", "application/json"}};
			if (!token.empty()) headers["Authorization"] = "Bearer " + token;
			bool success = false;
			// Dvars belong to the main game thread, while all HTTP requests run on the
			// asynchronous scheduler. Keep the trusted production endpoint immutable.
			Utils::WebIO request("ZW3-ZWNET/3.0.3", std::string(ZWNET_API_BASE) + path);
			request.setTimeout(5000)->setReadHttpErrorBody(true);
			const auto response = method == "GET" ? request.get(headers, &success) : request.post(body.dump(), headers, &success);
			if (!ActiveState() || response.empty()) return std::nullopt;
			auto parsed = nlohmann::json::parse(response);
			if (!success && !parsed.contains("error")) return std::nullopt;
			return parsed;
		}
		catch (const std::exception&) { return std::nullopt; }
		catch (...) { return std::nullopt; }
	}

	void ZWNet::SetState(const std::string& state, const std::string& error)
	{
		const auto stateText = FriendlyStateText(state);
		const auto errorText = FriendlyErrorText(error);
		Scheduler::Once([state, stateText, error, errorText]
		{
			if (!ActiveState()) return;
			if (state == "ERROR")
			{
				Dvar::Var("zwnet_start_phase").set("");
				Dvar::Var("zwnet_start_seconds").set(0);
			}
			Dvar::Var("ui_zwnet_state").set(state);
			Dvar::Var("ui_zwnet_state_text").set(stateText);
			Dvar::Var("ui_zwnet_error").set(error);
			Dvar::Var("ui_zwnet_error_text").set(errorText);
		}, Scheduler::Pipeline::MAIN);
	}

	void ZWNet::Refresh()
	{
		if (!ActiveState()) return;
		std::string refresh;
		{ std::lock_guard lock(StateMutex()); refresh = RefreshTokenState(); }
		if (refresh.empty()) { Login(); return; }
		const auto result = Request("POST", "/social/client/refresh", {{"refresh_token", refresh}});
		if (!result || result->contains("error"))
		{
			if (!ActiveState()) return;
			ClearSession();
			Login();
			return;
		}
		if (!ActiveState()) return;
		const auto access = result->at("access_token").get<std::string>();
		StoreSession(access, result->value("refresh_token", access));
		if (const auto me = Request("GET", "/social/me"); me && me->contains("id"))
		{
			std::lock_guard lock(StateMutex());
			CurrentPlayerIdState() = me->at("id").get<std::string>();
		}
		SetState("ONLINE");
		RefreshPlaylistCatalog(true);
		CompleteOnlineEntry();
		Request("POST", "/social/presence", {{"status", "MAIN_MENU"}, {"sequence", NextPresenceSequence()}, {"joinable", false}});
	}

	void ZWNet::Login()
	{
		if (!ActiveState()) return;
		{
			std::lock_guard lock(StateMutex());
			if (LoginInFlightState()) return;
			LoginInFlightState() = true;
		}
		SetState("SIGNING_IN");
		const auto guid = FormatPublicGuid();
		const auto entropy = Auth::GetMachineEntropy();
		const auto deviceId = EncodeLowerHex(Utils::Cryptography::SHA256::Compute(entropy));
		if (deviceId.size() != 64)
		{
			std::lock_guard lock(StateMutex());
			LoginInFlightState() = false;
			SetState("ERROR", "ZWNET_REQUEST_FAILED");
			return;
		}
		EnqueueAsync([requestBody = nlohmann::json{{"guid", guid}, {"device_id", deviceId}, {"client_version", ZWNET_CLIENT_VERSION}, {"mod_version", ZWNET_MOD_VERSION}}]() mutable
		{
			CompleteLogin(std::move(requestBody));
		});
	}

	void ZWNet::CompleteLogin(nlohmann::json requestBody)
	{
		if (!ActiveState()) return;
		const auto loginGuard = gsl::finally([]
		{
			std::lock_guard lock(StateMutex());
			LoginInFlightState() = false;
		});
		const auto result = Request("POST", "/social/client/login", requestBody);
		if (!ActiveState()) return;
		if (!result)
		{
			SetState("ERROR", "ZWNET_REQUEST_FAILED");
			return;
		}
		if (result->contains("error"))
		{
			SetState("LOGIN_REQUIRED", "ZWNET_ACCOUNT_LINK_REQUIRED");
			return;
		}
		const auto access = result->value("access_token", result->value("token", ""));
		const auto refresh = result->value("refresh_token", access);
		if (!StoreSession(access, refresh))
		{
			SetState("ERROR", "ZWNET_SESSION_STORAGE_FAILED");
			return;
		}
		if (result->contains("profile") && result->at("profile").contains("id"))
		{
			std::lock_guard lock(StateMutex());
			CurrentPlayerIdState() = result->at("profile").at("id").get<std::string>();
		}
		SetState("ONLINE");
		RefreshPlaylistCatalog(true);
		CompleteOnlineEntry();
		Request("POST", "/social/presence", {{"status", "MAIN_MENU"}, {"sequence", NextPresenceSequence()}, {"joinable", false}});
	}

	void ZWNet::BeginOnlineEntry()
	{
		if (!ActiveState()) return;
		OnlineEntryPendingState() = true;
		SetState("SIGNING_IN");

		bool hasRefreshToken = false;
		{
			std::lock_guard lock(StateMutex());
			hasRefreshToken = !RefreshTokenState().empty();
		}
		if (hasRefreshToken) EnqueueAsync([] { Refresh(); });
		else Login();
	}

	void ZWNet::CompleteOnlineEntry()
	{
		EnqueueAsync([]
		{
			if (!ActiveState() || !OnlineEntryPendingState()) return;

			auto party = Request("GET", "/zwnet/parties/current");
			if (!party || party->is_null())
			{
				party = Request("POST", "/zwnet/parties/create",
					{{"visibility", PartyVisibilityName(DesiredPartyPrivacy().load())}});
			}

			if (!party || party->is_null() || party->contains("error"))
			{
				SetState("ERROR", "ZWNET_PARTY_FAILED");
			}
			else
			{
				*party = PublishLocalRank(std::move(*party));
				UpdateLobbyDvars(*party);
				const auto partyState = party->value("state", "IDLE");
				const auto activeMatchmaking = IsActiveMatchmakingState(partyState);
				SearchingState() = activeMatchmaking;
				SetState(activeMatchmaking ? partyState : "IN_PARTY");
				if (activeMatchmaking) UpdateMatchmaking();
			}

			Scheduler::Once([]
			{
				if (!ActiveState() || !OnlineEntryPendingState().exchange(false)) return;
				if (auto* connecting = Game::Menus_FindByName(Game::uiContext, "popup_zwnet_connecting"))
				{
					Game::Menus_CloseRequest(Game::uiContext, connecting);
				}
				Game::Menus_OpenByName(Game::uiContext, "zwnet_matchmaking");
			}, Scheduler::Pipeline::MAIN);
		});
	}

	void ZWNet::AbandonOnlineSession()
	{
		CancelJoinInProgressPreview();
		ResetMatchLobbySoundSnapshot();
		SearchingState() = false;
		EndpointJoinInFlight() = false;
		ServerJoinTransitionState() = false;
		OnlineEntryPendingState() = false;
		LocalPartyLeader() = false;
		CachedPartyMemberCount() = 0;
		CachedPartyVisibility() = 2;
		CachedPartyJoinStateSupported() = false;
		{
			std::lock_guard lock(StateMutex());
			CurrentPartyIdState().clear();
			CurrentProposalIdState().clear();
			CurrentMatchIdState().clear();
		}
		Dvar::Var("zwnet_lobby_active").set(false);
		Dvar::Var("zwnet_lobby_party_id").set("");
		Dvar::Var("zwnet_lobby_visibility").set("OPEN");
		Dvar::Var("zwnet_lobby_owner").set("");
		Dvar::Var("zwnet_lobby_member_count").set(0);
		Dvar::Var("zwnet_lobby_status_text").set("IDLE");
		Dvar::Var("zwnet_lobby_can_start").set(false);
		Dvar::Var("zwnet_lobby_self_ready").set(false);
		for (std::size_t i = 0; i < 4; ++i)
		{
			const auto prefix = std::format("zwnet_lobby_member_{}", i);
			Dvar::Var(prefix + "_name").set("");
			Dvar::Var(prefix + "_guid").set("");
			Dvar::Var(prefix + "_role").set("");
			Dvar::Var(prefix + "_ready").set(false);
			Dvar::Var(prefix + "_self").set(false);
			Dvar::Var(prefix + "_shared_rank_known").set(false);
			Dvar::Var(prefix + "_shared_rank_level").set(1);
			Dvar::Var(prefix + "_shared_rank_prestige").set(0);
			Dvar::Var(prefix + "_rank_icon").set("");
			Dvar::Var(prefix + "_rank_level").set("");
		}
		Dvar::Var("zwnet_vote_active").set(false);
		Dvar::Var("zwnet_vote_selection").set("");
		Dvar::Var("zwnet_all_ready").set(false);
		Dvar::Var("zwnet_start_phase").set("");
		Dvar::Var("zwnet_start_seconds").set(0);
		Dvar::Var("zwnet_vote_reveal_time").set(0);
		Dvar::Var("zwnet_vote_winner_id").set("");
		Dvar::Var("zwnet_vote_winner_name").set("");
		Dvar::Var("zwnet_vote_winner_image").set("");
		Dvar::Var("zwnet_match_id").set("");
		Dvar::Var("zwnet_server_endpoint").set("");
		Dvar::Var("zwnet_server_hostname").set("");
		Dvar::Var("zwnet_server_status").set("NOT ASSIGNED");
		Dvar::Var("zwnet_join_status").set("WAITING IN LOBBY");
		Dvar::Var("zwnet_join_countdown").set(0);
		Dvar::Var("zwnet_managed_session").set(false);
	}

	void ZWNet::Register()
	{
		const auto result = Request("GET", "/social/client/register");
		if (!result || !result->contains("guid_link_url"))
		{
			SetState("ERROR", "ZWNET_REGISTRATION_UNAVAILABLE");
			return;
		}
		const auto url = JsonString(*result, "guid_link_url");
		if (url != "https://stats.zw3.eu/settings")
		{
			SetState("ERROR", "ZWNET_REGISTRATION_UNAVAILABLE");
			return;
		}
		Scheduler::Once([url]
		{
			if (!ActiveState()) return;
			Command::Execute("openLink " + url, false);
		}, Scheduler::Pipeline::MAIN);
	}

	void ZWNet::ClearPlaylistCatalog()
	{
		PlaylistSelectorOpen() = false;
		++PlaylistSelectorGeneration();
		PlaylistSelectionStarting() = false;
		auto& catalog = PlaylistCatalogState();
		{
			std::lock_guard lock(catalog.mutex);
			++catalog.generation;
			catalog.entries.clear();
			catalog.pendingEntries.clear();
			catalog.accountId.clear();
			catalog.selectedId.clear();
			catalog.partySelectedId.clear();
			catalog.revision = 0;
			catalog.pendingRevision = 0;
			catalog.partySelectedRevision = 0;
			catalog.page = 0;
			catalog.loaded = false;
			catalog.stale = false;
			catalog.inFlight = false;
			catalog.hasPending = false;
			catalog.notice = false;
		}
		Scheduler::Once([] { PublishPlaylistCatalog(); }, Scheduler::Pipeline::MAIN);
	}

	void ZWNet::PublishPlaylistCatalog()
	{
		if (!ActiveState()) return;
		std::string matchId;
		{
			std::lock_guard lock(StateMutex());
			matchId = CurrentMatchIdState();
		}
		const auto pinned = PlaylistSearchStarting() || PlaylistSelectionStarting() || SearchingState() ||
			InGameState() || !matchId.empty();
		auto& catalog = PlaylistCatalogState();
		std::lock_guard lock(catalog.mutex);
		if (!pinned && catalog.hasPending)
		{
			catalog.entries = std::move(catalog.pendingEntries);
			catalog.revision = catalog.pendingRevision;
			catalog.hasPending = false;
			catalog.pendingRevision = 0;
		}
		const auto selected = std::ranges::find_if(catalog.entries,
			[&](const ClientPlaylist& entry) { return entry.id == catalog.selectedId; });
		if (selected == catalog.entries.end())
		{
			catalog.selectedId.clear();
			if (!catalog.partySelectedId.empty())
			{
				const auto partySelected = std::ranges::find_if(catalog.entries,
					[&](const ClientPlaylist& entry) { return entry.id == catalog.partySelectedId; });
				if (partySelected != catalog.entries.end()) catalog.selectedId = partySelected->id;
			}
			if (catalog.selectedId.empty() && !pinned)
			{
				const auto first = std::ranges::find_if(catalog.entries,
					[](const ClientPlaylist& entry) { return entry.availability == "AVAILABLE"; });
				if (first != catalog.entries.end()) catalog.selectedId = first->id;
			}
		}
		const auto active = std::ranges::find_if(catalog.entries,
			[&](const ClientPlaylist& entry) { return entry.id == catalog.selectedId; });
		const auto selectionCurrent = active != catalog.entries.end() &&
			(catalog.partySelectedId.empty() ||
				(active->id == catalog.partySelectedId && active->revision == catalog.partySelectedRevision));
		const auto pageCount = std::max<std::size_t>(1,
			(catalog.entries.size() + ZWNET_PLAYLIST_PAGE_SIZE - 1) / ZWNET_PLAYLIST_PAGE_SIZE);
		if (catalog.page >= pageCount) catalog.page = pageCount - 1;
		Dvar::Var("zwnet_catalog_status").set(!catalog.loaded ?
			(catalog.stale ? "PLAYLIST SERVICE UNAVAILABLE" : "LOADING") :
			(catalog.stale ? "STALE - SERVICE UNAVAILABLE" : "READY"));
		Dvar::Var("zwnet_catalog_notice").set(catalog.notice);
		Dvar::Var("zwnet_catalog_page").set(static_cast<int>(catalog.page + 1));
		Dvar::Var("zwnet_catalog_pages").set(static_cast<int>(pageCount));
		Dvar::Var("zwnet_catalog_selected_name").set(active == catalog.entries.end() ?
			"NO AVAILABLE PLAYLIST" : active->name);
		Dvar::Var("zwnet_catalog_selected_description").set(active == catalog.entries.end() ?
			"Select an available playlist to find a match." : active->description);
		Dvar::Var("zwnet_catalog_selected_audience").set(active == catalog.entries.end() ?
			"" : active->audience);
		Dvar::Var("zwnet_catalog_selected_availability").set(active == catalog.entries.end() ?
			"NO_SELECTION" : active->availability);
		Dvar::Var("zwnet_catalog_selected_status").set(active == catalog.entries.end() ?
			(catalog.partySelectedId.empty() ? "Choose a playlist to continue." :
				"The party playlist is no longer in your authorized catalog.") :
			(!selectionCurrent ? "A new playlist revision is available. The party leader must confirm it." :
				active->availabilityDetail));
		Dvar::Var("zwnet_catalog_selected_revision").set(active == catalog.entries.end() ?
			0 : static_cast<int>(active->revision));
		Dvar::Var("zwnet_catalog_selected_rotation").set(active == catalog.entries.end() ?
			"No map rotation is available." : active->rotationSummary);
		Dvar::Var("zwnet_catalog_selected_image").set(active == catalog.entries.end() || active->mapId.empty() ?
			"" : ResolveVoteMapImage(active->mapId, active->mapImage));
		Dvar::Var("zwnet_catalog_selected_players").set(active == catalog.entries.end() ? "" :
			std::format("{}-{} PLAYERS", active->minPlayers, active->maxPlayers));
		Dvar::Var("zwnet_catalog_selected_zombie_settings").set(active == catalog.entries.end() ? "" :
			active->zombieSettingsSummary);
		Dvar::Var("zwnet_catalog_busy").set(PlaylistSelectionStarting());
		Dvar::Var("zwnet_catalog_can_select").set(LocalPartyLeader() && !pinned && !catalog.stale &&
			!PlaylistSelectionStarting());
		Dvar::Var("zwnet_catalog_can_search").set(active != catalog.entries.end() &&
			active->availability == "AVAILABLE" && !catalog.stale && !pinned &&
			LocalPartyLeader() && selectionCurrent);
		for (std::size_t slot = 0; slot < ZWNET_PLAYLIST_PAGE_SIZE; ++slot)
		{
			const auto prefix = std::format("zwnet_catalog_slot_{}", slot);
			const auto index = catalog.page * ZWNET_PLAYLIST_PAGE_SIZE + slot;
			const auto exists = index < catalog.entries.size();
			Dvar::Var(prefix + "_visible").set(exists);
			Dvar::Var(prefix + "_name").set(exists ? catalog.entries[index].name : "");
			Dvar::Var(prefix + "_description").set(exists ? catalog.entries[index].description : "");
			Dvar::Var(prefix + "_preview").set(exists ? catalog.entries[index].preview : "");
			Dvar::Var(prefix + "_image").set(exists && !catalog.entries[index].mapId.empty() ?
				ResolveVoteMapImage(catalog.entries[index].mapId, catalog.entries[index].mapImage) : "");
			Dvar::Var(prefix + "_audience").set(exists ? catalog.entries[index].audience : "");
			Dvar::Var(prefix + "_availability").set(exists ? catalog.entries[index].availability : "");
			Dvar::Var(prefix + "_status").set(exists ? catalog.entries[index].availabilityDetail : "");
			Dvar::Var(prefix + "_selected").set(exists &&
				catalog.entries[index].id == catalog.selectedId);
		}
	}

	void ZWNet::RefreshPlaylistCatalog(const bool force)
	{
		if (!ActiveState()) return;
		std::string accountId;
		bool hasToken = false;
		{
			std::lock_guard lock(StateMutex());
			accountId = CurrentPlayerIdState();
			hasToken = !AccessTokenState().empty();
		}
		if (accountId.empty() || !hasToken) return;
		auto& catalog = PlaylistCatalogState();
		std::uint64_t generation;
		bool accountChanged = false;
		{
			std::lock_guard lock(catalog.mutex);
			const auto now = std::chrono::steady_clock::now();
			if (catalog.accountId != accountId)
			{
				accountChanged = true;
				++catalog.generation;
				catalog.entries.clear();
				catalog.pendingEntries.clear();
				catalog.selectedId.clear();
				catalog.partySelectedId.clear();
				catalog.page = 0;
				catalog.revision = 0;
				catalog.pendingRevision = 0;
				catalog.partySelectedRevision = 0;
				catalog.loaded = false;
				catalog.stale = false;
				catalog.hasPending = false;
				catalog.notice = false;
				catalog.accountId = accountId;
				catalog.inFlight = false;
				catalog.lastAttempt = {};
			}
			// Changing accounts must invalidate a restricted catalog immediately,
			// even when the previous account still has an HTTP request in flight.
			if (catalog.inFlight ||
				(!force && now - catalog.lastAttempt < 30s)) return;
			catalog.inFlight = true;
			catalog.lastAttempt = now;
			generation = catalog.generation;
		}
		if (accountChanged)
		{
			PlaylistSelectorOpen() = false;
			++PlaylistSelectorGeneration();
			Scheduler::Once([] { PublishPlaylistCatalog(); }, Scheduler::Pipeline::MAIN);
		}
		const auto response = Request("GET", "/zwnet/playlists/catalog");
		if (response && response->is_object() && response->contains("error"))
		{
			const auto code = ResponseErrorCode(*response);
			if (code == "AUTH_REQUIRED" || code == "SESSION_INVALID")
			{
				{
					std::lock_guard lock(catalog.mutex);
					if (catalog.generation != generation || catalog.accountId != accountId) return;
					++catalog.generation;
					catalog.entries.clear();
					catalog.pendingEntries.clear();
					catalog.selectedId.clear();
					catalog.partySelectedId.clear();
					catalog.accountId.clear();
					catalog.revision = 0;
					catalog.pendingRevision = 0;
					catalog.partySelectedRevision = 0;
					catalog.page = 0;
					catalog.loaded = false;
					catalog.stale = true;
					catalog.inFlight = false;
					catalog.hasPending = false;
					catalog.notice = false;
				}
				Scheduler::Once([] { PublishPlaylistCatalog(); }, Scheduler::Pipeline::MAIN);
				return;
			}
		}
		std::int64_t revision = 0;
		std::vector<ClientPlaylist> entries;
		bool valid = false;
		try
		{
			valid = response && ParseClientPlaylistCatalog(*response, revision, entries);
			if (valid) VerifyPlaylistContent(entries);
		}
		catch (const nlohmann::json::exception&)
		{
			valid = false;
		}
		std::string currentAccountId;
		{
			std::lock_guard lock(StateMutex());
			currentAccountId = CurrentPlayerIdState();
		}
		{
			std::lock_guard lock(catalog.mutex);
			if (catalog.generation != generation || catalog.accountId != accountId) return;
		catalog.inFlight = false;
		if (currentAccountId != accountId)
		{
			++catalog.generation;
			catalog.entries.clear();
			catalog.pendingEntries.clear();
			catalog.selectedId.clear();
			catalog.partySelectedId.clear();
			catalog.accountId.clear();
			catalog.revision = 0;
			catalog.pendingRevision = 0;
			catalog.partySelectedRevision = 0;
			catalog.page = 0;
			catalog.loaded = false;
			catalog.stale = true;
			catalog.hasPending = false;
			catalog.notice = false;
		}
		else if (!valid)
			{
				catalog.stale = true;
			}
			else
			{
				// A refreshed authorization result takes effect immediately, even
				// while a match pins other catalog changes for continuity.
				std::unordered_set<std::string> visibleIds;
				for (const auto& entry : entries) visibleIds.insert(entry.id);
				const auto revoked = [&visibleIds](const ClientPlaylist& entry)
				{
					return entry.audience == "RESTRICTED" && !visibleIds.contains(entry.id);
				};
				std::erase_if(catalog.entries, revoked);
				std::erase_if(catalog.pendingEntries, revoked);
				const auto currentRevision = catalog.hasPending ?
					catalog.pendingRevision : catalog.revision;
				const auto changed = catalog.loaded && revision != currentRevision;
				catalog.loaded = true;
				catalog.stale = false;
				if (changed)
				{
					catalog.pendingEntries = std::move(entries);
					catalog.pendingRevision = revision;
					catalog.hasPending = true;
					catalog.notice = true;
				}
				else if (!catalog.hasPending)
				{
					catalog.entries = std::move(entries);
					catalog.revision = revision;
				}
				else catalog.pendingEntries = std::move(entries);
			}
		}
		Scheduler::Once([] { PublishPlaylistCatalog(); }, Scheduler::Pipeline::MAIN);
		if (valid && currentAccountId == accountId) PublishPartyContent();
	}

	void ZWNet::BeginPlaylistSelection()
	{
		if (!ActiveState() || SearchingState() || InGameState() || PlaylistSearchStarting()) return;
		PlaylistSelectorOpen() = true;
		++PlaylistSelectorGeneration();
		PlaylistSelectorOpenedAt() = PlaylistClockMilliseconds();
		Dvar::Var("zwnet_catalog_action_error").set("");
		SetState("IN_PARTY");
		PublishPlaylistCatalog();
	}

	void ZWNet::CancelPlaylistSelection()
	{
		PlaylistSelectorOpen() = false;
		++PlaylistSelectorGeneration();
		Dvar::Var("zwnet_catalog_action_error").set("");
		PublishPlaylistCatalog();
	}

	void ZWNet::HighlightPlaylistSlot(const int slot)
	{
		if (!PlaylistSelectorOpen() || PlaylistSelectionStarting() ||
			slot < 0 || slot >= static_cast<int>(ZWNET_PLAYLIST_PAGE_SIZE)) return;
		auto& catalog = PlaylistCatalogState();
		{
			std::lock_guard lock(catalog.mutex);
			const auto index = catalog.page * ZWNET_PLAYLIST_PAGE_SIZE + slot;
			if (!catalog.loaded || index >= catalog.entries.size()) return;
			catalog.selectedId = catalog.entries[index].id;
		}
		Dvar::Var("zwnet_catalog_action_error").set("");
		PublishPlaylistCatalog();
	}

	void ZWNet::ActivatePlaylistSlot(const int slot)
	{
		if (!PlaylistSelectorOpen() || PlaylistSearchStarting() || SearchingState() || InGameState() ||
			slot < 0 || slot >= static_cast<int>(ZWNET_PLAYLIST_PAGE_SIZE)) return;
		// Opening FIND MATCH and confirming a row are separate gestures. This also
		// prevents the opening click or key press from falling through to row zero.
		if (PlaylistClockMilliseconds() - PlaylistSelectorOpenedAt() < 200) return;
		if (!LocalPartyLeader())
		{
			Dvar::Var("zwnet_catalog_action_error").set("Only the party leader can start matchmaking.");
			return;
		}
		std::string playlistId;
		std::int64_t playlistRevision = 0;
		auto& catalog = PlaylistCatalogState();
		{
			std::lock_guard lock(catalog.mutex);
			const auto index = catalog.page * ZWNET_PLAYLIST_PAGE_SIZE + slot;
			if (!catalog.loaded || catalog.stale || index >= catalog.entries.size())
			{
				Dvar::Var("zwnet_catalog_action_error").set("The playlist list is not ready. Refresh and try again.");
				return;
			}
			catalog.selectedId = catalog.entries[index].id;
			if (catalog.entries[index].availability != "AVAILABLE")
			{
				Dvar::Var("zwnet_catalog_action_error").set(catalog.entries[index].availabilityDetail);
				return;
			}
			playlistId = catalog.entries[index].id;
			playlistRevision = catalog.entries[index].revision;
		}
		if (PlaylistSelectionStarting().exchange(true)) return;
		const auto generation = PlaylistSelectorGeneration().load();
		Dvar::Var("zwnet_catalog_action_error").set("");
		PublishPlaylistCatalog();
		EnqueueAsync([playlistId, playlistRevision, generation]
		{
			StartQuickPlay(playlistId, playlistRevision, generation);
		});
	}

	void ZWNet::ChangePlaylistPage(const int direction)
	{
		if (direction != -1 && direction != 1) return;
		auto& catalog = PlaylistCatalogState();
		{
			std::lock_guard lock(catalog.mutex);
			const auto pages = std::max<std::size_t>(1,
				(catalog.entries.size() + ZWNET_PLAYLIST_PAGE_SIZE - 1) / ZWNET_PLAYLIST_PAGE_SIZE);
			if (direction < 0 && catalog.page > 0) --catalog.page;
			else if (direction > 0 && catalog.page + 1 < pages) ++catalog.page;
		}
		PublishPlaylistCatalog();
	}

	void ZWNet::AcknowledgePlaylistNotice()
	{
		auto& catalog = PlaylistCatalogState();
		{
			std::lock_guard lock(catalog.mutex);
			catalog.notice = false;
		}
		PublishPlaylistCatalog();
	}

	std::optional<nlohmann::json> ZWNet::PublishPartyContent()
	{
		if (!ActiveState()) return std::nullopt;
		std::string partyId;
		{ std::lock_guard lock(StateMutex()); partyId = CurrentPartyIdState(); }
		if (partyId.empty()) return std::nullopt;
		std::unordered_set<std::string> unique;
		{
			auto& catalog = PlaylistCatalogState();
			std::lock_guard lock(catalog.mutex);
			for (const auto* collection : {&catalog.entries, &catalog.pendingEntries})
			{
				for (const auto& entry : *collection)
					for (const auto& contentId : entry.verifiedContent) unique.insert(contentId);
			}
		}
		std::vector<std::string> content(unique.begin(), unique.end());
		std::ranges::sort(content);
		const auto result = Request("POST", "/zwnet/parties/" + partyId + "/content",
			{{"content", content}, {"client_version", ZWNET_CLIENT_VERSION},
				{"mod_version", ZWNET_MOD_VERSION}});
		if (result && result->is_object() && !result->contains("error")) UpdateLobbyDvars(*result);
		return result;
	}

	void ZWNet::StartQuickPlay(std::string playlistId, const std::int64_t playlistRevision,
		const std::uint64_t selectorGeneration)
	{
		std::vector<std::string> verifiedContent;
		std::string playlistName;
		std::string playlistSettings;
		{
			auto& catalog = PlaylistCatalogState();
			std::lock_guard lock(catalog.mutex);
			const auto selected = std::ranges::find_if(catalog.entries,
				[&](const ClientPlaylist& entry)
				{
					return entry.id == playlistId && entry.revision == playlistRevision;
				});
			if (catalog.loaded && !catalog.stale && selected != catalog.entries.end() &&
				selected->availability == "AVAILABLE")
			{
				verifiedContent = selected->verifiedContent;
				playlistName = selected->name;
				playlistSettings = selected->zombieSettingsSummary;
			}
		}
		if (playlistName.empty() || !IsCurrentPlaylistActivation(selectorGeneration))
		{
			SearchingState() = false;
			PlaylistSelectionStarting() = false;
			if (IsCurrentPlaylistActivation(selectorGeneration))
				SetState("IN_PARTY", "ZWNET_PLAYLIST_REFRESH_REQUIRED");
			Scheduler::Once([] { PublishPlaylistCatalog(); }, Scheduler::Pipeline::MAIN);
			return;
		}
		if (PlaylistSearchStarting().exchange(true))
		{
			PlaylistSelectionStarting() = false;
			return;
		}
		const auto searchGuard = gsl::finally([]
		{
			PlaylistSearchStarting() = false;
			PlaylistSelectionStarting() = false;
			Scheduler::Once([] { PublishPlaylistCatalog(); }, Scheduler::Pipeline::MAIN);
		});
		Scheduler::Once([] { PublishPlaylistCatalog(); }, Scheduler::Pipeline::MAIN);
		ResetMatchLobbySoundSnapshot();
		SetState("SEARCH_STARTING");
		const auto fail = [](const std::string& error)
		{
			SearchingState() = false;
			SetState("IN_PARTY", error);
		};
		const auto cancelled = [selectorGeneration]
		{
			if (IsCurrentPlaylistActivation(selectorGeneration)) return false;
			SearchingState() = false;
			SetState("IN_PARTY");
			return true;
		};
		const auto accept = [selectorGeneration, playlistId, playlistName, playlistSettings, playlistRevision](const std::string& state)
		{
			if (!IsCurrentPlaylistActivation(selectorGeneration)) return false;
			PlaylistSelectorOpen() = false;
			++PlaylistSelectorGeneration();
			SearchingState() = true;
			SetState(state);
			Scheduler::Once([playlistId, playlistName, playlistSettings, playlistRevision]
			{
				if (!ActiveState()) return;
				Dvar::Var("zwnet_search_playlist_id").set(playlistId);
				Dvar::Var("zwnet_search_playlist_name").set(playlistName);
				Dvar::Var("zwnet_search_playlist_revision").set(static_cast<int>(playlistRevision));
				Dvar::Var("zwnet_search_playlist_settings").set(playlistSettings);
				Dvar::Var("zwnet_catalog_action_error").set("");
				Command::Execute("closemenu popup_zwnet_playlists", false);
			}, Scheduler::Pipeline::MAIN);
			return true;
		};

		auto party = Request("GET", "/zwnet/parties/current");
		if (cancelled()) return;
		if (!party || party->is_null())
		{
			party = Request("POST", "/zwnet/parties/create",
				{{"visibility", PartyVisibilityName(DesiredPartyPrivacy().load())}});
			if (cancelled()) return;
		}
		if (!party || party->is_null() || party->contains("error"))
		{
			fail("ZWNET_PARTY_FAILED");
			return;
		}
		party = ApplyPartyVisibility(std::move(*party));
		if (cancelled()) return;
		if (!party)
		{
			fail("ZWNET_PARTY_FAILED");
			return;
		}
		const auto partyId = JsonString(*party, "id");
		std::string currentPlayerId;
		{ std::lock_guard lock(StateMutex()); currentPlayerId = CurrentPlayerIdState(); }
		if (JsonString(*party, "leader_id") != currentPlayerId)
		{
			fail("ZWNET_LEADER_REQUIRED");
			return;
		}
		const auto contentResult = Request("POST", "/zwnet/parties/" + partyId + "/content",
			{{"content", verifiedContent}, {"client_version", ZWNET_CLIENT_VERSION},
				{"mod_version", ZWNET_MOD_VERSION}});
		if (cancelled()) return;
		if (!contentResult || !contentResult->is_object() || contentResult->contains("error"))
		{
			fail("ZWNET_CONTENT_MISSING");
			return;
		}
		party = *contentResult;
		const auto selectedRevision = party->contains("selected_playlist_revision") &&
			party->at("selected_playlist_revision").is_number_integer()
			? party->at("selected_playlist_revision").get<std::int64_t>() : 0;
		if (JsonString(*party, "selected_playlist_id") != playlistId ||
			selectedRevision != playlistRevision)
		{
			const auto selectedParty = Request("POST", "/zwnet/parties/" + partyId + "/select-playlist",
				{{"playlist_id", playlistId}, {"playlist_revision", playlistRevision}});
			if (selectedParty && selectedParty->is_object() && !selectedParty->contains("error"))
				UpdateLobbyDvars(*selectedParty);
			if (cancelled()) return;
			if (!selectedParty || !selectedParty->is_object() || selectedParty->contains("error"))
			{
				const auto code = selectedParty && selectedParty->is_object() ?
					ResponseErrorCode(*selectedParty) : std::string{};
				if (code == "PLAYLIST_REVISION_STALE" || code == "PLAYLIST_ACCESS_DENIED" ||
					code == "PLAYLIST_NO_CAPACITY")
				{
					RefreshPlaylistCatalog(true);
					fail("ZWNET_PLAYLIST_REFRESH_REQUIRED");
				}
				else if (code == "LEADER_REQUIRED") fail("ZWNET_LEADER_REQUIRED");
				else fail("ZWNET_PLAYLIST_SELECTION_FAILED");
				return;
			}
			party = *selectedParty;
		}
		*party = PublishLocalRank(std::move(*party));
		if (cancelled()) return;
		UpdateLobbyDvars(*party);
		const auto partyState = party->value("state", "IDLE");
		if (IsActiveMatchmakingState(partyState))
		{
			if (accept(partyState)) UpdateMatchmaking();
			return;
		}
		if (cancelled()) return;
		const auto result = Request("POST", "/zwnet/matchmaking/search", {{"playlist_id", playlistId}, {"playlist_revision", playlistRevision}, {"region_id", "eu-central"}, {"client_version", ZWNET_CLIENT_VERSION}, {"mod_version", ZWNET_MOD_VERSION}, {"content", verifiedContent}, {"ping_ms", 50}});
		if (!IsCurrentPlaylistActivation(selectorGeneration))
		{
			if (result && (!result->contains("error") ||
				ResponseErrorCode(*result) == "ALREADY_QUEUED" || ResponseErrorCode(*result) == "ALREADY_MATCHED"))
			{
				Request("POST", "/zwnet/matchmaking/cancel");
				if (const auto current = Request("GET", "/zwnet/parties/current");
					current && current->is_object() && !current->contains("error")) UpdateLobbyDvars(*current);
			}
			SearchingState() = false;
			SetState("IN_PARTY");
			return;
		}
		if (!result)
		{
			fail("ZWNET_SEARCH_FAILED");
			return;
		}
		if (result->contains("error"))
		{
			const auto code = ResponseErrorCode(*result);
			if (code == "ALREADY_QUEUED" || code == "ALREADY_MATCHED")
			{
				if (accept("SEARCHING")) UpdateMatchmaking();
				return;
			}
			if (code == "PLAYLIST_REVISION_STALE" || code == "PARTY_PLAYLIST_MISMATCH" ||
				code == "PLAYLIST_NO_CAPACITY" || code == "PLAYLIST_ACCESS_DENIED")
			{
				RefreshPlaylistCatalog(true);
				fail("ZWNET_PLAYLIST_REFRESH_REQUIRED");
				return;
			}
			if (code == "VERSION_MISMATCH") fail("ZWNET_VERSION_MISMATCH");
			else if (code == "LEADER_REQUIRED") fail("ZWNET_LEADER_REQUIRED");
			else if (code == "PARTY_TOO_LARGE") fail("ZWNET_PARTY_TOO_LARGE");
			else if (code == "CONTENT_MISSING" || code == "PARTY_CONTENT_MISSING") fail("ZWNET_CONTENT_MISSING");
			else fail("ZWNET_SEARCH_FAILED");
			return;
		}
		const auto joinedState = JsonString(*result, "state", "SEARCHING");
		if (accept(joinedState) && joinedState != "SEARCHING") UpdateMatchmaking();
	}

	std::optional<nlohmann::json> ZWNet::ApplyPartyVisibility(nlohmann::json party)
	{
		if (!party.is_object() || party.contains("error")) return std::nullopt;
		const auto partyId = JsonString(party, "id");
		if (!IsOpaquePartyId(partyId)) return std::nullopt;

		std::string currentPlayerId;
		{
			std::lock_guard lock(StateMutex());
			currentPlayerId = CurrentPlayerIdState();
		}
		const auto isLeader = !currentPlayerId.empty() &&
			JsonString(party, "leader_id") == currentPlayerId;
		LocalPartyLeader() = isLeader;
		if (!isLeader) return party;

		const auto desiredVisibility = std::string{
			PartyVisibilityName(DesiredPartyPrivacy().load())};
		const auto currentVisibility = NormalizePartyVisibility(
			JsonString(party, "visibility", "OPEN"));
		if (currentVisibility == desiredVisibility) return party;

		const auto updated = Request("POST",
			"/zwnet/parties/" + partyId + "/set-visibility",
			{{"visibility", desiredVisibility}});
		if (!updated || !updated->is_object() || updated->contains("error"))
		{
			return std::nullopt;
		}
		return *updated;
	}

	void ZWNet::RefreshPartyVisibility()
	{
		const auto resetPending = gsl::finally([]
		{
			VisibilitySyncPending() = false;
		});
		if (!ActiveState() || !LocalPartyLeader()) return;
		const auto party = Request("GET", "/zwnet/parties/current");
		if (!party || party->is_null() || party->contains("error")) return;
		const auto updated = ApplyPartyVisibility(*party);
		if (updated) UpdateLobbyDvars(*updated);
	}

	void ZWNet::CapturePartyPrivacy()
	{
		if (!ActiveState()) return;
		const auto privacy = std::clamp(
			Dvar::Var("partyPrivacy").get<int>(), 0, 2);
		DesiredPartyPrivacy() = privacy;
		if (LocalPartyLeader() && CachedPartyVisibility().load() != privacy &&
			!VisibilitySyncPending().exchange(true))
		{
			EnqueueAsync([] { RefreshPartyVisibility(); });
		}
	}

	nlohmann::json ZWNet::PublishLocalRank(nlohmann::json party)
	{
		if (!party.is_object() || party.contains("error"))
		{
			return party;
		}

		const auto partyId = JsonString(party, "id");
		if (partyId.empty())
		{
			return party;
		}

		const auto guid = FormatPublicGuid();
		const auto rank = ReadLocalLobbyRank();
		if (!rank)
		{
			CacheSharedLobbyRanks(party);
			return party;
		}
		const auto publishedRanks = ParseSharedLobbyRanks(party);
		const auto publishedRank = publishedRanks.find(guid);
		if (publishedRank != publishedRanks.end() &&
			publishedRank->second.level == rank->level &&
			publishedRank->second.prestige == rank->prestige)
		{
			CacheSharedLobbyRanks(party);
			return party;
		}

		const auto now = std::chrono::steady_clock::now();
		{
			std::lock_guard lock(SharedLobbyRankMutex);
			const auto rankChanged =
				partyId != LastRankPublishPartyId ||
				rank->level != LastRankPublishLevel ||
				rank->prestige != LastRankPublishPrestige;
			if (!rankChanged && now < NextRankPublishAttempt)
			{
				SharedLobbyRanks[guid] = *rank;
				return party;
			}

			LastRankPublishPartyId = partyId;
			LastRankPublishLevel = rank->level;
			LastRankPublishPrestige = rank->prestige;
			NextRankPublishAttempt = now + std::chrono::seconds(30);
		}

		const auto result = Request(
			"POST",
			"/zwnet/parties/" + partyId + "/player-rank",
			{{"level", rank->level}, {"prestige", rank->prestige}});
		if (result && result->is_object() && !result->contains("error"))
		{
			party = *result;
		}

		CacheSharedLobbyRanks(party);
		return party;
	}

	void ZWNet::UpdateLobbyDvars(const nlohmann::json& party)
	{
		if (party.is_null() || !party.is_object()) return;
		const auto sharedRanks = CacheSharedLobbyRanks(party);
		const auto partyId = JsonString(party, "id");
		const auto leaderId = JsonString(party, "leader_id");
		const auto state = JsonString(party, "state", "IDLE");
		auto selectedPlaylistId = JsonString(party, "selected_playlist_id");
		std::int64_t selectedPlaylistRevision = 0;
		if (party.contains("selected_playlist_revision") &&
			party.at("selected_playlist_revision").is_number_integer())
			selectedPlaylistRevision = party.at("selected_playlist_revision").get<std::int64_t>();
		if (selectedPlaylistId.size() > 96 || selectedPlaylistRevision < 1 ||
			!std::ranges::all_of(selectedPlaylistId, [](const unsigned char c)
			{
				return std::isalnum(c) || c == '-' || c == '_';
			}))
		{
			selectedPlaylistId.clear();
			selectedPlaylistRevision = 0;
		}
		const auto members = party.value("members", nlohmann::json::array());
		std::string owner;
		std::string currentPlayerId;
		{ std::lock_guard lock(StateMutex()); currentPlayerId = CurrentPlayerIdState(); }
		LocalPartyLeader() = !currentPlayerId.empty() && currentPlayerId == leaderId;
		const auto visibility = NormalizePartyVisibility(
			JsonString(party, "visibility", "OPEN"));
		CachedPartyMemberCount() = static_cast<int>(members.size());
		CachedPartyVisibility() = PartyVisibilityValue(visibility);
		CachedPartyJoinStateSupported() = IsPartyJoinStateSupported(state);
		for (const auto& member : members)
		{
			if (JsonString(member, "player_id") == leaderId)
			{
				owner = SafeDisplayName(JsonString(member, "display_name"));
				break;
			}
		}
		{
			std::lock_guard lock(StateMutex());
			CurrentPartyIdState() = partyId;
		}
		{
			auto& catalog = PlaylistCatalogState();
			std::lock_guard lock(catalog.mutex);
			catalog.partySelectedId = selectedPlaylistId;
			catalog.partySelectedRevision = selectedPlaylistRevision;
			if (!selectedPlaylistId.empty() &&
				(!PlaylistSelectorOpen() || catalog.selectedId.empty()))
				catalog.selectedId = selectedPlaylistId;
		}
		Scheduler::Once([] { PublishPlaylistCatalog(); }, Scheduler::Pipeline::MAIN);
		const auto stateText = FriendlyStateText(state);
		Scheduler::Once([partyId, leaderId, state, stateText, owner, members, currentPlayerId, sharedRanks, visibility]
		{
			if (!ActiveState()) return;
			Dvar::Var("zwnet_lobby_active").set(true);
			Dvar::Var("zwnet_lobby_party_id").set(partyId);
			Dvar::Var("zwnet_lobby_visibility").set(visibility);
			Dvar::Var("zwnet_lobby_owner").set(owner);
			Dvar::Var("zwnet_lobby_member_count").set(static_cast<int>(members.size()));
			Dvar::Var("zwnet_lobby_status_text").set(stateText);
			bool allReady = !members.empty();
			bool selfReady = false;
			for (std::size_t i = 0; i < 4; ++i)
			{
				const auto prefix = std::format("zwnet_lobby_member_{}", i);
				if (i < members.size())
				{
					const auto& member = members[i];
					const auto ready = member.value("ready", 0) == 1;
					const auto isSelf = JsonString(member, "player_id") == currentPlayerId;
					const auto displayName = SafeDisplayName(JsonString(member, "display_name"));
					Dvar::Var(prefix + "_name").set(displayName);
					Dvar::Var(prefix + "_guid").set(JsonString(member, "player_id"));
					Dvar::Var(prefix + "_role").set(JsonString(member, "player_id") == leaderId ? "PARTY LEADER" : "MEMBER");
					Dvar::Var(prefix + "_ready").set(ready);
					Dvar::Var(prefix + "_self").set(isSelf);
					const auto rankIt = sharedRanks.find(
						JsonString(member, "player_id"));
					const auto rankKnown = rankIt != sharedRanks.end();
					Dvar::Var(prefix + "_shared_rank_known").set(rankKnown);
					Dvar::Var(prefix + "_shared_rank_level").set(
						rankKnown ? rankIt->second.level : 1);
					Dvar::Var(prefix + "_shared_rank_prestige").set(
						rankKnown ? rankIt->second.prestige : 0);
					allReady = allReady && ready;
					if (isSelf) selfReady = ready;
				}
				else
				{
					Dvar::Var(prefix + "_name").set("");
					Dvar::Var(prefix + "_guid").set("");
					Dvar::Var(prefix + "_role").set("");
					Dvar::Var(prefix + "_ready").set(false);
					Dvar::Var(prefix + "_self").set(false);
					Dvar::Var(prefix + "_shared_rank_known").set(false);
					Dvar::Var(prefix + "_shared_rank_level").set(1);
					Dvar::Var(prefix + "_shared_rank_prestige").set(0);
				}
			}
			Dvar::Var("zwnet_lobby_self_ready").set(selfReady);
			Dvar::Var("zwnet_all_ready").set(allReady);
			Dvar::Var("zwnet_lobby_can_start").set(currentPlayerId == leaderId && allReady && state != "SEARCHING" && state != "IN_MATCH");
		}, Scheduler::Pipeline::MAIN);
	}

	void ZWNet::ResumeParty(const nlohmann::json& party)
	{
		if (!ActiveState() || !party.is_object() || JsonString(party, "id").empty()) return;
		CancelJoinInProgressPreview();
		auto resumedParty = PublishLocalRank(party);
		const auto partyState = JsonString(resumedParty, "state", "IDLE");
		const auto activeMatchmaking = IsActiveMatchmakingState(partyState);
		SearchingState() = activeMatchmaking;
		UpdateLobbyDvars(resumedParty);
		SetState(activeMatchmaking ? partyState : "IN_PARTY");
		Scheduler::Once([activeMatchmaking]
		{
			if (!ActiveState()) return;
			Command::Execute(activeMatchmaking
				? "openmenu zwnet_matchmaking"
				: "openmenu zwnet_party_lobby", false);
		}, Scheduler::Pipeline::MAIN);
		if (activeMatchmaking) UpdateMatchmaking();
	}

	void ZWNet::JoinParty(const std::string& partyId)
	{
		if (!IsOpaquePartyId(partyId))
		{
			SetState("ERROR", "ZWNET_PARTY_FAILED");
			return;
		}
		EnqueueAsync([partyId]
		{
			const auto response = Request("POST",
				"/zwnet/parties/" + partyId + "/join",
				nlohmann::json::object());
			if (!response || !response->is_object() || response->contains("error"))
			{
				SetState("ERROR", "ZWNET_PARTY_FAILED");
				return;
			}
			ResumeParty(*response);
		});
	}

	void ZWNet::JoinCapability(const std::string& capability)
	{
		if (!IsOpaqueJoinCapability(capability))
		{
			SetState("ERROR", "ZWNET_PARTY_FAILED");
			return;
		}
		EnqueueAsync([capability]
		{
			const auto response = Request("POST",
				"/zwnet/parties/join-capability",
				{{"capability", capability}});
			if (!response || !response->is_object() || response->contains("error"))
			{
				SetState("ERROR", "ZWNET_PARTY_FAILED");
				return;
			}
			ResumeParty(*response);
		});
	}

	bool ZWNet::BeginEndpointJoin(const std::string& endpoint)
	{
		if (!ActiveState() || endpoint.empty() || endpoint.size() > 255) return false;
		const Network::Address requestedTarget(endpoint);
		if (!requestedTarget.isValid()) return false;
		bool knownAssignedEndpoint = false;
		bool hasAccessToken = false;
		{
			std::lock_guard lock(StateMutex());
			hasAccessToken = !AccessTokenState().empty();
			const Network::Address assignedTarget(
				Dvar::Var("zwnet_server_endpoint").get<std::string>());
			knownAssignedEndpoint = !CurrentMatchIdState().empty() &&
				assignedTarget.isValid() && assignedTarget == requestedTarget;
		}
		EndpointJoinInFlight() = true;
		if (!hasAccessToken)
		{
			// Preserve native dedicated/private joins while requiring the endpoint's
			// existing getinfo response to prove that it is not a managed session.
			Scheduler::Once([endpoint]
			{
				EndpointJoinInFlight() = false;
				const Network::Address target(endpoint);
				if (target.isValid()) Party::Connect(target, false, true);
			}, Scheduler::Pipeline::MAIN);
			return true;
		}

		EnqueueAsync([endpoint, knownAssignedEndpoint]
		{
			const auto response = Request("POST",
				"/zwnet/matchmaking/join-by-endpoint",
				{{"endpoint", endpoint}});
			if (!response || !response->is_object())
			{
				if (knownAssignedEndpoint)
				{
					EndpointJoinInFlight() = false;
					SetState("ERROR", "ZWNET_REQUEST_FAILED");
					return;
				}
				// An unknown endpoint must prove through the existing getinfo flow
				// that it is unmanaged before the native connection may continue.
				Scheduler::Once([endpoint]
				{
					EndpointJoinInFlight() = false;
					const Network::Address target(endpoint);
					if (target.isValid()) Party::Connect(target, false, true);
				}, Scheduler::Pipeline::MAIN);
				return;
			}
			if (response->contains("error"))
			{
				EndpointJoinInFlight() = false;
				SetState("ERROR", "ZWNET_PARTY_FAILED");
				return;
			}

			if (!response->value("managed", false))
			{
				Scheduler::Once([endpoint]
				{
					EndpointJoinInFlight() = false;
					const Network::Address target(endpoint);
					if (target.isValid()) Party::Connect(target);
				}, Scheduler::Pipeline::MAIN);
				return;
			}

			const auto party = response->find("party");
			if (!response->value("connect", false) || party == response->end() ||
				!party->is_object() || party->contains("error"))
			{
				EndpointJoinInFlight() = false;
				SetState("ERROR", "ZWNET_PARTY_FAILED");
				return;
			}

			// The endpoint only identifies a managed lobby. Joining it grants no
			// transport access; the normal match descriptor must issue this player
			// a short-lived ticket before Party::Connect may run.
			EndpointJoinInFlight() = false;
			ResumeParty(*party);
			SearchingState() = true;
			UpdateMatchmaking();
		});
		return true;
	}

	void ZWNet::EnterLobby(std::string map)
	{
		auto party = Request("GET", "/zwnet/parties/current");
		if (!party || party->is_null())
		{
			party = Request("POST", "/zwnet/parties/create",
				{{"visibility", PartyVisibilityName(DesiredPartyPrivacy().load())}});
		}
		if (!party || party->contains("error")) { SetState("ERROR", "ZWNET_PARTY_FAILED"); return; }
		party = ApplyPartyVisibility(std::move(*party));
		if (!party) { SetState("ERROR", "ZWNET_PARTY_FAILED"); return; }
		*party = PublishLocalRank(std::move(*party));
		UpdateLobbyDvars(*party);
		const auto partyId = JsonString(*party, "id");
		if (!partyId.empty())
		{
			Request("POST", "/zwnet/parties/" + partyId + "/set-map", {{"map", map}});
			Request("POST", "/zwnet/parties/" + partyId + "/set-mode", {{"mode", "zw3"}});
		}
		SetState("IN_PARTY");
	}

	void ZWNet::RefreshLobby()
	{
		auto party = Request("GET", "/zwnet/parties/current");
		if (party && !party->is_null() && !party->contains("error"))
		{
			*party = PublishLocalRank(std::move(*party));
			UpdateLobbyDvars(*party);
			const auto partyState = party->value("state", "IDLE");
			if (IsActiveMatchmakingState(partyState))
			{
				SearchingState() = true;
				SetState(partyState);
				UpdateMatchmaking();
			}
		}
	}

	void ZWNet::RefreshActiveParty()
	{
		if (!ActiveState() || SearchingState() || ClosingOnlineSessionState()) return;

		std::string partyId;
		{
			std::lock_guard lock(StateMutex());
			partyId = CurrentPartyIdState();
		}
		if (partyId.empty()) return;

		auto party = Request("GET", "/zwnet/parties/current");
		if (party && party->is_object() && !party->contains("error"))
		{
			*party = PublishLocalRank(std::move(*party));
			UpdateLobbyDvars(*party);
		}
	}

	void ZWNet::LeaveParty()
	{
		CancelJoinInProgressPreview();
		const auto result = Request("POST", "/zwnet/parties/leave");
		if (result && !result->contains("error"))
		{
			ResetMatchLobbySoundSnapshot();
			std::lock_guard lock(StateMutex());
			CurrentPartyIdState().clear(); CurrentProposalIdState().clear(); CurrentMatchIdState().clear();
		}
		SearchingState() = false;
		Scheduler::Once([]
		{
			if (!ActiveState()) return;
			Dvar::Var("zwnet_lobby_active").set(false);
			Dvar::Var("zwnet_vote_active").set(false);
			Dvar::Var("zwnet_all_ready").set(false);
			Dvar::Var("zwnet_start_phase").set("");
			Dvar::Var("zwnet_start_seconds").set(0);
		}, Scheduler::Pipeline::MAIN);
	}

	void ZWNet::UpdateMatchLobbyDvars(const nlohmann::json& status)
	{
		if (!status.contains("lobby") || !status.at("lobby").is_object()) return;
		const auto& lobby = status.at("lobby");
		const auto membersIt = lobby.find("members");
		if (membersIt == lobby.end() || !membersIt->is_array()) return;
		struct LobbyMemberSnapshot
		{
			std::string playerId;
			std::string displayName;
			std::string role;
			bool ready{};
			bool rankKnown{};
			int rankLevel{1};
			int rankPrestige{};
		};
		std::array<LobbyMemberSnapshot, 4> members{};
		std::unordered_set<std::string> memberIds;
		std::size_t memberCount{};
		for (const auto& member : *membersIt)
		{
			if (memberCount >= members.size()) break;
			if (!member.is_object()) continue;
			auto& snapshot = members[memberCount++];
			snapshot.playerId = JsonString(member, "player_id");
			if (!snapshot.playerId.empty()) memberIds.emplace(snapshot.playerId);
			snapshot.displayName = SafeDisplayName(JsonString(member, "display_name"));
			snapshot.role = JsonString(member, "role", "MEMBER");
			const auto rankIt = member.find("rank");
			if (rankIt != member.end() && rankIt->is_object())
			{
				snapshot.rankLevel = std::clamp(rankIt->value("level", 1), 1, 54);
				snapshot.rankPrestige = std::max(rankIt->value("prestige", 0), 0);
				snapshot.rankKnown = rankIt->contains("level");
			}
			else if (member.contains("level") && member.at("level").is_number_integer())
			{
				snapshot.rankLevel = std::clamp(member.at("level").get<int>(), 1, 54);
				snapshot.rankPrestige = member.contains("prestige") &&
					member.at("prestige").is_number_integer()
					? std::max(member.at("prestige").get<int>(), 0)
					: 0;
				snapshot.rankKnown = true;
			}
			else if (member.contains("rank_level"))
			{
				snapshot.rankLevel = std::clamp(member.value("rank_level", 1), 1, 54);
				snapshot.rankPrestige = std::max(member.value("rank_prestige", 0), 0);
				snapshot.rankKnown = true;
			}
			if (!snapshot.rankKnown)
			{
				snapshot.rankKnown = Friends::TryGetZombieRankByGuid(
					snapshot.playerId, snapshot.rankLevel, snapshot.rankPrestige);
			}
			const auto readyIt = member.find("ready");
			if (readyIt != member.end())
			{
				if (readyIt->is_boolean()) snapshot.ready = readyIt->get<bool>();
				else if (readyIt->is_number_integer()) snapshot.ready = readyIt->get<std::int64_t>() != 0;
			}
		}
		const auto soundDelta = ObserveMatchLobbyMembers(
			JsonString(status, "match_id"), memberIds);
		const auto lobbyState = JsonString(status, "state", "WAITING_FOR_READY");
		CachedPartyMemberCount() = static_cast<int>(memberCount);
		CachedPartyJoinStateSupported() = IsPartyJoinStateSupported(lobbyState);
		const auto stateText = FriendlyStateText(lobbyState);
		const auto sharedRanks = GetCachedSharedLobbyRanks();
		std::string currentPlayerId;
		{ std::lock_guard lock(StateMutex()); currentPlayerId = CurrentPlayerIdState(); }
		Scheduler::Once([members = std::move(members), memberCount, stateText, currentPlayerId, sharedRanks, soundDelta]
		{
			if (!ActiveState()) return;
			static constexpr std::array memberPrefixes
			{
				"zwnet_lobby_member_0",
				"zwnet_lobby_member_1",
				"zwnet_lobby_member_2",
				"zwnet_lobby_member_3",
			};
			Dvar::Var("zwnet_lobby_active").set(true);
			Dvar::Var("zwnet_lobby_member_count").set(static_cast<int>(memberCount));
			Dvar::Var("zwnet_lobby_status_text").set(stateText);
			bool allReady = memberCount > 0;
			bool selfReady = false;
			for (std::size_t i = 0; i < members.size(); ++i)
			{
				const auto prefix = memberPrefixes[i];
				if (i < memberCount)
				{
					const auto& member = members[i];
					Dvar::Var(std::string{prefix} + "_name").set(member.displayName);
					Dvar::Var(std::string{prefix} + "_guid").set(member.playerId);
					Dvar::Var(std::string{prefix} + "_role").set(member.role);
					Dvar::Var(std::string{prefix} + "_ready").set(member.ready);
					Dvar::Var(std::string{prefix} + "_self").set(member.playerId == currentPlayerId);
					const auto rankIt = sharedRanks.find(member.playerId);
					const auto sharedRankKnown = rankIt != sharedRanks.end();
					const auto rankKnown = member.rankKnown || sharedRankKnown;
					Dvar::Var(std::string{prefix} + "_shared_rank_known").set(rankKnown);
					Dvar::Var(std::string{prefix} + "_shared_rank_level").set(
						member.rankKnown ? member.rankLevel :
						sharedRankKnown ? rankIt->second.level : 1);
					Dvar::Var(std::string{prefix} + "_shared_rank_prestige").set(
						member.rankKnown ? member.rankPrestige :
						sharedRankKnown ? rankIt->second.prestige : 0);
					allReady = allReady && member.ready;
					if (member.playerId == currentPlayerId) selfReady = member.ready;
				}
				else
				{
					Dvar::Var(std::string{prefix} + "_name").set("");
					Dvar::Var(std::string{prefix} + "_guid").set("");
					Dvar::Var(std::string{prefix} + "_role").set("");
					Dvar::Var(std::string{prefix} + "_ready").set(false);
					Dvar::Var(std::string{prefix} + "_self").set(false);
					Dvar::Var(std::string{prefix} + "_shared_rank_known").set(false);
					Dvar::Var(std::string{prefix} + "_shared_rank_level").set(1);
					Dvar::Var(std::string{prefix} + "_shared_rank_prestige").set(0);
				}
			}
			Dvar::Var("zwnet_lobby_self_ready").set(selfReady);
			Dvar::Var("zwnet_all_ready").set(allReady);
			Dvar::Var("zwnet_lobby_can_start").set(false);
			// Matchmaking rosters are HTTP-backed, so reproduce the native private
			// lobby membership sounds from stable player-ID deltas.
			if (soundDelta.left) Command::Execute("snd_playLocal mp_player_leave", false);
			if (soundDelta.joined) Command::Execute("snd_playLocal mp_player_join", false);
		}, Scheduler::Pipeline::MAIN);
	}

	void ZWNet::UpdatePresence()
	{
		if (!ActiveState() || ClosingOnlineSessionState() || OnlineEntryPendingState()) return;
		std::string matchId;
		std::string partyId;
		{
			std::lock_guard lock(StateMutex());
			matchId = CurrentMatchIdState();
			partyId = CurrentPartyIdState();
		}
		auto status = std::string{"MAIN_MENU"};
		const auto uiState = Dvar::Var("ui_zwnet_state").get<std::string>();
		if (InGameState()) status = "IN_MATCH";
		else if (uiState == "MAP_VOTE") status = "MAP_VOTE";
		else if (uiState == "RESERVING_SERVER" || uiState == "STARTING_SERVER" || uiState == "SERVER_STARTING") status = "SERVER_STARTING";
		else if (uiState == "JOIN_PREVIEW" || uiState == "COUNTDOWN" || uiState == "CONNECTING" || uiState == "DIRECT_CONNECTION" || uiState == "RELAY_CONNECTION") status = "CONNECTING";
		else if (uiState == "READY_CHECK" || uiState == "WAITING_FOR_READY" || uiState == "MATCH_FOUND") status = "IN_PARTY";
		else if (SearchingState()) status = "SEARCHING";
		else if (IsOpaquePartyId(partyId)) status = "IN_PARTY";
		const auto joinable = IsOpaquePartyId(partyId) &&
			CachedPartyMemberCount().load() > 0 &&
			CachedPartyMemberCount().load() < 4 &&
			CachedPartyVisibility().load() != 2 &&
			CachedPartyJoinStateSupported().load();
		const auto result = Request("POST", "/social/presence",
			{{"status", status}, {"sequence", NextPresenceSequence()}, {"joinable", joinable}});
		if (!result || result->contains("error"))
		{
			Logger::Print("ZWNET presence update failed; matchmaking state preserved\n");
		}
	}

	void ZWNet::ToggleReady(const bool ready)
	{
		std::string partyId;
		{ std::lock_guard lock(StateMutex()); partyId = CurrentPartyIdState(); }
		if (partyId.empty())
		{
			Scheduler::Once([]
			{
				if (ActiveState()) Dvar::Var("zwnet_ready_pending").set(false);
			}, Scheduler::Pipeline::MAIN);
			return;
		}
		const auto result = Request("POST", "/zwnet/parties/" + partyId + (ready ? "/unready" : "/ready"));
		Scheduler::Once([]
		{
			if (ActiveState()) Dvar::Var("zwnet_ready_pending").set(false);
		}, Scheduler::Pipeline::MAIN);
		if (result && !result->contains("error"))
		{
			Scheduler::Once([ready]
			{
				if (!ActiveState()) return;
				Dvar::Var("zwnet_lobby_self_ready").set(!ready);
			}, Scheduler::Pipeline::MAIN);
			UpdateLobbyDvars(*result);
			if (SearchingState()) UpdateMatchmaking();
		}
	}

	void ZWNet::StartPrivateMatch(std::string map)
	{
		std::string partyId;
		{ std::lock_guard lock(StateMutex()); partyId = CurrentPartyIdState(); }
		if (partyId.empty()) return;
		Request("POST", "/zwnet/parties/" + partyId + "/set-map", {{"map", map}});
		Request("POST", "/zwnet/parties/" + partyId + "/set-mode", {{"mode", "zw3"}});
		const auto result = Request("POST", "/zwnet/parties/" + partyId + "/start-private-match");
		if (!result || result->contains("error")) { SetState("ERROR", "ZWNET_PRIVATE_MATCH_FAILED"); return; }
		SearchingState() = true;
		SetState(result->value("state", "RESERVING_SERVER"));
		UpdateMatchmaking();
	}

	void ZWNet::VoteMap(const std::string& choice)
	{
		std::string proposalId;
		{ std::lock_guard lock(StateMutex()); proposalId = CurrentProposalIdState(); }
		if (proposalId.empty()) return;
		const auto result = Request("POST", "/zwnet/matchmaking/map-vote", {{"proposal_id", proposalId}, {"choice", choice}});
		if (!result) { SetState("ERROR", "ZWNET_MAP_VOTE_FAILED"); return; }
		if (result->contains("error"))
		{
			if (ResponseErrorCode(*result) == "MAP_VOTE_CLOSED")
			{
				UpdateMatchmaking();
				return;
			}
			SetState("ERROR", "ZWNET_MAP_VOTE_FAILED");
			return;
		}
		if (result->value("closed", false))
		{
			UpdateMatchmaking();
			return;
		}
		Scheduler::Once([choice]
		{
			if (!ActiveState()) return;
			Dvar::Var("zwnet_vote_selection").set(choice);
		}, Scheduler::Pipeline::MAIN);
		const auto* vote = result->contains("map_vote") && result->at("map_vote").is_object()
			? &result->at("map_vote")
			: result->contains("choices") ? &*result : nullptr;
		if (vote)
		{
			std::string matchId;
			{ std::lock_guard lock(StateMutex()); matchId = CurrentMatchIdState(); }
			UpdateVoteDvars({{"match_id", matchId}, {"map_vote", *vote}});
		}
	}

	void ZWNet::CancelSearch()
	{
		Request("POST", "/zwnet/matchmaking/cancel");
		SearchingState() = false; SetState("IDLE");
		Scheduler::Once([] { PublishPlaylistCatalog(); }, Scheduler::Pipeline::MAIN);
	}

	void ZWNet::CancelMatchmaking()
	{
		CloseOnlineSession(false, false);
		if (!ActiveState()) return;

		auto party = Request("GET", "/zwnet/parties/current");
		if (party && party->is_null())
		{
			party = Request("POST", "/zwnet/parties/create",
				{{"visibility", PartyVisibilityName(DesiredPartyPrivacy().load())}});
		}
		if (!party || !party->is_object() || party->contains("error"))
		{
			SetState("ERROR", "ZWNET_PARTY_FAILED");
			return;
		}
		party = ApplyPartyVisibility(std::move(*party));
		if (!party)
		{
			SetState("ERROR", "ZWNET_PARTY_FAILED");
			return;
		}
		*party = PublishLocalRank(std::move(*party));
		UpdateLobbyDvars(*party);
		SetState("IN_PARTY");
		RefreshPlaylistCatalog(true);
		UpdatePresence();
	}

	void ZWNet::CloseOnlineSession(const bool shuttingDown, const bool terminal)
	{
		CancelJoinInProgressPreview();
		if (!ActiveState() || ClosingOnlineSessionState().exchange(true)) return;
		ManagedReconnectInFlight() = false;
		PlaylistSelectionStarting() = false;
		CancelRelayHandshake();
		ClearManagedRouteAttempt();
		Auth::ClearManagedConnectTicket();
		const auto closingGuard = gsl::finally([] { ClosingOnlineSessionState() = false; });
		ResetMatchLobbySoundSnapshot();
		EndpointJoinInFlight() = false;
		ServerJoinTransitionState() = false;
		InGameState() = false;
		std::string matchId;
		{
			std::lock_guard lock(StateMutex());
			matchId = CurrentMatchIdState();
		}
		auto body = nlohmann::json::object();
		if (!matchId.empty()) body["match_id"] = matchId;
		if (terminal) body["terminal"] = true;
		Request("POST", "/zwnet/matchmaking/disconnect", body);
		Request("POST", "/social/presence", {{"status", "OFFLINE"}, {"sequence", NextPresenceSequence()}, {"joinable", false}});
		SearchingState() = false;
		Scheduler::Once([] { PublishPlaylistCatalog(); }, Scheduler::Pipeline::MAIN);
		{
			std::lock_guard lock(StateMutex());
			if (terminal) CurrentPartyIdState().clear();
			CurrentProposalIdState().clear();
			CurrentMatchIdState().clear();
		}
		if (shuttingDown) return;
		SetState("IDLE");
		Scheduler::Once([terminal]
		{
			if (!ActiveState()) return;
			if (terminal)
			{
				AbandonOnlineSession();
				return;
			}
			Dvar::Var("zwnet_vote_active").set(false);
			Dvar::Var("zwnet_vote_selection").set("");
			Dvar::Var("zwnet_all_ready").set(false);
			Dvar::Var("zwnet_start_phase").set("");
			Dvar::Var("zwnet_start_seconds").set(0);
			Dvar::Var("zwnet_vote_reveal_time").set(0);
			Dvar::Var("zwnet_vote_winner_id").set("");
			Dvar::Var("zwnet_vote_winner_name").set("");
			Dvar::Var("zwnet_vote_winner_image").set("");
			Dvar::Var("zwnet_server_endpoint").set("");
			Dvar::Var("zwnet_server_hostname").set("");
			Dvar::Var("zwnet_server_status").set("NOT ASSIGNED");
			Dvar::Var("zwnet_join_status").set("WAITING IN LOBBY");
		}, Scheduler::Pipeline::MAIN);
	}

	bool ZWNet::ReturnToMatchmakingLobby()
	{
		auto completed = false;
		for (auto attempt = 0; attempt < 7; ++attempt)
		{
			const auto status = Request("GET", "/zwnet/matchmaking/status");
			if (!status || !status->is_object() || status->contains("error")) return false;
			const auto state = JsonString(*status, "state");
			completed = state == "RESETTING" || state == "POST_MATCH" ||
				state == "FINISHED" || state == "IDLE";
			if (completed) break;
			if (attempt < 6) std::this_thread::sleep_for(1s);
		}
		if (!completed) return false;

		auto party = Request("GET", "/zwnet/parties/current");
		if (!party || !party->is_object() || party->is_null() || party->contains("error") ||
			JsonString(*party, "id").empty()) return false;
		*party = PublishLocalRank(std::move(*party));

		SearchingState() = false;
		InGameState() = false;
		EndpointJoinInFlight() = false;
		ServerJoinTransitionState() = false;
		ResetMatchLobbySoundSnapshot();
		{
			std::lock_guard lock(StateMutex());
			CurrentProposalIdState().clear();
			CurrentMatchIdState().clear();
		}
		UpdateLobbyDvars(*party);
		SetState("IN_PARTY");
		RefreshPlaylistCatalog(true);
		UpdatePresence();
		Scheduler::Once([]
		{
			if (!ActiveState()) return;
			Dvar::Var("zwnet_vote_active").set(false);
			Dvar::Var("zwnet_vote_selection").set("");
			Dvar::Var("zwnet_all_ready").set(false);
			Dvar::Var("zwnet_start_phase").set("");
			Dvar::Var("zwnet_start_seconds").set(0);
			Dvar::Var("zwnet_vote_reveal_time").set(0);
			Dvar::Var("zwnet_managed_session").set(false);
			Dvar::Var("zwnet_vote_winner_id").set("");
			Dvar::Var("zwnet_vote_winner_name").set("");
			Dvar::Var("zwnet_vote_winner_image").set("");
			Dvar::Var("zwnet_match_id").set("");
			Dvar::Var("zwnet_server_endpoint").set("");
			Dvar::Var("zwnet_server_hostname").set("");
			Dvar::Var("zwnet_server_status").set("NOT ASSIGNED");
			Dvar::Var("zwnet_join_status").set("WAITING IN LOBBY");
			for (const auto* menuName : { "class", "team_marinesopfor" })
			{
				if (auto* menu = Game::Menus_FindByName(Game::uiContext, menuName))
				{
					Game::Menus_CloseRequest(Game::uiContext, menu);
				}
			}
			Game::Menus_OpenByName(Game::uiContext, "zwnet_matchmaking");
		}, Scheduler::Pipeline::MAIN, 500ms);
		return true;
	}

	void ZWNet::ReturnToIdleMatchmakingMenu()
	{
		AbandonOnlineSession();
		Dvar::Var("zwnet_managed_session").set(false);
		SetState("IDLE");
		Command::Execute("set xblive_privateserver 0", false);
		Command::Execute("set xblive_privatematch 0", false);
		Command::Execute("set xblive_rankedmatch 0", false);
		for (const auto* menuName : { "class", "team_marinesopfor", "zwnet_matchmaking" })
		{
			if (auto* menu = Game::Menus_FindByName(Game::uiContext, menuName))
			{
				Game::Menus_CloseRequest(Game::uiContext, menu);
			}
		}
		Game::Menus_OpenByName(Game::uiContext, "pregame_loaderror");
		Game::Menus_OpenByName(Game::uiContext, "popup_zwnet_connecting");
	}

	void ZWNet::ScheduleReturnToIdleMatchmakingMenu()
	{
		static std::atomic_bool returnPending{false};
		if (returnPending.exchange(true)) return;

		const auto startedAt = Game::Sys_Milliseconds();
		Scheduler::Schedule([startedAt, disconnectedAt = -1]() mutable -> bool
		{
			const auto now = Game::Sys_Milliseconds();
			if (Game::CL_IsCgameInitialized())
			{
				if (now - startedAt <= 10000) return false;
				returnPending = false;
				return true;
			}
			if (disconnectedAt < 0)
			{
				disconnectedAt = now;
				return false;
			}
			if (now - disconnectedAt < 150) return false;

			returnPending = false;
			ReturnToIdleMatchmakingMenu();
			return true;
		}, Scheduler::Pipeline::MAIN, 50ms);
	}

	void ZWNet::HandleServerDisconnect(const bool terminal, const bool wasMatchmaking)
	{
		if (wasMatchmaking && !terminal && ReturnToMatchmakingLobby())
		{
			Logger::Print("ZWNET completed match: returned to the preserved party lobby\n");
			return;
		}
		CloseOnlineSession(false, terminal);
		if (wasMatchmaking) ScheduleReturnToIdleMatchmakingMenu();
	}

	bool ZWNet::BeginManagedReconnect(const std::string& endpoint)
	{
		if (!ActiveState()) return false;
		if (ManagedReconnectInFlight()) return true;
		std::string matchId;
		std::string playerId;
		{
			std::lock_guard lock(StateMutex());
			matchId = CurrentMatchIdState();
			playerId = CurrentPlayerIdState();
		}
		bool managed = false;
		bool bound = false;
		bool relay = false;
		{
			auto& route = RouteAttempt();
			std::lock_guard lock(route.mutex);
			managed = !matchId.empty() && route.matchId == matchId;
			if (managed)
			{
				const Network::Address requestedTarget(endpoint);
				bound = requestedTarget.isValid() && requestedTarget == route.assignedTarget &&
					route.playerId == playerId && !route.sessionId.empty();
				relay = route.routeIsRelay;
			}
		}
		if (!managed)
		{
			if (!Dvar::Var("zwnet_managed_session").get<bool>()) return false;
			Auth::ClearManagedConnectTicket();
			SetState("ERROR", "ZWNET_DESCRIPTOR_INVALID");
			return true;
		}
		if (ManagedReconnectInFlight()) return true;
		if (!bound || !IsOpaqueMatchId(matchId) || !IsRelayOpaque(playerId))
		{
			Auth::ClearManagedConnectTicket();
			SetState("ERROR", "ZWNET_DESCRIPTOR_INVALID");
			return true;
		}
		if (ManagedReconnectInFlight().exchange(true)) return true;
		++JoinTransitionGeneration();
		ServerJoinTransitionState() = true;
		EnqueueAsync([matchId, relay] { ConnectMatch(matchId, relay, true); });
		return true;
	}

	bool ZWNet::TryRelayAfterDirectTimeout(const std::string& endpoint)
	{
		if (!ActiveState()) return false;
		const Network::Address timedOutTarget(endpoint);
		if (!timedOutTarget.isValid()) return false;
		std::string matchId;
		std::string playerId;
		{
			std::lock_guard lock(StateMutex());
			matchId = CurrentMatchIdState();
			playerId = CurrentPlayerIdState();
		}
		if (!IsOpaqueMatchId(matchId) || !IsRelayOpaque(playerId)) return false;
		bool reconnect = false;
		{
			auto& route = RouteAttempt();
			std::lock_guard lock(route.mutex);
			if (!route.directWaiting || route.relayUsed ||
				route.matchId != matchId || route.playerId != playerId ||
				route.directTarget != timedOutTarget) return false;
			route.directWaiting = false;
			route.relayUsed = true;
			reconnect = route.reconnectAttempt;
		}
		// The existing direct getinfo timed out for this exact managed match.
		// A second descriptor is issued under the authenticated player session.
		++JoinTransitionGeneration();
		ServerJoinTransitionState() = true;
		EnqueueAsync([matchId, reconnect] { ConnectMatch(matchId, true, reconnect); });
		return true;
	}

	void ZWNet::CancelJoinInProgressPreview()
	{
		auto& preview = JoinPreview();
		{
			std::lock_guard lock(preview.mutex);
			++preview.generation;
			preview.active = false;
			preview.matchId.clear();
			preview.completedMatchId.clear();
		}
		JoinInProgressConnectionSoundPending() = false;
		Scheduler::Once([]
		{
			Dvar::Var("zwnet_join_preview_active").set(false);
			Dvar::Var("zwnet_join_preview_seconds").set(0);
		}, Scheduler::Pipeline::MAIN);
	}

	void ZWNet::BeginJoinInProgressPreview(const nlohmann::json& status)
	{
		const auto matchId = JsonString(status, "match_id");
		if (!IsOpaqueMatchId(matchId)) return;
		std::uint64_t generation{};
		{
			auto& preview = JoinPreview();
			std::lock_guard lock(preview.mutex);
			if ((preview.active && preview.matchId == matchId) || preview.completedMatchId == matchId) return;
			++preview.generation;
			generation = preview.generation;
			preview.active = true;
			preview.matchId = matchId;
			preview.completedMatchId.clear();
		}

		const auto map = JsonString(status, "map");
		auto mapName = std::string{};
		auto mapImage = std::string{};
		if (status.contains("selected_map") && status.at("selected_map").is_object())
		{
			mapName = JsonString(status.at("selected_map"), "name");
			mapImage = JsonString(status.at("selected_map"), "image");
		}
		Scheduler::Once([generation, matchId, map, mapName, mapImage]
		{
			if (!ActiveState()) return;
			{
				auto& preview = JoinPreview();
				std::lock_guard lock(preview.mutex);
				if (!preview.active || preview.generation != generation || preview.matchId != matchId) return;
			}
			Dvar::Var("ui_zwnet_state").set("JOIN_PREVIEW");
			Dvar::Var("ui_zwnet_state_text").set("JOINING GAME IN PROGRESS");
			Dvar::Var("zwnet_join_preview_active").set(true);
			Dvar::Var("zwnet_join_preview_seconds").set(3);
			Dvar::Var("zwnet_join_status").set("PREVIEWING ACTIVE MATCH");
			if (!map.empty())
			{
				Dvar::Var("ui_mapname").set(map);
				Dvar::Var("zwnet_vote_winner_id").set(map);
				Dvar::Var("zwnet_vote_winner_name").set(ResolveVoteMapDisplayName(map, mapName));
				Dvar::Var("zwnet_vote_winner_image").set(ResolveVoteMapImage(map, mapImage));
				Maps::SynchronizeMapDvars(map);
			}
			// This is the same confirmed roster-join alias used by the native
			// Private Match lobby. It fires only after the preview is visible.
			Command::Execute("snd_playLocal mp_player_join", false);
			Scheduler::Once([generation, matchId]
			{
				EnqueueAsync([generation, matchId]
				{
					{
						auto& preview = JoinPreview();
						std::lock_guard lock(preview.mutex);
						if (!preview.active || preview.generation != generation || preview.matchId != matchId) return;
					}
					const auto current = Request("GET", "/zwnet/matchmaking/status");
					const auto valid = current && current->is_object() && !current->contains("error") &&
						JsonString(*current, "match_id") == matchId && JsonString(*current, "state") == "CONNECTING" &&
						current->value("join_in_progress", false);
					if (!valid)
					{
						Request("POST", "/zwnet/matchmaking/disconnect", {{"match_id", matchId}});
						SearchingState() = false;
						CancelJoinInProgressPreview();
						SetState("ERROR", "ZWNET_SERVER_NOT_READY");
						return;
					}
					{
						auto& preview = JoinPreview();
						std::lock_guard lock(preview.mutex);
						if (!preview.active || preview.generation != generation || preview.matchId != matchId) return;
						preview.active = false;
						preview.completedMatchId = matchId;
					}
					Scheduler::Once([]
					{
						Dvar::Var("zwnet_join_preview_active").set(false);
						Dvar::Var("zwnet_join_preview_seconds").set(0);
					}, Scheduler::Pipeline::MAIN);
					ConnectMatch(matchId, false);
				});
			}, Scheduler::Pipeline::MAIN, 2500ms);
		}, Scheduler::Pipeline::MAIN);
	}

	void ZWNet::ConnectMatch(const std::string& matchId, const bool relay, const bool reconnect)
	{
		if (!relay && RelayAlreadyUsedForMatch(matchId))
		{
			ManagedReconnectInFlight() = false;
			Auth::ClearManagedConnectTicket();
			++JoinTransitionGeneration();
			ServerJoinTransitionState() = false;
			SetState("ERROR", "ZWNET_ROUTE_UNAVAILABLE");
			return;
		}
		CancelRelayHandshake();
		Auth::ClearManagedConnectTicket();
		std::string playerId;
		{
			std::lock_guard lock(StateMutex());
			playerId = CurrentPlayerIdState();
		}
		const auto fail = [relay, reconnect](const std::string& error)
		{
			SearchingState() = false;
			ManagedReconnectInFlight() = false;
			Auth::ClearManagedConnectTicket();
			++JoinTransitionGeneration();
			ServerJoinTransitionState() = false;
			JoinInProgressConnectionSoundPending() = false;
			if (relay || reconnect)
			{
				Scheduler::Once([] { Command::Execute("closemenu popup_reconnectingtoparty", false); },
					Scheduler::Pipeline::MAIN);
			}
			SetState("ERROR", error);
		};
		SetState(relay ? "RELAY_CONNECTION" : "DIRECT_CONNECTION");
		auto reconnectBody = nlohmann::json{{"match_id", matchId}};
		if (relay) reconnectBody["relay"] = true;
		const auto descriptor = reconnect ?
			Request("POST", "/zwnet/matchmaking/reconnect", reconnectBody) :
			Request("GET", "/zwnet/connect/" + matchId + (relay ? "?relay=1" : ""));
		if (!descriptor)
		{
			fail("ZWNET_REQUEST_FAILED");
			return;
		}
		if (descriptor->contains("error"))
		{
			const auto code = ResponseErrorCode(*descriptor);
			Scheduler::Once([]
			{
				if (!ActiveState()) return;
				Dvar::Var("zwnet_server_status").set("SERVER UNAVAILABLE");
				Dvar::Var("zwnet_join_status").set("RETURN TO LOBBY");
			}, Scheduler::Pipeline::MAIN);
			fail(code == "SERVER_NOT_READY" ? "ZWNET_SERVER_NOT_READY" : "ZWNET_ROUTE_UNAVAILABLE");
			return;
		}
		const auto key = relay ? "relay_endpoint" : "direct_endpoint";
		const auto endpoint = JsonString(*descriptor, key);
		if (endpoint.empty()) { fail("ZWNET_DESCRIPTOR_INVALID"); return; }
		if (JsonString(*descriptor, "match_id") != matchId ||
			JsonString(*descriptor, "route_type") != (relay ? "RELAY" : "DIRECT"))
		{
			fail("ZWNET_DESCRIPTOR_INVALID");
			return;
		}
		const Network::Address authorizedTarget(endpoint);
		const auto sessionId = JsonString(*descriptor, "session_id");
		const auto serverIdentity = JsonString(*descriptor, "server_identity");
		const auto instanceId = JsonString(*descriptor, "instance_id");
		if (serverIdentity.empty() || !IsRelayOpaque(instanceId))
		{
			fail("ZWNET_DESCRIPTOR_INVALID");
			return;
		}
		{
			std::lock_guard lock(StateMutex());
			if (CurrentPlayerIdState() != playerId || CurrentMatchIdState() != matchId)
			{
				fail("ZWNET_SESSION_EXPIRED");
				return;
			}
		}
		if (relay || reconnect)
		{
			auto& route = RouteAttempt();
			std::lock_guard lock(route.mutex);
			if (route.matchId == matchId && !route.sessionId.empty() &&
				(route.sessionId != sessionId || route.playerId != playerId ||
					route.serverIdentity != serverIdentity || route.instanceId != instanceId ||
					(reconnect && !relay && route.routeIsRelay == relay &&
						route.assignedTarget != authorizedTarget) ||
					(reconnect && route.routeIsRelay != relay &&
						!(relay && route.relayUsed))))
			{
				fail("ZWNET_DESCRIPTOR_INVALID");
				return;
			}
			if (reconnect && (route.matchId != matchId || route.sessionId.empty()))
			{
				fail("ZWNET_DESCRIPTOR_INVALID");
				return;
			}
		}
		if (!authorizedTarget.isValid() ||
			!Auth::SetManagedConnectTicket(authorizedTarget,
				JsonString(*descriptor, "connect_ticket"), matchId,
				sessionId))
		{
			fail("ZWNET_DESCRIPTOR_INVALID");
			return;
		}
		// The ticket is carried in Auth::Connect's binary packet, never a command line or URL.
		SearchingState() = false;
		if (relay)
		{
			const auto relayTicket = JsonString(*descriptor, "relay_ticket");
			if (!IsOpaqueMatchId(matchId) || !IsRelayOpaque(playerId) ||
				!IsRelayOpaque(sessionId) || !IsRelayOpaque(relayTicket))
			{
				fail("ZWNET_DESCRIPTOR_INVALID");
				return;
			}
			const auto nonce = std::format("{:016x}{:016x}",
				Utils::Cryptography::Rand::GenerateLong(),
				Utils::Cryptography::Rand::GenerateLong());
			auto hello = nlohmann::json{{"schema_version", 1},
				{"relay_ticket", relayTicket}, {"match_id", matchId},
				{"player_id", playerId}, {"session_id", sessionId},
				{"nonce", nonce}}.dump();
			if (hello.size() + sizeof("zwnetRelayHello") + 4 > 1400)
			{
				fail("ZWNET_DESCRIPTOR_INVALID");
				return;
			}
			{
				auto& route = RouteAttempt();
				std::lock_guard lock(route.mutex);
				if (route.matchId != matchId) route.matchId = matchId;
				route.playerId = playerId;
				route.sessionId = sessionId;
				route.serverIdentity = serverIdentity;
				route.instanceId = instanceId;
				route.assignedTarget = authorizedTarget;
				route.routeIsRelay = true;
				route.reconnectAttempt = reconnect;
				route.directWaiting = false;
				route.relayUsed = true;
			}
			++JoinTransitionGeneration();
			ServerJoinTransitionState() = true;
			Scheduler::Once([target = authorizedTarget, matchId, playerId, nonce, hello]() mutable
			{
				if (!ActiveState()) return;
				{
					std::lock_guard lock(StateMutex());
					if (CurrentPlayerIdState() != playerId || CurrentMatchIdState() != matchId)
					{
						Auth::ClearManagedConnectTicket();
						ManagedReconnectInFlight() = false;
						ServerJoinTransitionState() = false;
						return;
					}
				}
				{
					auto& pending = RelayState();
					std::lock_guard lock(pending.mutex);
					++pending.generation;
					pending.pending = true;
					pending.ready = false;
					pending.target = target;
					pending.matchId = matchId;
					pending.playerId = playerId;
					pending.nonce = nonce;
					pending.hello = hello;
					pending.deadline = std::chrono::steady_clock::now() + 8s;
				}
				Dvar::Var("zwnet_join_status").set("CONTACTING RELAY");
				Network::SendCommand(target, "zwnetRelayHello", hello);
				std::ranges::fill(hello, '\0');
			}, Scheduler::Pipeline::MAIN);
			return;
		}
		{
			auto& route = RouteAttempt();
			std::lock_guard lock(route.mutex);
			if (route.matchId != matchId)
			{
				route.matchId = matchId;
				route.relayUsed = false;
			}
			route.playerId = playerId;
			route.sessionId = sessionId;
			route.serverIdentity = serverIdentity;
			route.instanceId = instanceId;
			route.directTarget = authorizedTarget;
			route.assignedTarget = authorizedTarget;
			route.routeIsRelay = false;
			route.reconnectAttempt = reconnect;
			route.directWaiting = true;
		}
		Scheduler::Once([endpoint, target = authorizedTarget, matchId, playerId]
		{
			if (!ActiveState()) return;
			{
				std::lock_guard lock(StateMutex());
				if (CurrentPlayerIdState() != playerId || CurrentMatchIdState() != matchId)
				{
					Auth::ClearManagedConnectTicket();
					ManagedReconnectInFlight() = false;
					++JoinTransitionGeneration();
					ServerJoinTransitionState() = false;
					return;
				}
			}
			if (!target.isValid())
			{
				Dvar::Var("ui_zwnet_state").set("ERROR");
				Dvar::Var("ui_zwnet_state_text").set("ERROR");
				Dvar::Var("ui_zwnet_error").set("ZWNET_DESCRIPTOR_INVALID");
				Dvar::Var("ui_zwnet_error_text").set(FriendlyErrorText("ZWNET_DESCRIPTOR_INVALID"));
				Dvar::Var("zwnet_join_status").set("SERVER ADDRESS INVALID");
				return;
			}
			Dvar::Var("ui_zwnet_state").set("CONNECTING");
			Dvar::Var("ui_zwnet_error").set("");
			Dvar::Var("zwnet_server_endpoint").set(endpoint);
			Dvar::Var("zwnet_server_status").set("SERVER ASSIGNED");
			Dvar::Var("zwnet_join_status").set("JOINING SERVER");
			Dvar::Var("zwnet_managed_session").set(true);
			MarkJoinInProgressConnectionStarted(matchId);
			const auto transitionGeneration = ++JoinTransitionGeneration();
			ServerJoinTransitionState() = true;
			Scheduler::Once([transitionGeneration]
			{
				if (JoinTransitionGeneration() == transitionGeneration)
					ServerJoinTransitionState() = false;
			}, Scheduler::Pipeline::MAIN, 12s);
			Party::Connect(target);
			ManagedReconnectInFlight() = false;
		}, Scheduler::Pipeline::MAIN);
	}

	void ZWNet::UpdateVoteDvars(const nlohmann::json& status)
	{
		if (!status.contains("map_vote")) return;
		const auto vote = status.at("map_vote");
		const auto choices = vote.value("choices", nlohmann::json::array());
		if (choices.size() != 3) return;
		const auto allReady = status.value("all_ready", false);
		{
			std::lock_guard lock(StateMutex());
			CurrentProposalIdState() = JsonString(vote, "proposal_id");
			CurrentMatchIdState() = JsonString(status, "match_id");
		}
		Scheduler::Once([vote, choices, allReady]
		{
			if (!ActiveState()) return;
			const auto selected = vote.contains("selected") && vote.at("selected").is_string() ? vote.at("selected").get<std::string>() : "";
			Dvar::Var("zwnet_vote_active").set(true);
			Dvar::Var("zwnet_all_ready").set(allReady);
			Dvar::Var("zwnet_start_phase").set("");
			Dvar::Var("zwnet_start_seconds").set(0);
			Dvar::Var("zwnet_vote_proposal_id").set(JsonString(vote, "proposal_id"));
			Dvar::Var("zwnet_vote_seconds").set(vote.value("seconds_remaining", 0));
			Dvar::Var("zwnet_vote_selection").set(selected);
			Dvar::Var("zwnet_vote_reveal_time").set(0);
			Dvar::Var("zwnet_vote_winner_id").set("");
			Dvar::Var("zwnet_vote_winner_name").set("");
			Dvar::Var("zwnet_vote_winner_image").set("");
			Dvar::Var("zwnet_server_status").set("WAITING FOR MAP VOTE");
			Dvar::Var("zwnet_join_status").set("VOTE IN PROGRESS");
			for (std::size_t i = 0; i < 2; ++i)
			{
				const auto prefix = std::format("zwnet_vote_map_{}", i == 0 ? "a" : "b");
				const auto mapId = JsonString(choices[i], "id");
				Dvar::Var(prefix + "_id").set(mapId);
				Dvar::Var(prefix + "_name").set(ResolveVoteMapDisplayName(mapId, JsonString(choices[i], "name")));
				Dvar::Var(prefix + "_image").set(ResolveVoteMapImage(mapId, JsonString(choices[i], "image")));
				Dvar::Var(prefix + "_votes").set(choices[i].value("votes", 0));
			}
			Dvar::Var("zwnet_vote_random_votes").set(choices[2].value("votes", 0));
		}, Scheduler::Pipeline::MAIN);
	}

	void ZWNet::UpdateMatchmaking()
	{
		if (!ActiveState() || !SearchingState()) return;
		auto party = Request("GET", "/zwnet/parties/current");
		if (party && party->is_object() && !party->contains("error"))
		{
			*party = PublishLocalRank(std::move(*party));
		}
		const auto status = Request("GET", "/zwnet/matchmaking/status");
		if (!status)
		{
			if (party && !party->is_null() && !party->contains("error")) UpdateLobbyDvars(*party);
			Logger::Print("ZWNET matchmaking status poll failed; current session preserved\n");
			return;
		}
		if (status->contains("error"))
		{
			if (party && !party->is_null() && !party->contains("error")) UpdateLobbyDvars(*party);
			return;
		}
		if (party && !party->is_null() && !party->contains("error")) UpdateLobbyDvars(*party);
		if (status->contains("lobby") && status->at("lobby").is_object()) UpdateMatchLobbyDvars(*status);
		const auto state = JsonString(*status, "state", "SEARCHING");
		const auto joinCountdown = status->value("join_countdown_seconds", 0);
		const auto allReady = status->value("all_ready", false);
		const auto startPhase = JsonString(*status, "start_phase");
		const auto startSeconds = status->value("start_seconds", joinCountdown);
		if (state == "ERROR" || state == "FAILED")
		{
			SearchingState() = false;
			SetState("ERROR", "ZWNET_MATCH_FAILED");
			return;
		}
		if (state == "IDLE" || state == "FINISHED" || state == "FAILED") SearchingState() = false;
		SetState(state == "CONNECTING" && joinCountdown > 0 ? "COUNTDOWN" : state);
		if (state == "MAP_VOTE") UpdateVoteDvars(*status);
		else
		{
			const auto map = JsonString(*status, "map");
			const auto matchId = JsonString(*status, "match_id");
			if (!matchId.empty())
			{
				std::lock_guard lock(StateMutex());
				CurrentMatchIdState() = matchId;
			}
			auto mapName = std::string{};
			auto mapImage = std::string{};
			if (status->contains("selected_map") && status->at("selected_map").is_object())
			{
				mapName = JsonString(status->at("selected_map"), "name");
				mapImage = JsonString(status->at("selected_map"), "image");
			}
			auto serverStatus = std::string{"NOT ASSIGNED"};
			auto joinStatus = std::string{"WAITING IN LOBBY"};
			if (state == "WAITING_FOR_READY") { serverStatus = "START LOCKED"; joinStatus = "WAITING FOR ALL PLAYERS"; }
			else if (state == "RESERVING_SERVER") { serverStatus = "ALLOCATING SERVER"; joinStatus = "MAP LOCKED"; }
			else if (state == "SERVER_STARTING") { serverStatus = "SERVER STARTING"; joinStatus = "WAITING FOR SERVER"; }
			else if (state == "RESETTING" || state == "POST_MATCH") { serverStatus = "MATCH COMPLETE"; joinStatus = "RETURNING TO LOBBY"; }
			else if (state == "CONNECTING")
			{
				serverStatus = "SERVER READY";
				joinStatus = joinCountdown > 0 ? std::format("JOINING IN {}", joinCountdown) : "JOIN AUTHORIZED";
			}
			Scheduler::Once([map, mapName, mapImage, matchId, serverStatus, joinStatus, joinCountdown, allReady, startPhase, startSeconds]
			{
				if (!ActiveState()) return;
				const auto wasVoting = Dvar::Var("zwnet_vote_active").get<bool>();
				Dvar::Var("zwnet_vote_active").set(false);
				Dvar::Var("zwnet_all_ready").set(allReady);
				Dvar::Var("zwnet_start_phase").set(startPhase);
				Dvar::Var("zwnet_start_seconds").set(startSeconds);
				Dvar::Var("zwnet_match_id").set(matchId);
				Dvar::Var("zwnet_join_countdown").set(joinCountdown);
				Dvar::Var("zwnet_server_status").set(serverStatus);
				Dvar::Var("zwnet_join_status").set(joinStatus);
				if (!map.empty())
				{
					const auto winnerDisplayName = ResolveVoteMapDisplayName(map, mapName);
					const auto winnerImage = ResolveVoteMapImage(map, mapImage);
					if (Dvar::Var("zwnet_vote_winner_id").get<std::string>() != map)
					{
						const auto mapA = Dvar::Var("zwnet_vote_map_a_id").get<std::string>();
						const auto mapB = Dvar::Var("zwnet_vote_map_b_id").get<std::string>();
						Dvar::Var("zwnet_vote_reveal_slot").set(map == mapA ? 0 : (map == mapB ? 1 : 2));
						Dvar::Var("zwnet_vote_reveal_time").set(wasVoting ? Game::Sys_Milliseconds() : 0);
					}
					Dvar::Var("ui_mapname").set(map);
					Dvar::Var("zwnet_vote_winner_id").set(map);
					Dvar::Var("zwnet_vote_winner_name").set(winnerDisplayName);
					Dvar::Var("zwnet_vote_winner_image").set(winnerImage);
					Maps::SynchronizeMapDvars(map);
				}
			}, Scheduler::Pipeline::MAIN);
		}
		if (status->contains("match_id") && state == "CONNECTING" && joinCountdown <= 0 &&
			!InGameState() && !ServerJoinTransitionState() && !EndpointJoinInFlight())
		{
			const auto matchId = status->at("match_id").get<std::string>();
			if (status->value("join_in_progress", false)) BeginJoinInProgressPreview(*status);
			else if (!RelayAlreadyUsedForMatch(matchId)) ConnectMatch(matchId, false);
		}
	}

	void ZWNet::RefreshNetworkMetrics()
	{
		if (!ActiveState() || !NetworkMetricsEnabled()) return;
		static auto lastSuccess = std::chrono::steady_clock::time_point{};
		const auto response = Request("GET", "/api/status");
		if (response && response->is_object() && !response->contains("error") &&
			response->contains("online_players") && response->at("online_players").is_number_integer() &&
			response->contains("running_games") && response->at("running_games").is_number_integer())
		{
			const auto online = std::max(0, response->at("online_players").get<int>());
			const auto running = std::max(0, response->at("running_games").get<int>());
			lastSuccess = std::chrono::steady_clock::now();
			Scheduler::Once([online, running]
			{
				if (!ActiveState() || !NetworkMetricsEnabled()) return;
				Dvar::Var("zwnet_online_players_known").set(true);
				Dvar::Var("zwnet_online_players_text").set(std::format("{} {} ONLINE", online, online == 1 ? "PLAYER" : "PLAYERS"));
				Dvar::Var("zwnet_running_games_known").set(true);
				Dvar::Var("zwnet_running_games_text").set(std::format("{} RUNNING {}", running, running == 1 ? "GAME" : "GAMES"));
			}, Scheduler::Pipeline::MAIN);
			return;
		}

		if (lastSuccess.time_since_epoch().count() != 0 &&
			std::chrono::steady_clock::now() - lastSuccess <= 30s) return;
		Scheduler::Once([]
		{
			if (!NetworkMetricsEnabled()) return;
			Dvar::Var("zwnet_online_players_known").set(false);
			Dvar::Var("zwnet_online_players_text").set("PLAYERS ONLINE UNAVAILABLE");
			Dvar::Var("zwnet_running_games_known").set(false);
			Dvar::Var("zwnet_running_games_text").set("RUNNING GAMES UNAVAILABLE");
		}, Scheduler::Pipeline::MAIN);
	}

	void ZWNet::InitializeDvars()
	{
		Dvar::Register<const char*>("ui_zwnet_state", "OFFLINE", Game::DVAR_NONE, "Localized ZWNET state key");
		Dvar::Register<const char*>("ui_zwnet_state_text", "OFFLINE", Game::DVAR_NONE, "Readable ZWNET state text");
		Dvar::Register<const char*>("ui_zwnet_error", "", Game::DVAR_NONE, "Stable ZWNET error key");
		Dvar::Register<const char*>("ui_zwnet_error_text", "", Game::DVAR_NONE, "Readable ZWNET error text");
		Dvar::Register<const char*>("ui_zwnet_guid", PublicGuidText(), Game::DVAR_ROM, "Public ZW3 GUID used for Stats account linking");
		Dvar::Register<bool>("zwnet_managed_session", false, Game::DVAR_NONE, "Active ZWNET matchmaking session");
		Dvar::Register<const char*>("zwnet_catalog_status", "LOADING", Game::DVAR_NONE, "Client-safe playlist catalog state");
		Dvar::Register<bool>("zwnet_catalog_notice", false, Game::DVAR_NONE, "A relevant playlist update is available");
		Dvar::Register<bool>("zwnet_catalog_can_select", false, Game::DVAR_NONE, "Local party leader can change playlist selection");
		Dvar::Register<bool>("zwnet_catalog_can_search", false, Game::DVAR_NONE, "Selected playlist can be searched");
		Dvar::Register<int>("zwnet_catalog_page", 1, 1, 100, Game::DVAR_NONE, "Playlist page");
		Dvar::Register<int>("zwnet_catalog_pages", 1, 1, 100, Game::DVAR_NONE, "Playlist page count");
		Dvar::Register<const char*>("zwnet_catalog_selected_name", "NO AVAILABLE PLAYLIST", Game::DVAR_NONE, "Selected playlist title");
		Dvar::Register<const char*>("zwnet_catalog_selected_description", "", Game::DVAR_NONE, "Selected playlist description");
		Dvar::Register<const char*>("zwnet_catalog_selected_audience", "", Game::DVAR_NONE, "Selected playlist audience");
		Dvar::Register<const char*>("zwnet_catalog_selected_availability", "NO_SELECTION", Game::DVAR_NONE, "Selected playlist availability code");
		Dvar::Register<const char*>("zwnet_catalog_selected_status", "Choose a playlist to continue.", Game::DVAR_NONE, "Selected playlist availability detail");
		Dvar::Register<int>("zwnet_catalog_selected_revision", 0, 0, INT_MAX, Game::DVAR_NONE, "Selected playlist revision");
		Dvar::Register<const char*>("zwnet_catalog_selected_rotation", "No map rotation is available.", Game::DVAR_NONE, "Selected playlist rotation summary");
		Dvar::Register<const char*>("zwnet_catalog_selected_image", "", Game::DVAR_NONE, "Selected playlist preview material");
		Dvar::Register<const char*>("zwnet_catalog_selected_players", "", Game::DVAR_NONE, "Selected playlist player range");
		Dvar::Register<const char*>("zwnet_catalog_selected_zombie_settings", "", Game::DVAR_NONE, "Selected playlist zombie settings");
		Dvar::Register<const char*>("zwnet_catalog_action_error", "", Game::DVAR_NONE, "Playlist activation feedback");
		Dvar::Register<bool>("zwnet_catalog_busy", false, Game::DVAR_NONE, "Playlist activation is in flight");
		Dvar::Register<const char*>("zwnet_search_playlist_id", "", Game::DVAR_NONE, "Authoritative matchmaking playlist ID");
		Dvar::Register<const char*>("zwnet_search_playlist_name", "", Game::DVAR_NONE, "Authoritative matchmaking playlist name");
		Dvar::Register<int>("zwnet_search_playlist_revision", 0, 0, INT_MAX, Game::DVAR_NONE, "Authoritative matchmaking playlist revision");
		Dvar::Register<const char*>("zwnet_search_playlist_settings", "", Game::DVAR_NONE, "Authoritative matchmaking zombie settings");
		constexpr std::array slotVisible{"zwnet_catalog_slot_0_visible", "zwnet_catalog_slot_1_visible", "zwnet_catalog_slot_2_visible", "zwnet_catalog_slot_3_visible", "zwnet_catalog_slot_4_visible"};
		constexpr std::array slotSelected{"zwnet_catalog_slot_0_selected", "zwnet_catalog_slot_1_selected", "zwnet_catalog_slot_2_selected", "zwnet_catalog_slot_3_selected", "zwnet_catalog_slot_4_selected"};
		constexpr std::array slotName{"zwnet_catalog_slot_0_name", "zwnet_catalog_slot_1_name", "zwnet_catalog_slot_2_name", "zwnet_catalog_slot_3_name", "zwnet_catalog_slot_4_name"};
		constexpr std::array slotDescription{"zwnet_catalog_slot_0_description", "zwnet_catalog_slot_1_description", "zwnet_catalog_slot_2_description", "zwnet_catalog_slot_3_description", "zwnet_catalog_slot_4_description"};
		constexpr std::array slotPreview{"zwnet_catalog_slot_0_preview", "zwnet_catalog_slot_1_preview", "zwnet_catalog_slot_2_preview", "zwnet_catalog_slot_3_preview", "zwnet_catalog_slot_4_preview"};
		constexpr std::array slotImage{"zwnet_catalog_slot_0_image", "zwnet_catalog_slot_1_image", "zwnet_catalog_slot_2_image", "zwnet_catalog_slot_3_image", "zwnet_catalog_slot_4_image"};
		constexpr std::array slotAudience{"zwnet_catalog_slot_0_audience", "zwnet_catalog_slot_1_audience", "zwnet_catalog_slot_2_audience", "zwnet_catalog_slot_3_audience", "zwnet_catalog_slot_4_audience"};
		constexpr std::array slotAvailability{"zwnet_catalog_slot_0_availability", "zwnet_catalog_slot_1_availability", "zwnet_catalog_slot_2_availability", "zwnet_catalog_slot_3_availability", "zwnet_catalog_slot_4_availability"};
		constexpr std::array slotStatus{"zwnet_catalog_slot_0_status", "zwnet_catalog_slot_1_status", "zwnet_catalog_slot_2_status", "zwnet_catalog_slot_3_status", "zwnet_catalog_slot_4_status"};
		for (std::size_t slot = 0; slot < ZWNET_PLAYLIST_PAGE_SIZE; ++slot)
		{
			Dvar::Register<bool>(slotVisible[slot], false, Game::DVAR_NONE, "Playlist slot is populated");
			Dvar::Register<bool>(slotSelected[slot], false, Game::DVAR_NONE, "Playlist slot is selected");
			Dvar::Register<const char*>(slotName[slot], "", Game::DVAR_NONE, "Playlist display name");
			Dvar::Register<const char*>(slotDescription[slot], "", Game::DVAR_NONE, "Playlist description");
			Dvar::Register<const char*>(slotPreview[slot], "", Game::DVAR_NONE, "Playlist map preview");
			Dvar::Register<const char*>(slotImage[slot], "", Game::DVAR_NONE, "Playlist local map material");
			Dvar::Register<const char*>(slotAudience[slot], "", Game::DVAR_NONE, "Playlist audience");
			Dvar::Register<const char*>(slotAvailability[slot], "", Game::DVAR_NONE, "Playlist availability");
			Dvar::Register<const char*>(slotStatus[slot], "", Game::DVAR_NONE, "Playlist availability detail");
		}
		Dvar::Register<bool>("zwnet_lobby_active", false, Game::DVAR_NONE, "ZWNET party lobby is active");
		Dvar::Register<const char*>("zwnet_lobby_party_id", "", Game::DVAR_NONE, "Current ZWNET party");
		Dvar::Register<const char*>("zwnet_lobby_visibility", "OPEN", Game::DVAR_NONE, "Current ZWNET party visibility");
		Dvar::Register<const char*>("zwnet_lobby_owner", "", Game::DVAR_NONE, "Current party owner");
		Dvar::Register<int>("zwnet_lobby_member_count", 0, 0, 4, Game::DVAR_NONE, "Current party size");
		Dvar::Register<const char*>("zwnet_lobby_status_text", "IDLE", Game::DVAR_NONE, "Current party state");
		Dvar::Register<bool>("zwnet_lobby_can_start", false, Game::DVAR_NONE, "Private match can start");
		Dvar::Register<bool>("zwnet_lobby_self_ready", false, Game::DVAR_NONE, "Local party ready state");
		Dvar::Register<bool>("zwnet_ready_pending", false, Game::DVAR_NONE, "A ready-state update is in flight");
		Dvar::Register<bool>("zwnet_all_ready", false, Game::DVAR_NONE, "All active match players are ready");
		Dvar::Register<const char*>("zwnet_start_phase", "", Game::DVAR_NONE, "Server start phase");
		Dvar::Register<int>("zwnet_start_seconds", 0, 0, 300, Game::DVAR_NONE, "Server start phase time remaining");
		Dvar::Register<bool>("zwnet_join_preview_active", false, Game::DVAR_NONE, "Join-in-progress preview is visible");
		Dvar::Register<int>("zwnet_join_preview_seconds", 0, 0, 3, Game::DVAR_NONE, "Join-in-progress preview duration");
		Dvar::Register<bool>("zwnet_online_players_known", false, Game::DVAR_NONE, "Online-player metric is authoritative and fresh");
		Dvar::Register<const char*>("zwnet_online_players_text", "PLAYERS ONLINE UNAVAILABLE", Game::DVAR_NONE, "Online-player metric label");
		Dvar::Register<bool>("zwnet_running_games_known", false, Game::DVAR_NONE, "Running-game metric is authoritative and fresh");
		Dvar::Register<const char*>("zwnet_running_games_text", "RUNNING GAMES UNAVAILABLE", Game::DVAR_NONE, "Running-game metric label");
		constexpr std::array memberNames
		{
			"zwnet_lobby_member_0_name", "zwnet_lobby_member_1_name", "zwnet_lobby_member_2_name", "zwnet_lobby_member_3_name"
		};
		constexpr std::array memberRoles
		{
			"zwnet_lobby_member_0_role", "zwnet_lobby_member_1_role", "zwnet_lobby_member_2_role", "zwnet_lobby_member_3_role"
		};
		constexpr std::array memberGuids
		{
			"zwnet_lobby_member_0_guid", "zwnet_lobby_member_1_guid", "zwnet_lobby_member_2_guid", "zwnet_lobby_member_3_guid"
		};
		constexpr std::array memberReady
		{
			"zwnet_lobby_member_0_ready", "zwnet_lobby_member_1_ready", "zwnet_lobby_member_2_ready", "zwnet_lobby_member_3_ready"
		};
		constexpr std::array memberSelf
		{
			"zwnet_lobby_member_0_self", "zwnet_lobby_member_1_self", "zwnet_lobby_member_2_self", "zwnet_lobby_member_3_self"
		};
		constexpr std::array memberSharedRankKnown
		{
			"zwnet_lobby_member_0_shared_rank_known", "zwnet_lobby_member_1_shared_rank_known", "zwnet_lobby_member_2_shared_rank_known", "zwnet_lobby_member_3_shared_rank_known"
		};
		constexpr std::array memberSharedRankLevels
		{
			"zwnet_lobby_member_0_shared_rank_level", "zwnet_lobby_member_1_shared_rank_level", "zwnet_lobby_member_2_shared_rank_level", "zwnet_lobby_member_3_shared_rank_level"
		};
		constexpr std::array memberSharedRankPrestiges
		{
			"zwnet_lobby_member_0_shared_rank_prestige", "zwnet_lobby_member_1_shared_rank_prestige", "zwnet_lobby_member_2_shared_rank_prestige", "zwnet_lobby_member_3_shared_rank_prestige"
		};
		constexpr std::array memberRankIcons
		{
			"zwnet_lobby_member_0_rank_icon", "zwnet_lobby_member_1_rank_icon", "zwnet_lobby_member_2_rank_icon", "zwnet_lobby_member_3_rank_icon"
		};
		constexpr std::array memberRankLevels
		{
			"zwnet_lobby_member_0_rank_level", "zwnet_lobby_member_1_rank_level", "zwnet_lobby_member_2_rank_level", "zwnet_lobby_member_3_rank_level"
		};
		for (std::size_t i = 0; i < memberNames.size(); ++i)
		{
			Dvar::Register<const char*>(memberNames[i], "", Game::DVAR_NONE, "Party member name");
			Dvar::Register<const char*>(memberGuids[i], "", Game::DVAR_NONE, "Public-lobby member GUID");
			Dvar::Register<const char*>(memberRoles[i], "", Game::DVAR_NONE, "Party member role");
			Dvar::Register<bool>(memberReady[i], false, Game::DVAR_NONE, "Party member ready state");
			Dvar::Register<bool>(memberSelf[i], false, Game::DVAR_NONE, "Local public-lobby member slot");
			Dvar::Register<bool>(memberSharedRankKnown[i], false, Game::DVAR_NONE, "Public-lobby member shared rank is available");
			Dvar::Register<int>(memberSharedRankLevels[i], 1, 1, 54, Game::DVAR_NONE, "Public-lobby member shared rank level");
			Dvar::Register<int>(memberSharedRankPrestiges[i], 0, 0, 255, Game::DVAR_NONE, "Public-lobby member shared rank prestige");
			Dvar::Register<const char*>(memberRankIcons[i], "", Game::DVAR_NONE, "Public lobby member rank icon");
			Dvar::Register<const char*>(memberRankLevels[i], "", Game::DVAR_NONE, "Public lobby member rank level");
		}
		Dvar::Register<bool>("zwnet_vote_active", false, Game::DVAR_NONE, "Map vote is active");
		Dvar::Register<const char*>("zwnet_vote_proposal_id", "", Game::DVAR_NONE, "Current map vote");
		Dvar::Register<int>("zwnet_vote_seconds", 0, 0, 60, Game::DVAR_NONE, "Map vote time remaining");
		Dvar::Register<const char*>("zwnet_vote_selection", "", Game::DVAR_NONE, "Local map vote selection");
		constexpr std::array voteIds{"zwnet_vote_map_a_id", "zwnet_vote_map_b_id"};
		constexpr std::array voteNames{"zwnet_vote_map_a_name", "zwnet_vote_map_b_name"};
		constexpr std::array voteImages{"zwnet_vote_map_a_image", "zwnet_vote_map_b_image"};
		constexpr std::array voteCounts{"zwnet_vote_map_a_votes", "zwnet_vote_map_b_votes"};
		for (std::size_t i = 0; i < voteIds.size(); ++i)
		{
			Dvar::Register<const char*>(voteIds[i], "", Game::DVAR_NONE, "Map vote internal id");
			Dvar::Register<const char*>(voteNames[i], "", Game::DVAR_NONE, "Map vote display name");
			Dvar::Register<const char*>(voteImages[i], "", Game::DVAR_NONE, "Map vote preview material");
			Dvar::Register<int>(voteCounts[i], 0, 0, 4, Game::DVAR_NONE, "Map vote count");
		}
		Dvar::Register<int>("zwnet_vote_random_votes", 0, 0, 4, Game::DVAR_NONE, "Random map vote count");
		Dvar::Register<int>("zwnet_vote_reveal_time", 0, 0, INT_MAX, Game::DVAR_NONE, "Map result reveal start time");
		Dvar::Register<int>("zwnet_vote_reveal_slot", 0, 0, 2, Game::DVAR_NONE, "Map result source card: A, B or random");
		Dvar::Register<const char*>("zwnet_vote_winner_id", "", Game::DVAR_NONE, "Winning ZW3 map id");
		Dvar::Register<const char*>("zwnet_vote_winner_name", "", Game::DVAR_NONE, "Winning ZW3 map name");
		Dvar::Register<const char*>("zwnet_vote_winner_image", "", Game::DVAR_NONE, "Winning ZW3 map preview");
		Dvar::Register<const char*>("zwnet_match_id", "", Game::DVAR_NONE, "Current ZW3 match id");
		Dvar::Register<bool>("zwnet_match_return", false, Game::DVAR_NONE, "Completed ZW3 match should return to its lobby");
		Dvar::Register<const char*>("zwnet_server_endpoint", "", Game::DVAR_NONE, "Assigned public ZW3 server endpoint");
		Dvar::Register<const char*>("zwnet_server_hostname", "", Game::DVAR_NONE, "Connected ZW3 server hostname");
		Dvar::Register<const char*>("zwnet_server_status", "NOT ASSIGNED", Game::DVAR_NONE, "ZW3 server assignment status");
		Dvar::Register<const char*>("zwnet_join_status", "WAITING IN LOBBY", Game::DVAR_NONE, "ZW3 join status");
		Dvar::Register<int>("zwnet_join_countdown", 0, 0, 30, Game::DVAR_NONE, "Synchronized ZW3 join countdown");
		Dvar::Register<const char*>("zwnet_selected_player_guid", "", Game::DVAR_NONE, "Selected public-lobby player GUID");
		Dvar::Register<const char*>("zwnet_selected_player_name", "", Game::DVAR_NONE, "Selected public-lobby player name");
		Dvar::Register<const char*>("zwnet_selected_player_role", "", Game::DVAR_NONE, "Selected public-lobby player role");
		Dvar::Register<int>("zwnet_selected_player_rank", 1, 1, 54, Game::DVAR_NONE, "Selected public-lobby player rank");
		Dvar::Register<int>("zwnet_selected_player_prestige", 0, 0, 255, Game::DVAR_NONE, "Selected public-lobby player prestige");
		Dvar::Register<const char*>("zwnet_selected_player_rank_icon", "prestige_1", Game::DVAR_NONE, "Selected public-lobby player prestige icon");
		Dvar::Register<bool>("zwnet_selected_player_self", false, Game::DVAR_NONE, "Selected public-lobby player is local");
		Dvar::Register<const char*>("zwnet_selected_player_relationship", "UNAVAILABLE", Game::DVAR_NONE, "Selected public-lobby player friend state");
		Dvar::Register<bool>("zwnet_barracks_compare_active", false, Game::DVAR_NONE, "Barracks was opened for a lobby comparison");
		Dvar::Register<bool>("zw3_barracks_rank_known", false, Game::DVAR_NONE, "Local ZW3 Barracks rank data is available");
		Dvar::Register<int>("zw3_barracks_rank_level", 1, 1, 54, Game::DVAR_NONE, "Local ZW3 Barracks rank level");
		Dvar::Register<int>("zw3_barracks_rank_prestige", 0, 0, 255, Game::DVAR_NONE, "Local ZW3 Barracks prestige");
		Dvar::Register<int>("zw3_barracks_rank_experience", 0, 0, std::numeric_limits<int>::max(), Game::DVAR_NONE, "Local ZW3 Barracks experience");
		Dvar::Register<int>("zw3_barracks_rank_experience_target", 50, 1, std::numeric_limits<int>::max(), Game::DVAR_NONE, "Current ZW3 Barracks level XP target");
		Dvar::Register<int>("zw3_barracks_rank_experience_percent", 0, 0, 100, Game::DVAR_NONE, "Current ZW3 Barracks level XP completion percent");
		Dvar::Register<const char*>("zw3_barracks_rank_icon", "prestige_1", Game::DVAR_NONE, "Local ZW3 Barracks prestige icon");
		Dvar::Register<int>("zw3_barracks_zombie_kills", 0, 0, std::numeric_limits<int>::max(), Game::DVAR_NONE, "Lifetime ZW3 zombie kills");
		Dvar::Register<int>("zw3_barracks_zombie_deaths", 0, 0, std::numeric_limits<int>::max(), Game::DVAR_NONE, "Lifetime ZW3 zombie deaths");
		Dvar::Register<int>("zw3_barracks_zombie_revives", 0, 0, std::numeric_limits<int>::max(), Game::DVAR_NONE, "Lifetime ZW3 teammate revives");
		for (std::size_t slot = 0; slot < ChallengeSlotProgressDvars.size(); ++slot)
		{
			Dvar::Register<int>(ChallengeSlotProgressDvars[slot], 0, 0, std::numeric_limits<int>::max(), Game::DVAR_NONE, "Current ZW3 challenge progress");
			Dvar::Register<int>(ChallengeSlotTargetDvars[slot], 0, 0, std::numeric_limits<int>::max(), Game::DVAR_NONE, "Current ZW3 challenge target");
			Dvar::Register<int>(ChallengeSlotTierDvars[slot], 0, 0, 4, Game::DVAR_NONE, "Completed ZW3 challenge tiers");
			Dvar::Register<int>(ChallengeSlotTierCountDvars[slot], 0, 0, 4, Game::DVAR_NONE, "Available ZW3 challenge tiers");
			Dvar::Register<int>(ChallengeSlotRewardDvars[slot], 0, 0, std::numeric_limits<int>::max(), Game::DVAR_NONE, "Next ZW3 challenge XP reward");
			Dvar::Register<int>(ChallengeSlotPercentDvars[slot], 0, 0, 100, Game::DVAR_NONE, "Current ZW3 challenge completion percent");
			Dvar::Register<bool>(ChallengeSlotCompleteDvars[slot], false, Game::DVAR_NONE, "ZW3 challenge is complete");
		}
		constexpr std::array questChallengeDvars
		{
			"zw3_quest_mp_factory_sh", "zw3_quest_mp_asylum_sh", "zw3_quest_mp_prototype_sh",
			"zw3_quest_mp_sumpf_sh", "zw3_quest_mp_za_island", "zw3_quest_mp_deathmarch_chap4_a",
			"zw3_quest_mp_deathmarch_chap4_b", "zw3_quest_mp_surv_town", "zw3_quest_mp_burg",
			"zw3_quest_mp_lambeth"
		};
		for (const auto* challengeDvar : questChallengeDvars)
		{
			Dvar::Register<int>(challengeDvar, 0, 0, 10, Game::DVAR_ARCHIVE,
				"Completed tiers for a repeatable ZW3 questline");
		}
		{
			std::lock_guard lock(StateMutex());
			// Reading an engine-owned string Dvar here also establishes that the
			// Dvar critical sections are usable before the async login is released.
			const auto localPlayerName = Dvar::Var("name").get<std::string>();
			(void)localPlayerName;
			// Construct all function-local state on the main game thread before the
			// asynchronous worker can access the CRT-backed string objects.
			(void)AccessTokenState();
			(void)RefreshTokenState();
			(void)CurrentPartyIdState();
			(void)CurrentPlayerIdState();
			(void)CurrentProposalIdState();
			(void)CurrentMatchIdState();
		}
		ActiveState() = true;
		RefreshBarracksProfile();
	}

	ZWNet::ZWNet()
	{
		if (Dedicated::IsEnabled() || ZoneBuilder::IsEnabled()) return;
		PatchMaterialEnumerationScratch();
		Localization::Set("ZWNET_LOGIN_REQUIRED", "Sign in on the ZW3 Stats page.");
		Localization::Set("ZWNET_SESSION_EXPIRED", "Your ZW3 session expired. Please sign in again.");
		Localization::Set("ZWNET_SEARCH_FAILED", "Matchmaking could not be started.");
		Localization::Set("ZWNET_CATALOG_UNAVAILABLE", "Playlists are unavailable. Please refresh the list.");
		Localization::Set("ZWNET_PLAYLIST_REFRESH_REQUIRED", "This playlist changed or access ended. Refresh the list and choose again.");
		Localization::Set("ZWNET_PLAYLIST_SELECTION_FAILED", "The party playlist could not be changed. Refresh and try again.");
		Localization::Set("ZWNET_ROUTE_UNAVAILABLE", "No direct or relay route is available.");
		Localization::Set("ZWNET_RELAY_TIMEOUT", "The relay did not confirm the connection. Return to the lobby and try again.");
		Localization::Set("ZWNET_SERVER_NOT_READY", "The assigned ZW3 server is no longer available.");
		Localization::Set("ZWNET_DESCRIPTOR_INVALID", "The connection response was invalid.");
		Localization::Set("ZWNET_ACCOUNT_LINK_REQUIRED", "Link this GUID in ZW3 Stats Settings.");
		Localization::Set("ZWNET_SESSION_STORAGE_FAILED", "The secure ZW3 session could not be stored.");
		Localization::Set("ZWNET_REGISTRATION_UNAVAILABLE", "The ZW3 registration page is unavailable.");
		Localization::Set("ZWNET_PARTY_FAILED", "The party could not be created or loaded.");
		Localization::Set("ZWNET_PRIVATE_MATCH_FAILED", "The private match server could not be reserved.");
		Localization::Set("ZWNET_MAP_VOTE_FAILED", "Your map vote could not be submitted.");
		Localization::Set("ZWNET_REQUEST_FAILED", "The ZW3 online service did not respond safely.");
		Localization::Set("ZWNET_GUID_COPY_FAILED", "The ZW3 GUID could not be copied to the clipboard.");
		Localization::Set("ZWNET_MANUAL_JOIN_DENIED", "Manual test access is unavailable. Check your invitation and sign in again.");
		Network::OnClientPacket("zwnetRelayReady", [](Network::Address& address, const std::string& data)
		{
			if (data.size() > 256) return;
			const auto ready = nlohmann::json::parse(data, nullptr, false);
			if (!ready.is_object() || ready.size() != 2 ||
				!ready.contains("schema_version") || !ready.at("schema_version").is_number_integer() ||
				ready.at("schema_version").get<int>() != 1 ||
				!ready.contains("nonce") || !ready.at("nonce").is_string()) return;
			const auto nonce = ready.at("nonce").get<std::string>();
			if (nonce.size() != 32 || !std::ranges::all_of(nonce, [](const unsigned char character)
				{ return character >= '0' && character <= '9' || character >= 'a' && character <= 'f'; })) return;
			std::uint64_t generation = 0;
			Network::Address target;
			std::string matchId;
			std::string playerId;
			{
				auto& pending = RelayState();
				std::lock_guard lock(pending.mutex);
				if (!pending.pending || std::chrono::steady_clock::now() >= pending.deadline ||
					address != pending.target || nonce != pending.nonce) return;
				pending.pending = false;
				pending.ready = true;
				std::ranges::fill(pending.hello, '\0');
				pending.hello.clear();
				generation = pending.generation;
				target = pending.target;
				matchId = pending.matchId;
				playerId = pending.playerId;
			}
			Scheduler::Once([generation, target, matchId, playerId]
			{
				if (!ActiveState()) return;
				{
					auto& pending = RelayState();
					std::lock_guard lock(pending.mutex);
					if (pending.generation != generation || !pending.ready) return;
					pending.ready = false;
					pending.nonce.clear();
					pending.matchId.clear();
					pending.playerId.clear();
				}
				{
					std::lock_guard lock(StateMutex());
					if (CurrentPlayerIdState() != playerId || CurrentMatchIdState() != matchId)
					{
						Auth::ClearManagedConnectTicket();
						ManagedReconnectInFlight() = false;
						++JoinTransitionGeneration();
						ServerJoinTransitionState() = false;
						return;
					}
				}
				Dvar::Var("ui_zwnet_state").set("CONNECTING");
				Dvar::Var("ui_zwnet_error").set("");
				Dvar::Var("zwnet_server_endpoint").set(target.getString());
				Dvar::Var("zwnet_server_status").set("RELAY READY");
				Dvar::Var("zwnet_join_status").set("JOINING SERVER");
				Dvar::Var("zwnet_managed_session").set(true);
				MarkJoinInProgressConnectionStarted(matchId);
				const auto transitionGeneration = ++JoinTransitionGeneration();
				ServerJoinTransitionState() = true;
				Scheduler::Once([transitionGeneration]
				{
					if (JoinTransitionGeneration() == transitionGeneration)
						ServerJoinTransitionState() = false;
				},
					Scheduler::Pipeline::MAIN, 12s);
				Party::Connect(target);
				ManagedReconnectInFlight() = false;
			}, Scheduler::Pipeline::MAIN);
		});
		Scheduler::Loop([]
		{
			if (!ActiveState()) return;
			Network::Address target;
			std::string hello;
			bool timedOut = false;
			{
				auto& pending = RelayState();
				std::lock_guard lock(pending.mutex);
				if (!pending.pending) return;
				if (std::chrono::steady_clock::now() >= pending.deadline)
				{
					++pending.generation;
					pending.pending = false;
					std::ranges::fill(pending.hello, '\0');
					pending.hello.clear();
					pending.nonce.clear();
					pending.matchId.clear();
					pending.playerId.clear();
					timedOut = true;
				}
				else
				{
					target = pending.target;
					hello = pending.hello;
				}
			}
			if (timedOut)
			{
				Auth::ClearManagedConnectTicket();
				ManagedReconnectInFlight() = false;
				++JoinTransitionGeneration();
				ServerJoinTransitionState() = false;
				Command::Execute("closemenu popup_reconnectingtoparty", false);
				SetState("ERROR", "ZWNET_RELAY_TIMEOUT");
				return;
			}
			Network::SendCommand(target, "zwnetRelayHello", hello);
			std::ranges::fill(hello, '\0');
		}, Scheduler::Pipeline::MAIN, 1s);
		Command::Add("zwnet_login", [] { Login(); });
		Command::Add("zwnet_register", [] { EnqueueAsync([] { Register(); }); });
		Command::Add("zwnet_quickplay", []
		{
			CapturePartyPrivacy();
			Scheduler::Once([]
			{
				if (ActiveState() && !SearchingState() && !InGameState())
					Command::Execute("openmenu popup_zwnet_playlists", false);
			}, Scheduler::Pipeline::MAIN);
		});
		Command::Add("zwnet_manual_join", [](const Command::Params* params)
		{
			if (params->size() != 2 || !IsOpaqueMatchId(params->get(1)))
			{
				Logger::Print("Usage: zwnet_manual_join <manual match ID>\n");
				return;
			}
			const std::string matchId = params->get(1);
			EnqueueAsync([matchId]
			{
				const auto admitted = Request("POST", "/zwnet/manual/join", {{"match_id", matchId}});
				if (!admitted || admitted->contains("error") || JsonString(*admitted, "match_id") != matchId)
				{
					SetState("ERROR", "ZWNET_MANUAL_JOIN_DENIED");
					return;
				}
				{
					std::lock_guard lock(StateMutex());
					CurrentMatchIdState() = matchId;
				}
				ConnectMatch(matchId, false);
			});
		});
		Command::Add("zwnet_cancel", [] { EnqueueAsync([] { CancelSearch(); }); });
		Command::Add("zwnet_terminal_disconnect", []
		{
			TerminalDisconnectRequested() = true;
			Scheduler::Once([] { TerminalDisconnectRequested() = false; },
				Scheduler::Pipeline::MAIN, 5s);
		});
		Command::Add("zwnet_logout", []
		{
			EnqueueAsync([]
			{
				CloseOnlineSession(false, true);
				Request("POST", "/social/client/logout");
				if (!ActiveState()) return;
				ClearSession();
				SetState("OFFLINE");
			});
		});
		UIScript::Add("ZWNetQuickPlay", []([[maybe_unused]] const UIScript::Token&, [[maybe_unused]] const Game::uiInfo_s*) { Command::Execute("zwnet_quickplay", false); });
		UIScript::Add("ZWNET_BeginPlaylistSelection", []([[maybe_unused]] const UIScript::Token&, [[maybe_unused]] const Game::uiInfo_s*)
		{
			BeginPlaylistSelection();
		});
		UIScript::Add("ZWNET_CancelPlaylistSelection", []([[maybe_unused]] const UIScript::Token&, [[maybe_unused]] const Game::uiInfo_s*)
		{
			CancelPlaylistSelection();
		});
		UIScript::Add("ZWNET_RefreshPlaylists", []([[maybe_unused]] const UIScript::Token&, [[maybe_unused]] const Game::uiInfo_s*)
		{
			EnqueueAsync([] { RefreshPlaylistCatalog(true); });
		});
		UIScript::Add("ZWNET_AcknowledgePlaylists", []([[maybe_unused]] const UIScript::Token&, [[maybe_unused]] const Game::uiInfo_s*)
		{
			AcknowledgePlaylistNotice();
		});
		UIScript::Add("ZWNET_HighlightPlaylist", [](const UIScript::Token& token, [[maybe_unused]] const Game::uiInfo_s*)
		{
			HighlightPlaylistSlot(token.get<int>());
		});
		UIScript::Add("ZWNET_ActivatePlaylist", [](const UIScript::Token& token, [[maybe_unused]] const Game::uiInfo_s*)
		{
			ActivatePlaylistSlot(token.get<int>());
		});
		UIScript::Add("ZWNET_PlaylistPage", [](const UIScript::Token& token, [[maybe_unused]] const Game::uiInfo_s*)
		{
			ChangePlaylistPage(token.get<int>());
		});
		UIScript::Add("ZWNetCancel", []([[maybe_unused]] const UIScript::Token&, [[maybe_unused]] const Game::uiInfo_s*) { Command::Execute("zwnet_cancel", false); });
		UIScript::Add("ZWNET_CancelMatchmaking", []([[maybe_unused]] const UIScript::Token&, [[maybe_unused]] const Game::uiInfo_s*) { EnqueueAsync([] { CancelMatchmaking(); }); });
		UIScript::Add("ZWNET_SetMatchmakingMenuActive", [](const UIScript::Token& token, [[maybe_unused]] const Game::uiInfo_s*)
		{
			const auto enabled = token.get<int>() != 0;
			NetworkMetricsEnabled() = enabled;
			if (enabled) EnqueueAsync([] { RefreshNetworkMetrics(); });
		});
		UIScript::Add("ZWNET_QuitToMatchmaking", []([[maybe_unused]] const UIScript::Token&, [[maybe_unused]] const Game::uiInfo_s*)
		{
			TerminalDisconnectRequested() = true;
			Command::Execute("disconnect", false);
		});
		UIScript::Add("ZWNET_CloseOnlineSession", []([[maybe_unused]] const UIScript::Token&, [[maybe_unused]] const Game::uiInfo_s*)
		{
			bool hasMatch = false;
			{
				std::lock_guard lock(StateMutex());
				hasMatch = !CurrentMatchIdState().empty();
			}
			const bool wasMatch = Dvar::Var("zwnet_managed_session").get<bool>() || InGameState() || hasMatch;
			EnqueueAsync([] { CloseOnlineSession(false, true); });
			if (wasMatch && Game::CL_IsCgameInitialized()) ScheduleReturnToIdleMatchmakingMenu();
		});
		UIScript::Add("ZWNetLogin", []([[maybe_unused]] const UIScript::Token&, [[maybe_unused]] const Game::uiInfo_s*) { Command::Execute("zwnet_login", false); });
		UIScript::Add("ZWNET_ConnectOnline", []([[maybe_unused]] const UIScript::Token&, [[maybe_unused]] const Game::uiInfo_s*) { BeginOnlineEntry(); });
		UIScript::Add("ZWNET_CancelOnlineEntry", []([[maybe_unused]] const UIScript::Token&, [[maybe_unused]] const Game::uiInfo_s*) { OnlineEntryPendingState() = false; });
		UIScript::Add("ZWNET_AbandonOnlineSession", []([[maybe_unused]] const UIScript::Token&, [[maybe_unused]] const Game::uiInfo_s*) { AbandonOnlineSession(); });
		UIScript::Add("ZWNetRegister", []([[maybe_unused]] const UIScript::Token&, [[maybe_unused]] const Game::uiInfo_s*) { Command::Execute("zwnet_register", false); });
		UIScript::Add("ZWNetCopyGuid", []([[maybe_unused]] const UIScript::Token&, [[maybe_unused]] const Game::uiInfo_s*)
		{
			if (!CopyPublicGuidToClipboard()) SetState("ERROR", "ZWNET_GUID_COPY_FAILED");
		});
		UIScript::Add("ZWNET_EnterPrivateLobby", []([[maybe_unused]] const UIScript::Token&, [[maybe_unused]] const Game::uiInfo_s*)
		{
			CapturePartyPrivacy();
			const auto map = Dvar::Var("ui_mapname").get<std::string>();
			EnqueueAsync([map] { EnterLobby(map); });
		});
		UIScript::Add("ZWNET_RefreshLobby", []([[maybe_unused]] const UIScript::Token&, [[maybe_unused]] const Game::uiInfo_s*) { EnqueueAsync([] { RefreshLobby(); }); });
		UIScript::Add("ZWNET_LeaveParty", []([[maybe_unused]] const UIScript::Token&, [[maybe_unused]] const Game::uiInfo_s*) { EnqueueAsync([] { LeaveParty(); }); });
		UIScript::Add("ZWNET_ToggleReady", []([[maybe_unused]] const UIScript::Token&, [[maybe_unused]] const Game::uiInfo_s*)
		{
			if (Dvar::Var("zwnet_ready_pending").get<bool>()) return;
			const auto ready = Dvar::Var("zwnet_lobby_self_ready").get<bool>();
			Dvar::Var("zwnet_ready_pending").set(true);
			EnqueueAsync([ready] { ToggleReady(ready); });
		});
		UIScript::Add("ZWNET_SelectLobbyPlayer", []([[maybe_unused]] const UIScript::Token& token, [[maybe_unused]] const Game::uiInfo_s*)
		{
			const auto index = token.get<int>();
			if (index < 0 || index >= 4) return;
			const auto prefix = std::format("zwnet_lobby_member_{}", index);
			auto guid = Dvar::Var(prefix + "_guid").get<std::string>();
			const auto name = Dvar::Var(prefix + "_name").get<std::string>();
			if (guid.empty() || name.empty()) return;
			std::ranges::transform(guid, guid.begin(), [](const unsigned char character)
			{
				return static_cast<char>(std::tolower(character));
			});
			Dvar::Var("zwnet_selected_player_guid").set(guid);
			Dvar::Var("zwnet_selected_player_name").set(name);
			Dvar::Var("zwnet_selected_player_role").set(Dvar::Var(prefix + "_role").get<std::string>());
			Dvar::Var("zwnet_selected_player_rank").set(Dvar::Var(prefix + "_shared_rank_level").get<int>());
			Dvar::Var("zwnet_selected_player_prestige").set(Dvar::Var(prefix + "_shared_rank_prestige").get<int>());
			Dvar::Var("zwnet_selected_player_rank_icon").set(Dvar::Var(prefix + "_rank_icon").get<std::string>());
			Dvar::Var("zwnet_selected_player_self").set(Dvar::Var(prefix + "_self").get<bool>());
			Dvar::Var("zwnet_selected_player_relationship").set(Friends::GetLobbyPlayerRelationship(guid));
		});
		UIScript::Add("ZWNET_RefreshBarracksProfile", []([[maybe_unused]] const UIScript::Token&, [[maybe_unused]] const Game::uiInfo_s*)
		{
			RefreshBarracksProfile();
		});
		UIScript::Add("ZWNET_RefreshChallengeCategory", [](const UIScript::Token& token, [[maybe_unused]] const Game::uiInfo_s*)
		{
			RefreshChallengeCategory(token.get<int>());
		});
		UIScript::Add("ZWNET_StartPrivateMatch", []([[maybe_unused]] const UIScript::Token&, [[maybe_unused]] const Game::uiInfo_s*)
		{
			const auto map = Dvar::Var("ui_mapname").get<std::string>();
			EnqueueAsync([map] { StartPrivateMatch(map); });
		});
		UIScript::Add("ZWNET_VoteMapA", []([[maybe_unused]] const UIScript::Token&, [[maybe_unused]] const Game::uiInfo_s*) { EnqueueAsync([] { VoteMap("A"); }); });
		UIScript::Add("ZWNET_VoteMapB", []([[maybe_unused]] const UIScript::Token&, [[maybe_unused]] const Game::uiInfo_s*) { EnqueueAsync([] { VoteMap("B"); }); });
		UIScript::Add("ZWNET_VoteRandom", []([[maybe_unused]] const UIScript::Token&, [[maybe_unused]] const Game::uiInfo_s*) { EnqueueAsync([] { VoteMap("RANDOM"); }); });
		Events::OnCLDisconnected([](const bool wasConnected)
		{
			InGameState() = false;
			Dvar::Var("zwnet_match_return").set(false);
			// CL_ConnectFromParty performs an internal CL_Disconnect while moving
			// from the ZWNET lobby into the assigned dedicated server. That planned
			// transition must not revoke the match and reset the server underneath
			// the in-flight connection.
			if (ServerJoinTransitionState().exchange(false))
			{
				Logger::Print("ZWNET server join transition: preserving online session\n");
				return;
			}
			JoinInProgressConnectionSoundPending() = false;
			bool hasMatch = false;
			{
				std::lock_guard lock(StateMutex());
				hasMatch = !CurrentMatchIdState().empty();
			}
			const auto isManaged = Dvar::Var("zwnet_managed_session").get<bool>();
			const auto wasMatchmaking = hasMatch || isManaged;
			const auto isPrivateMatchClient = wasConnected && !wasMatchmaking &&
				!Dvar::Var("party_host").get<bool>() && Dvar::Var("xblive_privatematch").get<bool>();
			const auto terminal = TerminalDisconnectRequested().exchange(false);
			if (wasMatchmaking)
			{
				EnqueueAsync([terminal, wasMatchmaking] { HandleServerDisconnect(terminal, wasMatchmaking); });
			}
			else if (isPrivateMatchClient)
			{
				Scheduler::Once([]
				{
					Command::Execute("xrequirelivesignin", false);
					Command::Execute("set systemlink 0", false);
					Command::Execute("set splitscreen 0", false);
					Command::Execute("set onlinegame 1", false);
					Command::Execute("exec default_xboxlive.cfg", false);
					Command::Execute("set party_maxplayers 4", false);
					Command::Execute("set party_maxprivatepartyplayers 4", false);
					Command::Execute("set xblive_privateserver 0", false);
					Command::Execute("set xblive_rankedmatch 0", false);
					Command::Execute("xstartprivateparty", false);
					Command::Execute("set ui_mptype 0", false);
					Command::Execute("xcheckezpatch", false);
					Command::Execute("exec default_xboxlive.cfg", false);
					Command::Execute("set xblive_rankedmatch 0", false);
					Command::Execute("ui_enumeratesaved", false);
					Command::Execute("set xblive_privateserver 1", false);
					Command::Execute("xstartprivatematch", false);
					Command::Execute("openmenu menu_xboxlive_privatelobby", false);
				}, Scheduler::Pipeline::MAIN, 250ms);
			}
		});
		Events::OnCGameInit([]
		{
			CancelRelayHandshake();
			MarkManagedRouteConnected();
			Auth::ClearManagedConnectTicket();
			EndpointJoinInFlight() = false;
			ServerJoinTransitionState() = false;
			bool hasMatch = false;
			{
				std::lock_guard lock(StateMutex());
				hasMatch = !CurrentMatchIdState().empty();
			}
			InGameState() = hasMatch;
			if (hasMatch)
			{
				// `mouse_click` is a verified shipped UI alias. Delaying it until
				// CGame initialization prevents failed or cancelled joins from
				// producing a false successful-connection cue.
				if (JoinInProgressConnectionSoundPending().exchange(false))
					Command::Execute("snd_playLocal mouse_click", false);
				Dvar::Var("zwnet_managed_session").set(true);
				Dvar::Var("zwnet_server_hostname").set(Party::GetHostName());
				SetState("IN_MATCH");
				EnqueueAsync([] { UpdatePresence(); });
			}
		});
		Scheduler::OnGameInitialized([]
		{
			Logger::Print("ZWNET initialization: registering dvars\n");
			InitializeDvars();
		}, Scheduler::Pipeline::MAIN);
		Scheduler::OnGameInitialized([]
		{
			if (!ActiveState()) return;
			Logger::Print("ZWNET initialization: checking saved session\n");
			const auto hasSession = LoadSession();
			Logger::Print("ZWNET initialization: starting {}\n", hasSession ? "refresh" : "login");
			if (hasSession) EnqueueAsync([] { Refresh(); });
			else Login();
		}, Scheduler::Pipeline::MAIN, 2s);
		Scheduler::Loop(ProcessAsyncTasks, Scheduler::Pipeline::ASYNC, 50ms);
		// The backend owns all server-start and join deadlines. Poll at the same
		// one-second resolution displayed by the lobby instead of free-running a
		// second client countdown that can reach zero before connection begins.
		Scheduler::Loop(UpdateMatchmaking, Scheduler::Pipeline::ASYNC, 1s);
		Scheduler::Loop(RefreshActiveParty, Scheduler::Pipeline::ASYNC, 3s);
		Scheduler::Loop([] { RefreshPlaylistCatalog(false); }, Scheduler::Pipeline::ASYNC, 30s);
		Scheduler::Loop(RefreshNetworkMetrics, Scheduler::Pipeline::ASYNC, 10s);
		Scheduler::Loop([]
		{
			if (!ActiveState() || !InGameState() ||
				!Dvar::Var("zwnet_match_return").get<bool>()) return;
			std::string matchId;
			{
				std::lock_guard lock(StateMutex());
				matchId = CurrentMatchIdState();
			}
			Dvar::Var("zwnet_match_return").set(false);
			if (matchId.empty()) return;
			Logger::Print("ZWNET match complete: leaving finished server before reset\n");
			Command::Execute("disconnect", false);
		}, Scheduler::Pipeline::MAIN, 100ms);
		Scheduler::Loop(UpdatePresence, Scheduler::Pipeline::ASYNC, 30s);
		Scheduler::Loop(CapturePartyPrivacy, Scheduler::Pipeline::MAIN, 1s);
	}

	void ZWNet::preDestroy()
	{
		NetworkMetricsEnabled() = false;
		CancelJoinInProgressPreview();
		if (ActiveState()) CloseOnlineSession(true, true);
		ManagedReconnectInFlight() = false;
		CancelRelayHandshake();
		ClearManagedRouteAttempt();
		ActiveState() = false;
		SearchingState() = false;
		ResetMatchLobbySoundSnapshot();
		EndpointJoinInFlight() = false;
		ServerJoinTransitionState() = false;
		OnlineEntryPendingState() = false;
		InGameState() = false;
		{
			std::lock_guard lock(StateMutex());
			LoginInFlightState() = false;
		}
		std::lock_guard lock(AsyncTaskMutex());
		AsyncTasks().clear();
	}
}
