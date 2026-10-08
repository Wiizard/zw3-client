#include "STDInclude.hpp"

#include <wincrypt.h>

#include "ZWNet.hpp"
#include "Auth.hpp"
#include "Command.hpp"
#include "Dedicated.hpp"
#include "Events.hpp"
#include "FileSystem.hpp"
#include "Friends.hpp"
#include "Localization.hpp"
#include "Logger.hpp"
#include "Maps.hpp"
#include "Party.hpp"
#include "Scheduler.hpp"
#include "TextRenderer.hpp"

namespace Components
{
	constexpr auto apiBase = "https://backend.zw3.eu";

	constexpr auto clientVersion = "3.0.4";
	constexpr auto modVersion = "3.0.4";
	constexpr auto userAgent = "ZW3-ZWNET/3.0.4";

	template <typename Callback>
	class ScopeExit
	{
	public:
		explicit ScopeExit(Callback callback) : callback(std::move(callback))
		{
		}

		~ScopeExit()
		{
			this->callback();
		}

		ScopeExit(const ScopeExit&) = delete;
		ScopeExit& operator=(const ScopeExit&) = delete;

	private:
		Callback callback;
	};

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

	constexpr std::array challengeSlotProgressDvars
	{
		"zw3_ch_slot_0_progress", "zw3_ch_slot_1_progress", "zw3_ch_slot_2_progress",
		"zw3_ch_slot_3_progress", "zw3_ch_slot_4_progress"
	};

	constexpr std::array challengeSlotTargetDvars
	{
		"zw3_ch_slot_0_target", "zw3_ch_slot_1_target", "zw3_ch_slot_2_target",
		"zw3_ch_slot_3_target", "zw3_ch_slot_4_target"
	};

	constexpr std::array challengeSlotTierDvars
	{
		"zw3_ch_slot_0_tier", "zw3_ch_slot_1_tier", "zw3_ch_slot_2_tier",
		"zw3_ch_slot_3_tier", "zw3_ch_slot_4_tier"
	};

	constexpr std::array challengeSlotTierCountDvars
	{
		"zw3_ch_slot_0_tier_count", "zw3_ch_slot_1_tier_count", "zw3_ch_slot_2_tier_count",
		"zw3_ch_slot_3_tier_count", "zw3_ch_slot_4_tier_count"
	};

	constexpr std::array challengeSlotRewardDvars
	{
		"zw3_ch_slot_0_reward", "zw3_ch_slot_1_reward", "zw3_ch_slot_2_reward",
		"zw3_ch_slot_3_reward", "zw3_ch_slot_4_reward"
	};

	constexpr std::array challengeSlotPercentDvars
	{
		"zw3_ch_slot_0_percent", "zw3_ch_slot_1_percent", "zw3_ch_slot_2_percent",
		"zw3_ch_slot_3_percent", "zw3_ch_slot_4_percent"
	};

	constexpr std::array challengeSlotCompleteDvars
	{
		"zw3_ch_slot_0_complete", "zw3_ch_slot_1_complete", "zw3_ch_slot_2_complete",
		"zw3_ch_slot_3_complete", "zw3_ch_slot_4_complete"
	};

	using SharedLobbyRankMap = std::unordered_map<std::string, SharedLobbyRank>;

	static std::atomic_bool isActive = false;
	static std::atomic_bool isSearching = false;
	static std::atomic_bool isClosingOnlineSession = false;
	static std::atomic_bool isServerJoinTransition = false;
	static std::atomic_bool isOnlineEntryPending = false;
	static std::atomic_bool isInGame = false;
	static std::atomic_bool isLocalPartyLeader = false;
	static std::atomic_bool isVisibilitySyncPending = false;
	static std::atomic_bool isEndpointJoinInFlight = false;
	static std::atomic_bool isTerminalDisconnectRequested = false;
	static std::atomic_bool isCachedPartyJoinStateSupported = false;
	static std::atomic_bool isIdleReturnPending = false;
	static std::atomic_int desiredPartyPrivacy = 0;
	static std::atomic_int cachedPartyMemberCount = 0;
	static std::atomic_int cachedPartyVisibility = 2;

	static std::mutex stateMutex;
	static bool isLoginInFlight = false;
	static std::string accessToken;
	static std::string refreshToken;
	static std::string currentPartyId;
	static std::string currentPlayerId;
	static std::string currentProposalId;
	static std::string currentMatchId;
	static std::string presenceUiState = "OFFLINE";

	static std::mutex asyncTaskMutex;
	static std::deque<std::function<void()>> asyncTasks;

	static std::mutex sharedLobbyRankMutex;
	static SharedLobbyRankMap sharedLobbyRanks;
	static std::string sharedLobbyRankPartyId;
	static std::chrono::steady_clock::time_point nextRankPublishAttempt{};
	static std::string lastRankPublishPartyId;
	static int lastRankPublishLevel = -1;
	static int lastRankPublishPrestige = -1;

	struct MatchLobbySoundDelta
	{
		bool hasJoined = false;
		bool hasLeft = false;
	};

	static std::mutex matchLobbySoundMutex;
	static std::string matchLobbySoundMatchId;
	static std::unordered_set<std::string> matchLobbySoundMembers;
	static bool isMatchLobbySoundInitialized = false;

	static void SetDisplayText(const std::string& dvarName, const std::string& value)
	{
		Dvar::Var(dvarName).Set(TextRenderer::StripMaterialTextIcons(value));
	}

	static void SetPresenceUiState(const std::string& state)
	{
		std::lock_guard _(stateMutex);
		presenceUiState = state;
	}

	static const char* PartyVisibilityName(const int privacy)
	{
		switch (privacy)
		{
		case 1:
			return "INVITE_ONLY";
		case 2:
			return "CLOSED";
		default:
			return "OPEN";
		}
	}

	static std::string NormalizePartyVisibility(std::string visibility)
	{
		std::ranges::transform(visibility, visibility.begin(), [](const unsigned char character)
		{
			return static_cast<char>(std::toupper(character));
		});

		if (visibility == "INVITE" || visibility == "FRIENDS")
		{
			return "INVITE_ONLY";
		}

		if (visibility != "OPEN" && visibility != "INVITE_ONLY" && visibility != "CLOSED")
		{
			return "OPEN";
		}

		return visibility;
	}

	static int PartyVisibilityValue(const std::string& visibility)
	{
		if (visibility == "CLOSED")
		{
			return 2;
		}

		if (visibility == "INVITE_ONLY")
		{
			return 1;
		}

		return 0;
	}

	static bool IsPartyJoinStateSupported(const std::string& state)
	{
		return state == "IDLE" || state == "IN_PARTY"
			|| state == "SEARCHING" || state == "MATCH_FOUND"
			|| state == "MAP_VOTE" || state == "READY_CHECK"
			|| state == "WAITING_FOR_READY" || state == "RESERVING_SERVER"
			|| state == "STARTING_SERVER" || state == "SERVER_STARTING"
			|| state == "CONNECTING" || state == "IN_MATCH";
	}

	static bool IsOpaqueId(const std::string& value)
	{
		return std::ranges::all_of(value, [](const unsigned char character)
		{
			return std::isalnum(character) != 0 || character == '-' || character == '_';
		});
	}

	static bool IsOpaquePartyId(const std::string& value)
	{
		return value.starts_with("pty_") && value.size() <= 80 && IsOpaqueId(value);
	}

	static bool IsOpaqueJoinCapability(const std::string& value)
	{
		return value.size() >= 16 && value.size() <= 256 && IsOpaqueId(value);
	}

	static bool IsOpaqueMatchId(const std::string& value)
	{
		return value.starts_with("mat_") && value.size() <= 80 && IsOpaqueId(value);
	}

	static bool IsOpaqueDescriptorValue(const std::string& value)
	{
		return !value.empty() && value.size() <= 128 && IsOpaqueId(value);
	}

	constexpr std::size_t playlistPageSize = 5;
	constexpr std::size_t playlistMaxMaps = 512;
	constexpr auto contentManifestPath = "zw3/core/zwnet-content-manifest.json";
	constexpr std::uintmax_t contentManifestMaxBytes = 256 * 1024;
	constexpr std::size_t contentManifestMaxEntries = 256;
	constexpr std::size_t contentEntryMaxFiles = 32;
	constexpr std::size_t contentManifestMaxFiles = 1024;
	constexpr std::uintmax_t contentManifestMaxDeclaredBytes = 50ULL * 1024 * 1024 * 1024;
	constexpr std::size_t contentHashBufferSize = 1024 * 1024;

	constexpr std::array playlistSlotVisibleDvars
	{
		"zwnet_catalog_slot_0_visible", "zwnet_catalog_slot_1_visible", "zwnet_catalog_slot_2_visible",
		"zwnet_catalog_slot_3_visible", "zwnet_catalog_slot_4_visible"
	};

	constexpr std::array playlistSlotSelectedDvars
	{
		"zwnet_catalog_slot_0_selected", "zwnet_catalog_slot_1_selected", "zwnet_catalog_slot_2_selected",
		"zwnet_catalog_slot_3_selected", "zwnet_catalog_slot_4_selected"
	};

	constexpr std::array playlistSlotNameDvars
	{
		"zwnet_catalog_slot_0_name", "zwnet_catalog_slot_1_name", "zwnet_catalog_slot_2_name",
		"zwnet_catalog_slot_3_name", "zwnet_catalog_slot_4_name"
	};

	constexpr std::array playlistSlotDescriptionDvars
	{
		"zwnet_catalog_slot_0_description", "zwnet_catalog_slot_1_description", "zwnet_catalog_slot_2_description",
		"zwnet_catalog_slot_3_description", "zwnet_catalog_slot_4_description"
	};

	constexpr std::array playlistSlotPreviewDvars
	{
		"zwnet_catalog_slot_0_preview", "zwnet_catalog_slot_1_preview", "zwnet_catalog_slot_2_preview",
		"zwnet_catalog_slot_3_preview", "zwnet_catalog_slot_4_preview"
	};

	constexpr std::array playlistSlotImageDvars
	{
		"zwnet_catalog_slot_0_image", "zwnet_catalog_slot_1_image", "zwnet_catalog_slot_2_image",
		"zwnet_catalog_slot_3_image", "zwnet_catalog_slot_4_image"
	};

	constexpr std::array playlistSlotAudienceDvars
	{
		"zwnet_catalog_slot_0_audience", "zwnet_catalog_slot_1_audience", "zwnet_catalog_slot_2_audience",
		"zwnet_catalog_slot_3_audience", "zwnet_catalog_slot_4_audience"
	};

	constexpr std::array playlistSlotAvailabilityDvars
	{
		"zwnet_catalog_slot_0_availability", "zwnet_catalog_slot_1_availability", "zwnet_catalog_slot_2_availability",
		"zwnet_catalog_slot_3_availability", "zwnet_catalog_slot_4_availability"
	};

