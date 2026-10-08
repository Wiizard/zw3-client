#include "STDInclude.hpp"

#include <zlib.h>

#include "ZoneConvert.hpp"

namespace Components
{
	enum class FastFileArch
	{
		X86,
		X64,
	};

	enum class ConvertAction
	{
		Move,
		Copy,
		Replace,
		Drop,
		Convert,
		Prune,
	};

	struct ConvertStep
	{
		ConvertAction action;
		std::filesystem::path from;
		std::filesystem::path to;
	};

	using ConvertPlan = std::vector<ConvertStep>;

	struct FastFileMagic
	{
		const char* id;
		std::size_t payload;
	};

	struct ZoneGroup
	{
		const char* name;
		bool (*isShared)(const std::string& fileName);
	};

	struct ConvertPhase
	{
		const char* name;
		void (*makePlan)(ConvertPlan& plan);
	};

	struct ConvertActionInfo
	{
		const char* doing;
		void (*apply)(const ConvertStep& step);
	};

	struct RunningConverter
	{
		HANDLE process;
		std::size_t step;
	};

	bool ZoneConvert::isEnabled = false;

	constexpr auto* logFile = "zone-conversion.log";
	constexpr auto* converterName = "Unlinker.exe";
	constexpr auto* progressWindowClass = "ZW3ZoneConvert";
	constexpr auto* progressTitle = "ZW3 - Preparing x86 files";

	static const std::filesystem::path zoneRoot = "zone";
	static const std::filesystem::path convertedRoot = "zone/zw3/x86";
	static const std::string convertedName = "zw3";
	static const std::filesystem::path iw4xMainRoot = "main/iw4x/x86";
	static const std::filesystem::path zw3MainRoot = "main/zw3/x86";
	static const std::filesystem::path iw4xZoneRoot = "zone/iw4x/x86";
	static const std::filesystem::path legacyBasegameRoot = "iw4x";
	static const std::string legacyMarker = "converted.txt";

	constexpr std::size_t preambleSize = 8 + 4 + 1 + 8;
	constexpr std::size_t authBlockSize = 0x2000;
	constexpr int zoneVersion = 276;
	constexpr std::size_t sizeHeaderSize = 40;
	constexpr std::size_t assetListsSize = 32;
	constexpr std::size_t wantedSize = sizeHeaderSize + assetListsSize;
	constexpr std::size_t probeReadLimit = 0x10000;

	static const FastFileMagic magics[] =
	{
		{ "IWffu100", preambleSize },
		{ "IWff0100", preambleSize + 2 * authBlockSize },
		{ "ABff0100", preambleSize + 2 * authBlockSize },
	};

	static const char* const officialDlcMaps[] =
	{
		"mp_complex", "mp_compact", "mp_storm", "mp_overgrown", "mp_crash",
		"mp_abandon", "mp_vacant", "mp_trailerpark", "mp_strike", "mp_fuel2",
	};

	constexpr int windowWidth = 480;
	constexpr int windowHeight = 170;
	constexpr int windowMargin = 20;
	constexpr int barHeight = 16;
	constexpr float sweepPeriod = 1.9f;
	constexpr float sweepExtent = 0.32f;
	constexpr float sweepStrength = 0.5f;
	constexpr float fillSpeed = 6.0f;
	constexpr DWORD frameIntervalMs = 16;

	static void LogConversion(const std::string& message)
	{
		OutputDebugStringA(message.data());
		OutputDebugStringA("\n");

		std::ofstream stream(logFile, std::ios::app);

		if (stream.is_open())
		{
			stream << message << '\n';
		}
	}

	[[noreturn]] static void FailConversion(const std::string& message)
	{
		LogConversion(message);

		MessageBoxA(nullptr, message.data(), "ZW3", MB_ICONERROR | MB_OK);

		ExitProcess(1);
	}

	static std::string GetLastErrorText()
	{
		const DWORD errorCode = GetLastError();
		LPSTR messageBuffer = nullptr;

		const DWORD length = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			nullptr, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPSTR>(&messageBuffer), 0, nullptr);

		if (!messageBuffer)
		{
			return std::format("error {}", errorCode);
		}

		std::string message(messageBuffer, length);
		LocalFree(messageBuffer);
		return message;
	}

	static std::optional<std::string> TryInflateHead(std::ifstream& stream, const std::size_t offset, const std::size_t size)
	{
		stream.clear();
		stream.seekg(static_cast<std::streamoff>(offset));

		if (!stream)
		{
			return std::nullopt;
		}

		std::string input(probeReadLimit, '\0');
		stream.read(input.data(), static_cast<std::streamsize>(input.size()));
		input.resize(static_cast<std::size_t>(stream.gcount()));

		if (input.empty())
		{
			return std::nullopt;
		}

		z_stream inflater{};

		if (inflateInit(&inflater) != Z_OK)
		{
			return std::nullopt;
		}

		std::string output(size, '\0');
		inflater.next_in = reinterpret_cast<Bytef*>(input.data());
		inflater.avail_in = static_cast<uInt>(input.size());
		inflater.next_out = reinterpret_cast<Bytef*>(output.data());
		inflater.avail_out = static_cast<uInt>(output.size());

		const int result = inflate(&inflater, Z_NO_FLUSH);
		const std::size_t produced = output.size() - inflater.avail_out;
		inflateEnd(&inflater);

		if (result != Z_OK && result != Z_STREAM_END)
		{
			return std::nullopt;
		}

		output.resize(produced);
		return output;
	}

	static bool IsX64AssetList(const std::uint8_t* list)
	{
		std::uint32_t count = 0;
		std::memcpy(&count, list, sizeof(count));

		std::uint32_t padding = 0;
		std::memcpy(&padding, list + 4, sizeof(padding));

		std::uint64_t pointer = 0;
		std::memcpy(&pointer, list + 8, sizeof(pointer));

		if (padding != 0)
		{
			return false;
		}

		if (count == 0)
		{
			return pointer == 0;
		}

		const std::uint64_t followsStream = ~std::uint64_t(0);
		return pointer == followsStream || pointer == followsStream - 1;
	}

	static std::optional<FastFileArch> TryProbeFastFile(const std::filesystem::path& file)
	{
		std::ifstream stream(file, std::ios::binary);

		if (!stream.is_open())
		{
			return std::nullopt;
		}

		std::uint8_t preamble[preambleSize]{};
		stream.read(reinterpret_cast<char*>(preamble), sizeof(preamble));

		if (stream.gcount() != static_cast<std::streamsize>(sizeof(preamble)))
		{
			return std::nullopt;
		}

		int version = 0;
		std::memcpy(&version, preamble + 8, sizeof(version));

		if (version != zoneVersion)
		{
			return std::nullopt;
		}

		const FastFileMagic* magic = nullptr;

		for (const FastFileMagic& candidate : magics)
		{
			if (std::memcmp(preamble, candidate.id, 8) == 0)
			{
				magic = &candidate;
				break;
			}
		}

		if (!magic)
		{
			return std::nullopt;
		}

		const auto head = TryInflateHead(stream, magic->payload, wantedSize);

		if (!head || head->size() < wantedSize)
		{
			return std::nullopt;
		}

		const auto* const lists = reinterpret_cast<const std::uint8_t*>(head->data()) + sizeHeaderSize;

		if (IsX64AssetList(lists) && IsX64AssetList(lists + 16))
		{
			return FastFileArch::X64;
		}

		return FastFileArch::X86;
	}

	static bool IsOfficialDlc(const std::string& fileName)
	{
		std::string mapName = std::filesystem::path(fileName).stem().generic_string();

		if (mapName.starts_with("localized_"))
		{
			mapName.erase(0, std::strlen("localized_"));
		}

		if (mapName.ends_with("_load"))
		{
			mapName.erase(mapName.size() - std::strlen("_load"));
		}

		return std::ranges::any_of(officialDlcMaps, [&mapName](const char* const officialMap)
		{
			return mapName == officialMap;
		});
	}

	static const ZoneGroup ownGroups[] =
	{
		{ "zonebuilder", nullptr },
		{ "patch", nullptr },
		{ "dlc", &IsOfficialDlc },
	};

	static bool IsOwnGroup(const std::string& name)
	{
		return std::ranges::any_of(ownGroups, [&name](const ZoneGroup& group)
		{
			return name == group.name;
		});
	}

	static bool IsLegacyBackup(const std::string& name)
	{
		return name == "old" || name.ends_with("_old");
	}

	static void PlanLegacyBasegame(ConvertPlan& plan)
	{
		if (!std::filesystem::is_directory(legacyBasegameRoot))
		{
			return;
		}

		for (const auto& entry : std::filesystem::recursive_directory_iterator(legacyBasegameRoot))
		{
			if (!entry.is_regular_file())
			{
				continue;
			}

			const auto relative = entry.path().lexically_relative(legacyBasegameRoot);
			auto target = zw3MainRoot / relative;

			if (std::filesystem::exists(target))
			{
				plan.push_back({ ConvertAction::Drop, entry.path(), {} });
			}
			else
			{
				plan.push_back({ ConvertAction::Move, entry.path(), std::move(target) });
			}
		}

		plan.push_back({ ConvertAction::Prune, legacyBasegameRoot, {} });
	}

	static void PlanIw4xTree(ConvertPlan& plan, const std::filesystem::path& from, const std::filesystem::path& to)
	{
		if (!std::filesystem::is_directory(from))
		{
			return;
		}

		for (const auto& entry : std::filesystem::recursive_directory_iterator(from))
		{
			if (!entry.is_regular_file())
			{
				continue;
			}

			const auto relative = entry.path().lexically_relative(from);
			auto destination = to / relative;

			const bool isCurrent = std::filesystem::exists(destination)
				&& std::filesystem::file_size(destination) == entry.file_size()
				&& std::filesystem::last_write_time(destination) >= entry.last_write_time();

			if (isCurrent)
			{
				continue;
			}

			plan.push_back({ ConvertAction::Copy, entry.path(), std::move(destination) });
		}
	}

	static void PlanIw4xX86(ConvertPlan& plan)
	{
		PlanIw4xTree(plan, iw4xMainRoot, zw3MainRoot);
		PlanIw4xTree(plan, iw4xZoneRoot, convertedRoot);
	}

	[[noreturn]] static void RefuseRecovery(const std::filesystem::path& backup, const std::filesystem::path& group, const std::string& reason)
	{
		FailConversion(std::format("{}\n\n"
			"This installation was converted by an older version of ZW3, and its "
			"fastfiles cannot be put back where they belong without guessing.\n\n"
			"Move the fastfiles in \"{}\" back to \"{}\", delete \"{}\", and start "
			"the game again. See \"{}\" for what was found.",
			reason, backup.generic_string(), group.generic_string(), backup.generic_string(), logFile));
	}

	static std::string FindBackupLanguage(const std::filesystem::path& backup)
	{
		std::vector<std::string> languages;

		for (const auto& entry : std::filesystem::directory_iterator(zoneRoot))
		{
			if (!entry.is_directory())
			{
				continue;
			}

			std::string name = entry.path().filename().generic_string();

			if (name == convertedName || IsOwnGroup(name) || IsLegacyBackup(name))
			{
				continue;
			}

			languages.push_back(std::move(name));
		}

		if (languages.size() == 1)
		{
			return languages.front();
		}

		std::vector<std::pair<std::size_t, std::string>> matches;

		for (std::string& language : languages)
		{
			std::size_t matchCount = 0;

			for (const auto& entry : std::filesystem::directory_iterator(backup))
			{
				if (std::filesystem::exists(zoneRoot / language / entry.path().filename()))
				{
					++matchCount;
				}
			}

			matches.emplace_back(matchCount, std::move(language));
		}

		std::ranges::sort(matches, std::ranges::greater());

		const bool isUndecided = matches.empty()
			|| matches.front().first == 0
			|| (matches.size() > 1 && matches.front().first == matches[1].first);

		if (isUndecided)
		{
			FailConversion(std::format("\"{}\" holds the fastfiles an older version of ZW3 replaced, and "
				"there is no telling which of the language directories under \"{}\" "
				"they came out of.\n\n"
				"Move them back into the directory they belong to, delete \"{}\", "
				"and start the game again.",
				backup.generic_string(), zoneRoot.generic_string(), backup.generic_string()));
		}

		return matches.front().second;
	}

	static void PlanRecovery(ConvertPlan& plan)
	{
		if (!std::filesystem::is_directory(zoneRoot))
		{
			return;
		}

		for (const auto& entry : std::filesystem::directory_iterator(zoneRoot))
		{
			if (!entry.is_directory())
			{
				continue;
			}

			const std::filesystem::path& backup = entry.path();
			const std::string backupName = backup.filename().generic_string();

			if (!IsLegacyBackup(backupName))
			{
				continue;
			}

			std::string groupName;

			if (backupName == "old")
			{
				groupName = FindBackupLanguage(backup);
			}
			else
			{
				groupName = backupName.substr(0, backupName.size() - std::strlen("_old"));
			}

			const auto groupDir = zoneRoot / groupName;
			const auto convertedDir = convertedRoot / groupName;

			for (const auto& file : std::filesystem::directory_iterator(backup))
			{
				const std::string fileName = file.path().filename().generic_string();

				if (fileName == legacyMarker)
				{
					plan.push_back({ ConvertAction::Drop, file.path(), {} });
					continue;
				}

				if (!file.is_regular_file())
				{
					RefuseRecovery(backup, groupDir, "There is something in the backup directory that is not a fastfile.");
				}

				auto stock = groupDir / fileName;
				auto converted = convertedDir / fileName;

				if (std::filesystem::exists(converted))
				{
					if (std::filesystem::exists(stock))
					{
						RefuseRecovery(backup, groupDir, "There is a copy of this fastfile in both places, and "
							"nothing to say which of the two the game should read.");
					}

					plan.push_back({ ConvertAction::Move, file.path(), std::move(stock) });
					continue;
				}

				if (!std::filesystem::exists(stock))
				{
					plan.push_back({ ConvertAction::Move, file.path(), std::move(stock) });
					continue;
				}

				const auto arch = TryProbeFastFile(stock);

				if (arch != FastFileArch::X86)
				{
					if (arch)
					{
						RefuseRecovery(backup, groupDir, "The fastfile in the group directory is itself in the x64 "
							"layout, so it is not the converted copy of the one in the "
							"backup directory.");
					}

					RefuseRecovery(backup, groupDir, "The fastfile in the group directory cannot be read, so "
						"there is no telling whether it is the converted copy of "
						"the one in the backup directory.");
				}

				plan.push_back({ ConvertAction::Move, stock, std::move(converted) });
				plan.push_back({ ConvertAction::Move, file.path(), std::move(stock) });
			}

			plan.push_back({ ConvertAction::Prune, backup, {} });
		}
	}

	static void PlanAdoption(ConvertPlan& plan)
	{
		for (const ZoneGroup& group : ownGroups)
		{
			auto groupDir = zoneRoot / group.name;

			if (!std::filesystem::is_directory(groupDir))
			{
				continue;
			}

			for (const auto& entry : std::filesystem::directory_iterator(groupDir))
			{
				if (!entry.is_regular_file())
				{
					continue;
				}

				const std::string fileName = entry.path().filename().generic_string();

				if (group.isShared && group.isShared(fileName))
				{
					auto target = convertedRoot / group.name / fileName;

					if (!std::filesystem::exists(target) && TryProbeFastFile(entry.path()) == FastFileArch::X86)
					{
						plan.push_back({ ConvertAction::Copy, entry.path(), std::move(target) });
					}

					continue;
				}

				if (TryProbeFastFile(entry.path()) == FastFileArch::X64)
				{
					continue;
				}

				plan.push_back({ ConvertAction::Replace, entry.path(), convertedRoot / group.name / fileName });
			}

			plan.push_back({ ConvertAction::Prune, std::move(groupDir), {} });
		}
	}

	static void PlanConversion(ConvertPlan& plan)
	{
		if (!std::filesystem::is_directory(zoneRoot))
		{
			return;
		}

		for (const auto& entry : std::filesystem::directory_iterator(zoneRoot))
		{
			if (!entry.is_directory())
			{
				continue;
			}

			const std::string groupName = entry.path().filename().generic_string();

			if (groupName == convertedName || IsLegacyBackup(groupName))
			{
				continue;
			}

			for (const auto& file : std::filesystem::directory_iterator(entry.path()))
			{
				if (!file.is_regular_file())
				{
					continue;
				}

				auto target = convertedRoot / groupName / file.path().filename();

				if (std::filesystem::exists(target))
				{
					continue;
				}

				if (TryProbeFastFile(file.path()) != FastFileArch::X64)
				{
					continue;
				}

				plan.push_back({ ConvertAction::Convert, file.path(), std::move(target) });
			}
		}
	}

	static const ConvertPhase phases[] =
	{
		{ "moving ZW3's own files under \"main/zw3/x86\"", &PlanLegacyBasegame },
		{ "copying IW4x's x86 files for ZW3", &PlanIw4xX86 },
		{ "putting the stock layout back", &PlanRecovery },
		{ "taking over ZW3's fastfiles", &PlanAdoption },
		{ "converting fastfiles", &PlanConversion },
	};

	static COLORREF BlendColor(const COLORREF from, const COLORREF to, const float amount)
	{
		const auto channel = [from, to, amount](const int shift)
		{
			const float start = static_cast<float>((from >> shift) & 0xFF);
			const float end = static_cast<float>((to >> shift) & 0xFF);

			return static_cast<COLORREF>(std::lround(start + (end - start) * amount)) & 0xFF;
		};

		return channel(0) | (channel(8) << 8) | (channel(16) << 16);
	}

	static std::string DescribeRunning(const std::vector<std::string>& names)
	{
		if (names.empty())
		{
			return "Finishing up";
		}

		if (names.size() == 1)
		{
			return std::format("Converting {}", names.front());
		}

		return std::format("Converting {} (+{} more)", names.front(), names.size() - 1);
	}

	class ConvertProgress
	{
	public:
		ConvertProgress(const std::string& title, std::size_t fileCount, bool shouldShow);
		~ConvertProgress();

		ConvertProgress(const ConvertProgress&) = delete;
		ConvertProgress& operator=(const ConvertProgress&) = delete;

		void Running(const std::vector<std::string>& names);
		void Finished(std::size_t count);
		void Tick();

	private:
		static LRESULT CALLBACK Proc(HWND handle, UINT message, WPARAM wParam, LPARAM lParam);

		void Paint(HDC dc);
		void Pump() const;

		HWND window = nullptr;
		HFONT font = nullptr;

		std::vector<std::string> running;
		std::size_t done = 0;
		std::size_t total = 0;

		std::uint64_t lastFrameMs = 0;
		float sweep = 0.0f;
		float fill = 0.0f;
	};

	ConvertProgress::ConvertProgress(const std::string& title, const std::size_t fileCount, const bool shouldShow) : total(fileCount)
	{
		if (!shouldShow)
		{
			return;
		}

		static const bool isRegistered = []
		{
			WNDCLASSEXA windowClassInfo{};
			windowClassInfo.cbSize = sizeof(windowClassInfo);
			windowClassInfo.lpfnWndProc = &ConvertProgress::Proc;
			windowClassInfo.hInstance = GetModuleHandleA(nullptr);
			windowClassInfo.hCursor = LoadCursor(nullptr, IDC_ARROW);
			windowClassInfo.lpszClassName = progressWindowClass;

			return RegisterClassExA(&windowClassInfo) != 0;
		}();

		if (!isRegistered)
		{
			return;
		}

		const int x = (GetSystemMetrics(SM_CXSCREEN) - windowWidth) / 2;
		const int y = (GetSystemMetrics(SM_CYSCREEN) - windowHeight) / 2;

		this->window = CreateWindowExA(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST, progressWindowClass, title.data(),
			WS_POPUP | WS_CAPTION | WS_VISIBLE, x, y, windowWidth, windowHeight,
			nullptr, nullptr, GetModuleHandleA(nullptr), this);

		if (!this->window)
		{
			return;
		}

		NONCLIENTMETRICSA metrics{};
		metrics.cbSize = sizeof(metrics);

		if (SystemParametersInfoA(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0))
		{
			this->font = CreateFontIndirectA(&metrics.lfMessageFont);
		}

		this->lastFrameMs = GetTickCount64();

		SetForegroundWindow(this->window);
		this->Tick();
	}

	ConvertProgress::~ConvertProgress()
	{
		if (this->window)
		{
			DestroyWindow(this->window);
			this->window = nullptr;

			this->Pump();
		}

		if (this->font)
		{
			DeleteObject(this->font);
			this->font = nullptr;
		}
	}

	void ConvertProgress::Running(const std::vector<std::string>& names)
	{
		this->running = names;
		this->Tick();
	}

	void ConvertProgress::Finished(const std::size_t count)
	{
		this->done = count;
		this->Tick();
	}

	void ConvertProgress::Tick()
	{
		if (!this->window)
		{
			return;
		}

		const std::uint64_t nowMs = GetTickCount64();
		const float elapsedSeconds = static_cast<float>(nowMs - this->lastFrameMs) / 1000.0f;

		this->lastFrameMs = nowMs;
		this->sweep = std::fmod(this->sweep + elapsedSeconds / sweepPeriod, 1.0f);

		float target = 0.0f;

		if (this->total != 0)
		{
			target = static_cast<float>(this->done) / static_cast<float>(this->total);
		}

		this->fill += (target - this->fill) * std::min(1.0f, elapsedSeconds * fillSpeed);

		InvalidateRect(this->window, nullptr, FALSE);
		UpdateWindow(this->window);

		this->Pump();
	}

	LRESULT CALLBACK ConvertProgress::Proc(HWND handle, UINT message, WPARAM wParam, LPARAM lParam)
	{
		if (message == WM_NCCREATE)
		{
			const auto* const create = reinterpret_cast<CREATESTRUCTA*>(lParam);
			SetWindowLongPtrA(handle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
		}

		auto* const progress = reinterpret_cast<ConvertProgress*>(GetWindowLongPtrA(handle, GWLP_USERDATA));

		if (message == WM_PAINT && progress)
		{
			PAINTSTRUCT paint{};
			progress->Paint(BeginPaint(handle, &paint));
			EndPaint(handle, &paint);

			return 0;
		}

		if (message == WM_ERASEBKGND)
		{
			return 1;
		}

		if (message == WM_CLOSE)
		{
			return 0;
		}

		return DefWindowProcA(handle, message, wParam, lParam);
	}

	void ConvertProgress::Paint(HDC dc)
	{
		RECT client{};
		GetClientRect(this->window, &client);

		const LONG width = client.right - client.left;
		const LONG height = client.bottom - client.top;

		HDC const buffer = CreateCompatibleDC(dc);
		HBITMAP const bitmap = CreateCompatibleBitmap(dc, width, height);
		HGDIOBJ const oldBitmap = SelectObject(buffer, bitmap);

		const COLORREF back = GetSysColor(COLOR_BTNFACE);
		const COLORREF accent = GetSysColor(COLOR_HIGHLIGHT);
		const COLORREF track = BlendColor(back, GetSysColor(COLOR_BTNSHADOW), 0.45f);

		const auto fillRect = [buffer](const RECT& rect, const COLORREF color)
		{
			HBRUSH const brush = CreateSolidBrush(color);
			FillRect(buffer, &rect, brush);
			DeleteObject(brush);
		};

		fillRect(client, back);

		SetBkMode(buffer, TRANSPARENT);
		SetTextColor(buffer, GetSysColor(COLOR_BTNTEXT));

		HGDIOBJ oldFont = nullptr;

		if (this->font)
		{
			oldFont = SelectObject(buffer, this->font);
		}

		const auto writeLine = [buffer, width](const std::string& text, const LONG top, const UINT format)
		{
			RECT rect{ windowMargin, top, width - windowMargin, top + 20 };
			DrawTextA(buffer, text.data(), -1, &rect, format | DT_SINGLELINE | DT_NOPREFIX);
		};

		writeLine(DescribeRunning(this->running), windowMargin, DT_LEFT | DT_PATH_ELLIPSIS);

		const RECT bar{ windowMargin, windowMargin + 34, width - windowMargin, windowMargin + 34 + barHeight };

		writeLine(std::format("{} of {} fastfiles", this->done, this->total), bar.bottom + 14, DT_LEFT);

		fillRect(bar, track);

		const float span = static_cast<float>(bar.right - bar.left);
		const float filled = span * std::clamp(this->fill, 0.0f, 1.0f);
		const float sweepWidth = span * sweepExtent;
		const float sweepAt = -sweepWidth + this->sweep * (span + 2.0f * sweepWidth);

		for (float column = 0.0f; column < filled; ++column)
		{
			const float distance = std::abs(column - sweepAt) / sweepWidth;
			float shine = 0.0f;

			if (distance < 1.0f)
			{
				shine = (1.0f - distance) * (1.0f - distance) * sweepStrength;
			}

			const LONG left = bar.left + static_cast<LONG>(column);
			const RECT slice{ left, bar.top, left + 1, bar.bottom };

			fillRect(slice, BlendColor(accent, RGB(255, 255, 255), shine));
		}

		if (oldFont)
		{
			SelectObject(buffer, oldFont);
		}

		BitBlt(dc, 0, 0, width, height, buffer, 0, 0, SRCCOPY);

		SelectObject(buffer, oldBitmap);
		DeleteObject(bitmap);
		DeleteDC(buffer);
	}

	void ConvertProgress::Pump() const
	{
		MSG message;

		while (PeekMessageA(&message, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&message);
			DispatchMessageA(&message);
		}
	}

	static void MakeDirectories(const std::filesystem::path& directory)
	{
		std::error_code error;
		std::filesystem::create_directories(directory, error);

		if (error)
		{
			FailConversion(std::format("\"{}\" could not be created: {}.\n\n"
				"The game has nowhere to put the files it reads, so it cannot start.",
				directory.generic_string(), error.message()));
		}
	}

	static void MoveStep(const ConvertStep& step)
	{
		MakeDirectories(step.to.parent_path());

		std::error_code error;
		std::filesystem::rename(step.from, step.to, error);

		if (error)
		{
			FailConversion(std::format("\"{}\" could not be moved to \"{}\": {}.\n\n"
				"This game reads that file from where it was being moved to, so it "
				"cannot start until the move goes through.",
				step.from.generic_string(), step.to.generic_string(), error.message()));
		}
	}

	static void CopyStep(const ConvertStep& step)
	{
		MakeDirectories(step.to.parent_path());

		std::error_code error;
		std::filesystem::copy_file(step.from, step.to, std::filesystem::copy_options::overwrite_existing, error);

		if (error)
		{
			FailConversion(std::format("\"{}\" could not be copied to \"{}\": {}.\n\n"
				"This game reads that file from where it was being copied to, so it "
				"cannot start until the copy goes through.",
				step.from.generic_string(), step.to.generic_string(), error.message()));
		}
	}

	static void ReplaceStep(const ConvertStep& step)
	{
		std::error_code error;
		std::filesystem::remove(step.to, error);

		MoveStep(step);
	}

	static void DropStep(const ConvertStep& step)
	{
		std::error_code error;
		std::filesystem::remove(step.from, error);

		if (error)
		{
			LogConversion(std::format("\"{}\" could not be removed: {}", step.from.generic_string(), error.message()));
		}
	}

	static void PruneStep(const ConvertStep& step)
	{
		if (!std::filesystem::is_directory(step.from))
		{
			return;
		}

		std::vector<std::filesystem::path> directories;

		for (const auto& entry : std::filesystem::recursive_directory_iterator(step.from))
		{
			if (entry.is_directory())
			{
				directories.push_back(entry.path());
			}
		}

		std::error_code error;

		for (const auto& directory : directories | std::views::reverse)
		{
			std::filesystem::remove(directory, error);
		}

		std::filesystem::remove(step.from, error);

		if (std::filesystem::exists(step.from))
		{
			LogConversion(std::format("\"{}\" still holds something and was left alone", step.from.generic_string()));
		}
	}

	static const ConvertActionInfo actions[] =
	{
		{ "moving", &MoveStep },
		{ "copying", &CopyStep },
		{ "replacing", &ReplaceStep },
		{ "dropping", &DropStep },
		{ "converting", nullptr },
		{ "pruning", &PruneStep },
	};

	static const ConvertActionInfo& GetActionInfo(const ConvertAction action)
	{
		return actions[static_cast<std::size_t>(action)];
	}

	static HANDLE StartConverter(const ConvertStep& step)
	{
		const std::string converter = converterName;
		const std::string outputDirectory = step.to.parent_path().generic_string();
		const std::string input = step.from.generic_string();

		SECURITY_ATTRIBUTES security{};
		security.nLength = sizeof(security);
		security.bInheritHandle = TRUE;

		HANDLE const logHandle = CreateFileA(logFile, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
			&security, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

		const bool hasLog = logHandle != INVALID_HANDLE_VALUE;

		STARTUPINFOA startupInfo{};
		startupInfo.cb = sizeof(startupInfo);
		startupInfo.dwFlags = STARTF_USESHOWWINDOW;
		startupInfo.wShowWindow = SW_HIDE;

		if (hasLog)
		{
			startupInfo.dwFlags |= STARTF_USESTDHANDLES;
			startupInfo.hStdOutput = logHandle;
			startupInfo.hStdError = logHandle;
		}

		std::string commandLine = std::format(R"("{}" --game IW4MS --convert-to IW4 -o "{}" "{}")", converter, outputDirectory, input);

		PROCESS_INFORMATION processInfo{};

		const bool didStart = CreateProcessA(converter.data(), commandLine.data(), nullptr, nullptr,
			hasLog, CREATE_NO_WINDOW, nullptr, nullptr, &startupInfo, &processInfo) != FALSE;

		std::string startError;

		if (!didStart)
		{
			startError = GetLastErrorText();
		}

		if (hasLog)
		{
			CloseHandle(logHandle);
		}

		if (!didStart)
		{
			FailConversion(std::format("\"{}\" could not be started: {}\n\n"
				"Antivirus software such as Windows Defender often blocks or "
				"quarantines it. Add an exclusion for the ZW3 folder and start the "
				"game again.",
				converter, startError));
		}

		CloseHandle(processInfo.hThread);

		return processInfo.hProcess;
	}

	static std::string FindConversionFault(const ConvertStep& step, const DWORD exitCode)
	{
		if (exitCode != 0)
		{
			return std::format("{} exited with code {}", converterName, exitCode);
		}

		if (!std::filesystem::exists(step.to))
		{
			return std::format("{} wrote nothing to \"{}\"", converterName, step.to.generic_string());
		}

		if (TryProbeFastFile(step.to) != FastFileArch::X86)
		{
			return std::format("\"{}\" did not come out in the x86 layout", step.to.generic_string());
		}

		return {};
	}

	static std::vector<std::string> RunConverters(const std::vector<const ConvertStep*>& steps, ConvertProgress& progress, const std::size_t alreadyDone)
	{
		if (!std::filesystem::exists(converterName))
		{
			FailConversion(std::format("This installation ships fastfiles in the x64 layout, which have to "
				"be converted before the game can read them, and {} was not found "
				"next to the game.\n\n"
				"If you did install it, antivirus software such as Windows Defender "
				"may have quarantined it. Add an exclusion for the ZW3 folder and "
				"start the game again.",
				converterName));
		}

		SYSTEM_INFO systemInfo{};
		GetNativeSystemInfo(&systemInfo);

		const std::size_t processorCount = std::clamp<std::size_t>(systemInfo.dwNumberOfProcessors, 1, MAXIMUM_WAIT_OBJECTS);
		const std::size_t slotCount = std::min(processorCount, steps.size());

		LogConversion(std::format("converting {} fastfiles, {} at a time", steps.size(), slotCount));

		std::vector<RunningConverter> runningConverters;
		std::vector<std::string> failed;

		std::size_t next = 0;
		std::size_t finishedCount = alreadyDone;

		while (next < steps.size() || !runningConverters.empty())
		{
			const std::size_t runningBefore = runningConverters.size();

			while (runningConverters.size() < slotCount && next < steps.size())
			{
				runningConverters.push_back({ StartConverter(*steps[next]), next });
				++next;
			}

			if (runningConverters.size() != runningBefore)
			{
				std::vector<std::string> names;

				for (const RunningConverter& converter : runningConverters)
				{
					names.push_back(steps[converter.step]->from.filename().generic_string());
				}

				progress.Running(names);
			}

			std::vector<HANDLE> handles;

			for (const RunningConverter& converter : runningConverters)
			{
				handles.push_back(converter.process);
			}

			const DWORD waitResult = WaitForMultipleObjects(static_cast<DWORD>(handles.size()), handles.data(), FALSE, frameIntervalMs);

			if (waitResult == WAIT_TIMEOUT)
			{
				progress.Tick();
				continue;
			}

			if (waitResult == WAIT_FAILED)
			{
				FailConversion(std::format("Waiting for {} failed: {}\n\n"
					"The game cannot tell whether the fastfiles were converted. Start "
					"it again.",
					converterName, GetLastErrorText()));
			}

			const std::size_t index = waitResult - WAIT_OBJECT_0;
			const ConvertStep& step = *steps[runningConverters[index].step];

			DWORD exitCode = 1;
			GetExitCodeProcess(runningConverters[index].process, &exitCode);
			CloseHandle(runningConverters[index].process);
			runningConverters.erase(runningConverters.begin() + static_cast<std::ptrdiff_t>(index));

			const std::string fault = FindConversionFault(step, exitCode);

			if (!fault.empty())
			{
				std::error_code error;
				std::filesystem::remove(step.to, error);

				LogConversion(std::format("could not convert \"{}\": {}", step.from.generic_string(), fault));
				failed.push_back(step.from.filename().generic_string());
			}

			++finishedCount;
			progress.Finished(finishedCount);
		}

		return failed;
	}

	static std::vector<std::string> ExecutePlan(const ConvertPlan& plan, ConvertProgress& progress)
	{
		std::vector<const ConvertStep*> conversions;
		std::size_t copied = 0;

		for (const ConvertStep& step : plan)
		{
			const ConvertActionInfo& info = GetActionInfo(step.action);

			if (!info.apply)
			{
				conversions.push_back(&step);
				continue;
			}

			if (step.action == ConvertAction::Copy)
			{
				progress.Running({ step.from.filename().generic_string() });
			}

			if (step.to.empty())
			{
				LogConversion(std::format("{} \"{}\"", info.doing, step.from.generic_string()));
			}
			else
			{
				LogConversion(std::format("{} \"{}\" to \"{}\"", info.doing, step.from.generic_string(), step.to.generic_string()));
			}

			info.apply(step);

			if (step.action == ConvertAction::Copy)
			{
				++copied;
				progress.Finished(copied);
			}
		}

		if (conversions.empty())
		{
			return {};
		}

		return RunConverters(conversions, progress, copied);
	}

	ZoneConvert::ZoneConvert()
	{
		isEnabled = true;

		for (const ConvertPhase& phase : phases)
		{
			ConvertPlan plan;
			phase.makePlan(plan);

			if (plan.empty())
			{
				continue;
			}

			const auto conversionCount = static_cast<std::size_t>(std::ranges::count_if(plan, [](const ConvertStep& step)
			{
				return !GetActionInfo(step.action).apply;
			}));

			const auto copyCount = static_cast<std::size_t>(std::ranges::count_if(plan, [](const ConvertStep& step)
			{
				return step.action == ConvertAction::Copy;
			}));

			const std::size_t visibleWork = conversionCount + copyCount;

			LogConversion(std::format("{}: {} steps, {} of them conversions", phase.name, plan.size(), conversionCount));

			std::vector<std::string> failed;

			{
				ConvertProgress progress(progressTitle, visibleWork, visibleWork != 0);
				failed = ExecutePlan(plan, progress);
			}

			if (failed.empty())
			{
				continue;
			}

			std::string names;

			for (const std::string& name : failed)
			{
				names.append(name);
				names.push_back('\n');
			}

			FailConversion(std::format("{} of {} fastfiles could not be converted:\n\n{}\n"
				"See \"{}\" for what {} said about them.",
				failed.size(), conversionCount, names, logFile, converterName));
		}
	}

	bool ZoneConvert::IsEnabled()
	{
		return isEnabled;
	}

	std::string ZoneConvert::SearchPath(const std::string_view group)
	{
		std::string path = std::format("{}/{}/", convertedRoot.generic_string(), group);
		std::ranges::replace(path, '/', '\\');

		return path;
	}
}