	constexpr std::array playlistSlotStatusDvars
	{
		"zwnet_catalog_slot_0_status", "zwnet_catalog_slot_1_status", "zwnet_catalog_slot_2_status",
		"zwnet_catalog_slot_3_status", "zwnet_catalog_slot_4_status"
	};

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
		bool isLoaded = false;
		bool isStale = false;
		bool isInFlight = false;
		bool hasPending = false;
		bool hasNotice = false;
		std::chrono::steady_clock::time_point lastAttempt{};
	};

	struct JoinPreviewState
	{
		std::mutex mutex;
		std::uint64_t generation = 0;
		std::string matchId;
		std::string completedMatchId;
		bool isActive = false;
	};

	static std::mutex contentHashMutex;
	static std::unordered_map<std::string, ClientContentHash> contentHashCache;

	static ClientPlaylistCatalog playlistCatalog;
	static std::atomic_bool isPlaylistSearchStarting = false;
	static std::atomic_bool isPlaylistSelectionStarting = false;
	static std::atomic_bool isPlaylistSelectorOpen = false;
	static std::atomic_uint64_t playlistSelectorGeneration = 0;
	static std::atomic_int64_t playlistSelectorOpenedAtMs = 0;

	static JoinPreviewState joinPreview;
	static std::atomic_bool isJoinInProgressSoundPending = false;
	static std::atomic_bool isNetworkMetricsEnabled = false;
	static std::atomic_uint64_t joinTransitionGeneration = 0;
	static std::chrono::steady_clock::time_point lastMetricsSuccess{};

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
		bool isDirectWaiting = false;
		bool isRelayUsed = false;
		bool isRouteRelay = false;
		bool isReconnectAttempt = false;
	};

	struct RelayHandshake
	{
		std::mutex mutex;
		std::uint64_t generation = 0;
		bool isPending = false;
		bool isReady = false;
		Network::Address target;
		std::string matchId;
		std::string playerId;
		std::string nonce;
		std::string hello;
		std::chrono::steady_clock::time_point deadline{};
	};

	constexpr auto relayHandshakeTimeout = 8s;
	constexpr std::size_t relayPacketLimit = 1400;
	constexpr std::size_t relayReadyLimit = 256;

	static ManagedRouteAttempt routeAttempt;
	static RelayHandshake relayHandshake;
	static std::atomic_bool isManagedReconnectInFlight = false;

	static void CancelRelayHandshake()
	{
		std::lock_guard _(relayHandshake.mutex);

		++relayHandshake.generation;
		relayHandshake.isPending = false;
		relayHandshake.isReady = false;
		std::ranges::fill(relayHandshake.hello, '\0');
		relayHandshake.hello.clear();
		relayHandshake.nonce.clear();
		relayHandshake.matchId.clear();
		relayHandshake.playerId.clear();
	}

	static void ClearManagedRouteAttempt()
	{
		std::lock_guard _(routeAttempt.mutex);

		routeAttempt.matchId.clear();
		routeAttempt.playerId.clear();
		routeAttempt.sessionId.clear();
		routeAttempt.serverIdentity.clear();
		routeAttempt.instanceId.clear();
		routeAttempt.isDirectWaiting = false;
		routeAttempt.isRelayUsed = false;
		routeAttempt.isRouteRelay = false;
		routeAttempt.isReconnectAttempt = false;
	}

	static void MarkManagedRouteConnected()
	{
		std::lock_guard _(routeAttempt.mutex);
		routeAttempt.isDirectWaiting = false;
	}

	static bool IsRelayUsedForMatch(const std::string& matchId)
	{
		std::lock_guard _(routeAttempt.mutex);
		return routeAttempt.matchId == matchId && routeAttempt.isRelayUsed;
	}

	static void ResetManagedRoute()
	{
		isManagedReconnectInFlight = false;
		CancelRelayHandshake();
		ClearManagedRouteAttempt();
		Auth::ClearManagedConnectTicket();
	}

	static std::int64_t PlaylistClockMilliseconds()
	{
		return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
	}

	static bool IsCurrentPlaylistActivation(const std::uint64_t generation)
	{
		return isPlaylistSelectorOpen && playlistSelectorGeneration == generation;
	}

	static std::string SafePlaylistText(const std::string& value, const std::size_t limit)
	{
		std::string safe;
		safe.reserve(std::min(value.size(), limit));

		for (const auto character : value)
		{
			if (safe.size() >= limit)
			{
				break;
			}

			const auto code = static_cast<unsigned char>(character);

			if (code < 32 || code == 127)
			{
				continue;
			}

			safe.push_back(character);
		}

		return safe;
	}

	static const char* OnOffText(const bool isOn)
	{
		if (isOn)
		{
			return "ON";
		}

		return "OFF";
	}

	static bool IsContentId(const std::string& value)
	{
		if (value.empty() || value.size() > 120)
		{
			return false;
		}

		return std::ranges::all_of(value, [](const unsigned char character)
		{
			return std::isalnum(character) != 0 || character == '_' || character == '.' || character == '-';
		});
	}

	static bool IsLowerSha256(const std::string& value)
	{
		return value.size() == 64 && std::ranges::all_of(value, [](const unsigned char character)
		{
			return (character >= '0' && character <= '9') || (character >= 'a' && character <= 'f');
		});
	}

	static bool IsPlaylistId(const std::string& value)
	{
		return std::ranges::all_of(value, [](const unsigned char character)
		{
			return std::isalnum(character) != 0 || character == '-' || character == '_';
		});
	}

	static std::optional<std::filesystem::path> TryGetContentRelativePath(const std::string& raw)
	{
		if (raw.empty() || raw.size() > 240 || raw.find('\\') != std::string::npos || raw.find(':') != std::string::npos
			|| raw.front() == '/' || raw.back() == '/')
		{
			return std::nullopt;
		}

		const std::filesystem::path relative{ raw };

		if (relative.is_absolute() || relative.has_root_path())
		{
			return std::nullopt;
		}

		for (const auto& part : relative)
		{
			if (part == "." || part == ".." || part.empty())
			{
				return std::nullopt;
			}
		}

		const auto normalized = relative.generic_string();

		if (normalized != raw)
		{
			return std::nullopt;
		}

		static constexpr std::array<std::string_view, 5> allowedPrefixes
		{
			"zw3/", "main/", "zone/", "usermaps/", "userraw/"
		};

		const bool isAllowedPrefix = std::ranges::any_of(allowedPrefixes, [&normalized](const std::string_view prefix)
		{
			return normalized.starts_with(prefix);
		});

		if (normalized != "zw3.dll" && normalized != "zw3.exe" && !isAllowedPrefix)
		{
			return std::nullopt;
		}

		return relative;
	}

	static std::optional<ClientContentFileStamp> TryGetContentFileStamp(const std::filesystem::path& path)
	{
		std::error_code error;

		if (!std::filesystem::is_regular_file(path, error) || error)
		{
			return std::nullopt;
		}

		const auto size = std::filesystem::file_size(path, error);

		if (error)
		{
			return std::nullopt;
		}

		const auto modified = std::filesystem::last_write_time(path, error);

		if (error)
		{
			return std::nullopt;
		}

		return ClientContentFileStamp{ size, modified };
	}

	static std::optional<std::filesystem::path> TryResolveContentFile(const std::filesystem::path& base, const std::filesystem::path& relative)
	{
		std::error_code error;
		const auto resolved = std::filesystem::canonical(base / relative, error);

		if (error)
		{
			return std::nullopt;
		}

		const auto within = resolved.lexically_relative(base);

		if (within.empty() || within.is_absolute() || *within.begin() == "..")
		{
			return std::nullopt;
		}

		return resolved;
	}

	static std::optional<std::string> TryHashContentFile(const std::filesystem::path& path, const ClientContentFileStamp& expectedStamp)
	{
		const auto cacheKey = path.lexically_normal().generic_string();

		{
			std::lock_guard _(contentHashMutex);

			const auto cached = contentHashCache.find(cacheKey);

			if (cached != contentHashCache.end() && cached->second.stamp == expectedStamp)
			{
				return cached->second.sha256;
			}
		}

		const auto file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);

		if (file == INVALID_HANDLE_VALUE)
		{
			return std::nullopt;
		}

		ScopeExit closeFile([file]
		{
			CloseHandle(file);
		});

		hash_state state{};

		if (sha256_init(&state) != CRYPT_OK)
		{
			return std::nullopt;
		}

		std::vector<unsigned char> buffer(contentHashBufferSize);

		while (true)
		{
			DWORD bytesRead = 0;

			if (!::ReadFile(file, buffer.data(), static_cast<DWORD>(buffer.size()), &bytesRead, nullptr))
			{
				return std::nullopt;
			}

			if (bytesRead == 0)
			{
				break;
			}

			if (sha256_process(&state, buffer.data(), bytesRead) != CRYPT_OK)
			{
				return std::nullopt;
			}
		}

		std::array<unsigned char, 32> digest{};

		if (sha256_done(&state, digest.data()) != CRYPT_OK)
		{
			return std::nullopt;
		}

		const auto finalStamp = TryGetContentFileStamp(path);

		if (!finalStamp || *finalStamp != expectedStamp)
		{
			return std::nullopt;
		}

		static constexpr char hexDigits[] = "0123456789abcdef";

		std::string result;
		result.resize(digest.size() * 2);

		for (std::size_t i = 0; i < digest.size(); ++i)
		{
			result[i * 2] = hexDigits[digest[i] >> 4];
			result[i * 2 + 1] = hexDigits[digest[i] & 0x0F];
		}

		{
			std::lock_guard _(contentHashMutex);
			contentHashCache[cacheKey] = { *finalStamp, result };
		}

		return result;
	}

	static ClientContentManifest LoadClientContentManifest()
	{
		ClientContentManifest manifest;

		std::error_code error;
		const auto basePath = std::filesystem::canonical(std::filesystem::path{ (*Game::fs_basepath)->current.string }, error);

		std::optional<std::filesystem::path> path;
		std::uintmax_t size = 0;

		if (!error)
		{
			path = TryResolveContentFile(basePath, std::filesystem::path{ contentManifestPath });
		}

		if (path)
		{
			size = std::filesystem::file_size(*path, error);
		}

		if (error || size == 0 || size > contentManifestMaxBytes)
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

		const bool isValidDocument = document.is_object() && document.value("schema_version", 0) == 1
			&& document.contains("manifest_version") && document.at("manifest_version").is_string()
			&& document.contains("content") && document.at("content").is_array()
			&& document.at("content").size() <= contentManifestMaxEntries;

		if (!isValidDocument)
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
			const bool isValidItem = item.is_object() && item.contains("id") && item.at("id").is_string()
				&& item.contains("version") && item.at("version").is_string()
				&& item.contains("files") && item.at("files").is_array()
				&& !item.at("files").empty() && item.at("files").size() <= contentEntryMaxFiles;

			if (!isValidItem)
			{
				manifest.error = "A local ZW3 content definition is invalid.";
				return manifest;
			}

			ClientContentDefinition definition;
			definition.id = item.at("id").get<std::string>();
			definition.version = item.at("version").get<std::string>();

			if (!IsContentId(definition.id) || definition.version.empty() || definition.version.size() > 64
				|| manifest.entries.contains(definition.id))
			{
				manifest.error = "A local ZW3 content identity is invalid or duplicated.";
				return manifest;
			}

			totalFiles += item.at("files").size();

			if (totalFiles > contentManifestMaxFiles)
			{
				manifest.error = "The local ZW3 content manifest defines too many files.";
				return manifest;
			}

			std::unordered_set<std::string> paths;

			for (const auto& file : item.at("files"))
			{
				const bool isValidFile = file.is_object() && file.contains("path") && file.at("path").is_string()
					&& file.contains("size") && file.at("size").is_number_unsigned()
					&& file.contains("sha256") && file.at("sha256").is_string();

				if (!isValidFile)
				{
					manifest.error = "A local ZW3 content file definition is invalid.";
					return manifest;
				}

				const auto rawPath = file.at("path").get<std::string>();
				const auto relativePath = TryGetContentRelativePath(rawPath);
				const auto sha256 = file.at("sha256").get<std::string>();
				const auto fileSize = file.at("size").get<std::uintmax_t>();

				if (!relativePath || !paths.insert(Utils::String::ToLower(rawPath)).second || !IsLowerSha256(sha256)
					|| fileSize > contentManifestMaxDeclaredBytes - totalBytes)
				{
					manifest.error = "A local ZW3 content file path or hash is invalid.";
					return manifest;
				}

				totalBytes += fileSize;
				definition.files.push_back({ *relativePath, fileSize, sha256 });
			}

			manifest.entries.emplace(definition.id, std::move(definition));
		}

		return manifest;
	}

	static void VerifyPlaylistContent(std::vector<ClientPlaylist>& entries)
	{
		const bool needsContent = std::ranges::any_of(entries, [](const ClientPlaylist& entry)
		{
			return !entry.requiredContent.empty();
		});

		if (!needsContent)
		{
			for (auto& entry : entries)
			{
				entry.availabilityDetail = "No compatible server capacity is available.";

				if (entry.availability == "AVAILABLE")
				{
					entry.availabilityDetail = "Ready to search.";
				}
			}

			return;
		}

		const auto manifest = LoadClientContentManifest();

		std::error_code baseError;
		const auto basePath = std::filesystem::canonical(std::filesystem::path{ (*Game::fs_basepath)->current.string }, baseError);

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

			if (baseError)
			{
				entry.availability = "CONTENT_MANIFEST_INVALID";
				entry.availabilityDetail = "The local ZW3 game root could not be verified.";
				continue;
			}

			if (!manifest.error.empty())
			{
				entry.availability = "CONTENT_MANIFEST_INVALID";
				entry.availabilityDetail = manifest.error;
				continue;
			}

			bool hasFailed = false;

			for (const auto& contentId : entry.requiredContent)
			{
				const auto definition = manifest.entries.find(contentId);

				if (definition == manifest.entries.end())
				{
					entry.availability = "CONTENT_UNKNOWN";
					entry.availabilityDetail = SafePlaylistText("Update required: content " + contentId + " is not defined locally.", 180);
					hasFailed = true;
					break;
				}

				for (const auto& file : definition->second.files)
				{
					const auto absolutePath = TryResolveContentFile(basePath, file.relativePath);
					std::optional<ClientContentFileStamp> stamp;

					if (absolutePath)
					{
						stamp = TryGetContentFileStamp(*absolutePath);
					}

					if (!stamp || stamp->size != file.size)
					{
						entry.availability = "CONTENT_MISSING";
						entry.availabilityDetail = SafePlaylistText("Update required: " + contentId + " is missing " + file.relativePath.generic_string() + ".", 180);
						hasFailed = true;
						break;
					}

					const auto hash = TryHashContentFile(*absolutePath, *stamp);

					if (!hash || *hash != file.sha256)
					{
						entry.availability = "CONTENT_CORRUPT";
						entry.availabilityDetail = SafePlaylistText("Repair required: " + contentId + " failed its integrity check.", 180);
						hasFailed = true;
						break;
					}
				}

				if (hasFailed)
				{
					break;
				}

				entry.verifiedContent.push_back(contentId);
			}

			if (!hasFailed)
			{
				entry.availabilityDetail = std::format("Content verified (manifest {}).", manifest.version);
			}
		}
	}

	static bool TryParseZombieSettings(const nlohmann::json& row, std::string& summary)
	{
		std::string zombieMode = "NORMAL";
		bool hasHitmarkers = true;
		bool hasZombieCounter = false;
		bool hasDamageNumbers = true;
		bool hasDayNightCycle = true;
		bool hasOmnimovement = true;

		if (row.contains("zombie_settings"))
		{
			const auto& settings = row.at("zombie_settings");

			const bool isValidSettings = settings.is_object()
				&& settings.contains("mode") && settings.at("mode").is_string()
				&& settings.contains("hitmarkers") && settings.at("hitmarkers").is_boolean()
				&& settings.contains("zombie_counter") && settings.at("zombie_counter").is_boolean()
				&& settings.contains("damage_numbers") && settings.at("damage_numbers").is_boolean()
				&& settings.contains("day_night_cycle") && settings.at("day_night_cycle").is_boolean()
				&& settings.contains("omnimovement") && settings.at("omnimovement").is_boolean();

			if (!isValidSettings)
			{
				return false;
			}

			zombieMode = settings.at("mode").get<std::string>();
			hasHitmarkers = settings.at("hitmarkers").get<bool>();
			hasZombieCounter = settings.at("zombie_counter").get<bool>();
			hasDamageNumbers = settings.at("damage_numbers").get<bool>();
			hasDayNightCycle = settings.at("day_night_cycle").get<bool>();
			hasOmnimovement = settings.at("omnimovement").get<bool>();
		}

		if (zombieMode != "NORMAL" && zombieMode != "CLASSIC" && zombieMode != "HARDCORE")
		{
			return false;
		}

		summary = std::format("{} / HITMARKERS {} / COUNTER {} / DAMAGE {} / DAY-NIGHT {} / OMNI {}", zombieMode,
			OnOffText(hasHitmarkers), OnOffText(hasZombieCounter), OnOffText(hasDamageNumbers),
			OnOffText(hasDayNightCycle), OnOffText(hasOmnimovement));

		return true;
	}

	static bool TryParsePlaylistMaps(const nlohmann::json& row, ClientPlaylist& entry)
	{
		if (!row.contains("maps"))
		{
			return true;
		}

		const auto& maps = row.at("maps");

		if (!maps.is_array() || maps.empty() || maps.size() > playlistMaxMaps)
		{
			return false;
		}

		std::vector<std::string> mapNames;

		for (const auto& map : maps)
		{
			if (!map.is_object() || !map.contains("id") || !map.at("id").is_string() || !map.contains("name") || !map.at("name").is_string())
			{
				return false;
			}

			const auto mapId = map.at("id").get<std::string>();
			const auto mapName = SafePlaylistText(map.at("name").get<std::string>(), 60);

			const bool isPlainMapId = std::ranges::all_of(mapId, [](const unsigned char character)
			{
				return std::isalnum(character) != 0 || character == '_';
			});

			if (mapId.empty() || mapId.size() > 80 || mapName.empty() || !isPlainMapId)
			{
				return false;
			}

			if (mapNames.empty())
			{
				entry.mapId = mapId;

				if (map.contains("image") && map.at("image").is_string())
				{
					entry.mapImage = map.at("image").get<std::string>();
				}

				if (entry.mapImage.size() > 80)
				{
					return false;
				}
			}

			mapNames.push_back(mapName);
		}

		if (mapNames.size() == 1)
		{
			entry.preview = mapNames.front();
			entry.rotationSummary = "FIXED MAP  /  " + mapNames.front();
			return true;
		}

		entry.preview = std::format("{} MAP ROTATION", mapNames.size());
		entry.rotationSummary = std::format("{} maps: {}, {}", mapNames.size(), mapNames[0], mapNames[1]);

		if (mapNames.size() > 2)
		{
			entry.rotationSummary += std::format(" + {} more", mapNames.size() - 2);
		}

		entry.rotationSummary = SafePlaylistText(entry.rotationSummary, 180);
		return true;
	}

	static bool TryParseClientPlaylistCatalog(const nlohmann::json& data, std::int64_t& revision, std::vector<ClientPlaylist>& entries)
	{
		const bool isValidCatalog = data.is_object()
			&& data.contains("schema_version") && data.at("schema_version").is_number_integer()
			&& data.at("schema_version").get<int>() == 1
			&& data.contains("catalog_revision") && data.at("catalog_revision").is_number_integer()
			&& data.contains("playlists") && data.at("playlists").is_array()
			&& data.at("playlists").size() <= 128;

		if (!isValidCatalog)
		{
			return false;
		}

		revision = data.at("catalog_revision").get<std::int64_t>();

		if (revision < 0)
		{
			return false;
		}

		std::unordered_set<std::string> ids;

		for (const auto& row : data.at("playlists"))
		{
			const bool isValidRow = row.is_object()
				&& row.contains("id") && row.at("id").is_string()
				&& row.contains("name") && row.at("name").is_string()
				&& (!row.contains("description") || row.at("description").is_string())
				&& row.contains("revision") && row.at("revision").is_number_integer()
				&& row.contains("audience") && row.at("audience").is_string()
				&& row.contains("availability") && row.at("availability").is_string()
				&& row.contains("required_content") && row.at("required_content").is_array()
				&& row.at("required_content").size() <= 64;

			if (!isValidRow)
			{
				return false;
			}

			ClientPlaylist entry;
			entry.id = row.at("id").get<std::string>();
			entry.name = SafePlaylistText(row.at("name").get<std::string>(), 80);
			entry.description = SafePlaylistText(row.value("description", std::string{}), 180);
			entry.audience = row.at("audience").get<std::string>();
			entry.availability = row.at("availability").get<std::string>();
			entry.revision = row.at("revision").get<std::int64_t>();

			if (row.contains("min_players") && row.at("min_players").is_number_integer())
			{
				entry.minPlayers = row.at("min_players").get<int>();
			}

			if (row.contains("max_players") && row.at("max_players").is_number_integer())
			{
				entry.maxPlayers = row.at("max_players").get<int>();
			}

			if (!TryParseZombieSettings(row, entry.zombieSettingsSummary))
			{
				return false;
			}

			const bool isValidEntry = !entry.id.empty() && entry.id.size() <= 96 && !entry.name.empty()
				&& entry.revision >= 1 && ids.insert(entry.id).second
				&& entry.minPlayers >= 1 && entry.maxPlayers >= entry.minPlayers && entry.maxPlayers <= 18
				&& (entry.audience == "PUBLIC" || entry.audience == "RESTRICTED")
				&& (entry.availability == "AVAILABLE" || entry.availability == "NO_CAPACITY")
				&& IsPlaylistId(entry.id);

			if (!isValidEntry)
			{
				return false;
			}

			for (const auto& item : row.at("required_content"))
			{
				if (!item.is_string())
				{
					return false;
				}

				const auto contentId = item.get<std::string>();

				if (!IsContentId(contentId))
				{
					return false;
				}

				entry.requiredContent.push_back(contentId);
			}

			if (!TryParsePlaylistMaps(row, entry))
			{
				return false;
			}

			entries.push_back(std::move(entry));
		}

		return true;
	}

	static void MarkJoinInProgressConnectionStarted(const std::string& matchId)
	{
		{
			std::lock_guard _(joinPreview.mutex);

			if (joinPreview.completedMatchId != matchId)
			{
				return;
			}
		}

		isJoinInProgressSoundPending = true;

		Scheduler::Once([]
		{
			isJoinInProgressSoundPending = false;
		}, Scheduler::Pipeline::MAIN, 15s);
	}

	static void ResetMatchLobbySoundSnapshot()
	{
		std::lock_guard _(matchLobbySoundMutex);

		matchLobbySoundMatchId.clear();
		matchLobbySoundMembers.clear();
		isMatchLobbySoundInitialized = false;
	}

	static MatchLobbySoundDelta ObserveMatchLobbyMembers(const std::string& matchId, const std::unordered_set<std::string>& members)
	{
		std::lock_guard _(matchLobbySoundMutex);

		if (matchId.empty())
		{
			matchLobbySoundMatchId.clear();
			matchLobbySoundMembers.clear();
			isMatchLobbySoundInitialized = false;
			return {};
		}

		if (!isMatchLobbySoundInitialized || matchLobbySoundMatchId != matchId)
		{
			matchLobbySoundMatchId = matchId;
			matchLobbySoundMembers = members;
			isMatchLobbySoundInitialized = true;
			return {};
		}

		MatchLobbySoundDelta delta;

		for (const auto& member : members)
		{
			if (!matchLobbySoundMembers.contains(member))
			{
				delta.hasJoined = true;
			}
		}

		for (const auto& member : matchLobbySoundMembers)
		{
			if (!members.contains(member))
			{
				delta.hasLeft = true;
			}
		}

		matchLobbySoundMembers = members;
		return delta;
	}

	static const std::string& PublicGuid()
	{
		static const std::string guid = std::format("{:016x}", Auth::GetKeyHash());
		return guid;
	}

	static bool TryParseRankValue(const std::string& data, const std::string_view field, int& value)
	{
		const auto fieldPosition = data.find(field);

		if (fieldPosition == std::string::npos)
		{
			return false;
		}

		auto position = fieldPosition + field.size();

		while (position < data.size() && (data[position] == ' ' || data[position] == '\t'))
		{
			++position;
		}

		if (position >= data.size() || data[position] != ':')
		{
			return false;
		}

		++position;

		while (position < data.size() && (data[position] == ' ' || data[position] == '\t'))
		{
			++position;
		}

		if (position >= data.size())
		{
			return false;
		}

		const char* const start = data.data() + position;
		const char* const dataEnd = data.data() + data.size();
		char* end = nullptr;
		const auto parsed = std::strtol(start, &end, 10);

		if (end == start)
		{
			return false;
		}

		while (end < dataEnd && (*end == ' ' || *end == '\t'))
		{
			++end;
		}

		if (end < dataEnd && *end != ';')
		{
			return false;
		}

		if (parsed < std::numeric_limits<int>::min() || parsed > std::numeric_limits<int>::max())
		{
			return false;
		}

		value = static_cast<int>(parsed);
		return true;
	}

	static std::filesystem::path ScriptDataPath(const std::string& prefix)
	{
		return std::filesystem::path("zw3") / "core" / "scriptdata" / (prefix + PublicGuid());
	}

	static std::optional<SharedLobbyRank> ReadLocalLobbyRank()
	{
		std::string data;

		if (!Utils::IO::ReadFile(ScriptDataPath("rank_").string(), &data))
		{
			return std::nullopt;
		}

		int storedLevel = 0;
		int storedPrestige = 0;

		if (!TryParseRankValue(data, "level", storedLevel) || !TryParseRankValue(data, "prestige", storedPrestige)
			|| storedLevel < 0 || storedLevel > 53 || storedPrestige < 0 || storedPrestige > 20)
		{
			return std::nullopt;
		}

		return SharedLobbyRank{ storedLevel + 1, storedPrestige };
	}

	static int ExperienceTarget(const int storedLevel)
	{
		static constexpr int firstTargets[] = { 50, 125, 200, 300, 450, 650 };

		if (storedLevel >= 0 && storedLevel < static_cast<int>(std::size(firstTargets)))
		{
			return firstTargets[storedLevel];
		}

		return 650 + ((storedLevel - 5) * 250);
	}

	static std::optional<LocalBarracksRank> ReadLocalBarracksRank()
	{
		std::string data;

		if (!Utils::IO::ReadFile(ScriptDataPath("rank_").string(), &data))
		{
			return std::nullopt;
		}

		int storedLevel = 0;
		int storedPrestige = 0;
		int storedExperience = 0;

		if (!TryParseRankValue(data, "level", storedLevel) || !TryParseRankValue(data, "prestige", storedPrestige)
			|| !TryParseRankValue(data, "experience", storedExperience)
			|| storedLevel < 0 || storedLevel > 53 || storedPrestige < 0 || storedPrestige > 255 || storedExperience < 0)
		{
			return std::nullopt;
		}

		return LocalBarracksRank{ storedLevel + 1, storedPrestige, storedExperience, ExperienceTarget(storedLevel) };
	}

	static std::string ZombiePrestigeIcon(const int prestige)
	{
		const auto iconLevel = prestige + 1;

		if (iconLevel > 8)
		{
			return "skullicon";
		}

		return std::format("prestige_{}", iconLevel);
	}

	static bool TryParseChallengeEntry(const std::string& data, const std::string_view id, LocalChallengeProgress& entry)
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

		const char* const progressStart = data.data() + position + needle.size();
		char* progressEnd = nullptr;
		const auto progress = std::strtol(progressStart, &progressEnd, 10);

		if (progressEnd == progressStart || *progressEnd != ':')
		{
			return false;
		}

		const char* const tierStart = progressEnd + 1;
		char* tierEnd = nullptr;
		const auto tier = std::strtol(tierStart, &tierEnd, 10);

		if (tierEnd == tierStart || *tierEnd != ';' || progress < 0 || tier < 0)
		{
			return false;
		}

		entry.progress = static_cast<int>(progress);
		entry.tier = static_cast<int>(tier);
		return true;
	}

	static std::vector<ChallengeDefinition> ReadChallengeDefinitions()
	{
		Utils::CSV table("zw3/core/mp/zw3_challenge_ui.csv", true, false);
		std::vector<ChallengeDefinition> definitions;

		if (!table.IsValid())
		{
			return definitions;
		}

		definitions.reserve(table.GetRows());

		for (std::size_t row = 0; row < table.GetRows(); ++row)
		{
			ChallengeDefinition definition;
			definition.id = table.GetElementAt(row, 0);

			if (definition.id.empty())
			{
				continue;
			}

			for (std::size_t tier = 0; tier < definition.targets.size(); ++tier)
			{
				const auto targetText = table.GetElementAt(row, 4 + tier * 2);
				const auto rewardText = table.GetElementAt(row, 5 + tier * 2);

				if (targetText.empty())
				{
					break;
				}

				char* targetEnd = nullptr;
				char* rewardEnd = nullptr;
				const auto target = std::strtol(targetText.data(), &targetEnd, 10);
				const auto reward = std::strtol(rewardText.data(), &rewardEnd, 10);

				if (targetEnd == targetText.data() || *targetEnd != '\0' || target <= 0
					|| rewardEnd == rewardText.data() || *rewardEnd != '\0' || reward < 0)
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

	static LocalBarracksChallenges ReadLocalBarracksChallenges(const std::vector<ChallengeDefinition>& definitions)
	{
		LocalBarracksChallenges challenges;
		std::string data;

		if (!Utils::IO::ReadFile(ScriptDataPath("challenges_").string(), &data))
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

		if (TryParseChallengeEntry(data, "stat_zw3_zombie_kills", value))
		{
			challenges.zombieKills = value.progress;
		}

		if (TryParseChallengeEntry(data, "stat_zw3_zombie_deaths", value))
		{
			challenges.zombieDeaths = value.progress;
		}

		if (TryParseChallengeEntry(data, "stat_zw3_zombie_revives", value))
		{
			challenges.zombieRevives = value.progress;
		}

		const auto killer = challenges.entries.find("ch_zw3_zombie_killer");

		if (killer != challenges.entries.end())
		{
			challenges.zombieKills = std::max(challenges.zombieKills, killer->second.progress);
		}

		const auto reviver = challenges.entries.find("ch_zw3_reviver");

		if (reviver != challenges.entries.end())
		{
			challenges.zombieRevives = std::max(challenges.zombieRevives, reviver->second.progress);
		}

		return challenges;
	}

	static void RefreshChallengeCategory(const int startIndex)
	{
		const auto definitions = ReadChallengeDefinitions();
		const auto challenges = ReadLocalBarracksChallenges(definitions);

		for (std::size_t slot = 0; slot < challengeSlotProgressDvars.size(); ++slot)
		{
			int progress = 0;
			int target = 0;
			int tier = 0;
			int tierCount = 0;
			int reward = 0;
			int percent = 0;
			bool isComplete = false;

			const auto definitionIndex = startIndex + static_cast<int>(slot);

			if (definitionIndex >= 0 && definitionIndex < static_cast<int>(definitions.size()))
			{
				const auto& definition = definitions[definitionIndex];

				tierCount = static_cast<int>(std::ranges::count_if(definition.targets, [](const int value)
				{
					return value > 0;
				}));

				const auto entry = challenges.entries.find(definition.id);

				if (entry != challenges.entries.end())
				{
					progress = std::max(entry->second.progress, 0);
					tier = std::clamp(entry->second.tier, 0, tierCount);
				}

				isComplete = tierCount > 0 && tier >= tierCount;

				auto targetTier = tier;

				if (isComplete)
				{
					targetTier = tierCount - 1;
				}

				if (targetTier >= 0)
				{
					target = definition.targets[targetTier];

					if (!isComplete)
					{
						reward = definition.rewards[targetTier];
					}
				}

				if (isComplete)
				{
					percent = 100;
				}
				else if (target > 0)
				{
					percent = std::clamp(static_cast<int>((static_cast<std::int64_t>(progress) * 100) / target), 0, 100);
				}
			}

			Dvar::Var(challengeSlotProgressDvars[slot]).Set(progress);
			Dvar::Var(challengeSlotTargetDvars[slot]).Set(target);
			Dvar::Var(challengeSlotTierDvars[slot]).Set(tier);
			Dvar::Var(challengeSlotTierCountDvars[slot]).Set(tierCount);
			Dvar::Var(challengeSlotRewardDvars[slot]).Set(reward);
			Dvar::Var(challengeSlotPercentDvars[slot]).Set(percent);
			Dvar::Var(challengeSlotCompleteDvars[slot]).Set(isComplete);
		}
	}

	static void RefreshBarracksProfile()
	{
		const auto rank = ReadLocalBarracksRank();
		Dvar::Var("zw3_barracks_rank_known").Set(rank.has_value());

		if (!rank)
		{
			Dvar::Var("zw3_barracks_rank_level").Set(1);
			Dvar::Var("zw3_barracks_rank_prestige").Set(0);
			Dvar::Var("zw3_barracks_rank_experience").Set(0);
			Dvar::Var("zw3_barracks_rank_experience_target").Set(50);
			Dvar::Var("zw3_barracks_rank_experience_percent").Set(0);
			Dvar::Var("zw3_barracks_rank_icon").Set("prestige_1");
		}
		else
		{
			const auto percent = std::clamp(static_cast<int>((static_cast<std::int64_t>(rank->experience) * 100) / rank->experienceTarget), 0, 100);

			Dvar::Var("zw3_barracks_rank_level").Set(rank->level);
			Dvar::Var("zw3_barracks_rank_prestige").Set(rank->prestige);
			Dvar::Var("zw3_barracks_rank_experience").Set(rank->experience);
			Dvar::Var("zw3_barracks_rank_experience_target").Set(rank->experienceTarget);
			Dvar::Var("zw3_barracks_rank_experience_percent").Set(percent);
			Dvar::Var("zw3_barracks_rank_icon").Set(ZombiePrestigeIcon(rank->prestige));
		}

		const auto definitions = ReadChallengeDefinitions();
		const auto challenges = ReadLocalBarracksChallenges(definitions);

		Dvar::Var("zw3_barracks_zombie_kills").Set(challenges.zombieKills);
		Dvar::Var("zw3_barracks_zombie_deaths").Set(challenges.zombieDeaths);
		Dvar::Var("zw3_barracks_zombie_revives").Set(challenges.zombieRevives);
	}

	static bool IsPublicGuid(const std::string& guid)
	{
		return guid.size() == 16 && std::ranges::all_of(guid, [](const unsigned char character)
		{
			return std::isxdigit(character) != 0;
		});
	}

	static int JsonIntegerOr(const nlohmann::json& object, const char* key, const int fallback)
	{
		if (object.contains(key) && object.at(key).is_number_integer())
		{
			return object.at(key).get<int>();
		}

		return fallback;
	}

	static SharedLobbyRankMap ParseSharedLobbyRanks(const nlohmann::json& party)
	{
		SharedLobbyRankMap ranks;

		const auto settings = party.find("zombie_settings");

		if (settings != party.end() && settings->is_object())
		{
			const auto playerRanks = settings->find("zwnet_player_ranks");

			if (playerRanks != settings->end() && playerRanks->is_object())
			{
				for (const auto& [guid, value] : playerRanks->items())
				{
					if (guid.size() != 16 || !value.is_object())
					{
						continue;
					}

					SharedLobbyRank rank;
					rank.level = std::clamp(value.value("level", 1), 1, 54);
					rank.prestige = std::max(value.value("prestige", 0), 0);
					ranks[guid] = rank;
				}
			}
		}

		const auto members = party.find("members");

		if (members == party.end() || !members->is_array())
		{
			return ranks;
		}

		for (const auto& member : *members)
		{
			if (!member.is_object())
			{
				continue;
			}

			const auto playerId = member.find("player_id");

			if (playerId == member.end() || !playerId->is_string())
			{
				continue;
			}

			const auto guid = playerId->get<std::string>();

			if (!IsPublicGuid(guid))
			{
				continue;
			}

			SharedLobbyRank rank;
			const auto memberRank = member.find("rank");

			if (memberRank != member.end() && memberRank->is_object() && memberRank->contains("level") && memberRank->at("level").is_number_integer())
			{
				rank.level = std::clamp(memberRank->at("level").get<int>(), 1, 54);
				rank.prestige = std::max(JsonIntegerOr(*memberRank, "prestige", 0), 0);
			}
			else if (member.contains("level") && member.at("level").is_number_integer())
			{
				rank.level = std::clamp(member.at("level").get<int>(), 1, 54);
				rank.prestige = std::max(JsonIntegerOr(member, "prestige", 0), 0);
			}
			else
			{
				continue;
			}

			ranks[guid] = rank;
		}

		return ranks;
	}

	static SharedLobbyRankMap CacheSharedLobbyRanks(const nlohmann::json& party)
	{
		auto ranks = ParseSharedLobbyRanks(party);
		const auto localRank = ReadLocalLobbyRank();

		if (localRank)
		{
			ranks[PublicGuid()] = *localRank;
		}
		else
		{
			ranks.erase(PublicGuid());
		}

		const auto partyId = party.value("id", std::string{});

		{
			std::lock_guard _(sharedLobbyRankMutex);
			sharedLobbyRankPartyId = partyId;
			sharedLobbyRanks = ranks;
		}

		return ranks;
	}

	static SharedLobbyRankMap GetCachedSharedLobbyRanks()
	{
		std::lock_guard _(sharedLobbyRankMutex);
		return sharedLobbyRanks;
	}

	static std::string EncodeLowerHex(const std::string& bytes)
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

	static std::string FriendlyStateText(const std::string& state)
	{
		static const std::unordered_map<std::string, std::string> texts =
		{
			{ "SIGNING_IN", "SIGNING IN" },
			{ "LOGIN_REQUIRED", "LOGIN REQUIRED" },
			{ "SEARCH_STARTING", "STARTING SEARCH" },
			{ "IN_PARTY", "IN PARTY" },
			{ "MATCH_FOUND", "MATCH FOUND" },
			{ "MAP_VOTE", "MAP VOTE" },
			{ "READY_CHECK", "WAITING FOR READY" },
			{ "WAITING_FOR_READY", "WAITING FOR READY" },
			{ "RESERVING_SERVER", "RESERVING SERVER" },
			{ "STARTING_SERVER", "SERVER STARTING" },
			{ "SERVER_STARTING", "SERVER STARTING" },
			{ "COUNTDOWN", "JOIN COUNTDOWN" },
			{ "JOIN_PREVIEW", "JOINING GAME IN PROGRESS" },
			{ "DIRECT_CONNECTION", "DIRECT CONNECTION" },
			{ "RELAY_CONNECTION", "RELAY CONNECTION" },
			{ "IN_MATCH", "IN MATCH" },
		};

		const auto text = texts.find(state);

		if (text == texts.end())
		{
			return state;
		}

		return text->second;
	}

	static std::string FriendlyErrorText(const std::string& error)
	{
		static const std::unordered_map<std::string, std::string> texts =
		{
			{ "ZWNET_LOGIN_REQUIRED", "Sign in on the ZW3 Stats page." },
			{ "ZWNET_SESSION_EXPIRED", "Your ZW3 session expired. Please sign in again." },
			{ "ZWNET_SEARCH_FAILED", "The selected playlist could not enter matchmaking. Please try again." },
			{ "ZWNET_CATALOG_UNAVAILABLE", "Playlists are unavailable. Please refresh the list." },
			{ "ZWNET_PLAYLIST_REFRESH_REQUIRED", "This playlist changed or access ended. Refresh the list and choose again." },
			{ "ZWNET_PLAYLIST_SELECTION_FAILED", "The party playlist could not be changed. Refresh and try again." },
			{ "ZWNET_VERSION_MISMATCH", "Your ZW3 client version does not match the online service." },
			{ "ZWNET_LEADER_REQUIRED", "Only the party leader can start matchmaking." },
			{ "ZWNET_PARTY_TOO_LARGE", "This party has too many players for the selected playlist." },
			{ "ZWNET_CONTENT_MISSING", "Required ZW3 content is missing." },
			{ "ZWNET_ROUTE_UNAVAILABLE", "No direct or relay route is available." },
			{ "ZWNET_RELAY_TIMEOUT", "The relay did not confirm the connection. Return to the lobby and try again." },
			{ "ZWNET_SERVER_NOT_READY", "The assigned ZW3 server is no longer available. Return to the lobby and search again." },
			{ "ZWNET_DESCRIPTOR_INVALID", "The assigned server address is invalid." },
			{ "ZWNET_ACCOUNT_LINK_REQUIRED", "Link this GUID in ZW3 Stats Settings." },
			{ "ZWNET_SESSION_STORAGE_FAILED", "The secure ZW3 session could not be stored." },
			{ "ZWNET_REGISTRATION_UNAVAILABLE", "The ZW3 account page is unavailable." },
			{ "ZWNET_PARTY_FAILED", "The ZW3 party could not be created or loaded." },
			{ "ZWNET_PRIVATE_MATCH_FAILED", "The private ZW3 server could not be reserved." },
			{ "ZWNET_MAP_VOTE_FAILED", "Your map vote could not be submitted." },
			{ "ZWNET_MATCH_FAILED", "Server start failed. Return to the lobby and try again." },
			{ "ZWNET_GUID_COPY_FAILED", "The ZW3 GUID could not be copied." },
			{ "ZWNET_MANUAL_JOIN_DENIED", "Manual test access is unavailable. Check your invitation and sign in again." },
		};

		if (error.empty())
		{
			return {};
		}

		const auto text = texts.find(error);

		if (text == texts.end())
		{
			return "The ZW3 online service could not complete this request.";
		}

		return text->second;
	}

	static std::string JsonString(const nlohmann::json& object, const char* key, const char* fallback = "")
	{
		if (!object.is_object())
		{
			return fallback;
		}

		const auto value = object.find(key);

		if (value == object.end() || !value->is_string())
		{
			return fallback;
		}

		return value->get<std::string>();
	}

	static std::string SafeDisplayName(const std::string& name)
	{
		auto value = TextRenderer::EncodeUtf8ForGame(name, 48);
		Utils::String::Trim(value);

		if (value.empty())
		{
			return "ZW3 Player";
		}

		return value;
	}

	static std::string ResponseErrorCode(const nlohmann::json& response)
	{
		if (!response.contains("error") || !response.at("error").is_object())
		{
			return {};
		}

		return JsonString(response.at("error"), "code");
	}

	static bool IsActiveMatchmakingState(const std::string& state)
	{
		return state == "SEARCHING" || state == "MATCH_FOUND" || state == "MAP_VOTE"
			|| state == "READY_CHECK" || state == "WAITING_FOR_READY" || state == "RESERVING_SERVER"
			|| state == "STARTING_SERVER" || state == "SERVER_STARTING" || state == "CONNECTING"
			|| state == "IN_MATCH";
	}

	static std::uint64_t NextPresenceSequence()
	{
		static std::atomic_uint64_t sequence = 1;
		return sequence.fetch_add(1, std::memory_order_relaxed);
	}

	static bool TryCopyPublicGuidToClipboard()
	{
		if (!OpenClipboard(GetDesktopWindow()))
		{
			return false;
		}

		ScopeExit closeClipboard([]
		{
			CloseClipboard();
		});

		if (!EmptyClipboard())
		{
			return false;
		}

		const auto& guid = PublicGuid();
		auto* const memory = GlobalAlloc(GMEM_MOVEABLE, guid.size() + 1);

		if (!memory)
		{
			return false;
		}

		auto* const destination = static_cast<char*>(GlobalLock(memory));

		if (!destination)
		{
			GlobalFree(memory);
			return false;
		}

		std::memcpy(destination, guid.data(), guid.size() + 1);
		GlobalUnlock(memory);

		if (!SetClipboardData(CF_TEXT, memory))
		{
			GlobalFree(memory);
			return false;
		}

		return true;
	}

	static void ClearMatchDvars()
	{
		Dvar::Var("zwnet_vote_active").Set(false);
		Dvar::Var("zwnet_vote_selection").Set("");
		Dvar::Var("zwnet_all_ready").Set(false);
		Dvar::Var("zwnet_start_phase").Set("");
		Dvar::Var("zwnet_start_seconds").Set(0);
		Dvar::Var("zwnet_vote_reveal_time").Set(0);
		Dvar::Var("zwnet_vote_winner_id").Set("");
		Dvar::Var("zwnet_vote_winner_name").Set("");
		Dvar::Var("zwnet_vote_winner_image").Set("");
		Dvar::Var("zwnet_server_endpoint").Set("");
		Dvar::Var("zwnet_server_hostname").Set("");
		Dvar::Var("zwnet_server_status").Set("NOT ASSIGNED");
		Dvar::Var("zwnet_join_status").Set("WAITING IN LOBBY");
	}

	static void ClearMemberDvars(const std::string& prefix)
	{
		Dvar::Var(prefix + "_name").Set("");
		Dvar::Var(prefix + "_guid").Set("");
		Dvar::Var(prefix + "_role").Set("");
		Dvar::Var(prefix + "_ready").Set(false);
		Dvar::Var(prefix + "_self").Set(false);
		Dvar::Var(prefix + "_shared_rank_known").Set(false);
		Dvar::Var(prefix + "_shared_rank_level").Set(1);
		Dvar::Var(prefix + "_shared_rank_prestige").Set(0);
	}

	bool ZWNet::TryGetSharedLobbyRank(const std::string& guid, int& level, int& prestige)
	{
		auto normalizedGuid = guid;

		std::ranges::transform(normalizedGuid, normalizedGuid.begin(), [](const unsigned char character)
		{
			return static_cast<char>(std::tolower(character));
		});

		std::lock_guard _(sharedLobbyRankMutex);

		const auto rank = sharedLobbyRanks.find(normalizedGuid);

		if (rank == sharedLobbyRanks.end())
		{
			return false;
		}

		level = rank->second.level;
		prestige = rank->second.prestige;
		return true;
	}

	void ZWNet::EnqueueAsync(std::function<void()> task)
	{
		if (!isActive)
		{
			return;
		}

		std::lock_guard _(asyncTaskMutex);

		if (isActive)
		{
			asyncTasks.emplace_back(std::move(task));
		}
	}

	void ZWNet::ProcessAsyncTasks()
	{
		std::function<void()> task;

		{
			std::lock_guard _(asyncTaskMutex);

			if (!isActive || asyncTasks.empty())
			{
				return;
			}

			task = std::move(asyncTasks.front());
			asyncTasks.pop_front();
		}

		try
		{
			task();
		}
		catch (const std::exception&)
		{
			SetState("ERROR", "ZWNET_REQUEST_FAILED");
		}
	}

	std::string ZWNet::SessionPath()
	{
		const auto appdata = FileSystem::GetAppdataPath();

		if (appdata.empty())
		{
			return {};
		}

		const auto directory = appdata.parent_path() / BASEGAME;
		Utils::IO::CreateDir(directory.string());

		return (directory / "zwnet.session").string();
	}

	bool ZWNet::StoreSession(const std::string& newAccessToken, const std::string& newRefreshToken)
	{
		const auto path = SessionPath();

		if (newAccessToken.empty() || newRefreshToken.empty() || path.empty())
		{
			return false;
		}

		const auto plain = nlohmann::json{ { "access_token", newAccessToken }, { "refresh_token", newRefreshToken } }.dump();

		DATA_BLOB input{ static_cast<DWORD>(plain.size()), reinterpret_cast<BYTE*>(const_cast<char*>(plain.data())) };
		DATA_BLOB output{};

		if (!CryptProtectData(&input, L"ZW3 ZWNET session", nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &output))
		{
			return false;
		}

		const std::string encrypted(reinterpret_cast<char*>(output.pbData), output.cbData);
		LocalFree(output.pbData);

		if (!Utils::IO::WriteFile(path, encrypted))
		{
			return false;
		}

		std::lock_guard _(stateMutex);
		accessToken = newAccessToken;
		refreshToken = newRefreshToken;
		return true;
	}

	bool ZWNet::LoadSession()
	{
		const auto path = SessionPath();

		if (path.empty())
		{
			return false;
		}

		const auto encrypted = Utils::IO::ReadFile(path);

		if (encrypted.empty())
		{
			return false;
		}

		DATA_BLOB input{ static_cast<DWORD>(encrypted.size()), reinterpret_cast<BYTE*>(const_cast<char*>(encrypted.data())) };
		DATA_BLOB output{};

		if (!CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &output))
		{
			return false;
		}

		const std::string plain(reinterpret_cast<char*>(output.pbData), output.cbData);
		SecureZeroMemory(output.pbData, output.cbData);
		LocalFree(output.pbData);

		try
		{
			const auto data = nlohmann::json::parse(plain);

			std::lock_guard _(stateMutex);
			accessToken = data.at("access_token").get<std::string>();
			refreshToken = data.at("refresh_token").get<std::string>();
			return !accessToken.empty() && !refreshToken.empty();
		}
		catch (const nlohmann::json::exception&)
		{
			return false;
		}
	}

	void ZWNet::ClearSession()
	{
		isPlaylistSelectionStarting = false;
		ResetManagedRoute();

		{
			std::lock_guard _(stateMutex);

			std::ranges::fill(accessToken, '\0');
			std::ranges::fill(refreshToken, '\0');
			accessToken.clear();
			refreshToken.clear();
			currentPlayerId.clear();

			const auto path = SessionPath();

			if (!path.empty())
			{
				Utils::IO::RemoveFile(path);
			}
		}

		ClearPlaylistCatalog();
	}

	std::optional<nlohmann::json> ZWNet::Request(const std::string& method, const std::string& path, const nlohmann::json& body)
	{
		if (!isActive)
		{
			return std::nullopt;
		}

		try
		{
			std::string token;

			{
				std::lock_guard _(stateMutex);
				token = accessToken;
			}

			Utils::WebIO::Params headers = { { "Accept", "application/json" }, { "Content-Type", "application/json" } };

			if (!token.empty())
			{
				headers["Authorization"] = "Bearer " + token;
			}

			const auto url = std::string(apiBase) + path;
			bool isSuccessful = false;

			Utils::WebIO request(userAgent);
			request.SetTimeout(5000);
			request.SetReadHttpErrorBody(true);

			std::string response;

			if (method == "GET")
			{
				request.SetURL(url);
				response = request.Get(headers, &isSuccessful);
			}
			else
			{
				response = request.Post(url, body.dump(), headers, &isSuccessful);
			}

			if (!isActive || response.empty())
			{
				return std::nullopt;
			}

			auto parsed = nlohmann::json::parse(response);

			if (!isSuccessful && !parsed.contains("error"))
			{
				return std::nullopt;
			}

			return parsed;
		}
		catch (const std::exception&)
		{
			return std::nullopt;
		}
	}

	void ZWNet::SetState(const std::string& state, const std::string& error)
	{
		const auto stateText = FriendlyStateText(state);
		const auto errorText = FriendlyErrorText(error);

		SetPresenceUiState(state);

		Scheduler::Once([state, stateText, error, errorText]
		{
			if (!isActive)
			{
				return;
			}

			if (state == "ERROR")
			{
				Dvar::Var("zwnet_start_phase").Set("");
				Dvar::Var("zwnet_start_seconds").Set(0);
			}

			SetDisplayText("ui_zwnet_state", state);
			SetDisplayText("ui_zwnet_state_text", stateText);
			Dvar::Var("ui_zwnet_error").Set(error);
			Dvar::Var("ui_zwnet_error_text").Set(errorText);
		}, Scheduler::Pipeline::MAIN);
	}

	void ZWNet::Refresh()
	{
		if (!isActive)
		{
			return;
		}

		std::string refresh;

		{
			std::lock_guard _(stateMutex);
			refresh = refreshToken;
		}

		if (refresh.empty())
		{
			Login();
			return;
		}

		const auto result = Request("POST", "/social/client/refresh", { { "refresh_token", refresh } });

		if (!result || result->contains("error"))
		{
			if (!isActive)
			{
				return;
			}

			ClearSession();
			Login();
			return;
		}

		if (!isActive)
		{
			return;
		}

		const auto access = result->at("access_token").get<std::string>();
		StoreSession(access, result->value("refresh_token", access));

		const auto me = Request("GET", "/social/me");

		if (me && me->contains("id"))
		{
			std::lock_guard _(stateMutex);
			currentPlayerId = me->at("id").get<std::string>();
		}

		SetState("ONLINE");
		RefreshPlaylistCatalog(true);
		CompleteOnlineEntry();
		Request("POST", "/social/presence", { { "status", "MAIN_MENU" }, { "sequence", NextPresenceSequence() }, { "joinable", false } });
	}

	void ZWNet::Login()
	{
		if (!isActive)
		{
			return;
		}

		{
			std::lock_guard _(stateMutex);

			if (isLoginInFlight)
			{
				return;
			}

			isLoginInFlight = true;
		}

		SetState("SIGNING_IN");

		const auto deviceId = EncodeLowerHex(Utils::Cryptography::SHA256::Compute(Auth::GetMachineEntropy()));

		if (deviceId.size() != 64)
		{
			{
				std::lock_guard _(stateMutex);
				isLoginInFlight = false;
			}

			SetState("ERROR", "ZWNET_REQUEST_FAILED");
			return;
		}

		auto requestBody = nlohmann::json
		{
			{ "guid", PublicGuid() },
			{ "device_id", deviceId },
			{ "client_version", clientVersion },
			{ "mod_version", modVersion },
		};

		EnqueueAsync([requestBody = std::move(requestBody)]() mutable
		{
			CompleteLogin(std::move(requestBody));
		});
	}

	void ZWNet::CompleteLogin(nlohmann::json requestBody)
	{
		if (!isActive)
		{
			return;
		}

		ScopeExit loginGuard([]
		{
			std::lock_guard _(stateMutex);
			isLoginInFlight = false;
		});

		const auto result = Request("POST", "/social/client/login", requestBody);

		if (!isActive)
		{
			return;
		}

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
			std::lock_guard _(stateMutex);
			currentPlayerId = result->at("profile").at("id").get<std::string>();
		}

		SetState("ONLINE");
		RefreshPlaylistCatalog(true);
		CompleteOnlineEntry();
		Request("POST", "/social/presence", { { "status", "MAIN_MENU" }, { "sequence", NextPresenceSequence() }, { "joinable", false } });
	}

	void ZWNet::BeginOnlineEntry()
	{
		if (!isActive)
		{
			return;
		}

		isOnlineEntryPending = true;
		SetState("SIGNING_IN");

		bool hasRefreshToken = false;

		{
			std::lock_guard _(stateMutex);
			hasRefreshToken = !refreshToken.empty();
		}

		if (hasRefreshToken)
		{
			EnqueueAsync([]
			{
				Refresh();
			});
		}
		else
		{
			Login();
		}
	}

	std::optional<nlohmann::json> ZWNet::CurrentOrNewParty()
	{
		auto party = Request("GET", "/zwnet/parties/current");

		if (!party || party->is_null())
		{
			party = Request("POST", "/zwnet/parties/create", { { "visibility", PartyVisibilityName(desiredPartyPrivacy.load()) } });
		}

		return party;
	}

	void ZWNet::CompleteOnlineEntry()
	{
		EnqueueAsync([]
		{
			if (!isActive || !isOnlineEntryPending)
			{
				return;
			}

			const auto party = CurrentOrNewParty();

			if (!party || party->is_null() || party->contains("error"))
			{
				SetState("ERROR", "ZWNET_PARTY_FAILED");
			}
			else
			{
				const auto published = PublishLocalRank(*party);
				UpdateLobbyDvars(published);

				const auto partyState = published.value("state", "IDLE");
				const bool isMatchmaking = IsActiveMatchmakingState(partyState);
				isSearching = isMatchmaking;

				if (isMatchmaking)
				{
					SetState(partyState);
					UpdateMatchmaking();
				}
				else
				{
					SetState("IN_PARTY");
				}
			}

			Scheduler::Once([]
			{
				if (!isActive || !isOnlineEntryPending.exchange(false))
				{
					return;
				}

				auto* const connecting = Game::Menus_FindByName(Game::uiContext, "popup_zwnet_connecting");

				if (connecting)
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

		isSearching = false;
		isEndpointJoinInFlight = false;
		isServerJoinTransition = false;
		isOnlineEntryPending = false;
		isLocalPartyLeader = false;
		cachedPartyMemberCount = 0;
		cachedPartyVisibility = 2;
		isCachedPartyJoinStateSupported = false;

		{
			std::lock_guard _(stateMutex);
			currentPartyId.clear();
			currentProposalId.clear();
			currentMatchId.clear();
		}

		Dvar::Var("zwnet_lobby_active").Set(false);
		Dvar::Var("zwnet_lobby_party_id").Set("");
		Dvar::Var("zwnet_lobby_visibility").Set("OPEN");
		Dvar::Var("zwnet_lobby_owner").Set("");
		Dvar::Var("zwnet_lobby_member_count").Set(0);
		Dvar::Var("zwnet_lobby_status_text").Set("IDLE");
		Dvar::Var("zwnet_lobby_can_start").Set(false);
		Dvar::Var("zwnet_lobby_self_ready").Set(false);

		for (std::size_t i = 0; i < 4; ++i)
		{
			const auto prefix = std::format("zwnet_lobby_member_{}", i);

			ClearMemberDvars(prefix);
			Dvar::Var(prefix + "_rank_icon").Set("");
			Dvar::Var(prefix + "_rank_level").Set("");
		}

		ClearMatchDvars();
		Dvar::Var("zwnet_match_id").Set("");
		Dvar::Var("zwnet_join_countdown").Set(0);
		Dvar::Var("zwnet_managed_session").Set(false);
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
			if (isActive)
			{
				Command::Execute("openLink " + url, false);
			}
		}, Scheduler::Pipeline::MAIN);
	}

	static void ResetPlaylistCatalogLocked(const bool isStale)
	{
		++playlistCatalog.generation;
		playlistCatalog.entries.clear();
		playlistCatalog.pendingEntries.clear();
		playlistCatalog.accountId.clear();
		playlistCatalog.selectedId.clear();
		playlistCatalog.partySelectedId.clear();
		playlistCatalog.revision = 0;
		playlistCatalog.pendingRevision = 0;
		playlistCatalog.partySelectedRevision = 0;
		playlistCatalog.page = 0;
		playlistCatalog.isLoaded = false;
		playlistCatalog.isStale = isStale;
		playlistCatalog.isInFlight = false;
		playlistCatalog.hasPending = false;
		playlistCatalog.hasNotice = false;
	}

	void ZWNet::SchedulePlaylistCatalogPublish()
	{
		Scheduler::Once([]
		{
			PublishPlaylistCatalog();
		}, Scheduler::Pipeline::MAIN);
	}

	void ZWNet::ClearPlaylistCatalog()
	{
		isPlaylistSelectorOpen = false;
		++playlistSelectorGeneration;
		isPlaylistSelectionStarting = false;

		{
			std::lock_guard _(playlistCatalog.mutex);
			ResetPlaylistCatalogLocked(false);
		}

		SchedulePlaylistCatalogPublish();
	}

	void ZWNet::RefreshPlaylistCatalog(const bool isForced)
	{
		if (!isActive)
		{
			return;
		}

		std::string accountId;
		bool hasToken = false;

		{
			std::lock_guard _(stateMutex);
			accountId = currentPlayerId;
			hasToken = !accessToken.empty();
		}

		if (accountId.empty() || !hasToken)
		{
			return;
		}

		std::uint64_t generation = 0;
		bool hasAccountChanged = false;

		{
			std::lock_guard _(playlistCatalog.mutex);

			const auto now = std::chrono::steady_clock::now();

			if (playlistCatalog.accountId != accountId)
			{
				hasAccountChanged = true;
				ResetPlaylistCatalogLocked(false);
				playlistCatalog.accountId = accountId;
				playlistCatalog.lastAttempt = {};
			}

			if (playlistCatalog.isInFlight || (!isForced && now - playlistCatalog.lastAttempt < 30s))
			{
				return;
			}

			playlistCatalog.isInFlight = true;
			playlistCatalog.lastAttempt = now;
			generation = playlistCatalog.generation;
		}

		if (hasAccountChanged)
		{
			isPlaylistSelectorOpen = false;
			++playlistSelectorGeneration;
			SchedulePlaylistCatalogPublish();
		}

		const auto response = Request("GET", "/zwnet/playlists/catalog");

		if (response && response->is_object() && response->contains("error"))
		{
			const auto code = ResponseErrorCode(*response);

			if (code == "AUTH_REQUIRED" || code == "SESSION_INVALID")
			{
				{
					std::lock_guard _(playlistCatalog.mutex);

					if (playlistCatalog.generation != generation || playlistCatalog.accountId != accountId)
					{
						return;
					}

					ResetPlaylistCatalogLocked(true);
				}

				SchedulePlaylistCatalogPublish();
				return;
			}
		}

		std::int64_t revision = 0;
		std::vector<ClientPlaylist> entries;
		bool isValid = false;

		try
		{
			isValid = response && TryParseClientPlaylistCatalog(*response, revision, entries);

			if (isValid)
			{
				VerifyPlaylistContent(entries);
			}
		}
		catch (const nlohmann::json::exception&)
		{
			isValid = false;
		}

		std::string currentAccountId;

		{
			std::lock_guard _(stateMutex);
			currentAccountId = currentPlayerId;
		}

		{
			std::lock_guard _(playlistCatalog.mutex);

			if (playlistCatalog.generation != generation || playlistCatalog.accountId != accountId)
			{
				return;
			}

			playlistCatalog.isInFlight = false;

			if (currentAccountId != accountId)
			{
				ResetPlaylistCatalogLocked(true);
			}
			else if (!isValid)
			{
				playlistCatalog.isStale = true;
			}
			else
			{
				std::unordered_set<std::string> visibleIds;

				for (const auto& entry : entries)
				{
					visibleIds.insert(entry.id);
				}

				const auto isRevoked = [&visibleIds](const ClientPlaylist& entry)
				{
					return entry.audience == "RESTRICTED" && !visibleIds.contains(entry.id);
				};

				std::erase_if(playlistCatalog.entries, isRevoked);
				std::erase_if(playlistCatalog.pendingEntries, isRevoked);

				auto currentRevision = playlistCatalog.revision;

				if (playlistCatalog.hasPending)
				{
					currentRevision = playlistCatalog.pendingRevision;
				}

				const bool hasChanged = playlistCatalog.isLoaded && revision != currentRevision;

				playlistCatalog.isLoaded = true;
				playlistCatalog.isStale = false;

				if (hasChanged)
				{
					playlistCatalog.pendingEntries = std::move(entries);
					playlistCatalog.pendingRevision = revision;
					playlistCatalog.hasPending = true;
					playlistCatalog.hasNotice = true;
				}
				else if (!playlistCatalog.hasPending)
				{
					playlistCatalog.entries = std::move(entries);
					playlistCatalog.revision = revision;
				}
				else
				{
					playlistCatalog.pendingEntries = std::move(entries);
				}
			}
		}

		SchedulePlaylistCatalogPublish();

		if (isValid && currentAccountId == accountId)
		{
			PublishPartyContent();
		}
	}

	void ZWNet::BeginPlaylistSelection()
	{
		if (!isActive || isSearching || isInGame || isPlaylistSearchStarting)
		{
			return;
		}

		isPlaylistSelectorOpen = true;
		++playlistSelectorGeneration;
		playlistSelectorOpenedAtMs = PlaylistClockMilliseconds();

		Dvar::Var("zwnet_catalog_action_error").Set("");
		SetState("IN_PARTY");
		PublishPlaylistCatalog();
	}

	void ZWNet::CancelPlaylistSelection()
	{
		isPlaylistSelectorOpen = false;
		++playlistSelectorGeneration;

		Dvar::Var("zwnet_catalog_action_error").Set("");
		PublishPlaylistCatalog();
	}

	void ZWNet::HighlightPlaylistSlot(const int slot)
	{
		if (!isPlaylistSelectorOpen || isPlaylistSelectionStarting || slot < 0 || slot >= static_cast<int>(playlistPageSize))
		{
			return;
		}

		{
			std::lock_guard _(playlistCatalog.mutex);

			const auto index = playlistCatalog.page * playlistPageSize + slot;

			if (!playlistCatalog.isLoaded || index >= playlistCatalog.entries.size())
			{
				return;
			}

			playlistCatalog.selectedId = playlistCatalog.entries[index].id;
		}

		Dvar::Var("zwnet_catalog_action_error").Set("");
		PublishPlaylistCatalog();
	}

	void ZWNet::ActivatePlaylistSlot(const int slot)
	{
		if (!isPlaylistSelectorOpen || isPlaylistSearchStarting || isSearching || isInGame
			|| slot < 0 || slot >= static_cast<int>(playlistPageSize))
		{
			return;
		}

		if (PlaylistClockMilliseconds() - playlistSelectorOpenedAtMs < 200)
		{
			return;
		}

		if (!isLocalPartyLeader)
		{
			Dvar::Var("zwnet_catalog_action_error").Set("Only the party leader can start matchmaking.");
			return;
		}

		std::string playlistId;
		std::int64_t playlistRevision = 0;

		{
			std::lock_guard _(playlistCatalog.mutex);

			const auto index = playlistCatalog.page * playlistPageSize + slot;

			if (!playlistCatalog.isLoaded || playlistCatalog.isStale || index >= playlistCatalog.entries.size())
			{
				Dvar::Var("zwnet_catalog_action_error").Set("The playlist list is not ready. Refresh and try again.");
				return;
			}

			const auto& entry = playlistCatalog.entries[index];
			playlistCatalog.selectedId = entry.id;

			if (entry.availability != "AVAILABLE")
			{
				Dvar::Var("zwnet_catalog_action_error").Set(entry.availabilityDetail);
				return;
			}

			playlistId = entry.id;
			playlistRevision = entry.revision;
		}

		if (isPlaylistSelectionStarting.exchange(true))
		{
			return;
		}

		const auto generation = playlistSelectorGeneration.load();

		Dvar::Var("zwnet_catalog_action_error").Set("");
		PublishPlaylistCatalog();

		EnqueueAsync([playlistId, playlistRevision, generation]
		{
			StartQuickPlay(playlistId, playlistRevision, generation);
		});
	}

	void ZWNet::ChangePlaylistPage(const int direction)
	{
		if (direction != -1 && direction != 1)
		{
			return;
		}

		{
			std::lock_guard _(playlistCatalog.mutex);

			const auto pageCount = std::max<std::size_t>(1, (playlistCatalog.entries.size() + playlistPageSize - 1) / playlistPageSize);

			if (direction < 0 && playlistCatalog.page > 0)
			{
				--playlistCatalog.page;
			}
			else if (direction > 0 && playlistCatalog.page + 1 < pageCount)
			{
				++playlistCatalog.page;
			}
		}

		PublishPlaylistCatalog();
	}

	void ZWNet::AcknowledgePlaylistNotice()
	{
		{
			std::lock_guard _(playlistCatalog.mutex);
			playlistCatalog.hasNotice = false;
		}

		PublishPlaylistCatalog();
	}

	std::optional<nlohmann::json> ZWNet::PublishPartyContent()
	{
		if (!isActive)
		{
			return std::nullopt;
		}

		std::string partyId;

		{
			std::lock_guard _(stateMutex);
			partyId = currentPartyId;
		}

		if (partyId.empty())
		{
			return std::nullopt;
		}

		std::unordered_set<std::string> unique;

		{
			std::lock_guard _(playlistCatalog.mutex);

			for (const auto* collection : { &playlistCatalog.entries, &playlistCatalog.pendingEntries })
			{
				for (const auto& entry : *collection)
				{
					for (const auto& contentId : entry.verifiedContent)
					{
						unique.insert(contentId);
					}
				}
			}
		}

		std::vector<std::string> content(unique.begin(), unique.end());
		std::ranges::sort(content);

		const auto result = Request("POST", "/zwnet/parties/" + partyId + "/content",
			{ { "content", content }, { "client_version", clientVersion }, { "mod_version", modVersion } });

		if (result && result->is_object() && !result->contains("error"))
		{
			UpdateLobbyDvars(*result);
		}

		return result;
	}

	void ZWNet::StartQuickPlay(const std::string& playlistId, const std::int64_t playlistRevision, const std::uint64_t selectorGeneration)
	{
		std::vector<std::string> verifiedContent;
		std::string playlistName;
		std::string playlistSettings;

		{
			std::lock_guard _(playlistCatalog.mutex);

			const auto selected = std::ranges::find_if(playlistCatalog.entries, [&playlistId, playlistRevision](const ClientPlaylist& entry)
			{
				return entry.id == playlistId && entry.revision == playlistRevision;
			});

			const bool isSelectable = playlistCatalog.isLoaded && !playlistCatalog.isStale
				&& selected != playlistCatalog.entries.end() && selected->availability == "AVAILABLE";

			if (isSelectable)
			{
				verifiedContent = selected->verifiedContent;
				playlistName = selected->name;
				playlistSettings = selected->zombieSettingsSummary;
			}
		}

		if (playlistName.empty() || !IsCurrentPlaylistActivation(selectorGeneration))
		{
			isSearching = false;
			isPlaylistSelectionStarting = false;

			if (IsCurrentPlaylistActivation(selectorGeneration))
			{
				SetState("IN_PARTY", "ZWNET_PLAYLIST_REFRESH_REQUIRED");
			}

			SchedulePlaylistCatalogPublish();
			return;
		}

		if (isPlaylistSearchStarting.exchange(true))
		{
			isPlaylistSelectionStarting = false;
			return;
		}

		ScopeExit searchGuard([]
		{
			isPlaylistSearchStarting = false;
			isPlaylistSelectionStarting = false;
			SchedulePlaylistCatalogPublish();
		});

		SchedulePlaylistCatalogPublish();
		ResetMatchLobbySoundSnapshot();
		SetState("SEARCH_STARTING");

		const auto fail = [](const std::string& error)
		{
			isSearching = false;
			SetState("IN_PARTY", error);
		};

		const auto isCancelled = [selectorGeneration]
		{
			if (IsCurrentPlaylistActivation(selectorGeneration))
			{
				return false;
			}

			isSearching = false;
			SetState("IN_PARTY");
			return true;
		};

		const auto accept = [selectorGeneration, playlistId, playlistName, playlistSettings, playlistRevision](const std::string& state)
		{
			if (!IsCurrentPlaylistActivation(selectorGeneration))
			{
				return false;
			}

			isPlaylistSelectorOpen = false;
			++playlistSelectorGeneration;
			isSearching = true;
			SetState(state);

			Scheduler::Once([playlistId, playlistName, playlistSettings, playlistRevision]
			{
				if (!isActive)
				{
					return;
				}

				Dvar::Var("zwnet_search_playlist_id").Set(playlistId);
				Dvar::Var("zwnet_search_playlist_name").Set(playlistName);
				Dvar::Var("zwnet_search_playlist_revision").Set(static_cast<int>(playlistRevision));
				Dvar::Var("zwnet_search_playlist_settings").Set(playlistSettings);
				Dvar::Var("zwnet_catalog_action_error").Set("");
				Command::Execute("closemenu popup_zwnet_playlists", false);
			}, Scheduler::Pipeline::MAIN);

			return true;
		};

		auto party = Request("GET", "/zwnet/parties/current");

		if (isCancelled())
		{
			return;
		}

		if (!party || party->is_null())
		{
			party = Request("POST", "/zwnet/parties/create", { { "visibility", PartyVisibilityName(desiredPartyPrivacy.load()) } });

			if (isCancelled())
			{
				return;
			}
		}

		if (!party || party->is_null() || party->contains("error"))
		{
			fail("ZWNET_PARTY_FAILED");
			return;
		}

		party = ApplyPartyVisibility(std::move(*party));

		if (isCancelled())
		{
			return;
		}

		if (!party)
		{
			fail("ZWNET_PARTY_FAILED");
			return;
		}

		const auto partyId = JsonString(*party, "id");

		std::string playerId;

		{
			std::lock_guard _(stateMutex);
			playerId = currentPlayerId;
		}

		if (JsonString(*party, "leader_id") != playerId)
		{
			fail("ZWNET_LEADER_REQUIRED");
			return;
		}

		const auto contentResult = Request("POST", "/zwnet/parties/" + partyId + "/content",
			{ { "content", verifiedContent }, { "client_version", clientVersion }, { "mod_version", modVersion } });

		if (isCancelled())
		{
			return;
		}

		if (!contentResult || !contentResult->is_object() || contentResult->contains("error"))
		{
			fail("ZWNET_CONTENT_MISSING");
			return;
		}

		party = *contentResult;

		std::int64_t selectedRevision = 0;

		if (party->contains("selected_playlist_revision") && party->at("selected_playlist_revision").is_number_integer())
		{
			selectedRevision = party->at("selected_playlist_revision").get<std::int64_t>();
		}

		if (JsonString(*party, "selected_playlist_id") != playlistId || selectedRevision != playlistRevision)
		{
			const auto selectedParty = Request("POST", "/zwnet/parties/" + partyId + "/select-playlist",
				{ { "playlist_id", playlistId }, { "playlist_revision", playlistRevision } });

			const bool isSelected = selectedParty && selectedParty->is_object() && !selectedParty->contains("error");

			if (isSelected)
			{
				UpdateLobbyDvars(*selectedParty);
			}

			if (isCancelled())
			{
				return;
			}

			if (!isSelected)
			{
				std::string code;

				if (selectedParty && selectedParty->is_object())
				{
					code = ResponseErrorCode(*selectedParty);
				}

				if (code == "PLAYLIST_REVISION_STALE" || code == "PLAYLIST_ACCESS_DENIED" || code == "PLAYLIST_NO_CAPACITY")
				{
					RefreshPlaylistCatalog(true);
					fail("ZWNET_PLAYLIST_REFRESH_REQUIRED");
				}
				else if (code == "LEADER_REQUIRED")
				{
					fail("ZWNET_LEADER_REQUIRED");
				}
				else
				{
					fail("ZWNET_PLAYLIST_SELECTION_FAILED");
				}

				return;
			}

			party = *selectedParty;
		}

		*party = PublishLocalRank(std::move(*party));

		if (isCancelled())
		{
			return;
		}

		UpdateLobbyDvars(*party);

		const auto partyState = party->value("state", "IDLE");

		if (IsActiveMatchmakingState(partyState))
		{
			if (accept(partyState))
			{
				UpdateMatchmaking();
			}

			return;
		}

		if (isCancelled())
		{
			return;
		}

		const nlohmann::json search =
		{
			{ "playlist_id", playlistId },
			{ "playlist_revision", playlistRevision },
			{ "region_id", "eu-central" },
			{ "client_version", clientVersion },
			{ "mod_version", modVersion },
			{ "content", verifiedContent },
			{ "ping_ms", 50 },
		};

		const auto result = Request("POST", "/zwnet/matchmaking/search", search);

		if (!IsCurrentPlaylistActivation(selectorGeneration))
		{
			bool isQueued = false;

			if (result)
			{
				const auto code = ResponseErrorCode(*result);
				isQueued = !result->contains("error") || code == "ALREADY_QUEUED" || code == "ALREADY_MATCHED";
			}

			if (isQueued)
			{
				Request("POST", "/zwnet/matchmaking/cancel");

				const auto current = Request("GET", "/zwnet/parties/current");

				if (current && current->is_object() && !current->contains("error"))
				{
					UpdateLobbyDvars(*current);
				}
			}

			isSearching = false;
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
				if (accept("SEARCHING"))
				{
					UpdateMatchmaking();
				}

				return;
			}

			if (code == "PLAYLIST_REVISION_STALE" || code == "PARTY_PLAYLIST_MISMATCH" || code == "PLAYLIST_NO_CAPACITY" || code == "PLAYLIST_ACCESS_DENIED")
			{
				RefreshPlaylistCatalog(true);
				fail("ZWNET_PLAYLIST_REFRESH_REQUIRED");
			}
			else if (code == "VERSION_MISMATCH")
			{
				fail("ZWNET_VERSION_MISMATCH");
			}
			else if (code == "LEADER_REQUIRED")
			{
				fail("ZWNET_LEADER_REQUIRED");
			}
			else if (code == "PARTY_TOO_LARGE")
			{
				fail("ZWNET_PARTY_TOO_LARGE");
			}
			else if (code == "CONTENT_MISSING" || code == "PARTY_CONTENT_MISSING")
			{
				fail("ZWNET_CONTENT_MISSING");
			}
			else
			{
				fail("ZWNET_SEARCH_FAILED");
			}

			return;
		}

		const auto joinedState = JsonString(*result, "state", "SEARCHING");

		if (accept(joinedState) && joinedState != "SEARCHING")
		{
			UpdateMatchmaking();
		}
	}

	std::optional<nlohmann::json> ZWNet::ApplyPartyVisibility(nlohmann::json party)
	{
		if (!party.is_object() || party.contains("error"))
		{
			return std::nullopt;
		}

		const auto partyId = JsonString(party, "id");

		if (!IsOpaquePartyId(partyId))
		{
			return std::nullopt;
		}

		std::string playerId;

		{
			std::lock_guard _(stateMutex);
			playerId = currentPlayerId;
		}

		const bool isLeader = !playerId.empty() && JsonString(party, "leader_id") == playerId;
		isLocalPartyLeader = isLeader;

		if (!isLeader)
		{
			return party;
		}

		const std::string desiredVisibility = PartyVisibilityName(desiredPartyPrivacy.load());
		const auto visibility = NormalizePartyVisibility(JsonString(party, "visibility", "OPEN"));

		if (visibility == desiredVisibility)
		{
			return party;
		}

		const auto updated = Request("POST", "/zwnet/parties/" + partyId + "/set-visibility", { { "visibility", desiredVisibility } });

		if (!updated || !updated->is_object() || updated->contains("error"))
		{
			return std::nullopt;
		}

		return *updated;
	}

	void ZWNet::RefreshPartyVisibility()
	{
		ScopeExit resetPending([]
		{
			isVisibilitySyncPending = false;
		});

		if (!isActive || !isLocalPartyLeader)
		{
			return;
		}

		const auto party = Request("GET", "/zwnet/parties/current");

		if (!party || party->is_null() || party->contains("error"))
		{
			return;
		}

		const auto updated = ApplyPartyVisibility(*party);

		if (updated)
		{
			UpdateLobbyDvars(*updated);
		}
	}

	void ZWNet::CapturePartyPrivacy()
	{
		if (!isActive)
		{
			return;
		}

		const auto privacy = std::clamp(Dvar::Var("partyPrivacy").Get<int>(), 0, 2);
		desiredPartyPrivacy = privacy;

		if (isLocalPartyLeader && cachedPartyVisibility.load() != privacy && !isVisibilitySyncPending.exchange(true))
		{
			EnqueueAsync([]
			{
				RefreshPartyVisibility();
			});
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

		const auto& guid = PublicGuid();
		const auto rank = ReadLocalLobbyRank();

		if (!rank)
		{
			CacheSharedLobbyRanks(party);
			return party;
		}

		const auto publishedRanks = ParseSharedLobbyRanks(party);
		const auto publishedRank = publishedRanks.find(guid);

		if (publishedRank != publishedRanks.end() && publishedRank->second.level == rank->level && publishedRank->second.prestige == rank->prestige)
		{
			CacheSharedLobbyRanks(party);
			return party;
		}

		const auto now = std::chrono::steady_clock::now();

		{
			std::lock_guard _(sharedLobbyRankMutex);

			const bool hasRankChanged = partyId != lastRankPublishPartyId || rank->level != lastRankPublishLevel || rank->prestige != lastRankPublishPrestige;

			if (!hasRankChanged && now < nextRankPublishAttempt)
			{
				sharedLobbyRanks[guid] = *rank;
				return party;
			}

			lastRankPublishPartyId = partyId;
			lastRankPublishLevel = rank->level;
			lastRankPublishPrestige = rank->prestige;
			nextRankPublishAttempt = now + std::chrono::seconds(30);
		}

		const auto result = Request("POST", "/zwnet/parties/" + partyId + "/player-rank", { { "level", rank->level }, { "prestige", rank->prestige } });

		if (result && result->is_object() && !result->contains("error"))
		{
			party = *result;
		}

		CacheSharedLobbyRanks(party);
		return party;
	}

	static bool IsPartyMemberReady(const nlohmann::json& member)
	{
		if (!member.is_object())
		{
			return false;
		}

		const auto ready = member.find("ready");

		if (ready == member.end())
		{
			return false;
		}

		if (ready->is_boolean())
		{
			return ready->get<bool>();
		}

		if (ready->is_number_integer())
		{
			return ready->get<std::int64_t>() == 1;
		}

		return false;
	}

	void ZWNet::UpdateLobbyDvars(const nlohmann::json& party)
	{
		if (party.is_null() || !party.is_object())
		{
			return;
		}

		const auto sharedRanks = CacheSharedLobbyRanks(party);
		const auto partyId = JsonString(party, "id");
		const auto leaderId = JsonString(party, "leader_id");
		const auto state = JsonString(party, "state", "IDLE");
		const auto members = party.value("members", nlohmann::json::array());

		std::string playerId;

		{
			std::lock_guard _(stateMutex);
			playerId = currentPlayerId;
		}

		isLocalPartyLeader = !playerId.empty() && playerId == leaderId;

		const auto visibility = NormalizePartyVisibility(JsonString(party, "visibility", "OPEN"));
		cachedPartyMemberCount = static_cast<int>(members.size());
		cachedPartyVisibility = PartyVisibilityValue(visibility);
		isCachedPartyJoinStateSupported = IsPartyJoinStateSupported(state);

		std::string owner;

		for (const auto& member : members)
		{
			if (JsonString(member, "player_id") == leaderId)
			{
				owner = SafeDisplayName(JsonString(member, "display_name"));
				break;
			}
		}

		{
			std::lock_guard _(stateMutex);
			currentPartyId = partyId;
		}

		auto selectedPlaylistId = JsonString(party, "selected_playlist_id");
		std::int64_t selectedPlaylistRevision = 0;

		if (party.contains("selected_playlist_revision") && party.at("selected_playlist_revision").is_number_integer())
		{
			selectedPlaylistRevision = party.at("selected_playlist_revision").get<std::int64_t>();
		}

		if (selectedPlaylistId.size() > 96 || selectedPlaylistRevision < 1 || !IsPlaylistId(selectedPlaylistId))
		{
			selectedPlaylistId.clear();
			selectedPlaylistRevision = 0;
		}

		{
			std::lock_guard _(playlistCatalog.mutex);

			playlistCatalog.partySelectedId = selectedPlaylistId;
			playlistCatalog.partySelectedRevision = selectedPlaylistRevision;

			if (!selectedPlaylistId.empty() && (!isPlaylistSelectorOpen || playlistCatalog.selectedId.empty()))
			{
				playlistCatalog.selectedId = selectedPlaylistId;
			}
		}

		SchedulePlaylistCatalogPublish();

		const auto stateText = FriendlyStateText(state);

		Scheduler::Once([partyId, leaderId, state, stateText, owner, members, playerId, sharedRanks, visibility]
		{
			if (!isActive)
			{
				return;
			}

			Dvar::Var("zwnet_lobby_active").Set(true);
			SetDisplayText("zwnet_lobby_party_id", partyId);
			Dvar::Var("zwnet_lobby_visibility").Set(visibility);
			Dvar::Var("zwnet_lobby_owner").Set(owner);
			Dvar::Var("zwnet_lobby_member_count").Set(static_cast<int>(members.size()));
			SetDisplayText("zwnet_lobby_status_text", stateText);

			bool isAllReady = !members.empty();
			bool isSelfReady = false;

			for (std::size_t i = 0; i < 4; ++i)
			{
				const auto prefix = std::format("zwnet_lobby_member_{}", i);

				if (i >= members.size())
				{
					ClearMemberDvars(prefix);
					continue;
				}

				const auto& member = members[i];
				const auto memberId = JsonString(member, "player_id");
				const bool isReady = IsPartyMemberReady(member);
				const bool isSelf = memberId == playerId;

				Dvar::Var(prefix + "_name").Set(SafeDisplayName(JsonString(member, "display_name")));
				SetDisplayText(prefix + "_guid", memberId);

				if (memberId == leaderId)
				{
					Dvar::Var(prefix + "_role").Set("PARTY LEADER");
				}
				else
				{
					Dvar::Var(prefix + "_role").Set("MEMBER");
				}

				Dvar::Var(prefix + "_ready").Set(isReady);
				Dvar::Var(prefix + "_self").Set(isSelf);

				const auto rank = sharedRanks.find(memberId);
				const bool isRankKnown = rank != sharedRanks.end();

				Dvar::Var(prefix + "_shared_rank_known").Set(isRankKnown);

				if (isRankKnown)
				{
					Dvar::Var(prefix + "_shared_rank_level").Set(rank->second.level);
					Dvar::Var(prefix + "_shared_rank_prestige").Set(rank->second.prestige);
				}
				else
				{
					Dvar::Var(prefix + "_shared_rank_level").Set(1);
					Dvar::Var(prefix + "_shared_rank_prestige").Set(0);
				}

				isAllReady = isAllReady && isReady;

				if (isSelf)
				{
					isSelfReady = isReady;
				}
			}

			const bool canStart = playerId == leaderId && isAllReady && state != "SEARCHING" && state != "IN_MATCH";

			Dvar::Var("zwnet_lobby_self_ready").Set(isSelfReady);
			Dvar::Var("zwnet_all_ready").Set(isAllReady);
			Dvar::Var("zwnet_lobby_can_start").Set(canStart);
		}, Scheduler::Pipeline::MAIN);
	}

	void ZWNet::ResumeParty(const nlohmann::json& party)
	{
		if (!isActive || !party.is_object() || JsonString(party, "id").empty())
		{
			return;
		}

		CancelJoinInProgressPreview();

		const auto resumedParty = PublishLocalRank(party);
		const auto partyState = JsonString(resumedParty, "state", "IDLE");
		const bool isMatchmaking = IsActiveMatchmakingState(partyState);

		isSearching = isMatchmaking;
		UpdateLobbyDvars(resumedParty);

		if (isMatchmaking)
		{
			SetState(partyState);
		}
		else
		{
			SetState("IN_PARTY");
		}

		Scheduler::Once([isMatchmaking]
		{
			if (!isActive)
			{
				return;
			}

			if (isMatchmaking)
			{
				Command::Execute("openmenu zwnet_matchmaking", false);
			}
			else
			{
				Command::Execute("openmenu zwnet_party_lobby", false);
			}
		}, Scheduler::Pipeline::MAIN);

		if (isMatchmaking)
		{
			UpdateMatchmaking();
		}
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
			const auto response = Request("POST", "/zwnet/parties/" + partyId + "/join", nlohmann::json::object());

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
			const auto response = Request("POST", "/zwnet/parties/join-capability", { { "capability", capability } });

			if (!response || !response->is_object() || response->contains("error"))
			{
				SetState("ERROR", "ZWNET_PARTY_FAILED");
				return;
			}

			ResumeParty(*response);
		});
	}

	static void ConnectWhenUnmanaged(const std::string& endpoint)
	{
		Scheduler::Once([endpoint]
		{
			isEndpointJoinInFlight = false;

			const Network::Address target(endpoint);

			if (target.IsValid())
			{
				Party::Connect(target, false, true);
			}
		}, Scheduler::Pipeline::MAIN);
	}

	bool ZWNet::BeginEndpointJoin(const std::string& endpoint)
	{
		if (!isActive || endpoint.empty() || endpoint.size() > 255)
		{
			return false;
		}

		const Network::Address requestedTarget(endpoint);

		if (!requestedTarget.IsValid())
		{
			return false;
		}

		bool isKnownAssignedEndpoint = false;
		bool hasAccessToken = false;

		{
			std::lock_guard _(stateMutex);
			hasAccessToken = !accessToken.empty();

			const Network::Address assignedTarget(Dvar::Var("zwnet_server_endpoint").Get<std::string>());
			isKnownAssignedEndpoint = !currentMatchId.empty() && assignedTarget.IsValid() && assignedTarget == requestedTarget;
		}

		isEndpointJoinInFlight = true;

		if (!hasAccessToken)
		{
			ConnectWhenUnmanaged(endpoint);
			return true;
		}

		EnqueueAsync([endpoint, isKnownAssignedEndpoint]
		{
			const auto response = Request("POST", "/zwnet/matchmaking/join-by-endpoint", { { "endpoint", endpoint } });

			if (!response || !response->is_object())
			{
				if (isKnownAssignedEndpoint)
				{
					isEndpointJoinInFlight = false;
					SetState("ERROR", "ZWNET_REQUEST_FAILED");
					return;
				}

				ConnectWhenUnmanaged(endpoint);
				return;
			}

			if (response->contains("error"))
			{
				isEndpointJoinInFlight = false;
				SetState("ERROR", "ZWNET_PARTY_FAILED");
				return;
			}

			if (!response->value("managed", false))
			{
				Scheduler::Once([endpoint]
				{
					isEndpointJoinInFlight = false;

					const Network::Address target(endpoint);

					if (target.IsValid())
					{
						Party::Connect(target);
					}
				}, Scheduler::Pipeline::MAIN);

				return;
			}

			const auto party = response->find("party");

			if (!response->value("connect", false) || party == response->end() || !party->is_object() || party->contains("error"))
			{
				isEndpointJoinInFlight = false;
				SetState("ERROR", "ZWNET_PARTY_FAILED");
				return;
			}

			isEndpointJoinInFlight = false;
			ResumeParty(*party);
			isSearching = true;
			UpdateMatchmaking();
		});

		return true;
	}

	void ZWNet::EnterLobby(std::string map)
	{
		auto party = CurrentOrNewParty();

		if (!party || party->contains("error"))
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

		const auto partyId = JsonString(*party, "id");

		if (!partyId.empty())
		{
			Request("POST", "/zwnet/parties/" + partyId + "/set-map", { { "map", map } });
			Request("POST", "/zwnet/parties/" + partyId + "/set-mode", { { "mode", "zw3" } });
		}

		SetState("IN_PARTY");
	}

	void ZWNet::RefreshLobby()
	{
		auto party = Request("GET", "/zwnet/parties/current");

		if (!party || party->is_null() || party->contains("error"))
		{
			return;
		}

		*party = PublishLocalRank(std::move(*party));
		UpdateLobbyDvars(*party);

		const auto partyState = party->value("state", "IDLE");

		if (IsActiveMatchmakingState(partyState))
		{
			isSearching = true;
			SetState(partyState);
			UpdateMatchmaking();
		}
	}

	void ZWNet::RefreshActiveParty()
	{
		if (!isActive || isSearching || isClosingOnlineSession)
		{
			return;
		}

		std::string partyId;

		{
			std::lock_guard _(stateMutex);
			partyId = currentPartyId;
		}

		if (partyId.empty())
		{
			return;
		}

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

			std::lock_guard _(stateMutex);
			currentPartyId.clear();
			currentProposalId.clear();
			currentMatchId.clear();
		}

		isSearching = false;

		Scheduler::Once([]
		{
			if (!isActive)
			{
				return;
			}

			Dvar::Var("zwnet_lobby_active").Set(false);
			Dvar::Var("zwnet_vote_active").Set(false);
			Dvar::Var("zwnet_all_ready").Set(false);
			Dvar::Var("zwnet_start_phase").Set("");
			Dvar::Var("zwnet_start_seconds").Set(0);
		}, Scheduler::Pipeline::MAIN);
	}

	struct LobbyMemberSnapshot
	{
		std::string playerId;
		std::string displayName;
		std::string role;
		bool isReady = false;
		bool isRankKnown = false;
		int rankLevel = 1;
		int rankPrestige = 0;
	};

	void ZWNet::UpdateMatchLobbyDvars(const nlohmann::json& status)
	{
		if (!status.contains("lobby") || !status.at("lobby").is_object())
		{
			return;
		}

		const auto& lobby = status.at("lobby");
		const auto lobbyMembers = lobby.find("members");

		if (lobbyMembers == lobby.end() || !lobbyMembers->is_array())
		{
			return;
		}

		std::array<LobbyMemberSnapshot, 4> members{};
		std::unordered_set<std::string> memberIds;
		std::size_t memberCount = 0;

		for (const auto& member : *lobbyMembers)
		{
			if (memberCount >= members.size())
			{
				break;
			}

			if (!member.is_object())
			{
				continue;
			}

			auto& snapshot = members[memberCount];
			++memberCount;

			snapshot.playerId = JsonString(member, "player_id");

			if (!snapshot.playerId.empty())
			{
				memberIds.emplace(snapshot.playerId);
			}

			snapshot.displayName = SafeDisplayName(JsonString(member, "display_name"));
			snapshot.role = JsonString(member, "role", "MEMBER");

			const auto rank = member.find("rank");

			if (rank != member.end() && rank->is_object())
			{
				snapshot.rankLevel = std::clamp(rank->value("level", 1), 1, 54);
				snapshot.rankPrestige = std::max(rank->value("prestige", 0), 0);
				snapshot.isRankKnown = rank->contains("level");
			}
			else if (member.contains("level") && member.at("level").is_number_integer())
			{
				snapshot.rankLevel = std::clamp(member.at("level").get<int>(), 1, 54);
				snapshot.rankPrestige = std::max(JsonIntegerOr(member, "prestige", 0), 0);
				snapshot.isRankKnown = true;
			}
			else if (member.contains("rank_level"))
			{
				snapshot.rankLevel = std::clamp(member.value("rank_level", 1), 1, 54);
				snapshot.rankPrestige = std::max(member.value("rank_prestige", 0), 0);
				snapshot.isRankKnown = true;
			}

			if (!snapshot.isRankKnown)
			{
				snapshot.isRankKnown = Friends::TryGetZombieRankByGuid(snapshot.playerId, snapshot.rankLevel, snapshot.rankPrestige);
			}

			const auto ready = member.find("ready");

			if (ready != member.end() && ready->is_boolean())
			{
				snapshot.isReady = ready->get<bool>();
			}
			else if (ready != member.end() && ready->is_number_integer())
			{
				snapshot.isReady = ready->get<std::int64_t>() != 0;
			}
		}

		const auto soundDelta = ObserveMatchLobbyMembers(JsonString(status, "match_id"), memberIds);
		const auto lobbyState = JsonString(status, "state", "WAITING_FOR_READY");

		cachedPartyMemberCount = static_cast<int>(memberCount);
		isCachedPartyJoinStateSupported = IsPartyJoinStateSupported(lobbyState);

		const auto stateText = FriendlyStateText(lobbyState);
		const auto sharedRanks = GetCachedSharedLobbyRanks();

		std::string playerId;

		{
			std::lock_guard _(stateMutex);
			playerId = currentPlayerId;
		}

		Scheduler::Once([members = std::move(members), memberCount, stateText, playerId, sharedRanks, soundDelta]
		{
			if (!isActive)
			{
				return;
			}

			Dvar::Var("zwnet_lobby_active").Set(true);
			Dvar::Var("zwnet_lobby_member_count").Set(static_cast<int>(memberCount));
			SetDisplayText("zwnet_lobby_status_text", stateText);

			bool isAllReady = memberCount > 0;
			bool isSelfReady = false;

			for (std::size_t i = 0; i < members.size(); ++i)
			{
				const auto prefix = std::format("zwnet_lobby_member_{}", i);

				if (i >= memberCount)
				{
					ClearMemberDvars(prefix);
					continue;
				}

				const auto& member = members[i];
				const auto rank = sharedRanks.find(member.playerId);
				const bool isSharedRankKnown = rank != sharedRanks.end();

				Dvar::Var(prefix + "_name").Set(member.displayName);
				SetDisplayText(prefix + "_guid", member.playerId);
				SetDisplayText(prefix + "_role", member.role);
				Dvar::Var(prefix + "_ready").Set(member.isReady);
				Dvar::Var(prefix + "_self").Set(member.playerId == playerId);
				Dvar::Var(prefix + "_shared_rank_known").Set(member.isRankKnown || isSharedRankKnown);

				if (member.isRankKnown)
				{
					Dvar::Var(prefix + "_shared_rank_level").Set(member.rankLevel);
					Dvar::Var(prefix + "_shared_rank_prestige").Set(member.rankPrestige);
				}
				else if (isSharedRankKnown)
				{
					Dvar::Var(prefix + "_shared_rank_level").Set(rank->second.level);
					Dvar::Var(prefix + "_shared_rank_prestige").Set(rank->second.prestige);
				}
				else
				{
					Dvar::Var(prefix + "_shared_rank_level").Set(1);
					Dvar::Var(prefix + "_shared_rank_prestige").Set(0);
				}

				isAllReady = isAllReady && member.isReady;

				if (member.playerId == playerId)
				{
					isSelfReady = member.isReady;
				}
			}

			Dvar::Var("zwnet_lobby_self_ready").Set(isSelfReady);
			Dvar::Var("zwnet_all_ready").Set(isAllReady);
			Dvar::Var("zwnet_lobby_can_start").Set(false);

			if (soundDelta.hasLeft)
			{
				Game::SND_PlayLocalSoundAliasByName(0, "mp_player_leave", 0);
			}

			if (soundDelta.hasJoined)
			{
				Game::SND_PlayLocalSoundAliasByName(0, "mp_player_join", 0);
			}
		}, Scheduler::Pipeline::MAIN);
	}

	void ZWNet::UpdatePresence()
	{
		if (!isActive || isClosingOnlineSession || isOnlineEntryPending)
		{
			return;
		}

		std::string partyId;
		std::string uiState;

		{
			std::lock_guard _(stateMutex);
			partyId = currentPartyId;
			uiState = presenceUiState;
		}

		std::string status = "MAIN_MENU";

		if (isInGame)
		{
			status = "IN_MATCH";
		}
		else if (uiState == "MAP_VOTE")
		{
			status = "MAP_VOTE";
		}
		else if (uiState == "RESERVING_SERVER" || uiState == "STARTING_SERVER" || uiState == "SERVER_STARTING")
		{
			status = "SERVER_STARTING";
		}
		else if (uiState == "JOIN_PREVIEW" || uiState == "COUNTDOWN" || uiState == "CONNECTING" || uiState == "DIRECT_CONNECTION" || uiState == "RELAY_CONNECTION")
		{
			status = "CONNECTING";
		}
		else if (uiState == "READY_CHECK" || uiState == "WAITING_FOR_READY" || uiState == "MATCH_FOUND")
		{
			status = "IN_PARTY";
		}
		else if (isSearching)
		{
			status = "SEARCHING";
		}
		else if (IsOpaquePartyId(partyId))
		{
			status = "IN_PARTY";
		}

		const auto memberCount = cachedPartyMemberCount.load();
		const bool isJoinable = IsOpaquePartyId(partyId) && memberCount > 0 && memberCount < 4 && cachedPartyVisibility.load() != 2 && isCachedPartyJoinStateSupported.load();

		const auto result = Request("POST", "/social/presence", { { "status", status }, { "sequence", NextPresenceSequence() }, { "joinable", isJoinable } });

		if (!result || result->contains("error"))
		{
			Logger::Print("ZWNET presence update failed; matchmaking state preserved\n");
		}
	}

	static void ClearReadyPending()
	{
		Scheduler::Once([]
		{
			if (isActive)
			{
				Dvar::Var("zwnet_ready_pending").Set(false);
			}
		}, Scheduler::Pipeline::MAIN);
	}

	void ZWNet::ToggleReady(const bool isReady)
	{
		std::string partyId;

		{
			std::lock_guard _(stateMutex);
			partyId = currentPartyId;
		}

		if (partyId.empty())
		{
			ClearReadyPending();
			return;
		}

		std::string action = "/ready";

		if (isReady)
		{
			action = "/unready";
		}

		const auto result = Request("POST", "/zwnet/parties/" + partyId + action);
		ClearReadyPending();

		if (!result || result->contains("error"))
		{
			return;
		}

		Scheduler::Once([isReady]
		{
			if (isActive)
			{
				Dvar::Var("zwnet_lobby_self_ready").Set(!isReady);
			}
		}, Scheduler::Pipeline::MAIN);

		UpdateLobbyDvars(*result);

		if (isSearching)
		{
			UpdateMatchmaking();
		}
	}

	void ZWNet::StartPrivateMatch(std::string map)
	{
		std::string partyId;

		{
			std::lock_guard _(stateMutex);
			partyId = currentPartyId;
		}

		if (partyId.empty())
		{
			return;
		}

		Request("POST", "/zwnet/parties/" + partyId + "/set-map", { { "map", map } });
		Request("POST", "/zwnet/parties/" + partyId + "/set-mode", { { "mode", "zw3" } });

		const auto result = Request("POST", "/zwnet/parties/" + partyId + "/start-private-match");

		if (!result || result->contains("error"))
		{
			SetState("ERROR", "ZWNET_PRIVATE_MATCH_FAILED");
			return;
		}

		isSearching = true;
		SetState(result->value("state", "RESERVING_SERVER"));
		UpdateMatchmaking();
	}

	void ZWNet::VoteMap(const std::string& choice)
	{
		std::string proposalId;

		{
			std::lock_guard _(stateMutex);
			proposalId = currentProposalId;
		}

		if (proposalId.empty())
		{
			return;
		}

		const auto result = Request("POST", "/zwnet/matchmaking/map-vote", { { "proposal_id", proposalId }, { "choice", choice } });

		if (!result)
		{
			SetState("ERROR", "ZWNET_MAP_VOTE_FAILED");
			return;
		}

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
			if (isActive)
			{
				Dvar::Var("zwnet_vote_selection").Set(choice);
			}
		}, Scheduler::Pipeline::MAIN);

		const nlohmann::json* vote = nullptr;

		if (result->contains("map_vote") && result->at("map_vote").is_object())
		{
			vote = &result->at("map_vote");
		}
		else if (result->contains("choices"))
		{
			vote = &*result;
		}

		if (!vote)
		{
			return;
		}

		std::string matchId;

		{
			std::lock_guard _(stateMutex);
			matchId = currentMatchId;
		}

		UpdateVoteDvars({ { "match_id", matchId }, { "map_vote", *vote } });
	}

	void ZWNet::CancelSearch()
	{
		Request("POST", "/zwnet/matchmaking/cancel");
		isSearching = false;
		SetState("IDLE");
		SchedulePlaylistCatalogPublish();
	}

	void ZWNet::CancelMatchmaking()
	{
		CloseOnlineSession(false, false);

		if (!isActive)
		{
			return;
		}

		auto party = Request("GET", "/zwnet/parties/current");

		if (party && party->is_null())
		{
			party = Request("POST", "/zwnet/parties/create", { { "visibility", PartyVisibilityName(desiredPartyPrivacy.load()) } });
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

	void ZWNet::CloseOnlineSession(const bool isShuttingDown, const bool isTerminal)
	{
		CancelJoinInProgressPreview();

		if (!isActive || isClosingOnlineSession.exchange(true))
		{
			return;
		}

		isPlaylistSelectionStarting = false;
		ResetManagedRoute();

		ScopeExit closingGuard([]
		{
			isClosingOnlineSession = false;
		});

		ResetMatchLobbySoundSnapshot();
		isEndpointJoinInFlight = false;
		isServerJoinTransition = false;
		isInGame = false;

		std::string matchId;

		{
			std::lock_guard _(stateMutex);
			matchId = currentMatchId;
		}

		auto body = nlohmann::json::object();

		if (!matchId.empty())
		{
			body["match_id"] = matchId;
		}

		if (isTerminal)
		{
			body["terminal"] = true;
		}

		Request("POST", "/zwnet/matchmaking/disconnect", body);
		Request("POST", "/social/presence", { { "status", "OFFLINE" }, { "sequence", NextPresenceSequence() }, { "joinable", false } });

		isSearching = false;
		SchedulePlaylistCatalogPublish();

		{
			std::lock_guard _(stateMutex);

			if (isTerminal)
			{
				currentPartyId.clear();
			}

			currentProposalId.clear();
			currentMatchId.clear();
		}

		if (isShuttingDown)
		{
			return;
		}

		SetState("IDLE");

		Scheduler::Once([isTerminal]
		{
			if (!isActive)
			{
				return;
			}

			if (isTerminal)
			{
				AbandonOnlineSession();
				return;
			}

			ClearMatchDvars();
			Dvar::Var("zwnet_managed_session").Set(false);
		}, Scheduler::Pipeline::MAIN);
	}

	bool ZWNet::ReturnToMatchmakingLobby()
	{
		bool isCompleted = false;

		for (auto attempt = 0; attempt < 7; ++attempt)
		{
			const auto status = Request("GET", "/zwnet/matchmaking/status");

			if (!status || !status->is_object() || status->contains("error"))
			{
				return false;
			}

			const auto state = JsonString(*status, "state");
			isCompleted = state == "RESETTING" || state == "POST_MATCH" || state == "FINISHED" || state == "IDLE";

			if (isCompleted)
			{
				break;
			}

			if (attempt < 6)
			{
				std::this_thread::sleep_for(1s);
			}
		}

		if (!isCompleted)
		{
			return false;
		}

		auto party = Request("GET", "/zwnet/parties/current");

		if (!party || !party->is_object() || party->contains("error") || JsonString(*party, "id").empty())
		{
			return false;
		}

		*party = PublishLocalRank(std::move(*party));

		isSearching = false;
		isInGame = false;
		isEndpointJoinInFlight = false;
		isServerJoinTransition = false;
		ResetMatchLobbySoundSnapshot();

		{
			std::lock_guard _(stateMutex);
			currentProposalId.clear();
			currentMatchId.clear();
		}

		UpdateLobbyDvars(*party);
		SetState("IN_PARTY");
		RefreshPlaylistCatalog(true);
		UpdatePresence();

		Scheduler::Once([]
		{
			if (!isActive)
			{
				return;
			}

			ClearMatchDvars();
			Dvar::Var("zwnet_match_id").Set("");
			Dvar::Var("zwnet_managed_session").Set(false);

			for (const char* menuName : { "class", "team_marinesopfor" })
			{
				Game::menuDef_t* const menu = Game::Menus_FindByName(Game::uiContext, menuName);

				if (menu)
				{
					Game::Menus_CloseRequest(Game::uiContext, menu);
				}
			}

			Game::Menus_OpenByName(Game::uiContext, "zwnet_matchmaking");
		}, Scheduler::Pipeline::MAIN, 500ms);

		return true;
	}

	void ZWNet::ReturnToIdleMenu()
	{
		AbandonOnlineSession();
		Dvar::Var("zwnet_managed_session").Set(false);
		SetState("IDLE");

		Command::Execute("set xblive_privateserver 0", false);
		Command::Execute("set xblive_privatematch 0", false);
		Command::Execute("set xblive_rankedmatch 0", false);

		for (const char* menuName : { "class", "team_marinesopfor", "zwnet_matchmaking" })
		{
			Game::menuDef_t* const menu = Game::Menus_FindByName(Game::uiContext, menuName);

			if (menu)
			{
				Game::Menus_CloseRequest(Game::uiContext, menu);
			}
		}

		Game::Menus_OpenByName(Game::uiContext, "pregame_loaderror");
		Game::Menus_OpenByName(Game::uiContext, "popup_zwnet_connecting");
	}

	void ZWNet::ScheduleReturnToIdleMenu()
	{
		if (isIdleReturnPending.exchange(true))
		{
			return;
		}

		const auto startedAt = Game::Sys_Milliseconds();

		Scheduler::Schedule([startedAt, disconnectedAt = -1]() mutable
		{
			const auto now = Game::Sys_Milliseconds();

			if (Game::CL_IsCgameInitialized(0))
			{
				const bool hasTimedOut = (now - startedAt) > 10'000;

				if (hasTimedOut)
				{
					isIdleReturnPending = false;
				}

				return hasTimedOut;
			}

			if (disconnectedAt < 0)
			{
				disconnectedAt = now;
				return false;
			}

			if ((now - disconnectedAt) < 150)
			{
				return false;
			}

			isIdleReturnPending = false;
			ReturnToIdleMenu();
			return true;
		}, Scheduler::Pipeline::MAIN, 50ms);
	}

	void ZWNet::HandleServerDisconnect(const bool isTerminal, const bool wasMatchmaking)
	{
		if (wasMatchmaking && !isTerminal && ReturnToMatchmakingLobby())
		{
			Logger::Print("ZWNET completed match: returned to the preserved party lobby\n");
			return;
		}

		CloseOnlineSession(false, isTerminal);

		if (wasMatchmaking)
		{
			ScheduleReturnToIdleMenu();
		}
	}

	bool ZWNet::BeginManagedReconnect(const std::string& endpoint)
	{
		if (!isActive)
		{
			return false;
		}

		if (isManagedReconnectInFlight)
		{
			return true;
		}

		std::string matchId;
		std::string playerId;

		{
			std::lock_guard _(stateMutex);
			matchId = currentMatchId;
			playerId = currentPlayerId;
		}

		bool isManaged = false;
		bool isBound = false;
		bool isRelay = false;

		{
			std::lock_guard _(routeAttempt.mutex);
			isManaged = !matchId.empty() && routeAttempt.matchId == matchId;

			if (isManaged)
			{
				const Network::Address requestedTarget(endpoint);

				isBound = requestedTarget.IsValid()
					&& requestedTarget == routeAttempt.assignedTarget
					&& routeAttempt.playerId == playerId
					&& !routeAttempt.sessionId.empty();

				isRelay = routeAttempt.isRouteRelay;
			}
		}

		if (!isManaged)
		{
			if (!Dvar::Var("zwnet_managed_session").Get<bool>())
			{
				return false;
			}

			Auth::ClearManagedConnectTicket();
			SetState("ERROR", "ZWNET_DESCRIPTOR_INVALID");
			return true;
		}

		if (!isBound || !IsOpaqueMatchId(matchId) || !IsOpaqueDescriptorValue(playerId))
		{
			Auth::ClearManagedConnectTicket();
			SetState("ERROR", "ZWNET_DESCRIPTOR_INVALID");
			return true;
		}

		if (isManagedReconnectInFlight.exchange(true))
		{
			return true;
		}

		++joinTransitionGeneration;
		isServerJoinTransition = true;

		EnqueueAsync([matchId, isRelay]
		{
			ConnectMatch(matchId, isRelay, true);
		});

		return true;
	}

	bool ZWNet::TryRelayAfterDirectTimeout(const std::string& endpoint)
	{
		if (!isActive)
		{
			return false;
		}

		const Network::Address timedOutTarget(endpoint);

		if (!timedOutTarget.IsValid())
		{
			return false;
		}

		std::string matchId;
		std::string playerId;

		{
			std::lock_guard _(stateMutex);
			matchId = currentMatchId;
			playerId = currentPlayerId;
		}

		if (!IsOpaqueMatchId(matchId) || !IsOpaqueDescriptorValue(playerId))
		{
			return false;
		}

		bool isReconnect = false;

		{
			std::lock_guard _(routeAttempt.mutex);

			const bool isThisDirectAttempt = routeAttempt.isDirectWaiting
				&& !routeAttempt.isRelayUsed
				&& routeAttempt.matchId == matchId
				&& routeAttempt.playerId == playerId
				&& routeAttempt.directTarget == timedOutTarget;

			if (!isThisDirectAttempt)
			{
				return false;
			}

			routeAttempt.isDirectWaiting = false;
			routeAttempt.isRelayUsed = true;
			isReconnect = routeAttempt.isReconnectAttempt;
		}

		++joinTransitionGeneration;
		isServerJoinTransition = true;

		EnqueueAsync([matchId, isReconnect]
		{
			ConnectMatch(matchId, true, isReconnect);
		});

		return true;
	}

	void ZWNet::ConnectMatch(const std::string& matchId, const bool isRelay, const bool isReconnect)
	{
		if (!isRelay && IsRelayUsedForMatch(matchId))
		{
			isManagedReconnectInFlight = false;
			Auth::ClearManagedConnectTicket();
			++joinTransitionGeneration;
			isServerJoinTransition = false;
			SetState("ERROR", "ZWNET_ROUTE_UNAVAILABLE");
			return;
		}

		CancelRelayHandshake();
		Auth::ClearManagedConnectTicket();

		std::string playerId;

		{
			std::lock_guard _(stateMutex);
			playerId = currentPlayerId;
		}

		const auto fail = [isRelay, isReconnect](const std::string& error)
		{
			isSearching = false;
			isManagedReconnectInFlight = false;
			Auth::ClearManagedConnectTicket();
			++joinTransitionGeneration;
			isServerJoinTransition = false;
			isJoinInProgressSoundPending = false;

			if (isRelay || isReconnect)
			{
				Scheduler::Once([]
				{
					Command::Execute("closemenu popup_reconnectingtoparty", false);
				}, Scheduler::Pipeline::MAIN);
			}

			SetState("ERROR", error);
		};

		const char* key = "direct_endpoint";
		const char* routeType = "DIRECT";

		if (isRelay)
		{
			SetState("RELAY_CONNECTION");
			key = "relay_endpoint";
			routeType = "RELAY";
		}
		else
		{
			SetState("DIRECT_CONNECTION");
		}

		std::optional<nlohmann::json> descriptor;

		if (isReconnect)
		{
			nlohmann::json body = { { "match_id", matchId } };

			if (isRelay)
			{
				body["relay"] = true;
			}

			descriptor = Request("POST", "/zwnet/matchmaking/reconnect", body);
		}
		else
		{
			std::string path = "/zwnet/connect/" + matchId;

			if (isRelay)
			{
				path += "?relay=1";
			}

			descriptor = Request("GET", path);
		}

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
				if (!isActive)
				{
					return;
				}

				Dvar::Var("zwnet_server_status").Set("SERVER UNAVAILABLE");
				Dvar::Var("zwnet_join_status").Set("RETURN TO LOBBY");
			}, Scheduler::Pipeline::MAIN);

			if (code == "SERVER_NOT_READY")
			{
				fail("ZWNET_SERVER_NOT_READY");
			}
			else
			{
				fail("ZWNET_ROUTE_UNAVAILABLE");
			}

			return;
		}

		const auto endpoint = JsonString(*descriptor, key);

		if (endpoint.empty())
		{
			fail("ZWNET_DESCRIPTOR_INVALID");
			return;
		}

		if (JsonString(*descriptor, "match_id") != matchId || JsonString(*descriptor, "route_type") != routeType)
		{
			fail("ZWNET_DESCRIPTOR_INVALID");
			return;
		}

		const Network::Address authorizedTarget(endpoint);
		const auto sessionId = JsonString(*descriptor, "session_id");
		const auto serverIdentity = JsonString(*descriptor, "server_identity");
		const auto instanceId = JsonString(*descriptor, "instance_id");

		if (serverIdentity.empty() || !IsOpaqueDescriptorValue(instanceId))
		{
			fail("ZWNET_DESCRIPTOR_INVALID");
			return;
		}

		bool isSameSession = false;

		{
			std::lock_guard _(stateMutex);
			isSameSession = currentPlayerId == playerId && currentMatchId == matchId;
		}

		if (!isSameSession)
		{
			fail("ZWNET_SESSION_EXPIRED");
			return;
		}

		if (isRelay || isReconnect)
		{
			bool isRouteConsistent = true;

			{
				std::lock_guard _(routeAttempt.mutex);

				const bool hasRoute = routeAttempt.matchId == matchId && !routeAttempt.sessionId.empty();

				const bool isSameRoute = routeAttempt.sessionId == sessionId
					&& routeAttempt.playerId == playerId
					&& routeAttempt.serverIdentity == serverIdentity
					&& routeAttempt.instanceId == instanceId;

				const bool hasDirectTargetMoved = isReconnect && !isRelay && !routeAttempt.isRouteRelay
					&& routeAttempt.assignedTarget != authorizedTarget;

				const bool hasRouteKindChanged = isReconnect && routeAttempt.isRouteRelay != isRelay
					&& !(isRelay && routeAttempt.isRelayUsed);

				if (hasRoute && (!isSameRoute || hasDirectTargetMoved || hasRouteKindChanged))
				{
					isRouteConsistent = false;
				}

				if (isReconnect && !hasRoute)
				{
					isRouteConsistent = false;
				}
			}

			if (!isRouteConsistent)
			{
				fail("ZWNET_DESCRIPTOR_INVALID");
				return;
			}
		}

		if (!authorizedTarget.IsValid() || !Auth::SetManagedConnectTicket(authorizedTarget, JsonString(*descriptor, "connect_ticket"), matchId, sessionId))
		{
			fail("ZWNET_DESCRIPTOR_INVALID");
			return;
		}

		isSearching = false;

		if (isRelay)
		{
			const auto relayTicket = JsonString(*descriptor, "relay_ticket");

			const bool isRelayDescriptorValid = IsOpaqueMatchId(matchId)
				&& IsOpaqueDescriptorValue(playerId)
				&& IsOpaqueDescriptorValue(sessionId)
				&& IsOpaqueDescriptorValue(relayTicket);

			if (!isRelayDescriptorValid)
			{
				fail("ZWNET_DESCRIPTOR_INVALID");
				return;
			}

			const auto nonce = std::format("{:016x}{:016x}", Utils::Cryptography::Rand::GenerateLong(), Utils::Cryptography::Rand::GenerateLong());

			std::string hello = nlohmann::json
			{
				{ "schema_version", 1 },
				{ "relay_ticket", relayTicket },
				{ "match_id", matchId },
				{ "player_id", playerId },
				{ "session_id", sessionId },
				{ "nonce", nonce },
			}.dump();

			if (hello.size() + sizeof("zwnetRelayHello") + 4 > relayPacketLimit)
			{
				fail("ZWNET_DESCRIPTOR_INVALID");
				return;
			}

			{
				std::lock_guard _(routeAttempt.mutex);

				routeAttempt.matchId = matchId;
				routeAttempt.playerId = playerId;
				routeAttempt.sessionId = sessionId;
				routeAttempt.serverIdentity = serverIdentity;
				routeAttempt.instanceId = instanceId;
				routeAttempt.assignedTarget = authorizedTarget;
				routeAttempt.isRouteRelay = true;
				routeAttempt.isReconnectAttempt = isReconnect;
				routeAttempt.isDirectWaiting = false;
				routeAttempt.isRelayUsed = true;
			}

			++joinTransitionGeneration;
			isServerJoinTransition = true;

			Scheduler::Once([target = authorizedTarget, matchId, playerId, nonce, hello]() mutable
			{
				if (!isActive)
				{
					return;
				}

				bool isSameSession = false;

				{
					std::lock_guard _(stateMutex);
					isSameSession = currentPlayerId == playerId && currentMatchId == matchId;
				}

				if (!isSameSession)
				{
					Auth::ClearManagedConnectTicket();
					isManagedReconnectInFlight = false;
					isServerJoinTransition = false;
					return;
				}

				{
					std::lock_guard _(relayHandshake.mutex);

					++relayHandshake.generation;
					relayHandshake.isPending = true;
					relayHandshake.isReady = false;
					relayHandshake.target = target;
					relayHandshake.matchId = matchId;
					relayHandshake.playerId = playerId;
					relayHandshake.nonce = nonce;
					relayHandshake.hello = hello;
					relayHandshake.deadline = std::chrono::steady_clock::now() + relayHandshakeTimeout;
				}

				Dvar::Var("zwnet_join_status").Set("CONTACTING RELAY");
				Network::SendCommand(target, "zwnetRelayHello", hello);
				std::ranges::fill(hello, '\0');
			}, Scheduler::Pipeline::MAIN);

			return;
		}

		{
			std::lock_guard _(routeAttempt.mutex);

			if (routeAttempt.matchId != matchId)
			{
				routeAttempt.matchId = matchId;
				routeAttempt.isRelayUsed = false;
			}

			routeAttempt.playerId = playerId;
			routeAttempt.sessionId = sessionId;
			routeAttempt.serverIdentity = serverIdentity;
			routeAttempt.instanceId = instanceId;
			routeAttempt.directTarget = authorizedTarget;
			routeAttempt.assignedTarget = authorizedTarget;
			routeAttempt.isRouteRelay = false;
			routeAttempt.isReconnectAttempt = isReconnect;
			routeAttempt.isDirectWaiting = true;
		}

		Scheduler::Once([endpoint, target = authorizedTarget, matchId, playerId]
		{
			if (!isActive)
			{
				return;
			}

			bool isSameSession = false;

			{
				std::lock_guard _(stateMutex);
				isSameSession = currentPlayerId == playerId && currentMatchId == matchId;
			}

			if (!isSameSession)
			{
				Auth::ClearManagedConnectTicket();
				isManagedReconnectInFlight = false;
				++joinTransitionGeneration;
				isServerJoinTransition = false;
				return;
			}

			SetPresenceUiState("CONNECTING");
			Dvar::Var("ui_zwnet_state").Set("CONNECTING");
			Dvar::Var("ui_zwnet_error").Set("");
			Dvar::Var("zwnet_server_endpoint").Set(endpoint);
			Dvar::Var("zwnet_server_status").Set("SERVER ASSIGNED");
			Dvar::Var("zwnet_join_status").Set("JOINING SERVER");
			Dvar::Var("zwnet_managed_session").Set(true);

			MarkJoinInProgressConnectionStarted(matchId);

			const auto transitionGeneration = ++joinTransitionGeneration;
			isServerJoinTransition = true;

			Scheduler::Once([transitionGeneration]
			{
				if (joinTransitionGeneration == transitionGeneration)
				{
					isServerJoinTransition = false;
				}
			}, Scheduler::Pipeline::MAIN, 12s);

			Party::Connect(target);
			isManagedReconnectInFlight = false;
		}, Scheduler::Pipeline::MAIN);
	}

	static std::string ResolveVoteMapImage(const std::string& mapId, const std::string& serverImage)
	{
		if (!mapId.empty())
		{
			const char* const arenaImage = Localization::GetMapImageName(mapId.data());

			if (arenaImage[0])
			{
				return arenaImage;
			}
		}

		const bool isServerPreview = serverImage != mapId && serverImage.starts_with("preview_") && !serverImage.starts_with("preview_mp_mp_");

		if (isServerPreview)
		{
			return serverImage;
		}

		if (!mapId.empty())
		{
			return "preview_" + mapId;
		}

		return serverImage;
	}

	static std::string ResolveVoteMapDisplayName(const std::string& mapId, const std::string& serverName)
	{
		if (!serverName.empty() && serverName != mapId)
		{
			return serverName;
		}

		if (!mapId.empty())
		{
			const char* const localizedName = Localization::LocalizeMapName(mapId.data());

			if (localizedName[0] && localizedName != mapId)
			{
				return localizedName;
			}
		}

		return mapId;
	}

	void ZWNet::PublishPlaylistCatalog()
	{
		if (!isActive)
		{
			return;
		}

		bool hasMatch = false;

		{
			std::lock_guard _(stateMutex);
			hasMatch = !currentMatchId.empty();
		}

		const bool isPinned = isPlaylistSearchStarting || isPlaylistSelectionStarting || isSearching || isInGame || hasMatch;

		std::lock_guard _(playlistCatalog.mutex);

		if (!isPinned && playlistCatalog.hasPending)
		{
			playlistCatalog.entries = std::move(playlistCatalog.pendingEntries);
			playlistCatalog.pendingEntries.clear();
			playlistCatalog.revision = playlistCatalog.pendingRevision;
			playlistCatalog.hasPending = false;
			playlistCatalog.pendingRevision = 0;
		}

		const auto findEntry = [](const std::string& id)
		{
			return std::ranges::find_if(playlistCatalog.entries, [&id](const ClientPlaylist& entry)
			{
				return entry.id == id;
			});
		};

		if (findEntry(playlistCatalog.selectedId) == playlistCatalog.entries.end())
		{
			playlistCatalog.selectedId.clear();

			if (!playlistCatalog.partySelectedId.empty())
			{
				const auto partySelected = findEntry(playlistCatalog.partySelectedId);

				if (partySelected != playlistCatalog.entries.end())
				{
					playlistCatalog.selectedId = partySelected->id;
				}
			}

			if (playlistCatalog.selectedId.empty() && !isPinned)
			{
				const auto first = std::ranges::find_if(playlistCatalog.entries, [](const ClientPlaylist& entry)
				{
					return entry.availability == "AVAILABLE";
				});

				if (first != playlistCatalog.entries.end())
				{
					playlistCatalog.selectedId = first->id;
				}
			}
		}

		const auto active = findEntry(playlistCatalog.selectedId);
		const bool hasActive = active != playlistCatalog.entries.end();

		bool isSelectionCurrent = false;

		if (hasActive)
		{
			isSelectionCurrent = playlistCatalog.partySelectedId.empty()
				|| (active->id == playlistCatalog.partySelectedId && active->revision == playlistCatalog.partySelectedRevision);
		}

		const auto pageCount = std::max<std::size_t>(1, (playlistCatalog.entries.size() + playlistPageSize - 1) / playlistPageSize);

		if (playlistCatalog.page >= pageCount)
		{
			playlistCatalog.page = pageCount - 1;
		}

		std::string catalogStatus = "READY";

		if (!playlistCatalog.isLoaded && playlistCatalog.isStale)
		{
			catalogStatus = "PLAYLIST SERVICE UNAVAILABLE";
		}
		else if (!playlistCatalog.isLoaded)
		{
			catalogStatus = "LOADING";
		}
		else if (playlistCatalog.isStale)
		{
			catalogStatus = "STALE - SERVICE UNAVAILABLE";
		}

		Dvar::Var("zwnet_catalog_status").Set(catalogStatus);
		Dvar::Var("zwnet_catalog_notice").Set(playlistCatalog.hasNotice);
		Dvar::Var("zwnet_catalog_page").Set(static_cast<int>(playlistCatalog.page + 1));
		Dvar::Var("zwnet_catalog_pages").Set(static_cast<int>(pageCount));

		if (hasActive)
		{
			Dvar::Var("zwnet_catalog_selected_name").Set(active->name);
			Dvar::Var("zwnet_catalog_selected_description").Set(active->description);
			Dvar::Var("zwnet_catalog_selected_audience").Set(active->audience);
			Dvar::Var("zwnet_catalog_selected_availability").Set(active->availability);

			if (isSelectionCurrent)
			{
				Dvar::Var("zwnet_catalog_selected_status").Set(active->availabilityDetail);
			}
			else
			{
				Dvar::Var("zwnet_catalog_selected_status").Set("A new playlist revision is available. The party leader must confirm it.");
			}

			Dvar::Var("zwnet_catalog_selected_revision").Set(static_cast<int>(active->revision));
			Dvar::Var("zwnet_catalog_selected_rotation").Set(active->rotationSummary);

			if (active->mapId.empty())
			{
				Dvar::Var("zwnet_catalog_selected_image").Set("");
			}
			else
			{
				Dvar::Var("zwnet_catalog_selected_image").Set(ResolveVoteMapImage(active->mapId, active->mapImage));
			}

			Dvar::Var("zwnet_catalog_selected_players").Set(std::format("{}-{} PLAYERS", active->minPlayers, active->maxPlayers));
			Dvar::Var("zwnet_catalog_selected_zombie_settings").Set(active->zombieSettingsSummary);
		}
		else
		{
			Dvar::Var("zwnet_catalog_selected_name").Set("NO AVAILABLE PLAYLIST");
			Dvar::Var("zwnet_catalog_selected_description").Set("Select an available playlist to find a match.");
			Dvar::Var("zwnet_catalog_selected_audience").Set("");
			Dvar::Var("zwnet_catalog_selected_availability").Set("NO_SELECTION");

			if (playlistCatalog.partySelectedId.empty())
			{
				Dvar::Var("zwnet_catalog_selected_status").Set("Choose a playlist to continue.");
			}
			else
			{
				Dvar::Var("zwnet_catalog_selected_status").Set("The party playlist is no longer in your authorized catalog.");
			}

			Dvar::Var("zwnet_catalog_selected_revision").Set(0);
			Dvar::Var("zwnet_catalog_selected_rotation").Set("No map rotation is available.");
			Dvar::Var("zwnet_catalog_selected_image").Set("");
			Dvar::Var("zwnet_catalog_selected_players").Set("");
			Dvar::Var("zwnet_catalog_selected_zombie_settings").Set("");
		}

		const bool isSelectionStarting = isPlaylistSelectionStarting;
		const bool canSelect = isLocalPartyLeader && !isPinned && !playlistCatalog.isStale && !isSelectionStarting;
		const bool canSearch = hasActive && active->availability == "AVAILABLE" && !playlistCatalog.isStale && !isPinned
			&& isLocalPartyLeader && isSelectionCurrent;

		Dvar::Var("zwnet_catalog_busy").Set(isSelectionStarting);
		Dvar::Var("zwnet_catalog_can_select").Set(canSelect);
		Dvar::Var("zwnet_catalog_can_search").Set(canSearch);

		for (std::size_t slot = 0; slot < playlistPageSize; ++slot)
		{
			const auto index = playlistCatalog.page * playlistPageSize + slot;

			if (index >= playlistCatalog.entries.size())
			{
				Dvar::Var(playlistSlotVisibleDvars[slot]).Set(false);
				Dvar::Var(playlistSlotNameDvars[slot]).Set("");
				Dvar::Var(playlistSlotDescriptionDvars[slot]).Set("");
				Dvar::Var(playlistSlotPreviewDvars[slot]).Set("");
				Dvar::Var(playlistSlotImageDvars[slot]).Set("");
				Dvar::Var(playlistSlotAudienceDvars[slot]).Set("");
				Dvar::Var(playlistSlotAvailabilityDvars[slot]).Set("");
				Dvar::Var(playlistSlotStatusDvars[slot]).Set("");
				Dvar::Var(playlistSlotSelectedDvars[slot]).Set(false);
				continue;
			}

			const auto& entry = playlistCatalog.entries[index];

			Dvar::Var(playlistSlotVisibleDvars[slot]).Set(true);
			Dvar::Var(playlistSlotNameDvars[slot]).Set(entry.name);
			Dvar::Var(playlistSlotDescriptionDvars[slot]).Set(entry.description);
			Dvar::Var(playlistSlotPreviewDvars[slot]).Set(entry.preview);

			if (entry.mapId.empty())
			{
				Dvar::Var(playlistSlotImageDvars[slot]).Set("");
			}
			else
			{
				Dvar::Var(playlistSlotImageDvars[slot]).Set(ResolveVoteMapImage(entry.mapId, entry.mapImage));
			}

			Dvar::Var(playlistSlotAudienceDvars[slot]).Set(entry.audience);
			Dvar::Var(playlistSlotAvailabilityDvars[slot]).Set(entry.availability);
			Dvar::Var(playlistSlotStatusDvars[slot]).Set(entry.availabilityDetail);
			Dvar::Var(playlistSlotSelectedDvars[slot]).Set(entry.id == playlistCatalog.selectedId);
		}
	}

	void ZWNet::CancelJoinInProgressPreview()
	{
		{
			std::lock_guard _(joinPreview.mutex);

			++joinPreview.generation;
			joinPreview.isActive = false;
			joinPreview.matchId.clear();
			joinPreview.completedMatchId.clear();
		}

		isJoinInProgressSoundPending = false;

		Scheduler::Once([]
		{
			if (!isActive)
			{
				return;
			}

			Dvar::Var("zwnet_join_preview_active").Set(false);
			Dvar::Var("zwnet_join_preview_seconds").Set(0);
		}, Scheduler::Pipeline::MAIN);
	}

	static bool IsCurrentJoinPreview(const std::uint64_t generation, const std::string& matchId)
	{
		std::lock_guard _(joinPreview.mutex);
		return joinPreview.isActive && joinPreview.generation == generation && joinPreview.matchId == matchId;
	}

	void ZWNet::BeginJoinInProgressPreview(const nlohmann::json& status)
	{
		const auto matchId = JsonString(status, "match_id");

		if (!IsOpaqueMatchId(matchId))
		{
			return;
		}

		std::uint64_t generation = 0;

		{
			std::lock_guard _(joinPreview.mutex);

			if ((joinPreview.isActive && joinPreview.matchId == matchId) || joinPreview.completedMatchId == matchId)
			{
				return;
			}

			++joinPreview.generation;
			generation = joinPreview.generation;
			joinPreview.isActive = true;
			joinPreview.matchId = matchId;
			joinPreview.completedMatchId.clear();
		}

		const auto map = JsonString(status, "map");
		std::string mapName;
		std::string mapImage;

		if (status.contains("selected_map") && status.at("selected_map").is_object())
		{
			mapName = JsonString(status.at("selected_map"), "name");
			mapImage = JsonString(status.at("selected_map"), "image");
		}

		Scheduler::Once([generation, matchId, map, mapName, mapImage]
		{
			if (!isActive || !IsCurrentJoinPreview(generation, matchId))
			{
				return;
			}

			SetPresenceUiState("JOIN_PREVIEW");
			Dvar::Var("ui_zwnet_state").Set("JOIN_PREVIEW");
			Dvar::Var("ui_zwnet_state_text").Set("JOINING GAME IN PROGRESS");
			Dvar::Var("zwnet_join_preview_active").Set(true);
			Dvar::Var("zwnet_join_preview_seconds").Set(3);
			Dvar::Var("zwnet_join_status").Set("PREVIEWING ACTIVE MATCH");

			if (!map.empty())
			{
				SetDisplayText("ui_mapname", map);
				SetDisplayText("zwnet_vote_winner_id", map);
				SetDisplayText("zwnet_vote_winner_name", ResolveVoteMapDisplayName(map, mapName));
				SetDisplayText("zwnet_vote_winner_image", ResolveVoteMapImage(map, mapImage));
				Maps::SynchronizeMapDvars(map);
			}

			Game::SND_PlayLocalSoundAliasByName(0, "mp_player_join", 0);

			Scheduler::Once([generation, matchId]
			{
				EnqueueAsync([generation, matchId]
				{
					if (!IsCurrentJoinPreview(generation, matchId))
					{
						return;
					}

					const auto current = Request("GET", "/zwnet/matchmaking/status");

					const bool isStillJoinable = current && current->is_object() && !current->contains("error")
						&& JsonString(*current, "match_id") == matchId && JsonString(*current, "state") == "CONNECTING"
						&& current->value("join_in_progress", false);

					if (!isStillJoinable)
					{
						Request("POST", "/zwnet/matchmaking/disconnect", { { "match_id", matchId } });
						isSearching = false;
						CancelJoinInProgressPreview();
						SetState("ERROR", "ZWNET_SERVER_NOT_READY");
						return;
					}

					{
						std::lock_guard _(joinPreview.mutex);

						if (!joinPreview.isActive || joinPreview.generation != generation || joinPreview.matchId != matchId)
						{
							return;
						}

						joinPreview.isActive = false;
						joinPreview.completedMatchId = matchId;
					}

					Scheduler::Once([]
					{
						if (!isActive)
						{
							return;
						}

						Dvar::Var("zwnet_join_preview_active").Set(false);
						Dvar::Var("zwnet_join_preview_seconds").Set(0);
					}, Scheduler::Pipeline::MAIN);

					ConnectMatch(matchId, false, false);
				});
			}, Scheduler::Pipeline::MAIN, 2500ms);
		}, Scheduler::Pipeline::MAIN);
	}

	void ZWNet::UpdateVoteDvars(const nlohmann::json& status)
	{
		if (!status.contains("map_vote"))
		{
			return;
		}

		const auto vote = status.at("map_vote");
		const auto choices = vote.value("choices", nlohmann::json::array());

		if (choices.size() != 3)
		{
			return;
		}

		const auto isAllReady = status.value("all_ready", false);

		{
			std::lock_guard _(stateMutex);
			currentProposalId = JsonString(vote, "proposal_id");
			currentMatchId = JsonString(status, "match_id");
		}

		Scheduler::Once([vote, choices, isAllReady]
		{
			if (!isActive)
			{
				return;
			}

			std::string selected;

			if (vote.contains("selected") && vote.at("selected").is_string())
			{
				selected = vote.at("selected").get<std::string>();
			}

			Dvar::Var("zwnet_vote_active").Set(true);
			Dvar::Var("zwnet_all_ready").Set(isAllReady);
			Dvar::Var("zwnet_start_phase").Set("");
			Dvar::Var("zwnet_start_seconds").Set(0);
			SetDisplayText("zwnet_vote_proposal_id", JsonString(vote, "proposal_id"));
			Dvar::Var("zwnet_vote_seconds").Set(JsonIntegerOr(vote, "seconds_remaining", 0));
			SetDisplayText("zwnet_vote_selection", selected);
			Dvar::Var("zwnet_vote_reveal_time").Set(0);
			Dvar::Var("zwnet_vote_winner_id").Set("");
			Dvar::Var("zwnet_vote_winner_name").Set("");
			Dvar::Var("zwnet_vote_winner_image").Set("");
			Dvar::Var("zwnet_server_status").Set("WAITING FOR MAP VOTE");
			Dvar::Var("zwnet_join_status").Set("VOTE IN PROGRESS");

			for (const auto& [index, letter] : { std::pair<std::size_t, const char*>{ 0, "a" }, std::pair<std::size_t, const char*>{ 1, "b" } })
			{
				const auto prefix = std::format("zwnet_vote_map_{}", letter);
				const auto mapId = JsonString(choices[index], "id");

				SetDisplayText(prefix + "_id", mapId);
				SetDisplayText(prefix + "_name", ResolveVoteMapDisplayName(mapId, JsonString(choices[index], "name")));
				SetDisplayText(prefix + "_image", ResolveVoteMapImage(mapId, JsonString(choices[index], "image")));
				Dvar::Var(prefix + "_votes").Set(JsonIntegerOr(choices[index], "votes", 0));
			}

			Dvar::Var("zwnet_vote_random_votes").Set(JsonIntegerOr(choices[2], "votes", 0));
		}, Scheduler::Pipeline::MAIN);
	}

	static void ServerStatusText(const std::string& state, const int joinCountdown, std::string& serverStatus, std::string& joinStatus)
	{
		serverStatus = "NOT ASSIGNED";
		joinStatus = "WAITING IN LOBBY";

		if (state == "WAITING_FOR_READY")
		{
			serverStatus = "START LOCKED";
			joinStatus = "WAITING FOR ALL PLAYERS";
		}
		else if (state == "RESERVING_SERVER")
		{
			serverStatus = "ALLOCATING SERVER";
			joinStatus = "MAP LOCKED";
		}
		else if (state == "SERVER_STARTING")
		{
			serverStatus = "SERVER STARTING";
			joinStatus = "WAITING FOR SERVER";
		}
		else if (state == "RESETTING" || state == "POST_MATCH")
		{
			serverStatus = "MATCH COMPLETE";
			joinStatus = "RETURNING TO LOBBY";
		}
		else if (state == "CONNECTING" && joinCountdown > 0)
		{
			serverStatus = "SERVER READY";
			joinStatus = std::format("JOINING IN {}", joinCountdown);
		}
		else if (state == "CONNECTING")
		{
			serverStatus = "SERVER READY";
			joinStatus = "JOIN AUTHORIZED";
		}
	}

	void ZWNet::UpdateMatchmaking()
	{
		if (!isActive || !isSearching)
		{
			return;
		}

		auto party = Request("GET", "/zwnet/parties/current");
		const bool isPartyValid = party && party->is_object() && !party->contains("error");

		if (isPartyValid)
		{
			*party = PublishLocalRank(std::move(*party));
		}

		const auto status = Request("GET", "/zwnet/matchmaking/status");

		if (isPartyValid)
		{
			UpdateLobbyDvars(*party);
		}

		if (!status)
		{
			Logger::Print("ZWNET matchmaking status poll failed; current session preserved\n");
			return;
		}

		if (status->contains("error"))
		{
			return;
		}

		if (status->contains("lobby") && status->at("lobby").is_object())
		{
			UpdateMatchLobbyDvars(*status);
		}

		const auto state = JsonString(*status, "state", "SEARCHING");
		const auto joinCountdown = status->value("join_countdown_seconds", 0);
		const auto isAllReady = status->value("all_ready", false);
		const auto startPhase = JsonString(*status, "start_phase");
		const auto startSeconds = status->value("start_seconds", joinCountdown);

		if (state == "ERROR" || state == "FAILED")
		{
			isSearching = false;
			SetState("ERROR", "ZWNET_MATCH_FAILED");
			return;
		}

		if (state == "IDLE" || state == "FINISHED")
		{
			isSearching = false;
		}

		if (state == "CONNECTING" && joinCountdown > 0)
		{
			SetState("COUNTDOWN");
		}
		else
		{
			SetState(state);
		}

		if (state == "MAP_VOTE")
		{
			UpdateVoteDvars(*status);
		}
		else
		{
			const auto map = JsonString(*status, "map");
			const auto matchId = JsonString(*status, "match_id");

			if (!matchId.empty())
			{
				std::lock_guard _(stateMutex);
				currentMatchId = matchId;
			}

			std::string mapName;
			std::string mapImage;

			if (status->contains("selected_map") && status->at("selected_map").is_object())
			{
				mapName = JsonString(status->at("selected_map"), "name");
				mapImage = JsonString(status->at("selected_map"), "image");
			}

			std::string serverStatus;
			std::string joinStatus;
			ServerStatusText(state, joinCountdown, serverStatus, joinStatus);

			Scheduler::Once([map, mapName, mapImage, matchId, serverStatus, joinStatus, joinCountdown, isAllReady, startPhase, startSeconds]
			{
				if (!isActive)
				{
					return;
				}

				const bool wasVoting = Dvar::Var("zwnet_vote_active").Get<bool>();

				Dvar::Var("zwnet_vote_active").Set(false);
				Dvar::Var("zwnet_all_ready").Set(isAllReady);
				SetDisplayText("zwnet_start_phase", startPhase);
				Dvar::Var("zwnet_start_seconds").Set(startSeconds);
				SetDisplayText("zwnet_match_id", matchId);
				Dvar::Var("zwnet_join_countdown").Set(joinCountdown);
				Dvar::Var("zwnet_server_status").Set(serverStatus);
				Dvar::Var("zwnet_join_status").Set(joinStatus);

				if (map.empty())
				{
					return;
				}

				if (Dvar::Var("zwnet_vote_winner_id").Get<std::string>() != map)
				{
					int revealSlot = 2;

					if (map == Dvar::Var("zwnet_vote_map_a_id").Get<std::string>())
					{
						revealSlot = 0;
					}
					else if (map == Dvar::Var("zwnet_vote_map_b_id").Get<std::string>())
					{
						revealSlot = 1;
					}

					int revealTimeMs = 0;

					if (wasVoting)
					{
						revealTimeMs = Game::Sys_Milliseconds();
					}

					Dvar::Var("zwnet_vote_reveal_slot").Set(revealSlot);
					Dvar::Var("zwnet_vote_reveal_time").Set(revealTimeMs);
				}

				SetDisplayText("ui_mapname", map);
				SetDisplayText("zwnet_vote_winner_id", map);
				SetDisplayText("zwnet_vote_winner_name", ResolveVoteMapDisplayName(map, mapName));
				SetDisplayText("zwnet_vote_winner_image", ResolveVoteMapImage(map, mapImage));
				Maps::SynchronizeMapDvars(map);
			}, Scheduler::Pipeline::MAIN);
		}

		const bool isReadyToConnect = status->contains("match_id") && status->at("match_id").is_string() && state == "CONNECTING" && joinCountdown <= 0
			&& !isInGame && !isServerJoinTransition && !isEndpointJoinInFlight;

		if (!isReadyToConnect)
		{
			return;
		}

		const auto matchId = status->at("match_id").get<std::string>();

		if (status->value("join_in_progress", false))
		{
			BeginJoinInProgressPreview(*status);
		}
		else if (!IsRelayUsedForMatch(matchId))
		{
			ConnectMatch(matchId, false, false);
		}
	}

	void ZWNet::RefreshNetworkMetrics()
	{
		if (!isActive || !isNetworkMetricsEnabled)
		{
			return;
		}

		const auto response = Request("GET", "/api/status");

		const bool hasMetrics = response && response->is_object() && !response->contains("error")
			&& response->contains("online_players") && response->at("online_players").is_number_integer()
			&& response->contains("running_games") && response->at("running_games").is_number_integer();

		if (hasMetrics)
		{
			const auto onlinePlayers = std::max(0, response->at("online_players").get<int>());
			const auto runningGames = std::max(0, response->at("running_games").get<int>());

			lastMetricsSuccess = std::chrono::steady_clock::now();

			Scheduler::Once([onlinePlayers, runningGames]
			{
				if (!isActive || !isNetworkMetricsEnabled)
				{
					return;
				}

				const char* playerNoun = "PLAYERS";
				const char* gameNoun = "GAMES";

				if (onlinePlayers == 1)
				{
					playerNoun = "PLAYER";
				}

				if (runningGames == 1)
				{
					gameNoun = "GAME";
				}

				Dvar::Var("zwnet_online_players_known").Set(true);
				Dvar::Var("zwnet_online_players_text").Set(std::format("{} {} ONLINE", onlinePlayers, playerNoun));
				Dvar::Var("zwnet_running_games_known").Set(true);
				Dvar::Var("zwnet_running_games_text").Set(std::format("{} RUNNING {}", runningGames, gameNoun));
			}, Scheduler::Pipeline::MAIN);

			return;
		}

		const bool hasRecentSuccess = lastMetricsSuccess.time_since_epoch().count() != 0
			&& std::chrono::steady_clock::now() - lastMetricsSuccess <= 30s;

		if (hasRecentSuccess)
		{
			return;
		}

		Scheduler::Once([]
		{
			if (!isActive || !isNetworkMetricsEnabled)
			{
				return;
			}

			Dvar::Var("zwnet_online_players_known").Set(false);
			Dvar::Var("zwnet_online_players_text").Set("PLAYERS ONLINE UNAVAILABLE");
			Dvar::Var("zwnet_running_games_known").Set(false);
			Dvar::Var("zwnet_running_games_text").Set("RUNNING GAMES UNAVAILABLE");
		}, Scheduler::Pipeline::MAIN);
	}

	static void RunSafely(void (*function)())
	{
		try
		{
			function();
		}
		catch (const std::exception&)
		{
			Logger::Print("ZWNET could not read a response from the online service\n");
		}
	}

	static void RegisterStringDvars(const std::initializer_list<std::pair<const char*, const char*>> names, const char* description)
	{
		for (const auto& [name, value] : names)
		{
			Dvar::Register(name, value, Game::DVAR_NONE, description);
		}
	}

	void ZWNet::InitializeDvars()
	{
		Dvar::Register("ui_zwnet_state", "OFFLINE", Game::DVAR_NONE, "Localized ZWNET state key");
		Dvar::Register("ui_zwnet_state_text", "OFFLINE", Game::DVAR_NONE, "Readable ZWNET state text");
		Dvar::Register("ui_zwnet_error", "", Game::DVAR_NONE, "Stable ZWNET error key");
		Dvar::Register("ui_zwnet_error_text", "", Game::DVAR_NONE, "Readable ZWNET error text");
		Dvar::Register("ui_zwnet_guid", PublicGuid().data(), Game::DVAR_ROM, "Public ZW3 GUID used for Stats account linking");
		Dvar::Register("zwnet_managed_session", false, Game::DVAR_NONE, "Active ZWNET matchmaking session");
		Dvar::Register("zwnet_catalog_status", "LOADING", Game::DVAR_NONE, "Client-safe playlist catalog state");
		Dvar::Register("zwnet_catalog_notice", false, Game::DVAR_NONE, "A relevant playlist update is available");
		Dvar::Register("zwnet_catalog_can_select", false, Game::DVAR_NONE, "Local party leader can change playlist selection");
		Dvar::Register("zwnet_catalog_can_search", false, Game::DVAR_NONE, "Selected playlist can be searched");
		Dvar::Register("zwnet_catalog_page", 1, 1, 100, Game::DVAR_NONE, "Playlist page");
		Dvar::Register("zwnet_catalog_pages", 1, 1, 100, Game::DVAR_NONE, "Playlist page count");
		Dvar::Register("zwnet_catalog_selected_name", "NO AVAILABLE PLAYLIST", Game::DVAR_NONE, "Selected playlist title");
		Dvar::Register("zwnet_catalog_selected_description", "", Game::DVAR_NONE, "Selected playlist description");
		Dvar::Register("zwnet_catalog_selected_audience", "", Game::DVAR_NONE, "Selected playlist audience");
		Dvar::Register("zwnet_catalog_selected_availability", "NO_SELECTION", Game::DVAR_NONE, "Selected playlist availability code");
		Dvar::Register("zwnet_catalog_selected_status", "Choose a playlist to continue.", Game::DVAR_NONE, "Selected playlist availability detail");
		Dvar::Register("zwnet_catalog_selected_revision", 0, 0, std::numeric_limits<int>::max(), Game::DVAR_NONE, "Selected playlist revision");
		Dvar::Register("zwnet_catalog_selected_rotation", "No map rotation is available.", Game::DVAR_NONE, "Selected playlist rotation summary");
		Dvar::Register("zwnet_catalog_selected_image", "", Game::DVAR_NONE, "Selected playlist preview material");
		Dvar::Register("zwnet_catalog_selected_players", "", Game::DVAR_NONE, "Selected playlist player range");
		Dvar::Register("zwnet_catalog_selected_zombie_settings", "", Game::DVAR_NONE, "Selected playlist zombie settings");
		Dvar::Register("zwnet_catalog_action_error", "", Game::DVAR_NONE, "Playlist activation feedback");
		Dvar::Register("zwnet_catalog_busy", false, Game::DVAR_NONE, "Playlist activation is in flight");
		Dvar::Register("zwnet_search_playlist_id", "", Game::DVAR_NONE, "Authoritative matchmaking playlist ID");
		Dvar::Register("zwnet_search_playlist_name", "", Game::DVAR_NONE, "Authoritative matchmaking playlist name");
		Dvar::Register("zwnet_search_playlist_revision", 0, 0, std::numeric_limits<int>::max(), Game::DVAR_NONE, "Authoritative matchmaking playlist revision");
		Dvar::Register("zwnet_search_playlist_settings", "", Game::DVAR_NONE, "Authoritative matchmaking zombie settings");

		for (std::size_t slot = 0; slot < playlistPageSize; ++slot)
		{
			Dvar::Register(playlistSlotVisibleDvars[slot], false, Game::DVAR_NONE, "Playlist slot is populated");
			Dvar::Register(playlistSlotSelectedDvars[slot], false, Game::DVAR_NONE, "Playlist slot is selected");
			Dvar::Register(playlistSlotNameDvars[slot], "", Game::DVAR_NONE, "Playlist display name");
			Dvar::Register(playlistSlotDescriptionDvars[slot], "", Game::DVAR_NONE, "Playlist description");
			Dvar::Register(playlistSlotPreviewDvars[slot], "", Game::DVAR_NONE, "Playlist map preview");
			Dvar::Register(playlistSlotImageDvars[slot], "", Game::DVAR_NONE, "Playlist local map material");
			Dvar::Register(playlistSlotAudienceDvars[slot], "", Game::DVAR_NONE, "Playlist audience");
			Dvar::Register(playlistSlotAvailabilityDvars[slot], "", Game::DVAR_NONE, "Playlist availability");
			Dvar::Register(playlistSlotStatusDvars[slot], "", Game::DVAR_NONE, "Playlist availability detail");
		}

		Dvar::Register("zwnet_lobby_active", false, Game::DVAR_NONE, "ZWNET party lobby is active");
		Dvar::Register("zwnet_lobby_party_id", "", Game::DVAR_NONE, "Current ZWNET party");
		Dvar::Register("zwnet_lobby_visibility", "OPEN", Game::DVAR_NONE, "Current ZWNET party visibility");
		Dvar::Register("zwnet_lobby_owner", "", Game::DVAR_NONE, "Current party owner");
		Dvar::Register("zwnet_lobby_member_count", 0, 0, 4, Game::DVAR_NONE, "Current party size");
		Dvar::Register("zwnet_lobby_status_text", "IDLE", Game::DVAR_NONE, "Current party state");
		Dvar::Register("zwnet_lobby_can_start", false, Game::DVAR_NONE, "Private match can start");
		Dvar::Register("zwnet_lobby_self_ready", false, Game::DVAR_NONE, "Local party ready state");
		Dvar::Register("zwnet_ready_pending", false, Game::DVAR_NONE, "A ready-state update is in flight");
		Dvar::Register("zwnet_all_ready", false, Game::DVAR_NONE, "All active match players are ready");
		Dvar::Register("zwnet_start_phase", "", Game::DVAR_NONE, "Server start phase");
		Dvar::Register("zwnet_start_seconds", 0, 0, 300, Game::DVAR_NONE, "Server start phase time remaining");
		Dvar::Register("zwnet_join_preview_active", false, Game::DVAR_NONE, "Join-in-progress preview is visible");
		Dvar::Register("zwnet_join_preview_seconds", 0, 0, 3, Game::DVAR_NONE, "Join-in-progress preview duration");
		Dvar::Register("zwnet_online_players_known", false, Game::DVAR_NONE, "Online-player metric is authoritative and fresh");
		Dvar::Register("zwnet_online_players_text", "PLAYERS ONLINE UNAVAILABLE", Game::DVAR_NONE, "Online-player metric label");
		Dvar::Register("zwnet_running_games_known", false, Game::DVAR_NONE, "Running-game metric is authoritative and fresh");
		Dvar::Register("zwnet_running_games_text", "RUNNING GAMES UNAVAILABLE", Game::DVAR_NONE, "Running-game metric label");

		static std::deque<std::string> memberDvarNames;

		const auto memberDvarName = [](const std::size_t member, const char* suffix) -> const char*
		{
			memberDvarNames.emplace_back(std::format("zwnet_lobby_member_{}{}", member, suffix));
			return memberDvarNames.back().data();
		};

		for (std::size_t i = 0; i < 4; ++i)
		{
			Dvar::Register(memberDvarName(i, "_name"), "", Game::DVAR_NONE, "Party member name");
			Dvar::Register(memberDvarName(i, "_guid"), "", Game::DVAR_NONE, "Public-lobby member GUID");
			Dvar::Register(memberDvarName(i, "_role"), "", Game::DVAR_NONE, "Party member role");
			Dvar::Register(memberDvarName(i, "_ready"), false, Game::DVAR_NONE, "Party member ready state");
			Dvar::Register(memberDvarName(i, "_self"), false, Game::DVAR_NONE, "Local public-lobby member slot");
			Dvar::Register(memberDvarName(i, "_shared_rank_known"), false, Game::DVAR_NONE, "Public-lobby member shared rank is available");
			Dvar::Register(memberDvarName(i, "_shared_rank_level"), 1, 1, 54, Game::DVAR_NONE, "Public-lobby member shared rank level");
			Dvar::Register(memberDvarName(i, "_shared_rank_prestige"), 0, 0, 255, Game::DVAR_NONE, "Public-lobby member shared rank prestige");
			Dvar::Register(memberDvarName(i, "_rank_icon"), "", Game::DVAR_NONE, "Public lobby member rank icon");
			Dvar::Register(memberDvarName(i, "_rank_level"), "", Game::DVAR_NONE, "Public lobby member rank level");
		}

		Dvar::Register("zwnet_vote_active", false, Game::DVAR_NONE, "Map vote is active");
		Dvar::Register("zwnet_vote_proposal_id", "", Game::DVAR_NONE, "Current map vote");
		Dvar::Register("zwnet_vote_seconds", 0, 0, 60, Game::DVAR_NONE, "Map vote time remaining");
		Dvar::Register("zwnet_vote_selection", "", Game::DVAR_NONE, "Local map vote selection");

		RegisterStringDvars({ { "zwnet_vote_map_a_id", "" }, { "zwnet_vote_map_b_id", "" } }, "Map vote internal id");
		RegisterStringDvars({ { "zwnet_vote_map_a_name", "" }, { "zwnet_vote_map_b_name", "" } }, "Map vote display name");
		RegisterStringDvars({ { "zwnet_vote_map_a_image", "" }, { "zwnet_vote_map_b_image", "" } }, "Map vote preview material");
		Dvar::Register("zwnet_vote_map_a_votes", 0, 0, 4, Game::DVAR_NONE, "Map vote count");
		Dvar::Register("zwnet_vote_map_b_votes", 0, 0, 4, Game::DVAR_NONE, "Map vote count");

		Dvar::Register("zwnet_vote_random_votes", 0, 0, 4, Game::DVAR_NONE, "Random map vote count");
		Dvar::Register("zwnet_vote_reveal_time", 0, 0, std::numeric_limits<int>::max(), Game::DVAR_NONE, "Map result reveal start time");
		Dvar::Register("zwnet_vote_reveal_slot", 0, 0, 2, Game::DVAR_NONE, "Map result source card: A, B or random");
		Dvar::Register("zwnet_vote_winner_id", "", Game::DVAR_NONE, "Winning ZW3 map id");
		Dvar::Register("zwnet_vote_winner_name", "", Game::DVAR_NONE, "Winning ZW3 map name");
		Dvar::Register("zwnet_vote_winner_image", "", Game::DVAR_NONE, "Winning ZW3 map preview");
		Dvar::Register("zwnet_match_id", "", Game::DVAR_NONE, "Current ZW3 match id");
		Dvar::Register("zwnet_match_return", false, Game::DVAR_NONE, "Completed ZW3 match should return to its lobby");
		Dvar::Register("zwnet_server_endpoint", "", Game::DVAR_NONE, "Assigned public ZW3 server endpoint");
		Dvar::Register("zwnet_server_hostname", "", Game::DVAR_NONE, "Connected ZW3 server hostname");
		Dvar::Register("zwnet_server_status", "NOT ASSIGNED", Game::DVAR_NONE, "ZW3 server assignment status");
		Dvar::Register("zwnet_join_status", "WAITING IN LOBBY", Game::DVAR_NONE, "ZW3 join status");
		Dvar::Register("zwnet_join_countdown", 0, 0, 30, Game::DVAR_NONE, "Synchronized ZW3 join countdown");
		Dvar::Register("zwnet_selected_player_guid", "", Game::DVAR_NONE, "Selected public-lobby player GUID");
		Dvar::Register("zwnet_selected_player_name", "", Game::DVAR_NONE, "Selected public-lobby player name");
		Dvar::Register("zwnet_selected_player_role", "", Game::DVAR_NONE, "Selected public-lobby player role");
		Dvar::Register("zwnet_selected_player_rank", 1, 1, 54, Game::DVAR_NONE, "Selected public-lobby player rank");
		Dvar::Register("zwnet_selected_player_prestige", 0, 0, 255, Game::DVAR_NONE, "Selected public-lobby player prestige");
		Dvar::Register("zwnet_selected_player_rank_icon", "prestige_1", Game::DVAR_NONE, "Selected public-lobby player prestige icon");
		Dvar::Register("zwnet_selected_player_self", false, Game::DVAR_NONE, "Selected public-lobby player is local");
		Dvar::Register("zwnet_selected_player_relationship", "UNAVAILABLE", Game::DVAR_NONE, "Selected public-lobby player friend state");
		Dvar::Register("zwnet_barracks_compare_active", false, Game::DVAR_NONE, "Barracks was opened for a lobby comparison");
		Dvar::Register("zw3_barracks_rank_known", false, Game::DVAR_NONE, "Local ZW3 Barracks rank data is available");
		Dvar::Register("zw3_barracks_rank_level", 1, 1, 54, Game::DVAR_NONE, "Local ZW3 Barracks rank level");
		Dvar::Register("zw3_barracks_rank_prestige", 0, 0, 255, Game::DVAR_NONE, "Local ZW3 Barracks prestige");
		Dvar::Register("zw3_barracks_rank_experience", 0, 0, std::numeric_limits<int>::max(), Game::DVAR_NONE, "Local ZW3 Barracks experience");
		Dvar::Register("zw3_barracks_rank_experience_target", 50, 1, std::numeric_limits<int>::max(), Game::DVAR_NONE, "Current ZW3 Barracks level XP target");
		Dvar::Register("zw3_barracks_rank_experience_percent", 0, 0, 100, Game::DVAR_NONE, "Current ZW3 Barracks level XP completion percent");
		Dvar::Register("zw3_barracks_rank_icon", "prestige_1", Game::DVAR_NONE, "Local ZW3 Barracks prestige icon");
		Dvar::Register("zw3_barracks_zombie_kills", 0, 0, std::numeric_limits<int>::max(), Game::DVAR_NONE, "Lifetime ZW3 zombie kills");
		Dvar::Register("zw3_barracks_zombie_deaths", 0, 0, std::numeric_limits<int>::max(), Game::DVAR_NONE, "Lifetime ZW3 zombie deaths");
		Dvar::Register("zw3_barracks_zombie_revives", 0, 0, std::numeric_limits<int>::max(), Game::DVAR_NONE, "Lifetime ZW3 teammate revives");

		for (std::size_t slot = 0; slot < challengeSlotProgressDvars.size(); ++slot)
		{
			Dvar::Register(challengeSlotProgressDvars[slot], 0, 0, std::numeric_limits<int>::max(), Game::DVAR_NONE, "Current ZW3 challenge progress");
			Dvar::Register(challengeSlotTargetDvars[slot], 0, 0, std::numeric_limits<int>::max(), Game::DVAR_NONE, "Current ZW3 challenge target");
			Dvar::Register(challengeSlotTierDvars[slot], 0, 0, 4, Game::DVAR_NONE, "Completed ZW3 challenge tiers");
			Dvar::Register(challengeSlotTierCountDvars[slot], 0, 0, 4, Game::DVAR_NONE, "Available ZW3 challenge tiers");
			Dvar::Register(challengeSlotRewardDvars[slot], 0, 0, std::numeric_limits<int>::max(), Game::DVAR_NONE, "Next ZW3 challenge XP reward");
			Dvar::Register(challengeSlotPercentDvars[slot], 0, 0, 100, Game::DVAR_NONE, "Current ZW3 challenge completion percent");
			Dvar::Register(challengeSlotCompleteDvars[slot], false, Game::DVAR_NONE, "ZW3 challenge is complete");
		}

		constexpr const char* questChallengeDvars[] =
		{
			"zw3_quest_mp_factory_sh", "zw3_quest_mp_asylum_sh", "zw3_quest_mp_prototype_sh",
			"zw3_quest_mp_sumpf_sh", "zw3_quest_mp_za_island", "zw3_quest_mp_deathmarch_chap4_a",
			"zw3_quest_mp_deathmarch_chap4_b", "zw3_quest_mp_surv_town", "zw3_quest_mp_burg",
			"zw3_quest_mp_lambeth",
		};

		for (const auto* questDvar : questChallengeDvars)
		{
			Dvar::Register(questDvar, 0, 0, 10, Game::DVAR_ARCHIVE, "Completed tiers for a repeatable ZW3 questline");
		}

		isActive = true;
		RefreshBarracksProfile();
	}

	ZWNet::ZWNet()
	{
		if (Dedicated::IsEnabled())
		{
			return;
		}

		Localization::Set("ZWNET_LOGIN_REQUIRED", "Sign in on the ZW3 Stats page.");
		Localization::Set("ZWNET_SESSION_EXPIRED", "Your ZW3 session expired. Please sign in again.");
		Localization::Set("ZWNET_SEARCH_FAILED", "Matchmaking could not be started.");
		Localization::Set("ZWNET_CATALOG_UNAVAILABLE", "Playlists are unavailable. Please refresh the list.");
		Localization::Set("ZWNET_PLAYLIST_REFRESH_REQUIRED", "This playlist changed or access ended. Refresh the list and choose again.");
		Localization::Set("ZWNET_PLAYLIST_SELECTION_FAILED", "The party playlist could not be changed. Refresh and try again.");
		Localization::Set("ZWNET_ROUTE_UNAVAILABLE", "No direct or relay route is available.");
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
		Localization::Set("ZWNET_RELAY_TIMEOUT", "The relay did not confirm the connection. Return to the lobby and try again.");

		Network::OnPacket("zwnetRelayReady", [](Network::Address& address, const std::string& data)
		{
			if (data.size() > relayReadyLimit)
			{
				return;
			}

			const auto ready = nlohmann::json::parse(data, nullptr, false);

			const bool isReadyShaped = ready.is_object()
				&& ready.size() == 2
				&& ready.contains("schema_version") && ready.at("schema_version").is_number_integer()
				&& ready.at("schema_version").get<int>() == 1
				&& ready.contains("nonce") && ready.at("nonce").is_string();

			if (!isReadyShaped)
			{
				return;
			}

			const auto nonce = ready.at("nonce").get<std::string>();

			const bool isNonceShaped = nonce.size() == 32 && std::ranges::all_of(nonce, [](const unsigned char character)
			{
				return (character >= '0' && character <= '9') || (character >= 'a' && character <= 'f');
			});

			if (!isNonceShaped)
			{
				return;
			}

			std::uint64_t generation = 0;
			Network::Address target;
			std::string matchId;
			std::string playerId;

			{
				std::lock_guard _(relayHandshake.mutex);

				const bool isAwaited = relayHandshake.isPending
					&& std::chrono::steady_clock::now() < relayHandshake.deadline
					&& address == relayHandshake.target
					&& nonce == relayHandshake.nonce;

				if (!isAwaited)
				{
					return;
				}

				relayHandshake.isPending = false;
				relayHandshake.isReady = true;
				std::ranges::fill(relayHandshake.hello, '\0');
				relayHandshake.hello.clear();
				generation = relayHandshake.generation;
				target = relayHandshake.target;
				matchId = relayHandshake.matchId;
				playerId = relayHandshake.playerId;
			}

			Scheduler::Once([generation, target, matchId, playerId]
			{
				if (!isActive)
				{
					return;
				}

				{
					std::lock_guard _(relayHandshake.mutex);

					if (relayHandshake.generation != generation || !relayHandshake.isReady)
					{
						return;
					}

					relayHandshake.isReady = false;
					relayHandshake.nonce.clear();
					relayHandshake.matchId.clear();
					relayHandshake.playerId.clear();
				}

				bool isSameSession = false;

				{
					std::lock_guard _(stateMutex);
					isSameSession = currentPlayerId == playerId && currentMatchId == matchId;
				}

				if (!isSameSession)
				{
					Auth::ClearManagedConnectTicket();
					isManagedReconnectInFlight = false;
					++joinTransitionGeneration;
					isServerJoinTransition = false;
					return;
				}

				SetPresenceUiState("CONNECTING");
				Dvar::Var("ui_zwnet_state").Set("CONNECTING");
				Dvar::Var("ui_zwnet_error").Set("");
				Dvar::Var("zwnet_server_endpoint").Set(target.GetString());
				Dvar::Var("zwnet_server_status").Set("RELAY READY");
				Dvar::Var("zwnet_join_status").Set("JOINING SERVER");
				Dvar::Var("zwnet_managed_session").Set(true);

				MarkJoinInProgressConnectionStarted(matchId);

				const auto transitionGeneration = ++joinTransitionGeneration;
				isServerJoinTransition = true;

				Scheduler::Once([transitionGeneration]
				{
					if (joinTransitionGeneration == transitionGeneration)
					{
						isServerJoinTransition = false;
					}
				}, Scheduler::Pipeline::MAIN, 12s);

				Party::Connect(target);
				isManagedReconnectInFlight = false;
			}, Scheduler::Pipeline::MAIN);
		});

		Scheduler::Loop([]
		{
			if (!isActive)
			{
				return;
			}

			Network::Address target;
			std::string hello;
			bool hasTimedOut = false;

			{
				std::lock_guard _(relayHandshake.mutex);

				if (!relayHandshake.isPending)
				{
					return;
				}

				if (std::chrono::steady_clock::now() >= relayHandshake.deadline)
				{
					++relayHandshake.generation;
					relayHandshake.isPending = false;
					std::ranges::fill(relayHandshake.hello, '\0');
					relayHandshake.hello.clear();
					relayHandshake.nonce.clear();
					relayHandshake.matchId.clear();
					relayHandshake.playerId.clear();
					hasTimedOut = true;
				}
				else
				{
					target = relayHandshake.target;
					hello = relayHandshake.hello;
				}
			}

			if (hasTimedOut)
			{
				Auth::ClearManagedConnectTicket();
				isManagedReconnectInFlight = false;
				++joinTransitionGeneration;
				isServerJoinTransition = false;
				Command::Execute("closemenu popup_reconnectingtoparty", false);
				SetState("ERROR", "ZWNET_RELAY_TIMEOUT");
				return;
			}

			Network::SendCommand(target, "zwnetRelayHello", hello);
			std::ranges::fill(hello, '\0');
		}, Scheduler::Pipeline::MAIN, 1s);

		Command::Add("zwnet_login", []
		{
			Login();
		});

		Command::Add("zwnet_register", []
		{
			EnqueueAsync([]
			{
				Register();
			});
		});

		Command::Add("zwnet_quickplay", []
		{
			CapturePartyPrivacy();

			Scheduler::Once([]
			{
				if (isActive && !isSearching && !isInGame)
				{
					Command::Execute("openmenu popup_zwnet_playlists", false);
				}
			}, Scheduler::Pipeline::MAIN);
		});

		Command::Add("zwnet_manual_join", [](const Command::Params* params)
		{
			if (params->Size() != 2 || !IsOpaqueMatchId(params->Get(1)))
			{
				Logger::Print("Usage: zwnet_manual_join <manual match ID>\n");
				return;
			}

			const std::string matchId = params->Get(1);

			EnqueueAsync([matchId]
			{
				const auto admitted = Request("POST", "/zwnet/manual/join", { { "match_id", matchId } });

				if (!admitted || admitted->contains("error") || JsonString(*admitted, "match_id") != matchId)
				{
					SetState("ERROR", "ZWNET_MANUAL_JOIN_DENIED");
					return;
				}

				{
					std::lock_guard _(stateMutex);
					currentMatchId = matchId;
				}

				ConnectMatch(matchId, false, false);
			});
		});

		Command::Add("zwnet_cancel", []
		{
			EnqueueAsync([]
			{
				CancelSearch();
			});
		});

		Command::Add("zwnet_terminal_disconnect", []
		{
			isTerminalDisconnectRequested = true;

			Scheduler::Once([]
			{
				isTerminalDisconnectRequested = false;
			}, Scheduler::Pipeline::MAIN, 5s);
		});

		Command::Add("zwnet_logout", []
		{
			EnqueueAsync([]
			{
				CloseOnlineSession(false, true);
				Request("POST", "/social/client/logout");

				if (!isActive)
				{
					return;
				}

				ClearSession();
				SetState("OFFLINE");
			});
		});

		UIScript::Add("ZWNetQuickPlay", [](const UIScript::Token&)
		{
			Command::Execute("zwnet_quickplay", false);
		});

		UIScript::Add("ZWNET_BeginPlaylistSelection", [](const UIScript::Token&)
		{
			BeginPlaylistSelection();
		});

		UIScript::Add("ZWNET_CancelPlaylistSelection", [](const UIScript::Token&)
		{
			CancelPlaylistSelection();
		});

		UIScript::Add("ZWNET_RefreshPlaylists", [](const UIScript::Token&)
		{
			EnqueueAsync([]
			{
				RefreshPlaylistCatalog(true);
			});
		});

		UIScript::Add("ZWNET_AcknowledgePlaylists", [](const UIScript::Token&)
		{
			AcknowledgePlaylistNotice();
		});

		UIScript::Add("ZWNET_HighlightPlaylist", [](const UIScript::Token& token)
		{
			HighlightPlaylistSlot(token.Get<int>());
		});

		UIScript::Add("ZWNET_ActivatePlaylist", [](const UIScript::Token& token)
		{
			ActivatePlaylistSlot(token.Get<int>());
		});

		UIScript::Add("ZWNET_PlaylistPage", [](const UIScript::Token& token)
		{
			ChangePlaylistPage(token.Get<int>());
		});

		UIScript::Add("ZWNetCancel", [](const UIScript::Token&)
		{
			Command::Execute("zwnet_cancel", false);
		});

		UIScript::Add("ZWNET_CancelMatchmaking", [](const UIScript::Token&)
		{
			EnqueueAsync([]
			{
				CancelMatchmaking();
			});
		});

		UIScript::Add("ZWNET_SetMatchmakingMenuActive", [](const UIScript::Token& token)
		{
			const bool isEnabled = token.Get<int>() != 0;
			isNetworkMetricsEnabled = isEnabled;

			if (isEnabled)
			{
				EnqueueAsync([]
				{
					RefreshNetworkMetrics();
				});
			}
		});

		UIScript::Add("ZWNET_QuitToMatchmaking", [](const UIScript::Token&)
		{
			isTerminalDisconnectRequested = true;
			Command::Execute("disconnect", false);
		});

		UIScript::Add("ZWNET_CloseOnlineSession", [](const UIScript::Token&)
		{
			bool hasMatch = false;

			{
				std::lock_guard _(stateMutex);
				hasMatch = !currentMatchId.empty();
			}

			const bool wasMatch = Dvar::Var("zwnet_managed_session").Get<bool>() || isInGame || hasMatch;

			EnqueueAsync([]
			{
				CloseOnlineSession(false, true);
			});

			if (wasMatch && Game::CL_IsCgameInitialized(0))
			{
				ScheduleReturnToIdleMenu();
			}
		});

		UIScript::Add("ZWNetLogin", [](const UIScript::Token&)
		{
			Command::Execute("zwnet_login", false);
		});

		UIScript::Add("ZWNET_ConnectOnline", [](const UIScript::Token&)
		{
			BeginOnlineEntry();
		});

		UIScript::Add("ZWNET_CancelOnlineEntry", [](const UIScript::Token&)
		{
			isOnlineEntryPending = false;
		});

		UIScript::Add("ZWNET_AbandonOnlineSession", [](const UIScript::Token&)
		{
			AbandonOnlineSession();
		});

		UIScript::Add("ZWNetRegister", [](const UIScript::Token&)
		{
			Command::Execute("zwnet_register", false);
		});

		UIScript::Add("ZWNetCopyGuid", [](const UIScript::Token&)
		{
			if (!TryCopyPublicGuidToClipboard())
			{
				SetState("ERROR", "ZWNET_GUID_COPY_FAILED");
			}
		});

		UIScript::Add("ZWNET_EnterPrivateLobby", [](const UIScript::Token&)
		{
			CapturePartyPrivacy();

			const auto map = Dvar::Var("ui_mapname").Get<std::string>();

			EnqueueAsync([map]
			{
				EnterLobby(map);
			});
		});

		UIScript::Add("ZWNET_RefreshLobby", [](const UIScript::Token&)
		{
			EnqueueAsync([]
			{
				RefreshLobby();
			});
		});

		UIScript::Add("ZWNET_LeaveParty", [](const UIScript::Token&)
		{
			EnqueueAsync([]
			{
				LeaveParty();
			});
		});

		UIScript::Add("ZWNET_ToggleReady", [](const UIScript::Token&)
		{
			if (Dvar::Var("zwnet_ready_pending").Get<bool>())
			{
				return;
			}

			const auto isReady = Dvar::Var("zwnet_lobby_self_ready").Get<bool>();
			Dvar::Var("zwnet_ready_pending").Set(true);

			EnqueueAsync([isReady]
			{
				ToggleReady(isReady);
			});
		});

		UIScript::Add("ZWNET_SelectLobbyPlayer", [](const UIScript::Token& token)
		{
			const auto index = token.Get<int>();

			if (index < 0 || index >= 4)
			{
				return;
			}

			const auto prefix = std::format("zwnet_lobby_member_{}", index);
			auto guid = Dvar::Var(prefix + "_guid").Get<std::string>();
			const auto name = Dvar::Var(prefix + "_name").Get<std::string>();

			if (guid.empty() || name.empty())
			{
				return;
			}

			std::ranges::transform(guid, guid.begin(), [](const unsigned char character)
			{
				return static_cast<char>(std::tolower(character));
			});

			Dvar::Var("zwnet_selected_player_guid").Set(guid);
			Dvar::Var("zwnet_selected_player_name").Set(name);
			Dvar::Var("zwnet_selected_player_role").Set(Dvar::Var(prefix + "_role").Get<std::string>());
			Dvar::Var("zwnet_selected_player_rank").Set(Dvar::Var(prefix + "_shared_rank_level").Get<int>());
			Dvar::Var("zwnet_selected_player_prestige").Set(Dvar::Var(prefix + "_shared_rank_prestige").Get<int>());
			Dvar::Var("zwnet_selected_player_rank_icon").Set(Dvar::Var(prefix + "_rank_icon").Get<std::string>());
			Dvar::Var("zwnet_selected_player_self").Set(Dvar::Var(prefix + "_self").Get<bool>());
			Dvar::Var("zwnet_selected_player_relationship").Set(Friends::GetLobbyPlayerRelationship(guid));
		});

		UIScript::Add("ZWNET_RefreshBarracksProfile", [](const UIScript::Token&)
		{
			RefreshBarracksProfile();
		});

		UIScript::Add("ZWNET_RefreshChallengeCategory", [](const UIScript::Token& token)
		{
			RefreshChallengeCategory(token.Get<int>());
		});

		UIScript::Add("ZWNET_StartPrivateMatch", [](const UIScript::Token&)
		{
			const auto map = Dvar::Var("ui_mapname").Get<std::string>();

			EnqueueAsync([map]
			{
				StartPrivateMatch(map);
			});
		});

		UIScript::Add("ZWNET_VoteMapA", [](const UIScript::Token&)
		{
			EnqueueAsync([]
			{
				VoteMap("A");
			});
		});

		UIScript::Add("ZWNET_VoteMapB", [](const UIScript::Token&)
		{
			EnqueueAsync([]
			{
				VoteMap("B");
			});
		});

		UIScript::Add("ZWNET_VoteRandom", [](const UIScript::Token&)
		{
			EnqueueAsync([]
			{
				VoteMap("RANDOM");
			});
		});

		Events::OnCLDisconnected([](const bool wasConnected)
		{
			isInGame = false;
			Dvar::Var("zwnet_match_return").Set(false);

			if (isServerJoinTransition.exchange(false))
			{
				Logger::Print("ZWNET server join transition: preserving online session\n");
				return;
			}

			isJoinInProgressSoundPending = false;

			bool hasMatch = false;

			{
				std::lock_guard _(stateMutex);
				hasMatch = !currentMatchId.empty();
			}

			const bool isManaged = Dvar::Var("zwnet_managed_session").Get<bool>();
			const bool wasMatchmaking = hasMatch || isManaged;
			const bool isPrivateMatchClient = wasConnected && !wasMatchmaking && Party::IsPrivateMatchClient();
			const bool isTerminal = isTerminalDisconnectRequested.exchange(false);

			if (wasMatchmaking)
			{
				EnqueueAsync([isTerminal, wasMatchmaking]
				{
					HandleServerDisconnect(isTerminal, wasMatchmaking);
				});
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
			isEndpointJoinInFlight = false;
			isServerJoinTransition = false;

			bool hasMatch = false;

			{
				std::lock_guard _(stateMutex);
				hasMatch = !currentMatchId.empty();
			}

			isInGame = hasMatch;

			if (!hasMatch)
			{
				return;
			}

			if (isJoinInProgressSoundPending.exchange(false))
			{
				Game::SND_PlayLocalSoundAliasByName(0, "mouse_click", 0);
			}

			Dvar::Var("zwnet_managed_session").Set(true);
			SetDisplayText("zwnet_server_hostname", Party::GetHostName());
			SetState("IN_MATCH");

			EnqueueAsync([]
			{
				UpdatePresence();
			});
		});

		Scheduler::OnGameInitialized([]
		{
			Logger::Print("ZWNET initialization: registering dvars\n");
			InitializeDvars();
		}, Scheduler::Pipeline::MAIN);

		Scheduler::OnGameInitialized([]
		{
			if (!isActive)
			{
				return;
			}

			Logger::Print("ZWNET initialization: checking saved session\n");

			if (LoadSession())
			{
				Logger::Print("ZWNET initialization: starting refresh\n");

				EnqueueAsync([]
				{
					Refresh();
				});
			}
			else
			{
				Logger::Print("ZWNET initialization: starting login\n");
				Login();
			}
		}, Scheduler::Pipeline::MAIN, 2s);

		Scheduler::Loop(ProcessAsyncTasks, Scheduler::Pipeline::ASYNC, 50ms);

		Scheduler::Loop([]
		{
			RunSafely(UpdateMatchmaking);
		}, Scheduler::Pipeline::ASYNC, 1s);

		Scheduler::Loop([]
		{
			RunSafely(RefreshActiveParty);
		}, Scheduler::Pipeline::ASYNC, 3s);

		Scheduler::Loop([]
		{
			if (!isActive || !Dvar::Var("zwnet_vote_active").Get<bool>())
			{
				return;
			}

			const auto seconds = Dvar::Var("zwnet_vote_seconds").Get<int>();

			if (seconds > 0)
			{
				Dvar::Var("zwnet_vote_seconds").Set(seconds - 1);
			}
		}, Scheduler::Pipeline::MAIN, 1s);

		Scheduler::Loop([]
		{
			RunSafely([]
			{
				RefreshPlaylistCatalog(false);
			});
		}, Scheduler::Pipeline::ASYNC, 30s);

		Scheduler::Loop([]
		{
			RunSafely(RefreshNetworkMetrics);
		}, Scheduler::Pipeline::ASYNC, 10s);

		Scheduler::Loop([]
		{
			if (!isActive || !isInGame || !Dvar::Var("zwnet_match_return").Get<bool>())
			{
				return;
			}

			std::string matchId;

			{
				std::lock_guard _(stateMutex);
				matchId = currentMatchId;
			}

			Dvar::Var("zwnet_match_return").Set(false);

			if (matchId.empty())
			{
				return;
			}

			Logger::Print("ZWNET match complete: leaving finished server before reset\n");
			Command::Execute("disconnect", false);
		}, Scheduler::Pipeline::MAIN, 100ms);

		Scheduler::Loop([]
		{
			RunSafely(UpdatePresence);
		}, Scheduler::Pipeline::ASYNC, 30s);

		Scheduler::Loop(CapturePartyPrivacy, Scheduler::Pipeline::MAIN, 1s);

		Scheduler::OnShutdown([]
		{
			isNetworkMetricsEnabled = false;
			CancelJoinInProgressPreview();

			if (isActive)
			{
				CloseOnlineSession(true, true);
			}

			isActive = false;
			isSearching = false;
			ResetMatchLobbySoundSnapshot();
			isEndpointJoinInFlight = false;
			isServerJoinTransition = false;
			isOnlineEntryPending = false;
			isInGame = false;

			{
				std::lock_guard _(stateMutex);
				isLoginInFlight = false;
			}

			std::lock_guard _(asyncTaskMutex);
			asyncTasks.clear();
		});
	}
}
