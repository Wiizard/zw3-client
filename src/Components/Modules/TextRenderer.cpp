#include "TextRenderer.hpp"
#include "Events.hpp"
#include "Materials.hpp"
#include "Renderer.hpp"

#pragma warning(push)
#pragma warning(disable: 4005)
#include <dwrite.h>
#pragma warning(pop)

#pragma comment(lib, "dwrite.lib")

namespace Game
{
	float* con_screenMin = reinterpret_cast<float*>(0xA15F48);
}

namespace Components
{
	namespace
	{
		constexpr char UNICODE_GLYPH_ESCAPE = '\x03';
		constexpr std::size_t UNICODE_GLYPH_HEX_LENGTH = 6;
		constexpr char UNICODE_RUN_ESCAPE = '\x04';
		constexpr std::size_t UNICODE_RUN_ID_HEX_LENGTH = 8;
		constexpr float UNICODE_RUN_RASTER_HEIGHT = 64.0f;
		constexpr unsigned int UNICODE_RUN_BITMAP_PADDING = 64;
		constexpr unsigned int UNICODE_RUN_TEXTURE_PADDING = 2;
		constexpr unsigned int MAX_UNICODE_RUN_BITMAP_DIMENSION = 4096;
		constexpr float UNICODE_GLYPH_BASELINE_OFFSET = -2.0f;
		constexpr std::size_t MAX_RUNTIME_UNICODE_GLYPHS = 512;
		constexpr std::size_t MAX_RUNTIME_UNICODE_RUNS = 512;
		constexpr std::size_t MAX_UNICODE_GLYPHS_PER_FRAME = 8;
		constexpr std::size_t MAX_UNICODE_RUNS_PER_FRAME = 4;

		struct RuntimeUnicodeRun
		{
			Game::Material* material{};
			float bearingX{};
			float bearingY{};
			float width{};
			float height{};
			float advance{};
		};

		struct UnicodeRunDefinition
		{
			std::wstring text;
			std::size_t characterCount{};
		};

		bool RasterizeUnicodeRun(const std::string& materialName, const std::wstring& text,
			RuntimeUnicodeRun& run);

		std::mutex UnicodeRunMutex;
		std::unordered_map<std::wstring, std::uint32_t> UnicodeRunIds;
		std::unordered_map<std::uint32_t, UnicodeRunDefinition> UnicodeRunDefinitions;
		std::unordered_map<std::uint32_t, RuntimeUnicodeRun> UnicodeGlyphCache;
		std::unordered_set<std::uint32_t> PendingUnicodeGlyphs;
		std::unordered_map<std::uint32_t, RuntimeUnicodeRun> UnicodeRunCache;
		std::unordered_set<std::uint32_t> PendingUnicodeRuns;
		std::uint32_t NextUnicodeRunId = 1;

		int HexDigitValue(const char character)
		{
			if (character >= '0' && character <= '9') return character - '0';
			if (character >= 'A' && character <= 'F') return character - 'A' + 10;
			if (character >= 'a' && character <= 'f') return character - 'a' + 10;
			return -1;
		}

		bool ParseUnicodeGlyphEscape(const char*& text, std::uint32_t& codepoint)
		{
			if (!text || *text != UNICODE_GLYPH_ESCAPE) return false;

			std::uint32_t value{};
			for (std::size_t index = 0; index < UNICODE_GLYPH_HEX_LENGTH; ++index)
			{
				const auto character = text[index + 1];
				if (character == '\0') return false;
				const auto digit = HexDigitValue(character);
				if (digit < 0) return false;
				value = (value << 4) | static_cast<std::uint32_t>(digit);
			}

			if (value > 0x10FFFF || (value >= 0xD800 && value <= 0xDFFF)) return false;
			text += UNICODE_GLYPH_HEX_LENGTH + 1;
			codepoint = value;
			return true;
		}

		bool ParseUnicodeRunEscape(const char*& text, std::uint32_t& runId)
		{
			if (!text || *text != UNICODE_RUN_ESCAPE) return false;

			std::uint32_t value{};
			for (std::size_t index = 0; index < UNICODE_RUN_ID_HEX_LENGTH; ++index)
			{
				const auto character = text[index + 1];
				if (character == '\0') return false;
				const auto digit = HexDigitValue(character);
				if (digit < 0) return false;
				value = (value << 4) | static_cast<std::uint32_t>(digit);
			}

			if (value == 0) return false;
			text += UNICODE_RUN_ID_HEX_LENGTH + 1;
			runId = value;
			return true;
		}

		std::uint32_t RegisterUnicodeRun(const std::wstring& text, const std::size_t characterCount)
		{
			std::lock_guard lock(UnicodeRunMutex);
			if (const auto entry = UnicodeRunIds.find(text); entry != UnicodeRunIds.end())
			{
				return entry->second;
			}

			auto runId = NextUnicodeRunId++;
			while (runId == 0 || UnicodeRunDefinitions.contains(runId))
			{
				runId = NextUnicodeRunId++;
			}

			UnicodeRunIds.emplace(text, runId);
			UnicodeRunDefinitions.emplace(runId, UnicodeRunDefinition{text, characterCount});
			return runId;
		}

		std::size_t GetUnicodeRunCharacterCount(const std::uint32_t runId)
		{
			std::lock_guard lock(UnicodeRunMutex);
			if (const auto entry = UnicodeRunDefinitions.find(runId); entry != UnicodeRunDefinitions.end())
			{
				return entry->second.characterCount;
			}
			return 1;
		}

		std::optional<RuntimeUnicodeRun> GetUnicodeGlyph(const std::uint32_t codepoint)
		{
			std::lock_guard lock(UnicodeRunMutex);
			if (const auto entry = UnicodeGlyphCache.find(codepoint); entry != UnicodeGlyphCache.end())
			{
				if (entry->second.material) return entry->second;
				return std::nullopt;
			}

			if (UnicodeGlyphCache.size() + PendingUnicodeGlyphs.size() < MAX_RUNTIME_UNICODE_GLYPHS)
			{
				PendingUnicodeGlyphs.insert(codepoint);
			}
			return std::nullopt;
		}

		std::optional<RuntimeUnicodeRun> GetUnicodeRun(const std::uint32_t runId)
		{
			std::lock_guard lock(UnicodeRunMutex);
			if (const auto entry = UnicodeRunCache.find(runId); entry != UnicodeRunCache.end())
			{
				if (entry->second.material) return entry->second;
				return std::nullopt;
			}

			if (UnicodeRunDefinitions.contains(runId)
				&& UnicodeRunCache.size() + PendingUnicodeRuns.size() < MAX_RUNTIME_UNICODE_RUNS)
			{
				PendingUnicodeRuns.insert(runId);
			}
			return std::nullopt;
		}

		bool RasterizeUnicodeGlyph(const std::uint32_t codepoint, RuntimeUnicodeRun& glyph)
		{
			if (codepoint > 0xFFFF) return false;

			const auto deviceContext = CreateCompatibleDC(nullptr);
			if (!deviceContext) return false;
			const auto deleteDeviceContext = gsl::finally([deviceContext] { DeleteDC(deviceContext); });

			static constexpr std::array fontNames
			{
				L"Segoe UI Symbol",
				L"Segoe UI",
				L"Arial",
				L"Tahoma",
			};

			GLYPHMETRICS metrics{};
			std::vector<unsigned char> bitmap;
			bool found = false;
			for (const auto* fontName : fontNames)
			{
				const auto font = CreateFontW(-static_cast<int>(UNICODE_RUN_RASTER_HEIGHT), 0, 0, 0, FW_NORMAL,
					FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
					ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, fontName);
				if (!font) continue;

				const auto previousFont = SelectObject(deviceContext, font);
				const auto restoreFont = gsl::finally([deviceContext, previousFont, font]
				{
					SelectObject(deviceContext, previousFont);
					DeleteObject(font);
				});

				const auto character = static_cast<wchar_t>(codepoint);
				WORD glyphIndex{};
				if (GetGlyphIndicesW(deviceContext, &character, 1, &glyphIndex,
					GGI_MARK_NONEXISTING_GLYPHS) == GDI_ERROR || glyphIndex == 0xFFFF)
				{
					continue;
				}

				MAT2 transform{};
				transform.eM11.value = 1;
				transform.eM22.value = 1;
				const auto bitmapSize = GetGlyphOutlineW(deviceContext, glyphIndex,
					GGO_GRAY8_BITMAP | GGO_GLYPH_INDEX, &metrics, 0, nullptr, &transform);
				if (bitmapSize == GDI_ERROR || metrics.gmBlackBoxX == 0 || metrics.gmBlackBoxY == 0) continue;

				bitmap.resize(bitmapSize);
				if (GetGlyphOutlineW(deviceContext, glyphIndex, GGO_GRAY8_BITMAP | GGO_GLYPH_INDEX,
					&metrics, bitmapSize, bitmap.data(), &transform) == GDI_ERROR)
				{
					bitmap.clear();
					continue;
				}

				found = true;
				break;
			}

			if (!found)
			{
				return RasterizeUnicodeRun(std::format("runtime_unicode_glyph_fallback_{:06X}", codepoint),
					std::wstring(1, static_cast<wchar_t>(codepoint)), glyph);
			}

			const auto width = static_cast<unsigned int>(metrics.gmBlackBoxX);
			const auto height = static_cast<unsigned int>(metrics.gmBlackBoxY);
			if (width > 256 || height > 256) return false;
			const auto name = std::format("runtime_unicode_glyph_{:06X}", codepoint);
			auto* image = Materials::CreateImage(name, width, height, 1, 0x1000003, D3DFMT_A8R8G8B8);
			if (!image || !image->texture.map) return false;

			D3DLOCKED_RECT lockedRect{};
			if (FAILED(image->texture.map->LockRect(0, &lockedRect, nullptr, 0))) return false;
			const auto unlockTexture = gsl::finally([image] { image->texture.map->UnlockRect(0); });
			const auto sourcePitch = (width + 3u) & ~3u;
			for (auto y = 0u; y < height; ++y)
			{
				const auto* source = bitmap.data() + static_cast<std::size_t>(y) * sourcePitch;
				auto* destination = static_cast<unsigned char*>(lockedRect.pBits)
					+ static_cast<std::size_t>(y) * lockedRect.Pitch;
				for (auto x = 0u; x < width; ++x)
				{
					const auto alpha = static_cast<unsigned char>(std::min(255u,
						static_cast<unsigned int>(source[x]) * 255u / 64u));
					destination[x * 4 + 0] = 255;
					destination[x * 4 + 1] = 255;
					destination[x * 4 + 2] = 255;
					destination[x * 4 + 3] = alpha;
				}
			}

			glyph.material = Materials::Create(name, image);
			glyph.bearingX = static_cast<float>(metrics.gmptGlyphOrigin.x) / UNICODE_RUN_RASTER_HEIGHT;
			glyph.bearingY = -static_cast<float>(metrics.gmptGlyphOrigin.y) / UNICODE_RUN_RASTER_HEIGHT;
			glyph.width = static_cast<float>(width) / UNICODE_RUN_RASTER_HEIGHT;
			glyph.height = static_cast<float>(height) / UNICODE_RUN_RASTER_HEIGHT;
			glyph.advance = static_cast<float>(std::max<LONG>(1, metrics.gmCellIncX)) / UNICODE_RUN_RASTER_HEIGHT;
			return glyph.material != nullptr;
		}

		class UnicodeRunTextRenderer final : public IDWriteTextRenderer
		{
		public:
			UnicodeRunTextRenderer(IDWriteBitmapRenderTarget* target, IDWriteRenderingParams* renderingParams)
				: target_(target), renderingParams_(renderingParams)
			{
			}

			HRESULT STDMETHODCALLTYPE QueryInterface(const IID& iid, void** object) override
			{
				if (!object) return E_POINTER;
				if (iid == __uuidof(IUnknown) || iid == __uuidof(IDWritePixelSnapping)
					|| iid == __uuidof(IDWriteTextRenderer))
				{
					*object = this;
					AddRef();
					return S_OK;
				}
				*object = nullptr;
				return E_NOINTERFACE;
			}

			ULONG STDMETHODCALLTYPE AddRef() override
			{
				return 1;
			}

			ULONG STDMETHODCALLTYPE Release() override
			{
				return 1;
			}

			HRESULT STDMETHODCALLTYPE IsPixelSnappingDisabled(void*, BOOL* disabled) override
			{
				if (!disabled) return E_POINTER;
				*disabled = FALSE;
				return S_OK;
			}

			HRESULT STDMETHODCALLTYPE GetCurrentTransform(void*, DWRITE_MATRIX* transform) override
			{
				if (!transform) return E_POINTER;
				*transform = DWRITE_MATRIX{1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f};
				return S_OK;
			}

			HRESULT STDMETHODCALLTYPE GetPixelsPerDip(void*, FLOAT* pixelsPerDip) override
			{
				if (!pixelsPerDip) return E_POINTER;
				*pixelsPerDip = 1.0f;
				return S_OK;
			}

			HRESULT STDMETHODCALLTYPE DrawGlyphRun(void*, const FLOAT baselineOriginX,
				const FLOAT baselineOriginY, const DWRITE_MEASURING_MODE measuringMode,
				const DWRITE_GLYPH_RUN* glyphRun, const DWRITE_GLYPH_RUN_DESCRIPTION*, IUnknown*) override
			{
				return target_->DrawGlyphRun(baselineOriginX, baselineOriginY, measuringMode,
					glyphRun, renderingParams_, RGB(255, 255, 255));
			}

			HRESULT STDMETHODCALLTYPE DrawUnderline(void*, FLOAT, FLOAT, const DWRITE_UNDERLINE*, IUnknown*) override
			{
				return S_OK;
			}

			HRESULT STDMETHODCALLTYPE DrawStrikethrough(void*, FLOAT, FLOAT,
				const DWRITE_STRIKETHROUGH*, IUnknown*) override
			{
				return S_OK;
			}

			HRESULT STDMETHODCALLTYPE DrawInlineObject(void*, FLOAT, FLOAT, IDWriteInlineObject*,
				BOOL, BOOL, IUnknown*) override
			{
				return S_OK;
			}

		private:
			IDWriteBitmapRenderTarget* target_;
			IDWriteRenderingParams* renderingParams_;
		};

		template <typename T>
		void ReleaseComObject(T*& object)
		{
			if (object)
			{
				object->Release();
				object = nullptr;
			}
		}

		bool RasterizeUnicodeRun(const std::string& materialName, const std::wstring& text,
			RuntimeUnicodeRun& run)
		{
			if (text.empty() || text.size() > static_cast<std::size_t>(std::numeric_limits<UINT32>::max()))
			{
				return false;
			}

			IDWriteFactory* factory{};
			IDWriteTextFormat* textFormat{};
			IDWriteTextLayout* textLayout{};
			IDWriteGdiInterop* gdiInterop{};
			IDWriteBitmapRenderTarget* bitmapTarget{};
			IDWriteRenderingParams* renderingParams{};
			const auto releaseObjects = gsl::finally([&]
			{
				ReleaseComObject(renderingParams);
				ReleaseComObject(bitmapTarget);
				ReleaseComObject(gdiInterop);
				ReleaseComObject(textLayout);
				ReleaseComObject(textFormat);
				ReleaseComObject(factory);
			});

			if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
				reinterpret_cast<IUnknown**>(&factory))) || !factory)
			{
				return false;
			}

			if (FAILED(factory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
				DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, UNICODE_RUN_RASTER_HEIGHT,
				L"", &textFormat)) || !textFormat)
			{
				return false;
			}
			textFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
			textFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
			textFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);

			if (FAILED(factory->CreateTextLayout(text.data(), static_cast<UINT32>(text.size()), textFormat,
				static_cast<FLOAT>(MAX_UNICODE_RUN_BITMAP_DIMENSION - 2 * UNICODE_RUN_BITMAP_PADDING),
				static_cast<FLOAT>(MAX_UNICODE_RUN_BITMAP_DIMENSION - 2 * UNICODE_RUN_BITMAP_PADDING),
				&textLayout)) || !textLayout)
			{
				return false;
			}

			DWRITE_TEXT_METRICS textMetrics{};
			if (FAILED(textLayout->GetMetrics(&textMetrics))) return false;
			UINT32 lineCount{};
			textLayout->GetLineMetrics(nullptr, 0, &lineCount);
			if (lineCount == 0) return false;
			std::vector<DWRITE_LINE_METRICS> lineMetrics(lineCount);
			if (FAILED(textLayout->GetLineMetrics(lineMetrics.data(), lineCount, &lineCount))) return false;

			const auto bitmapWidth = static_cast<unsigned int>(std::ceil(textMetrics.widthIncludingTrailingWhitespace))
				+ 2 * UNICODE_RUN_BITMAP_PADDING;
			const auto bitmapHeight = static_cast<unsigned int>(std::ceil(textMetrics.height))
				+ 2 * UNICODE_RUN_BITMAP_PADDING;
			if (bitmapWidth == 0 || bitmapHeight == 0 || bitmapWidth > MAX_UNICODE_RUN_BITMAP_DIMENSION
				|| bitmapHeight > MAX_UNICODE_RUN_BITMAP_DIMENSION)
			{
				return false;
			}

			if (FAILED(factory->GetGdiInterop(&gdiInterop)) || !gdiInterop
				|| FAILED(gdiInterop->CreateBitmapRenderTarget(nullptr, bitmapWidth, bitmapHeight, &bitmapTarget))
				|| !bitmapTarget || FAILED(factory->CreateRenderingParams(&renderingParams)) || !renderingParams)
			{
				return false;
			}

			const auto memoryDc = bitmapTarget->GetMemoryDC();
			if (!memoryDc || !PatBlt(memoryDc, 0, 0, bitmapWidth, bitmapHeight, BLACKNESS)) return false;

			UnicodeRunTextRenderer renderer(bitmapTarget, renderingParams);
			if (FAILED(textLayout->Draw(nullptr, &renderer, static_cast<FLOAT>(UNICODE_RUN_BITMAP_PADDING),
				static_cast<FLOAT>(UNICODE_RUN_BITMAP_PADDING))))
			{
				return false;
			}

			const auto bitmap = static_cast<HBITMAP>(GetCurrentObject(memoryDc, OBJ_BITMAP));
			DIBSECTION bitmapInfo{};
			if (!bitmap || GetObjectW(bitmap, sizeof(bitmapInfo), &bitmapInfo) != sizeof(bitmapInfo)
				|| !bitmapInfo.dsBm.bmBits || bitmapInfo.dsBm.bmBitsPixel != 32)
			{
				return false;
			}

			const auto* pixels = static_cast<const unsigned char*>(bitmapInfo.dsBm.bmBits);
			auto minX = bitmapWidth;
			auto minY = bitmapHeight;
			auto maxX = 0u;
			auto maxY = 0u;
			for (auto y = 0u; y < bitmapHeight; ++y)
			{
				const auto* row = pixels + static_cast<std::size_t>(y) * bitmapInfo.dsBm.bmWidthBytes;
				for (auto x = 0u; x < bitmapWidth; ++x)
				{
					const auto* pixel = row + static_cast<std::size_t>(x) * 4;
					if (std::max({pixel[0], pixel[1], pixel[2]}) == 0) continue;
					minX = std::min(minX, x);
					minY = std::min(minY, y);
					maxX = std::max(maxX, x);
					maxY = std::max(maxY, y);
				}
			}

			if (minX > maxX || minY > maxY) return false;
			const auto inkWidth = maxX - minX + 1;
			const auto inkHeight = maxY - minY + 1;
			const auto width = inkWidth + 2 * UNICODE_RUN_TEXTURE_PADDING;
			const auto height = inkHeight + 2 * UNICODE_RUN_TEXTURE_PADDING;

			auto* image = Materials::CreateImage(materialName, width, height, 1, 0x1000003, D3DFMT_A8R8G8B8);
			if (!image || !image->texture.map) return false;

			D3DLOCKED_RECT lockedRect{};
			if (FAILED(image->texture.map->LockRect(0, &lockedRect, nullptr, 0))) return false;
			const auto unlockTexture = gsl::finally([image] { image->texture.map->UnlockRect(0); });
			for (auto y = 0u; y < height; ++y)
			{
				auto* destination = static_cast<unsigned char*>(lockedRect.pBits)
					+ static_cast<std::size_t>(y) * lockedRect.Pitch;
				std::memset(destination, 0, static_cast<std::size_t>(width) * 4);
			}
			for (auto y = 0u; y < inkHeight; ++y)
			{
				const auto* source = pixels + static_cast<std::size_t>(y + minY) * bitmapInfo.dsBm.bmWidthBytes
					+ static_cast<std::size_t>(minX) * 4;
				auto* destination = static_cast<unsigned char*>(lockedRect.pBits)
					+ static_cast<std::size_t>(y + UNICODE_RUN_TEXTURE_PADDING) * lockedRect.Pitch
					+ static_cast<std::size_t>(UNICODE_RUN_TEXTURE_PADDING) * 4;
				for (auto x = 0u; x < inkWidth; ++x)
				{
					const auto* sourcePixel = source + static_cast<std::size_t>(x) * 4;
					const auto alpha = std::max({sourcePixel[0], sourcePixel[1], sourcePixel[2]});
					destination[x * 4 + 0] = 255;
					destination[x * 4 + 1] = 255;
					destination[x * 4 + 2] = 255;
					destination[x * 4 + 3] = alpha;
				}
			}

			run.material = Materials::Create(materialName, image);
			run.bearingX = (static_cast<float>(minX) - static_cast<float>(UNICODE_RUN_BITMAP_PADDING))
				/ UNICODE_RUN_RASTER_HEIGHT
				- static_cast<float>(UNICODE_RUN_TEXTURE_PADDING) / UNICODE_RUN_RASTER_HEIGHT;
			run.bearingY = (static_cast<float>(minY) - static_cast<float>(UNICODE_RUN_BITMAP_PADDING)
				- lineMetrics[0].baseline - static_cast<float>(UNICODE_RUN_TEXTURE_PADDING))
				/ UNICODE_RUN_RASTER_HEIGHT;
			run.width = static_cast<float>(width) / UNICODE_RUN_RASTER_HEIGHT;
			run.height = static_cast<float>(height) / UNICODE_RUN_RASTER_HEIGHT;
			run.advance = std::max(1.0f, textMetrics.widthIncludingTrailingWhitespace)
				/ UNICODE_RUN_RASTER_HEIGHT;
			return run.material != nullptr;
		}

		void BuildPendingUnicodeGlyphs([[maybe_unused]] IDirect3DDevice9* device)
		{
			std::vector<std::uint32_t> pending;
			{
				std::lock_guard lock(UnicodeRunMutex);
				while (!PendingUnicodeGlyphs.empty() && pending.size() < MAX_UNICODE_GLYPHS_PER_FRAME)
				{
					const auto entry = PendingUnicodeGlyphs.begin();
					pending.push_back(*entry);
					PendingUnicodeGlyphs.erase(entry);
				}
				for (const auto codepoint : pending) UnicodeGlyphCache.try_emplace(codepoint);
			}

			for (const auto codepoint : pending)
			{
				RuntimeUnicodeRun glyph{};
				RasterizeUnicodeGlyph(codepoint, glyph);
				std::lock_guard lock(UnicodeRunMutex);
				UnicodeGlyphCache[codepoint] = glyph;
			}
		}

		void BuildPendingUnicodeRuns([[maybe_unused]] IDirect3DDevice9* device)
		{
			std::vector<std::pair<std::uint32_t, std::wstring>> pending;
			{
				std::lock_guard lock(UnicodeRunMutex);
				while (!PendingUnicodeRuns.empty() && pending.size() < MAX_UNICODE_RUNS_PER_FRAME)
				{
					const auto entry = PendingUnicodeRuns.begin();
					const auto definition = UnicodeRunDefinitions.find(*entry);
					if (definition != UnicodeRunDefinitions.end())
					{
						pending.emplace_back(*entry, definition->second.text);
						UnicodeRunCache.try_emplace(*entry);
					}
					PendingUnicodeRuns.erase(entry);
				}
			}

			for (const auto& [runId, text] : pending)
			{
				RuntimeUnicodeRun run{};
				RasterizeUnicodeRun(std::format("runtime_unicode_run_{:08X}", runId), text, run);
				std::lock_guard lock(UnicodeRunMutex);
				UnicodeRunCache[runId] = run;
			}
		}
	}

	unsigned TextRenderer::colorTableDefault[TEXT_COLOR_COUNT]
	{
		ColorRgb(0, 0, 0),          // TEXT_COLOR_BLACK
		ColorRgb(255, 92, 92),      // TEXT_COLOR_RED
		ColorRgb(0, 255, 0),        // TEXT_COLOR_GREEN
		ColorRgb(255, 255, 0),      // TEXT_COLOR_YELLOW
		ColorRgb(0, 0, 255),        // TEXT_COLOR_BLUE
		ColorRgb(0, 255, 255),      // TEXT_COLOR_LIGHT_BLUE
		ColorRgb(255, 92, 255),     // TEXT_COLOR_PINK
		ColorRgb(255, 255, 255),    // TEXT_COLOR_DEFAULT
		ColorRgb(255, 255, 255),    // TEXT_COLOR_AXIS
		ColorRgb(255, 255, 255),    // TEXT_COLOR_ALLIES
		ColorRgb(255, 255, 255),    // TEXT_COLOR_RAINBOW
		ColorRgb(255, 255, 255),    // TEXT_COLOR_SERVER

		ColorRgb(200, 75, 200),     // TEXT_COLOR_REAL_PINK
		ColorRgb(255, 240, 20),     // TEXT_COLOR_REAL_YELLOW
		ColorRgb(128, 0, 128),      // TEXT_COLOR_DARK_PURPLE
		ColorRgb(20, 180, 180),     // TEXT_COLOR_TEAL
		ColorRgb(255, 255, 255),    // TEXT_COLOR_INVALIDCHAR 16 = @, can't be typed ingame
		ColorRgb(60, 75, 35),       // TEXT_COLOR_OLIVE
		ColorRgb(93, 23, 255),      // TEXT_COLOR_BLURPLE
		ColorRgb(255, 0, 0),        // TEXT_COLOR_PURE_RED
		ColorRgb(0, 255, 0),        // TEXT_COLOR_PURE_GREEN
		ColorRgb(0, 0, 255),        // TEXT_COLOR_PURE_BLUE
		ColorRgb(128, 0, 0),        // TEXT_COLOR_MAROON
		ColorRgb(255, 105, 180),    // TEXT_COLOR_HOT_PINK
		ColorRgb(170, 240, 209),    // TEXT_COLOR_MINT
		ColorRgb(255, 213, 165),    // TEXT_COLOR_PEACH
		ColorRgb(187, 231, 151),    // TEXT_COLOR_PASTEL_GREEN
		ColorRgb(255, 120, 120),    // TEXT_COLOR_LIGHT_RED
	};

	unsigned TextRenderer::colorTableNew[TEXT_COLOR_COUNT]
	{
		ColorRgb(0, 0, 0),          // TEXT_COLOR_BLACK
		ColorRgb(255, 49, 49),      // TEXT_COLOR_RED
		ColorRgb(134, 192, 0),      // TEXT_COLOR_GREEN
		ColorRgb(255, 173, 34),     // TEXT_COLOR_YELLOW
		ColorRgb(0, 135, 193),      // TEXT_COLOR_BLUE
		ColorRgb(32, 197, 255),     // TEXT_COLOR_LIGHT_BLUE
		ColorRgb(151, 80, 221),     // TEXT_COLOR_PINK
		ColorRgb(255, 255, 255),    // TEXT_COLOR_DEFAULT
		ColorRgb(255, 255, 255),    // TEXT_COLOR_AXIS
		ColorRgb(255, 255, 255),    // TEXT_COLOR_ALLIES
		ColorRgb(255, 255, 255),    // TEXT_COLOR_RAINBOW
		ColorRgb(255, 255, 255),    // TEXT_COLOR_SERVER

		ColorRgb(200, 75, 200),     // TEXT_COLOR_REAL_PINK
		ColorRgb(255, 240, 20),     // TEXT_COLOR_REAL_YELLOW
		ColorRgb(128, 0, 128),      // TEXT_COLOR_DARK_PURPLE
		ColorRgb(20, 180, 180),     // TEXT_COLOR_TEAL
		ColorRgb(255, 255, 255),    // TEXT_COLOR_INVALIDCHAR 16 = @, can't be typed ingame
		ColorRgb(60, 75, 35),       // TEXT_COLOR_OLIVE
		ColorRgb(93, 23, 255),      // TEXT_COLOR_BLURPLE
		ColorRgb(255, 0, 0),        // TEXT_COLOR_PURE_RED
		ColorRgb(0, 255, 0),        // TEXT_COLOR_PURE_GREEN
		ColorRgb(0, 0, 255),        // TEXT_COLOR_PURE_BLUE
		ColorRgb(128, 0, 0),        // TEXT_COLOR_MAROON
		ColorRgb(255, 105, 180),    // TEXT_COLOR_HOT_PINK
		ColorRgb(170, 240, 209),    // TEXT_COLOR_MINT
		ColorRgb(255, 213, 165),    // TEXT_COLOR_PEACH
		ColorRgb(187, 231, 151),    // TEXT_COLOR_PASTEL_GREEN
		ColorRgb(255, 120, 120),    // TEXT_COLOR_LIGHT_RED
	};

	unsigned(*TextRenderer::currentColorTable)[TEXT_COLOR_COUNT];
	TextRenderer::FontIconAutocompleteContext TextRenderer::autocompleteContextArray[FONT_ICON_ACI_COUNT];
	std::map<std::string, TextRenderer::FontIconTableEntry> TextRenderer::fontIconLookup;
	std::vector<TextRenderer::FontIconTableEntry> TextRenderer::fontIconList;

	TextRenderer::BufferedLocalizedString TextRenderer::stringHintAutoComplete(REFERENCE_HINT_AUTO_COMPLETE, STRING_BUFFER_SIZE_SMALL);
	TextRenderer::BufferedLocalizedString TextRenderer::stringHintModifier(REFERENCE_HINT_MODIFIER, STRING_BUFFER_SIZE_SMALL);
	TextRenderer::BufferedLocalizedString TextRenderer::stringListHeader(REFERENCE_MODIFIER_LIST_HEADER, STRING_BUFFER_SIZE_SMALL);
	TextRenderer::BufferedLocalizedString TextRenderer::stringListFlipHorizontal(REFERENCE_MODIFIER_LIST_FLIP_HORIZONTAL, STRING_BUFFER_SIZE_SMALL);
	TextRenderer::BufferedLocalizedString TextRenderer::stringListFlipVertical(REFERENCE_MODIFIER_LIST_FLIP_VERTICAL, STRING_BUFFER_SIZE_SMALL);
	TextRenderer::BufferedLocalizedString TextRenderer::stringListBig(REFERENCE_MODIFIER_LIST_BIG, STRING_BUFFER_SIZE_SMALL);

	Dvar::Var TextRenderer::cg_newColors;
	Dvar::Var TextRenderer::cg_fontIconAutocomplete;
	Dvar::Var TextRenderer::cg_fontIconAutocompleteHint;
	Game::dvar_t* TextRenderer::sv_customTextColor;
	Dvar::Var TextRenderer::r_colorBlind;
	Game::dvar_t* TextRenderer::g_ColorBlind_MyTeam;
	Game::dvar_t* TextRenderer::g_ColorBlind_EnemyTeam;
	Game::dvar_t** TextRenderer::con_inputBoxColor = reinterpret_cast<Game::dvar_t**>(0x9FD4BC);

	TextRenderer::BufferedLocalizedString::BufferedLocalizedString(const char* reference, const std::size_t bufferSize)
		: stringReference(reference),
		stringBuffer(std::make_unique<char[]>(bufferSize)),
		stringBufferSize(bufferSize),
		stringWidth{-1}
	{

	}

	void TextRenderer::BufferedLocalizedString::Cache()
	{
		const auto* formattingString = Game::UI_SafeTranslateString(stringReference);

		if (formattingString != nullptr)
		{
			strncpy_s(stringBuffer.get(), stringBufferSize, formattingString, _TRUNCATE);
			for (auto& width : stringWidth)
			{
				width = -1;
			}
		}
	}

	const char* TextRenderer::BufferedLocalizedString::Format(const char* value)
	{
		const auto* formattingString = Game::UI_SafeTranslateString(stringReference);
		if (formattingString == nullptr)
		{
			stringBuffer[0] = '\0';
			return stringBuffer.get();
		}

		Game::ConversionArguments conversionArguments{};
		conversionArguments.args[conversionArguments.argCount++] = value;
		Game::UI_ReplaceConversions(formattingString, &conversionArguments, stringBuffer.get(), stringBufferSize);

		for (auto& width : stringWidth)
		{
			width = -1;
		}

		return stringBuffer.get();
	}

	const char* TextRenderer::BufferedLocalizedString::GetString() const
	{
		return stringBuffer.get();
	}

	int TextRenderer::BufferedLocalizedString::GetWidth(const FontIconAutocompleteInstance autocompleteInstance, Game::Font_s* font)
	{
		assert(autocompleteInstance < FONT_ICON_ACI_COUNT);
		if (stringWidth[autocompleteInstance] < 0)
		{
			stringWidth[autocompleteInstance] = Game::R_TextWidth(GetString(), std::numeric_limits<int>::max(), font);
		}

		return stringWidth[autocompleteInstance];
	}

	TextRenderer::FontIconAutocompleteContext::FontIconAutocompleteContext()
		: autocompleteActive(false),
		inModifiers(false),
		userClosed(false),
		lastHash(0),
		results{},
		resultCount(0),
		hasMoreResults(false),
		resultOffset(0),
		lastResultOffset(0),
		selectedOffset(0),
		maxFontIconWidth(0.0f),
		maxMaterialNameWidth(0.0f),
		stringSearchStartWith(REFERENCE_SEARCH_START_WITH, STRING_BUFFER_SIZE_BIG)
	{

	}

	unsigned TextRenderer::HsvToRgb(HsvColor hsv)
	{
		unsigned rgb;
		unsigned char region, p, q, t;
		unsigned int h, s, v, remainder;

		if (hsv.s == 0)
		{
			rgb = ColorRgb(hsv.v, hsv.v, hsv.v);
			return rgb;
		}

		// converting to 16 bit to prevent overflow
		h = hsv.h;
		s = hsv.s;
		v = hsv.v;

		region = static_cast<uint8_t>(h / 43);
		remainder = (h - (region * 43)) * 6;

		p = static_cast<uint8_t>((v * (255 - s)) >> 8);
		q = static_cast<uint8_t>((v * (255 - ((s * remainder) >> 8))) >> 8);
		t = static_cast<uint8_t>((v * (255 - ((s * (255 - remainder)) >> 8))) >> 8);

		switch (region)
		{
		case 0:
			rgb = ColorRgb(static_cast<uint8_t>(v), t, p);
			break;
		case 1:
			rgb = ColorRgb(q, static_cast<uint8_t>(v), p);
			break;
		case 2:
			rgb = ColorRgb(p, static_cast<uint8_t>(v), t);
			break;
		case 3:
			rgb = ColorRgb(p, q, static_cast<uint8_t>(v));
			break;
		case 4:
			rgb = ColorRgb(t, p, static_cast<uint8_t>(v));
			break;
		default:
			rgb = ColorRgb(static_cast<uint8_t>(v), p, q);
			break;
		}

		return rgb;
	}

	void TextRenderer::DrawAutocompleteBox(const FontIconAutocompleteContext& context, const float x, const float y, const float w, const float h, const float* color)
	{
		const float borderColor[4]
		{
			color[0] * 0.5f,
			color[1] * 0.5f,
			color[2] * 0.5f,
			color[3]
		};

		Game::R_AddCmdDrawStretchPic(x, y, w, h, 0.0, 0.0, 0.0, 0.0, color, Game::cls->whiteMaterial);
		Game::R_AddCmdDrawStretchPic(x, y, FONT_ICON_AUTOCOMPLETE_BOX_BORDER, h, 0.0, 0.0, 0.0, 0.0, borderColor, Game::cls->whiteMaterial);
		Game::R_AddCmdDrawStretchPic(x + w - FONT_ICON_AUTOCOMPLETE_BOX_BORDER, y, FONT_ICON_AUTOCOMPLETE_BOX_BORDER, h, 0.0, 0.0, 0.0, 0.0, borderColor, Game::cls->whiteMaterial);
		Game::R_AddCmdDrawStretchPic(x, y, w, FONT_ICON_AUTOCOMPLETE_BOX_BORDER, 0.0, 0.0, 0.0, 0.0, borderColor, Game::cls->whiteMaterial);
		Game::R_AddCmdDrawStretchPic(x, y + h - FONT_ICON_AUTOCOMPLETE_BOX_BORDER, w, FONT_ICON_AUTOCOMPLETE_BOX_BORDER, 0.0, 0.0, 0.0, 0.0, borderColor, Game::cls->whiteMaterial);

		if (context.resultOffset > 0)
		{
			Game::R_AddCmdDrawStretchPic(x + w - FONT_ICON_AUTOCOMPLETE_BOX_BORDER - FONT_ICON_AUTOCOMPLETE_ARROW_SIZE,
				y + FONT_ICON_AUTOCOMPLETE_BOX_BORDER,
				FONT_ICON_AUTOCOMPLETE_ARROW_SIZE,
				FONT_ICON_AUTOCOMPLETE_ARROW_SIZE,
				1.0f, 1.0f, 0.0f, 0.0f, WHITE_COLOR, Game::sharedUiInfo->assets.scrollBarArrowDown);
		}

		if (context.hasMoreResults)
		{
			Game::R_AddCmdDrawStretchPic(x + w - FONT_ICON_AUTOCOMPLETE_BOX_BORDER - FONT_ICON_AUTOCOMPLETE_ARROW_SIZE,
				y + h - FONT_ICON_AUTOCOMPLETE_BOX_BORDER - FONT_ICON_AUTOCOMPLETE_ARROW_SIZE,
				FONT_ICON_AUTOCOMPLETE_ARROW_SIZE,
				FONT_ICON_AUTOCOMPLETE_ARROW_SIZE,
				1.0f, 1.0f, 0.0f, 0.0f, WHITE_COLOR, Game::sharedUiInfo->assets.scrollBarArrowUp);
		}
	}

	void TextRenderer::UpdateAutocompleteContextResults(FontIconAutocompleteContext& context, Game::Font_s* font, const float textXScale)
	{
		context.resultCount = 0;
		context.hasMoreResults = false;
		context.lastResultOffset = context.resultOffset;

		auto skipCount = context.resultOffset;

		const auto queryLen = context.lastQuery.size();
		for (const auto& fontIconEntry : fontIconList)
		{
			const auto compareValue = fontIconEntry.iconName.compare(0, queryLen, context.lastQuery);

			if (compareValue == 0)
			{
				if (skipCount > 0)
				{
					skipCount--;
				}
				else if (context.resultCount < FontIconAutocompleteContext::MAX_RESULTS)
				{
					context.results[context.resultCount++] =
					{
						Utils::String::VA(":%s:", fontIconEntry.iconName.data()),
						fontIconEntry.iconName
					};
				}
				else
				{
					context.hasMoreResults = true;
				}
			}
			else if (compareValue > 0)
			{
				break;
			}
		}

		context.maxFontIconWidth = 0;
		context.maxMaterialNameWidth = 0;
		for (auto resultIndex = 0u; resultIndex < context.resultCount; resultIndex++)
		{
			const auto& result = context.results[resultIndex];
			const auto fontIconWidth = static_cast<float>(Game::R_TextWidth(result.fontIconName.c_str(), std::numeric_limits<int>::max(), font)) * textXScale;
			const auto materialNameWidth = static_cast<float>(Game::R_TextWidth(result.materialName.c_str(), std::numeric_limits<int>::max(), font)) * textXScale;

			if (fontIconWidth > context.maxFontIconWidth)
				context.maxFontIconWidth = fontIconWidth;
			if (materialNameWidth > context.maxMaterialNameWidth)
				context.maxMaterialNameWidth = materialNameWidth;
		}
	}

	void TextRenderer::UpdateAutocompleteContext(FontIconAutocompleteContext& context, const Game::field_t* edit, Game::Font_s* font, const float textXScale)
	{
		int fontIconStart = -1;
		auto inModifiers = false;

		for (auto i = 0; i < edit->cursor; i++)
		{
			const auto c = static_cast<unsigned char>(edit->buffer[i]);
			if (c == FONT_ICON_SEPARATOR_CHARACTER)
			{
				if (fontIconStart < 0)
				{
					fontIconStart = i + 1;
					inModifiers = false;
				}
				else
				{
					fontIconStart = -1;
					inModifiers = false;
				}
			}
			else if (std::isspace(c))
			{
				fontIconStart = -1;
				inModifiers = false;
			}
			else if (c == FONT_ICON_MODIFIER_SEPARATOR_CHARACTER)
			{
				if (fontIconStart >= 0 && !inModifiers)
				{
					inModifiers = true;
				}
				else
				{
					fontIconStart = -1;
					inModifiers = false;
				}
			}
		}

		if (fontIconStart < 0 // Not in fonticon sequence
			|| fontIconStart == edit->cursor // Did not type the first letter yet
			|| !std::isalpha(static_cast<unsigned char>(edit->buffer[fontIconStart])) // First letter of the icon is not alphabetic
			|| (fontIconStart > 1 && std::isalnum(static_cast<unsigned char>(edit->buffer[fontIconStart - 2]))) // Letter before sequence is alnum
			)
		{
			context.autocompleteActive = false;
			context.userClosed = false;
			context.lastHash = 0;
			context.resultCount = 0;
			return;
		}

		context.inModifiers = inModifiers;

		// Update scroll
		if (context.selectedOffset < context.resultOffset)
		{
			context.resultOffset = context.selectedOffset;
		}
		else if(context.selectedOffset >= context.resultOffset + FontIconAutocompleteContext::MAX_RESULTS)
		{
			context.resultOffset = context.selectedOffset - (FontIconAutocompleteContext::MAX_RESULTS - 1);
		}

		// If the user closed the context do not draw or update
		if (context.userClosed)
		{
			return;
		}

		context.autocompleteActive = true;

		// No need to update results when in modifiers
		if (context.inModifiers)
		{
			return;
		}

		// Check if results need updates
		const auto currentFontIconHash = Game::R_HashString(&edit->buffer[fontIconStart], edit->cursor - fontIconStart);
		if (currentFontIconHash == context.lastHash && context.lastResultOffset == context.resultOffset)
		{
			return;
		}

		// If query was updated then reset scroll parameters
		if (currentFontIconHash != context.lastHash)
		{
			context.resultOffset = 0;
			context.selectedOffset = 0;
			context.lastHash = currentFontIconHash;
		}

		// Update results for query and scroll and update search string
		context.lastQuery = std::string(&edit->buffer[fontIconStart], edit->cursor - fontIconStart);
		context.stringSearchStartWith.Format(context.lastQuery.c_str());
		UpdateAutocompleteContextResults(context, font, textXScale);
	}

	void TextRenderer::DrawAutocompleteModifiers(const FontIconAutocompleteInstance instance, const float x, const float y, Game::Font_s* font, const float textXScale, const float textYScale)
	{
		assert(instance < FONT_ICON_ACI_COUNT);
		const auto& context = autocompleteContextArray[instance];

		// Check which is the longest string to be able to calculate how big the box needs to be
		const auto longestStringLength = std::max(std::max(std::max(stringListHeader.GetWidth(instance, font), stringListFlipHorizontal.GetWidth(instance, font)),
			stringListFlipVertical.GetWidth(instance, font)),
			stringListBig.GetWidth(instance, font));

		// Draw background box
		const auto boxWidth = static_cast<float>(longestStringLength) * textXScale;
		constexpr auto totalLines = 4u;
		const auto lineHeight = static_cast<float>(font->pixelHeight) * textYScale;
		DrawAutocompleteBox(context,
			x - FONT_ICON_AUTOCOMPLETE_BOX_PADDING,
			y - FONT_ICON_AUTOCOMPLETE_BOX_PADDING,
			boxWidth + FONT_ICON_AUTOCOMPLETE_BOX_PADDING * 2,
			static_cast<float>(totalLines) * lineHeight + FONT_ICON_AUTOCOMPLETE_BOX_PADDING * 2,
			(*con_inputBoxColor)->current.vector);

		auto currentY = y + lineHeight;

		// Draw header line: "Following modifiers are available:"
		Game::R_AddCmdDrawText(stringListHeader.GetString(), std::numeric_limits<int>::max(), font, x, currentY, textXScale, textYScale, 0.0, TEXT_COLOR, 0);
		currentY += lineHeight;

		// Draw modifier hints
		Game::R_AddCmdDrawText(stringListFlipHorizontal.GetString(), std::numeric_limits<int>::max(), font, x, currentY, textXScale, textYScale, 0.0, TEXT_COLOR, 0);
		currentY += lineHeight;
		Game::R_AddCmdDrawText(stringListFlipVertical.GetString(), std::numeric_limits<int>::max(), font, x, currentY, textXScale, textYScale, 0.0, TEXT_COLOR, 0);
		currentY += lineHeight;
		Game::R_AddCmdDrawText(stringListBig.GetString(), std::numeric_limits<int>::max(), font, x, currentY, textXScale, textYScale, 0.0, TEXT_COLOR, 0);
	}

	void TextRenderer::DrawAutocompleteResults(const FontIconAutocompleteInstance instance, const float x, const float y, Game::Font_s* font, const float textXScale, const float textYScale)
	{
		assert(instance < FONT_ICON_ACI_COUNT);
		auto& context = autocompleteContextArray[instance];

		const auto hintEnabled = cg_fontIconAutocompleteHint.get<bool>();

		// Check which is the longest string to be able to calculate how big the box needs to be
		auto longestStringLength = context.stringSearchStartWith.GetWidth(instance, font);
		if(hintEnabled)
			longestStringLength = std::max(std::max(longestStringLength, stringHintAutoComplete.GetWidth(instance, font)), stringHintModifier.GetWidth(instance, font));

		const auto colSpacing = FONT_ICON_AUTOCOMPLETE_COL_SPACING * textXScale;
		const auto boxWidth = std::max(context.maxFontIconWidth + context.maxMaterialNameWidth + colSpacing, static_cast<float>(longestStringLength) * textXScale);
		const auto lineHeight = static_cast<float>(font->pixelHeight) * textYScale;

		// Draw background box
		const auto totalLines = 1u + context.resultCount + (hintEnabled ? 2u : 0u);
		const auto arrowPadding = context.resultOffset > 0 || context.hasMoreResults ? FONT_ICON_AUTOCOMPLETE_ARROW_SIZE : 0.0f;
		DrawAutocompleteBox(context,
			x - FONT_ICON_AUTOCOMPLETE_BOX_PADDING,
			y - FONT_ICON_AUTOCOMPLETE_BOX_PADDING,
			boxWidth + FONT_ICON_AUTOCOMPLETE_BOX_PADDING * 2 + arrowPadding,
			static_cast<float>(totalLines) * lineHeight + FONT_ICON_AUTOCOMPLETE_BOX_PADDING * 2,
			(*con_inputBoxColor)->current.vector);

		// Draw header line "Search results for: xyz"
		auto currentY = y + lineHeight;
		Game::R_AddCmdDrawText(context.stringSearchStartWith.GetString(), std::numeric_limits<int>::max(), font, x, currentY, textXScale, textYScale, 0.0, TEXT_COLOR, 0);
		currentY += lineHeight;

		// Draw search results
		const auto selectedIndex = context.selectedOffset - context.resultOffset;
		for (auto resultIndex = 0u; resultIndex < context.resultCount; resultIndex++)
		{
			const auto& result = context.results[resultIndex];
			Game::R_AddCmdDrawText(result.fontIconName.c_str(), std::numeric_limits<int>::max(), font, x, currentY, textXScale, textYScale, 0.0, TEXT_COLOR, 0);

			if (selectedIndex == resultIndex)
				Game::R_AddCmdDrawText(Utils::String::VA("^2%s", result.materialName.c_str()), std::numeric_limits<int>::max(), font, x + context.maxFontIconWidth + colSpacing, currentY, textXScale, textYScale, 0.0, TEXT_COLOR, 0);
			else
				Game::R_AddCmdDrawText(result.materialName.c_str(), std::numeric_limits<int>::max(), font, x + context.maxFontIconWidth + colSpacing, currentY, textXScale, textYScale, 0.0, TEXT_COLOR, 0);
			currentY += lineHeight;
		}

		// Draw extra hint if enabled
		if (hintEnabled)
		{
			Game::R_AddCmdDrawText(stringHintAutoComplete.GetString(), std::numeric_limits<int>::max(), font, x, currentY, textXScale, textYScale, 0.0, HINT_COLOR, 0);
			currentY += lineHeight;
			Game::R_AddCmdDrawText(stringHintModifier.GetString(), std::numeric_limits<int>::max(), font, x, currentY, textXScale, textYScale, 0.0, HINT_COLOR, 0);
		}
	}

	void TextRenderer::DrawAutocomplete(const FontIconAutocompleteInstance instance, const float x, const float y, Game::Font_s* font, const float textXScale, const float textYScale)
	{
		assert(instance < FONT_ICON_ACI_COUNT);
		const auto& context = autocompleteContextArray[instance];

		if (context.inModifiers)
			DrawAutocompleteModifiers(instance, x, y, font, textXScale, textYScale);
		else
			DrawAutocompleteResults(instance, x, y, font, textXScale, textYScale);
	}

	void TextRenderer::Con_DrawInput_Hk(const int localClientNum)
	{
		// Call original function
		Utils::Hook::Call<void(int)>(0x5A4480)(localClientNum);

		auto& autocompleteContext = autocompleteContextArray[FONT_ICON_ACI_CONSOLE];
		if (cg_fontIconAutocomplete.get<bool>() == false)
		{
			autocompleteContext.autocompleteActive = false;
			return;
		}

		UpdateAutocompleteContext(autocompleteContext, Game::g_consoleField, Game::cls->consoleFont, 1.0f);
		if (autocompleteContext.autocompleteActive)
		{
			const auto x = Game::conDrawInputGlob->leftX;
			const auto y = Game::con_screenMin[1] + 6.0f + static_cast<float>(2 * Game::R_TextHeight(Game::cls->consoleFont));
			DrawAutocomplete(FONT_ICON_ACI_CONSOLE, x, y, Game::cls->consoleFont, 1.0f, 1.0f);
		}
	}

	void TextRenderer::Field_Draw_Say(const int localClientNum, Game::field_t* edit, const int x, const int y, const int horzAlign, const int vertAlign)
	{
		Game::Field_Draw(localClientNum, edit, x, y, horzAlign, vertAlign);

		auto& autocompleteContext = autocompleteContextArray[FONT_ICON_ACI_CHAT];
		if (cg_fontIconAutocomplete.get<bool>() == false)
		{
			autocompleteContext.autocompleteActive = false;
			return;
		}

		auto* screenPlacement = Game::ScrPlace_GetActivePlacement(localClientNum);
		const auto scale = edit->charHeight / 48.0f;
		auto* font = Game::UI_GetFontHandle(screenPlacement, 0, scale);
		const auto normalizedScale = Game::R_NormalizedTextScale(font, scale);
		auto xx = static_cast<float>(x);
		auto yy = static_cast<float>(y);
		yy += static_cast<float>(Game::R_TextHeight(font)) * normalizedScale * 1.5f;
		auto ww = normalizedScale;
		auto hh = normalizedScale;
		Game::ScrPlace_ApplyRect(screenPlacement, &xx, &yy, &ww, &hh, horzAlign, vertAlign);

		UpdateAutocompleteContext(autocompleteContext, edit, font, ww);
		if (autocompleteContext.autocompleteActive)
		{
			DrawAutocomplete(FONT_ICON_ACI_CHAT, std::floor(xx), std::floor(yy), font, ww, hh);
		}
	}

	void TextRenderer::AutocompleteUp(FontIconAutocompleteContext& context)
	{
		if (context.selectedOffset > 0)
		{
			context.selectedOffset--;
		}
	}

	void TextRenderer::AutocompleteDown(FontIconAutocompleteContext& context)
	{
		if (context.resultCount < FontIconAutocompleteContext::MAX_RESULTS)
		{
			if (context.resultCount > 0 && context.selectedOffset < context.resultOffset + context.resultCount - 1)
			{
				++context.selectedOffset;
			}
		}
		else if (context.selectedOffset == context.resultOffset + context.resultCount - 1)
		{
			if (context.hasMoreResults)
			{
				++context.selectedOffset;
			}
		}
		else
		{
			context.selectedOffset++;
		}
	}

	void TextRenderer::AutocompleteFill(const FontIconAutocompleteContext& context, Game::ScreenPlacement* scrPlace, Game::field_t* edit, const bool closeFontIcon)
	{
		if (context.selectedOffset >= context.resultOffset + context.resultCount)
			return;

		const auto selectedResultIndex = context.selectedOffset - context.resultOffset;
		std::string remainingFillData = context.results[selectedResultIndex].materialName.substr(context.lastQuery.size());

		if (closeFontIcon)
		{
			remainingFillData += ":";
		}

		const std::string moveData(&edit->buffer[edit->cursor]);

		const auto remainingBufferCharacters = std::extent_v<decltype(Game::field_t::buffer)> - edit->cursor - moveData.size() - 1;
		if (remainingFillData.size() > remainingBufferCharacters)
		{
			remainingFillData = remainingFillData.erase(remainingBufferCharacters);
		}

		if (!remainingFillData.empty())
		{
			strncpy(&edit->buffer[edit->cursor], remainingFillData.c_str(), remainingFillData.size());
			strncpy(&edit->buffer[edit->cursor + remainingFillData.size()], moveData.c_str(), moveData.size());
			edit->buffer[std::extent_v<decltype(Game::field_t::buffer)> - 1] = '\0';
			edit->cursor += static_cast<int>(remainingFillData.size());
			Game::Field_AdjustScroll(scrPlace, edit);
		}
	}

	bool TextRenderer::AutocompleteHandleKeyDown(FontIconAutocompleteContext& context, const int key, Game::ScreenPlacement* scrPlace, Game::field_t* edit)
	{
		switch (key)
		{
		case Game::K_UPARROW:
		case Game::K_KP_UPARROW:
			AutocompleteUp(context);
			return true;

		case Game::K_DOWNARROW:
		case Game::K_KP_DOWNARROW:
			AutocompleteDown(context);
			return true;

		case Game::K_ENTER:
		case Game::K_KP_ENTER:
			if(context.resultCount > 0)
			{
				AutocompleteFill(context, scrPlace, edit, true);
				return true;
			}
			return false;

		case Game::K_TAB:
			AutocompleteFill(context, scrPlace, edit, false);
			return true;

		case Game::K_ESCAPE:
			if (!context.userClosed)
			{
				context.autocompleteActive = false;
				context.userClosed = true;
				return true;
			}
			return false;

		default:
			return false;
		}
	}

	bool TextRenderer::HandleFontIconAutocompleteKey(const int localClientNum, const FontIconAutocompleteInstance autocompleteInstance, const int key)
	{
		assert(autocompleteInstance < FONT_ICON_ACI_COUNT);
		if (autocompleteInstance >= FONT_ICON_ACI_COUNT)
			return false;

		auto& autocompleteContext = autocompleteContextArray[autocompleteInstance];
		if (!autocompleteContext.autocompleteActive)
			return false;

		if (autocompleteInstance == FONT_ICON_ACI_CONSOLE)
			return AutocompleteHandleKeyDown(autocompleteContext, key, Game::scrPlaceFull, Game::g_consoleField);

		if (autocompleteInstance == FONT_ICON_ACI_CHAT)
			return AutocompleteHandleKeyDown(autocompleteContext, key, &Game::scrPlaceView[localClientNum], &Game::playerKeys[localClientNum].chatField);

		return false;
	}

	void TextRenderer::Console_Key_Hk(const int localClientNum, const int key)
	{
		if (HandleFontIconAutocompleteKey(localClientNum, FONT_ICON_ACI_CONSOLE, key))
			return;

		Utils::Hook::Call<void(int, int)>(0x4311E0)(localClientNum, key);
	}

	bool TextRenderer::ChatHandleKeyDown(const int localClientNum, const int key)
	{
		return HandleFontIconAutocompleteKey(localClientNum, FONT_ICON_ACI_CHAT, key);
	}

	constexpr auto Message_Key = 0x5A7E50;
	__declspec(naked) void TextRenderer::Message_Key_Stub()
	{
		__asm
		{
			pushad

			push eax
			push edi
			call ChatHandleKeyDown
			add esp, 0x8
			test al,al
			jnz skipHandling

			popad
			call Message_Key
			ret

		skipHandling:
			popad
			mov al, 1
			ret
		}
	}

	float TextRenderer::GetMonospaceWidth(Game::Font_s* font, int rendererFlags)
	{
		if (rendererFlags & Game::TEXT_RENDERFLAG_FORCEMONOSPACE)
		{
			return Game::R_GetCharacterGlyph(font, 'o')->dx;
		}

		return 0.0f;
	}

	void TextRenderer::GlowColor(Game::GfxColor* result, const Game::GfxColor baseColor, const Game::GfxColor forcedGlowColor, int renderFlags)
	{
		if (renderFlags & Game::TEXT_RENDERFLAG_GLOW_FORCE_COLOR)
		{
			result->array[0] = forcedGlowColor.array[0];
			result->array[1] = forcedGlowColor.array[1];
			result->array[2] = forcedGlowColor.array[2];
		}
		else
		{
			result->array[0] = static_cast<char>(std::floor(static_cast<float>(static_cast<uint8_t>(baseColor.array[0])) * 0.06f));
			result->array[1] = static_cast<char>(std::floor(static_cast<float>(static_cast<uint8_t>(baseColor.array[1])) * 0.06f));
			result->array[2] = static_cast<char>(std::floor(static_cast<float>(static_cast<uint8_t>(baseColor.array[2])) * 0.06f));
		}
	}

	unsigned TextRenderer::R_FontGetRandomLetter(const int seed)
	{
		static constexpr char RANDOM_CHARACTERS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1234567890";
		return RANDOM_CHARACTERS[seed % (std::extent_v<decltype(RANDOM_CHARACTERS)> - 1)];
	}

	void TextRenderer::DrawTextFxExtraCharacter(Game::Material* material, const int charIndex, const float x, const float y, const float w, const float h, const float sinAngle, const float cosAngle, const unsigned color)
	{
		Game::RB_DrawStretchPicRotate(material, x, y, w, h, static_cast<float>(charIndex % 16) * 0.0625f, 0.0f, static_cast<float>(charIndex % 16) * 0.0625f + 0.0625f, 1.0f, sinAngle, cosAngle, color);
	}

	Game::GfxImage* TextRenderer::GetFontIconColorMap(const Game::Material* fontIconMaterial)
	{
		for (auto i = 0u; i < fontIconMaterial->textureCount; i++)
		{
			if (fontIconMaterial->textureTable[i].nameHash == COLOR_MAP_HASH)
			{
				return fontIconMaterial->textureTable[i].u.image;
			}
		}

		return nullptr;
	}

	bool TextRenderer::IsFontIcon(const char*& text, FontIconInfo& fontIcon)
	{
		const auto* curPos = text;

		while (*curPos != ' ' && *curPos != FONT_ICON_SEPARATOR_CHARACTER && *curPos != 0 && *curPos != FONT_ICON_MODIFIER_SEPARATOR_CHARACTER)
			curPos++;

		const auto* nameEnd = curPos;

		if (*curPos == FONT_ICON_MODIFIER_SEPARATOR_CHARACTER)
		{
			auto breakArgs = false;
			while (!breakArgs)
			{
				curPos++;
				switch(*curPos)
				{
				case FONT_ICON_MODIFIER_FLIP_HORIZONTALLY:
					fontIcon.flipHorizontal = true;
					break;

				case FONT_ICON_MODIFIER_FLIP_VERTICALLY:
					fontIcon.flipVertical = true;
					break;

				case FONT_ICON_MODIFIER_BIG:
					fontIcon.big = true;
					break;

				case FONT_ICON_SEPARATOR_CHARACTER:
					breakArgs = true;
					break;

				default:
					return false;
				}
			}
		}

		if (*curPos != FONT_ICON_SEPARATOR_CHARACTER)
		{
			return false;
		}

		const std::string fontIconName(text, nameEnd - text);

		const auto foundFontIcon = fontIconLookup.find(fontIconName);
		if (foundFontIcon == fontIconLookup.end())
		{
			return false;
		}

		auto& entry = foundFontIcon->second;
		if (entry.material == nullptr)
		{
			auto* materialEntry = Game::DB_FindXAssetEntry(Game::XAssetType::ASSET_TYPE_MATERIAL, entry.materialName.data());
			if (materialEntry == nullptr)
				return false;
			auto* material = materialEntry->asset.header.material;
			if (material == nullptr || material->techniqueSet == nullptr || material->techniqueSet->name == nullptr)
				return false;

			if (std::strcmp(material->techniqueSet->name, "2d") != 0)
			{
				Logger::PrintError(Game::CON_CHANNEL_ERROR, "Fonticon material '{}' does not have 2d techset!\n", material->info.name);
				material = Game::DB_FindXAssetHeader(Game::ASSET_TYPE_MATERIAL, "default").material;
			}

			entry.material = material;
		}

		text = curPos + 1;
		fontIcon.material = entry.material;
		return true;
	}

	float TextRenderer::GetNormalizedFontIconWidth(const FontIconInfo& fontIcon)
	{
		const auto* colorMap = GetFontIconColorMap(fontIcon.material);
		if (colorMap == nullptr)
		{
			return 0.0f;
		}

		const auto sizeMultiplier = fontIcon.big ? 1.5f : 1.0f;
		auto colWidth = static_cast<float>(colorMap->width);
		auto colHeight = static_cast<float>(colorMap->height);
		if (fontIcon.material->info.textureAtlasColumnCount > 1)
			colWidth /= static_cast<float>(fontIcon.material->info.textureAtlasColumnCount);
		if (fontIcon.material->info.textureAtlasRowCount > 1)
			colHeight /= static_cast<float>(fontIcon.material->info.textureAtlasRowCount);
		return (colWidth / colHeight) * sizeMultiplier;
	}

	float TextRenderer::GetFontIconWidth(const FontIconInfo& fontIcon, const Game::Font_s* font, const float xScale)
	{
		const auto* colorMap = GetFontIconColorMap(fontIcon.material);
		if (colorMap == nullptr)
		{
			return 0.0f;
		}

		const auto sizeMultiplier = fontIcon.big ? 1.5f : 1.0f;
		auto colWidth = static_cast<float>(colorMap->width);
		auto colHeight = static_cast<float>(colorMap->height);
		if (fontIcon.material->info.textureAtlasColumnCount > 1)
			colWidth /= static_cast<float>(fontIcon.material->info.textureAtlasColumnCount);
		if (fontIcon.material->info.textureAtlasRowCount > 1)
			colHeight /= static_cast<float>(fontIcon.material->info.textureAtlasRowCount);
		return static_cast<float>(font->pixelHeight) * (colWidth / colHeight) * xScale * sizeMultiplier;
	}

	float TextRenderer::DrawFontIcon(const FontIconInfo& fontIcon, const float x, const float y, const float sinAngle, const float cosAngle, const Game::Font_s* font, const float xScale, const float yScale, const unsigned color)
	{
		const auto* colorMap = GetFontIconColorMap(fontIcon.material);
		if (colorMap == nullptr)
		{
			return 0.0f;
		}

		float s0, t0, s1, t1;
		if (fontIcon.flipHorizontal)
		{
			s0 = 1.0f;
			s1 = 0.0f;
		}
		else
		{
			s0 = 0.0f;
			s1 = 1.0f;
		}
		if (fontIcon.flipVertical)
		{
			t0 = 1.0f;
			t1 = 0.0f;
		}
		else
		{
			t0 = 0.0f;
			t1 = 1.0f;
		}

		Game::Material_Process2DTextureCoordsForAtlasing(fontIcon.material, &s0, &s1, &t0, &t1);
		const auto sizeMultiplier = fontIcon.big ? 1.5f : 1.0f;

		auto colWidth = static_cast<float>(colorMap->width);
		auto colHeight = static_cast<float>(colorMap->height);
		if (fontIcon.material->info.textureAtlasColumnCount > 1)
			colWidth /= static_cast<float>(fontIcon.material->info.textureAtlasColumnCount);
		if (fontIcon.material->info.textureAtlasRowCount > 1)
			colHeight /= static_cast<float>(fontIcon.material->info.textureAtlasRowCount);

		const auto h = static_cast<float>(font->pixelHeight) * yScale * sizeMultiplier;
		const auto w = static_cast<float>(font->pixelHeight) * (colWidth / colHeight) * xScale * sizeMultiplier;

		const auto yy = y - (h + yScale * static_cast<float>(font->pixelHeight)) * 0.5f;
		Game::RB_DrawStretchPicRotate(fontIcon.material, x, yy, w, h, s0, t0, s1, t1, sinAngle, cosAngle, color);

		return w;
	}

	float TextRenderer::DrawHudIcon(const char*& text, const float x, const float y, const float sinAngle, const float cosAngle, const Game::Font_s* font, const float xScale, const float yScale, const unsigned color)
	{
		float s0, s1, t0, t1;

		if (*text == '\x01')
		{
			s0 = 0.0;
			t0 = 0.0;
			s1 = 1.0;
			t1 = 1.0;
		}
		else
		{
			s0 = 1.0;
			t0 = 0.0;
			s1 = 0.0;
			t1 = 1.0;
		}

		++text;

		if (*text == 0)
		{
			return 0.0f;
		}

		const auto v12 = font->pixelHeight * (*text - 16) + 16;
		const auto w = static_cast<float>((((v12 >> 24) & 0x1F) + v12) >> 5) * xScale;
		++text;

		if (*text == 0)
		{
			return 0.0f;
		}

		const auto h = static_cast<float>((font->pixelHeight * (*text - 16) + 16) >> 5) * yScale;
		++text;

		if (*text == 0)
		{
			return 0.0f;
		}

		const auto materialNameLen = static_cast<uint8_t>(*text);
		++text;

		for (auto i = 0u; i < materialNameLen; i++)
		{
			if (text[i] == 0)
			{
				return 0.0f;
			}
		}

		const std::string materialName(text, materialNameLen);
		text += materialNameLen;

		auto* material = Game::DB_FindXAssetHeader(Game::XAssetType::ASSET_TYPE_MATERIAL, materialName.data()).material;
		if (material == nullptr || material->techniqueSet == nullptr || material->techniqueSet->name == nullptr || std::strcmp(material->techniqueSet->name, "2d") != 0)
		{
			material = Game::DB_FindXAssetHeader(Game::XAssetType::ASSET_TYPE_MATERIAL, "default").material;
		}

		const auto yy = y - (h + yScale * static_cast<float>(font->pixelHeight)) * 0.5f;

		Game::RB_DrawStretchPicRotate(material, x, yy, w, h, s0, t0, s1, t1, sinAngle, cosAngle, color);

		return w;
	}

	void TextRenderer::RotateXY(const float cosAngle, const float sinAngle, const float pivotX, const float pivotY, const float x, const float y, float* outX, float* outY)
	{
		*outX = (x - pivotX) * cosAngle + pivotX - (y - pivotY) * sinAngle;
		*outY = (y - pivotY) * cosAngle + pivotY + (x - pivotX) * sinAngle;
	}

	void TextRenderer::DrawText2D(const char* text, float x, float y, Game::Font_s* font, float xScale, float yScale, float sinAngle, float cosAngle, Game::GfxColor color, int maxLength, int renderFlags, int cursorPos, char cursorLetter, float padding, Game::GfxColor glowForcedColor, int fxBirthTime, int fxLetterTime, int fxDecayStartTime, int fxDecayDuration, Game::Material* fxMaterial, Game::Material* fxMaterialGlow)
	{
		UpdateColorTable();

		Game::GfxColor dropShadowColor{0};
		dropShadowColor.array[3] = color.array[3];

		int randSeed = 1;
		bool drawRandomCharAtEnd = false;
		const auto forceMonospace = renderFlags & Game::TEXT_RENDERFLAG_FORCEMONOSPACE;
		const auto monospaceWidth = GetMonospaceWidth(font, renderFlags);
		auto* material = font->material;
		Game::Material* glowMaterial = nullptr;

		bool decaying;
		int decayTimeElapsed;
		if(renderFlags & Game::TEXT_RENDERFLAG_FX_DECODE)
		{
			if (!Game::SetupPulseFXVars(text, maxLength, fxBirthTime, fxLetterTime, fxDecayStartTime, fxDecayDuration, &drawRandomCharAtEnd, &randSeed, &maxLength, &decaying, &decayTimeElapsed))
				return;
		}
		else
		{
			drawRandomCharAtEnd = false;
			randSeed = 1;
			decaying = false;
			decayTimeElapsed = 0;
		}

		Game::FontPassType passes[Game::FONTPASS_COUNT];
		unsigned passCount = 0;

		if(renderFlags & Game::TEXT_RENDERFLAG_OUTLINE)
		{
			if(renderFlags & Game::TEXT_RENDERFLAG_GLOW)
			{
				glowMaterial = font->glowMaterial;
				passes[passCount++] = Game::FONTPASS_GLOW;
			}

			passes[passCount++] = Game::FONTPASS_OUTLINE;
			passes[passCount++] = Game::FONTPASS_NORMAL;
		}
		else
		{
			passes[passCount++] = Game::FONTPASS_NORMAL;

			if (renderFlags & Game::TEXT_RENDERFLAG_GLOW)
			{
				glowMaterial = font->glowMaterial;
				passes[passCount++] = Game::FONTPASS_GLOW;
			}
		}

		const auto startX = x - xScale * 0.5f;
		const auto startY = y - 0.5f * yScale;

		for (auto passIndex = 0u; passIndex < passCount; passIndex++)
		{
			float xRot, yRot;
			const char* curText = text;
			auto maxLengthRemaining = maxLength;
			auto currentColor = color;
			auto subtitleAllowGlow = false;
			auto extraFxChar = 0;
			auto drawExtraFxChar = false;
			auto passRandSeed = randSeed;
			auto count = 0;
			auto xa = startX;
			auto xy = startY;

			while (*curText && maxLengthRemaining)
			{
				if (passes[passIndex] == Game::FONTPASS_NORMAL && renderFlags & Game::TEXT_RENDERFLAG_CURSOR && count == cursorPos)
				{
					RotateXY(cosAngle, sinAngle, startX, startY, xa, xy, &xRot, &yRot);
					Game::RB_DrawCursor(material, cursorLetter, xRot, yRot, sinAngle, cosAngle, font, xScale, yScale, color.packed);
				}

				auto letter = Game::SEH_ReadCharFromString(&curText, nullptr);

				if (letter == '^' && *curText >= COLOR_FIRST_CHAR && *curText <= COLOR_LAST_CHAR)
				{
					const auto colorIndex = ColorIndexForChar(*curText);
					subtitleAllowGlow = false;
					if (colorIndex == TEXT_COLOR_DEFAULT)
					{
						currentColor = color;
					}
					else if (renderFlags & Game::TEXT_RENDERFLAG_SUBTITLETEXT && colorIndex == TEXT_COLOR_GREEN)
					{
						constexpr Game::GfxColor altColor{ MY_ALTCOLOR_TWO };
						subtitleAllowGlow = true;
						// Swap r and b for whatever reason
						currentColor.packed = ColorRgba(altColor.array[2], altColor.array[1], altColor.array[0], Game::ModulateByteColors(altColor.array[3], color.array[3]));
					}
					else
					{
						const Game::GfxColor colorTableColor{ (*currentColorTable)[colorIndex] };
						// Swap r and b for whatever reason
						currentColor.packed = ColorRgba(colorTableColor.array[2], colorTableColor.array[1], colorTableColor.array[0], color.array[3]);
					}

					if (!(renderFlags & Game::TEXT_RENDERFLAG_CURSOR && cursorPos > count && cursorPos < count + 2))
					{
						curText++;
						count += 2;
						continue;
					}
				}

				auto finalColor = currentColor;

				if (letter == '^' && (*curText == '\x01' || *curText == '\x02'))
				{
					RotateXY(cosAngle, sinAngle, startX, startY, xa, xy, &xRot, &yRot);
					xa += DrawHudIcon(curText, xRot, yRot, sinAngle, cosAngle, font, xScale, yScale, ColorRgba(255, 255, 255, finalColor.array[3]));

					if (renderFlags & Game::TEXT_RENDERFLAG_PADDING)
						xa += xScale * padding;
					++count;
					--maxLengthRemaining;
					continue;
				}

				if (letter == '^')
				{
					const char* unicodeEnd = curText;
					std::optional<RuntimeUnicodeRun> unicodeText;
					std::size_t tokenLength{};
					bool isUnicodeGlyph = false;
					std::uint32_t value{};
					if (ParseUnicodeGlyphEscape(unicodeEnd, value))
					{
						unicodeText = GetUnicodeGlyph(value);
						tokenLength = UNICODE_GLYPH_HEX_LENGTH + 2;
						isUnicodeGlyph = true;
					}
					else
					{
						unicodeEnd = curText;
						if (ParseUnicodeRunEscape(unicodeEnd, value))
						{
							unicodeText = GetUnicodeRun(value);
							tokenLength = UNICODE_RUN_ID_HEX_LENGTH + 2;
						}
					}

					if (tokenLength != 0)
					{
						curText = unicodeEnd;
						if (!unicodeText)
						{
							letter = '?';
							count += static_cast<int>(tokenLength - 1);
						}
						else
						{
							const auto fontHeight = static_cast<float>(font->pixelHeight);
							const auto runWidth = unicodeText->width * fontHeight * xScale;
							const auto runHeight = unicodeText->height * fontHeight * yScale;
							const auto xAdj = unicodeText->bearingX * fontHeight * xScale;
							const auto yAdj = unicodeText->bearingY * fontHeight * yScale
								+ (isUnicodeGlyph ? UNICODE_GLYPH_BASELINE_OFFSET * yScale : 0.0f);
							const auto drawRun = [&](const float xOffset, const float yOffset, const unsigned packedColor)
							{
								RotateXY(cosAngle, sinAngle, startX, startY, xa + xAdj + xOffset,
									xy + yAdj + yOffset, &xRot, &yRot);
								Game::RB_DrawStretchPicRotate(unicodeText->material, xRot, yRot, runWidth, runHeight,
									0.0f, 0.0f, 1.0f, 1.0f, sinAngle, cosAngle, packedColor);
							};

							if (passes[passIndex] == Game::FONTPASS_NORMAL)
							{
								if (renderFlags & Game::TEXT_RENDERFLAG_DROPSHADOW)
								{
									const auto offset = (renderFlags & Game::TEXT_RENDERFLAG_DROPSHADOW_EXTRA) ? 2.0f : 1.0f;
									drawRun(offset, offset, dropShadowColor.packed);
								}
								drawRun(0.0f, 0.0f, finalColor.packed);
							}
							else if (passes[passIndex] == Game::FONTPASS_OUTLINE)
							{
								const auto outlineSize = (renderFlags & Game::TEXT_RENDERFLAG_OUTLINE_EXTRA) ? 1.3f : 1.0f;
								for (const auto offset : MY_OFFSETS)
								{
									drawRun(outlineSize * offset[0], outlineSize * offset[1], dropShadowColor.packed);
								}
							}
							else if (passes[passIndex] == Game::FONTPASS_GLOW
								&& ((renderFlags & Game::TEXT_RENDERFLAG_SUBTITLETEXT) == 0 || subtitleAllowGlow))
							{
								GlowColor(&finalColor, finalColor, glowForcedColor, renderFlags);
								for (const auto offset : MY_OFFSETS)
								{
									drawRun(2.0f * offset[0] * xScale, 2.0f * offset[1] * yScale, finalColor.packed);
								}
							}

							if (forceMonospace) xa += monospaceWidth * xScale;
							else xa += unicodeText->advance * fontHeight * xScale;
							if (renderFlags & Game::TEXT_RENDERFLAG_PADDING) xa += xScale * padding;
							count += static_cast<int>(tokenLength);
							--maxLengthRemaining;
							continue;
						}
					}
				}

				if (letter == FONT_ICON_SEPARATOR_CHARACTER)
				{
					FontIconInfo fontIconInfo{};
					const char* fontIconEnd = curText;
					if (IsFontIcon(fontIconEnd, fontIconInfo) && !(renderFlags & Game::TEXT_RENDERFLAG_CURSOR && cursorPos > count && cursorPos <= count + (fontIconEnd - curText)))
					{
						RotateXY(cosAngle, sinAngle, startX, startY, xa, xy, &xRot, &yRot);

						if(passes[passIndex] == Game::FONTPASS_NORMAL)
							xa += DrawFontIcon(fontIconInfo, xRot, yRot, sinAngle, cosAngle, font, xScale, yScale, ColorRgba(255, 255, 255, finalColor.array[3]));
						else
							xa += GetFontIconWidth(fontIconInfo, font, xScale);

						if (renderFlags & Game::TEXT_RENDERFLAG_PADDING)
							xa += xScale * padding;
						count += (fontIconEnd - curText) + 1;
						--maxLengthRemaining;
						curText = fontIconEnd;
						continue;
					}
				}

				if (drawRandomCharAtEnd && maxLengthRemaining == 1)
				{
					letter = R_FontGetRandomLetter(Game::RandWithSeed(&passRandSeed));

					if(Game::RandWithSeed(&passRandSeed) % 2)
					{
						drawExtraFxChar = true;
						letter = 'O';
					}
				}

				if (letter == '\n')
				{
					xa = startX;
					xy += static_cast<float>(font->pixelHeight) * yScale;
					continue;
				}

				if (letter == '\r')
				{
					xy += static_cast<float>(font->pixelHeight) * yScale;
					continue;
				}

				auto skipDrawing = false;
				if (decaying)
				{
					char decayAlpha;
					Game::GetDecayingLetterInfo(letter, &passRandSeed, decayTimeElapsed, fxBirthTime, fxDecayDuration, currentColor.array[3], &skipDrawing, &decayAlpha, &letter, &drawExtraFxChar);
					finalColor.array[3] = decayAlpha;
				}

				if (drawExtraFxChar)
				{
					auto tempSeed = passRandSeed;
					extraFxChar = Game::RandWithSeed(&tempSeed);
				}

				auto glyph = Game::R_GetCharacterGlyph(font, letter);
				auto xAdj = static_cast<float>(glyph->x0) * xScale;
				auto yAdj = static_cast<float>(glyph->y0) * yScale;

				if (!skipDrawing)
				{
					if (passes[passIndex] == Game::FONTPASS_NORMAL)
					{
						if (renderFlags & Game::TEXT_RENDERFLAG_DROPSHADOW)
						{
							auto ofs = 1.0f;
							if (renderFlags & Game::TEXT_RENDERFLAG_DROPSHADOW_EXTRA)
								ofs += 1.0f;

							xRot = xa + xAdj + ofs;
							yRot = xy + yAdj + ofs;
							RotateXY(cosAngle, sinAngle, startX, startY, xRot, yRot, &xRot, &yRot);
							if (drawExtraFxChar)
								DrawTextFxExtraCharacter(fxMaterial, extraFxChar, xRot, yRot, static_cast<float>(glyph->pixelWidth) * xScale, static_cast<float>(glyph->pixelHeight) * yScale, sinAngle, cosAngle, dropShadowColor.packed);
							else
								Game::RB_DrawChar(material, xRot, yRot, static_cast<float>(glyph->pixelWidth) * xScale, static_cast<float>(glyph->pixelHeight) * yScale, sinAngle, cosAngle, glyph, dropShadowColor.packed);
						}

						RotateXY(cosAngle, sinAngle, startX, startY, xa + xAdj, xy + yAdj, &xRot, &yRot);
						if (drawExtraFxChar)
							DrawTextFxExtraCharacter(fxMaterial, extraFxChar, xRot, yRot, static_cast<float>(glyph->pixelWidth) * xScale, static_cast<float>(glyph->pixelHeight) * yScale, sinAngle, cosAngle, finalColor.packed);
						else
							Game::RB_DrawChar(material, xRot, yRot, static_cast<float>(glyph->pixelWidth) * xScale, static_cast<float>(glyph->pixelHeight) * yScale, sinAngle, cosAngle, glyph, finalColor.packed);
					}
					else if (passes[passIndex] == Game::FONTPASS_OUTLINE)
					{
						auto outlineSize = 1.0f;
						if (renderFlags & Game::TEXT_RENDERFLAG_OUTLINE_EXTRA)
							outlineSize = 1.3f;

						for (const auto offset : MY_OFFSETS)
						{
							RotateXY(cosAngle, sinAngle, startX, startY, xa + xAdj + outlineSize * offset[0], xy + yAdj + outlineSize * offset[1], &xRot, &yRot);
							if (drawExtraFxChar)
								DrawTextFxExtraCharacter(fxMaterial, extraFxChar, xRot, yRot, static_cast<float>(glyph->pixelWidth) * xScale, static_cast<float>(glyph->pixelHeight) * yScale, sinAngle, cosAngle, dropShadowColor.packed);
							else
								Game::RB_DrawChar(material, xRot, yRot, static_cast<float>(glyph->pixelWidth) * xScale, static_cast<float>(glyph->pixelHeight) * yScale, sinAngle, cosAngle, glyph, dropShadowColor.packed);
						}
					}
					else if(passes[passIndex] == Game::FONTPASS_GLOW && ((renderFlags & Game::TEXT_RENDERFLAG_SUBTITLETEXT) == 0 || subtitleAllowGlow))
					{
						GlowColor(&finalColor, finalColor, glowForcedColor, renderFlags);

						for (const auto offset : MY_OFFSETS)
						{
							RotateXY(cosAngle, sinAngle, startX, startY, xa + xAdj + 2.0f * offset[0] * xScale, xy + yAdj + 2.0f * offset[1] * yScale, &xRot, &yRot);
							if (drawExtraFxChar)
								DrawTextFxExtraCharacter(fxMaterialGlow, extraFxChar, xRot, yRot, static_cast<float>(glyph->pixelWidth) * xScale, static_cast<float>(glyph->pixelHeight) * yScale, sinAngle, cosAngle, finalColor.packed);
							else
								Game::RB_DrawChar(glowMaterial, xRot, yRot, static_cast<float>(glyph->pixelWidth) * xScale, static_cast<float>(glyph->pixelHeight) * yScale, sinAngle, cosAngle, glyph, finalColor.packed);
						}
					}
				}

				if (forceMonospace)
					xa += monospaceWidth * xScale;
				else
					xa += static_cast<float>(glyph->dx) * xScale;

				if (renderFlags & Game::TEXT_RENDERFLAG_PADDING)
					xa += xScale * padding;

				++count;
				--maxLengthRemaining;
			}

			if (renderFlags & Game::TEXT_RENDERFLAG_CURSOR && count == cursorPos)
			{
				RotateXY(cosAngle, sinAngle, startX, startY, xa, xy, &xRot, &yRot);
				Game::RB_DrawCursor(material, cursorLetter, xRot, yRot, sinAngle, cosAngle, font, xScale, yScale, color.packed);
			}
		}
	}

	int TextRenderer::R_TextWidth_Hk(const char* text, int maxChars, Game::Font_s* font)
	{
		auto lineWidth = 0;
		auto maxWidth = 0;

		if (maxChars <= 0)
		{
			maxChars = std::numeric_limits<int>::max();
		}

		if (text == nullptr)
		{
			return 0;
		}

		auto count = 0;
		while (text && *text && count < maxChars)
		{
			const auto letter = Game::SEH_ReadCharFromString(&text, nullptr);
			if (letter == '\r' || letter == '\n')
			{
				lineWidth = 0;
			}
			else
			{
				if (letter == '^' && text)
				{
					if (*text >= COLOR_FIRST_CHAR && *text <= COLOR_LAST_CHAR)
					{
						++text;
						continue;
					}

					if (*text >= '\x01' && *text <= '\x02' && text[1] != '\0' && text[2] != '\0' && text[3] != '\0')
					{
						const auto width = text[1];
						const auto materialNameLength = text[3];

						// This is how the game calculates width and height. Probably some 1 byte floating point number.
						// Details to be investigated if necessary.
						const auto v9 = font->pixelHeight * (width - 16) + 16;
						const auto w = ((((v9 >> 24) & 0x1F) + v9) >> 5);

						lineWidth += w;
						if (lineWidth > maxWidth)
						{
							maxWidth = lineWidth;
						}

						text += 4;
						for (auto currentLength = 0; currentLength < materialNameLength && *text; currentLength++)
						{
							++text;
						}
						continue;
					}

					const char* unicodeEnd = text;
					std::optional<RuntimeUnicodeRun> unicodeText;
					std::uint32_t value{};
					if (ParseUnicodeGlyphEscape(unicodeEnd, value))
					{
						unicodeText = GetUnicodeGlyph(value);
					}
					else
					{
						unicodeEnd = text;
						if (ParseUnicodeRunEscape(unicodeEnd, value)) unicodeText = GetUnicodeRun(value);
						else unicodeEnd = nullptr;
					}
					if (unicodeEnd)
					{
						text = unicodeEnd;
						if (unicodeText)
						{
							lineWidth += static_cast<int>(std::roundf(unicodeText->advance
								* static_cast<float>(font->pixelHeight)));
						}
						else
						{
							lineWidth += R_GetCharacterGlyph(font, '?')->dx;
						}
						maxWidth = std::max(maxWidth, lineWidth);
						++count;
						continue;
					}
				}

				if (letter == FONT_ICON_SEPARATOR_CHARACTER)
				{
					FontIconInfo fontIconInfo{};
					const char* fontIconEnd = text;
					if (IsFontIcon(fontIconEnd, fontIconInfo))
					{
						lineWidth += static_cast<int>(GetFontIconWidth(fontIconInfo, font, 1.0f));
						if (lineWidth > maxWidth)
						{
							maxWidth = lineWidth;
						}
						text = fontIconEnd;
						continue;
					}
				}

				lineWidth += R_GetCharacterGlyph(font, letter)->dx;
				if (lineWidth > maxWidth)
				{
					maxWidth = lineWidth;
				}

				++count;
			}
		}

		return maxWidth;
	}

	unsigned int TextRenderer::ColorIndex(const char index)
	{
		auto result = index - '0';
		if (static_cast<unsigned int>(result) >= TEXT_COLOR_COUNT || result < 0) result = 7;
		return result;
	}

	void TextRenderer::StripColors(const char* in, char* out, std::size_t max)
	{
		if (!in || !out) return;

		max--;
		std::size_t current = 0;
		while (*in != 0 && current < max)
		{
			const char index = *(in + 1);
			if (*in == '^' && (ColorIndex(index) != 7 || index == '7'))
			{
				++in;
			}
			else
			{
				*out = *in;
				++out;
				++current;
			}

			++in;
		}

		*out = '\0';
	}

	std::string TextRenderer::StripColors(const std::string& in)
	{
		char buffer[1024]{}; // 1024 is a lucky number in the engine
		StripColors(in.data(), buffer, sizeof(buffer));
		return std::string{ buffer };
	}

	std::string TextRenderer::EncodeUtf8ForGame(const std::string_view text, const std::size_t maxCharacters)
	{
		if (text.empty() || maxCharacters == 0
			|| text.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
		{
			return {};
		}

		const auto textLength = static_cast<int>(text.size());
		const auto wideLength = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), textLength, nullptr, 0);
		if (wideLength <= 0) return {};

		std::wstring wideText(static_cast<std::size_t>(wideLength), L'\0');
		if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), textLength,
			wideText.data(), wideLength) != wideLength)
		{
			return {};
		}

		const auto effectiveMaxCharacters = std::min(maxCharacters,
			static_cast<std::size_t>(STRING_BUFFER_SIZE_BIG / 8));
		std::vector<std::uint32_t> codepoints;
		codepoints.reserve(std::min(wideText.size(), effectiveMaxCharacters));
		std::size_t characterCount{};
		for (std::size_t index = 0; index < wideText.size() && characterCount < effectiveMaxCharacters;)
		{
			std::uint32_t codepoint = static_cast<std::uint16_t>(wideText[index]);
			int codeUnitCount = 1;
			if (codepoint >= 0xD800 && codepoint <= 0xDBFF && index + 1 < wideText.size())
			{
				const auto trailing = static_cast<std::uint32_t>(wideText[index + 1]);
				if (trailing >= 0xDC00 && trailing <= 0xDFFF)
				{
					codepoint = 0x10000 + ((codepoint - 0xD800) << 10) + (trailing - 0xDC00);
					codeUnitCount = 2;
				}
			}

			index += static_cast<std::size_t>(codeUnitCount);
			if (codepoint < 0x20 || (codepoint >= 0x7F && codepoint <= 0x9F)
				|| (codepoint >= 0x202A && codepoint <= 0x202E)
				|| (codepoint >= 0x2066 && codepoint <= 0x2069))
			{
				continue;
			}

			codepoints.push_back(codepoint);
			++characterCount;
		}

		// User-controlled names must not be able to inject the game's color codes.
		std::vector<std::uint32_t> sanitizedCodepoints;
		sanitizedCodepoints.reserve(codepoints.size());
		for (std::size_t index = 0; index < codepoints.size(); ++index)
		{
			if (codepoints[index] == '^' && index + 1 < codepoints.size()
				&& codepoints[index + 1] >= static_cast<std::uint32_t>(COLOR_FIRST_CHAR)
				&& codepoints[index + 1] <= static_cast<std::uint32_t>(COLOR_LAST_CHAR))
			{
				++index;
				continue;
			}
			sanitizedCodepoints.push_back(codepoints[index]);
		}
		if (sanitizedCodepoints.empty()) return {};

		const auto isWhitespace = [](const std::uint32_t codepoint)
		{
			return codepoint == 0x20 || codepoint == 0xA0 || codepoint == 0x1680
				|| (codepoint >= 0x2000 && codepoint <= 0x200A) || codepoint == 0x2028
				|| codepoint == 0x2029 || codepoint == 0x202F || codepoint == 0x205F
				|| codepoint == 0x3000;
		};

		const auto appendUtf16 = [](std::wstring& output, const std::uint32_t codepoint)
		{
			if (codepoint <= 0xFFFF)
			{
				output.push_back(static_cast<wchar_t>(codepoint));
			}
			else
			{
				const auto value = codepoint - 0x10000;
				output.push_back(static_cast<wchar_t>(0xD800 + (value >> 10)));
				output.push_back(static_cast<wchar_t>(0xDC00 + (value & 0x3FF)));
			}
		};

		const auto convertToWindows1252 = [&](const std::uint32_t codepoint, char& converted)
		{
			wchar_t utf16[2]{};
			int utf16Length = 1;
			if (codepoint <= 0xFFFF)
			{
				utf16[0] = static_cast<wchar_t>(codepoint);
			}
			else
			{
				const auto value = codepoint - 0x10000;
				utf16[0] = static_cast<wchar_t>(0xD800 + (value >> 10));
				utf16[1] = static_cast<wchar_t>(0xDC00 + (value & 0x3FF));
				utf16Length = 2;
			}

			BOOL usedDefaultCharacter = FALSE;
			return WideCharToMultiByte(1252, WC_NO_BEST_FIT_CHARS, utf16, utf16Length,
				&converted, 1, nullptr, &usedDefaultCharacter) == 1 && !usedDefaultCharacter;
		};

		for (auto& codepoint : sanitizedCodepoints)
		{
			if (isWhitespace(codepoint)) codepoint = 0x20;
		}

		std::wstring completeText;
		for (const auto codepoint : sanitizedCodepoints) appendUtf16(completeText, codepoint);

		bool requiresRightToLeftLayout = false;
		std::vector<WORD> characterTypes(completeText.size());
		if (!completeText.empty() && GetStringTypeW(CT_CTYPE2, completeText.data(),
			static_cast<int>(completeText.size()), characterTypes.data()))
		{
			requiresRightToLeftLayout = std::ranges::any_of(characterTypes,
				[](const WORD type) { return type == C2_RIGHTTOLEFT; });
		}

		const auto appendRunToken = [](std::string& output, const std::uint32_t runId)
		{
			output.push_back('^');
			output.push_back(UNICODE_RUN_ESCAPE);
			output.append(std::format("{:08X}", runId));
		};
		const auto appendGlyphToken = [](std::string& output, const std::uint32_t codepoint)
		{
			output.push_back('^');
			output.push_back(UNICODE_GLYPH_ESCAPE);
			output.append(std::format("{:06X}", codepoint));
		};

		if (requiresRightToLeftLayout)
		{
			std::string result;
			appendRunToken(result, RegisterUnicodeRun(completeText, sanitizedCodepoints.size()));
			return result;
		}

		const auto isGraphemeExtend = [&](const std::uint32_t codepoint)
		{
			if (codepoint == 0x200C || codepoint == 0x200D
				|| (codepoint >= 0xFE00 && codepoint <= 0xFE0F)
				|| (codepoint >= 0x1F3FB && codepoint <= 0x1F3FF)
				|| (codepoint >= 0xE0020 && codepoint <= 0xE007F)
				|| (codepoint >= 0xE0100 && codepoint <= 0xE01EF))
			{
				return true;
			}

			std::wstring utf16;
			appendUtf16(utf16, codepoint);
			WORD types[2]{};
			if (!GetStringTypeW(CT_CTYPE3, utf16.data(), static_cast<int>(utf16.size()), types))
			{
				return false;
			}

			for (std::size_t index = 0; index < utf16.size(); ++index)
			{
				if (types[index] & (C3_NONSPACING | C3_DIACRITIC | C3_VOWELMARK)) return true;
			}
			return false;
		};

		const auto getClusterEnd = [&](const std::size_t start)
		{
			auto end = start + 1;
			while (end < sanitizedCodepoints.size())
			{
				if (isGraphemeExtend(sanitizedCodepoints[end])
					|| sanitizedCodepoints[end - 1] == 0x200D)
				{
					++end;
					continue;
				}
				break;
			}
			return end;
		};

		const auto clusterToWindows1252 = [&](const std::size_t start, const std::size_t end,
			std::string& convertedText)
		{
			convertedText.clear();
			for (auto index = start; index < end; ++index)
			{
				char converted{};
				if (!convertToWindows1252(sanitizedCodepoints[index], converted)) return false;
				convertedText.push_back(converted);
			}
			return true;
		};

		std::string result;
		result.reserve(std::min(text.size(), effectiveMaxCharacters) * 2);
		for (std::size_t start = 0; start < sanitizedCodepoints.size();)
		{
			const auto clusterEnd = getClusterEnd(start);
			std::string windows1252Text;
			if (clusterToWindows1252(start, clusterEnd, windows1252Text))
			{
				result.append(windows1252Text);
				start = clusterEnd;
				continue;
			}
			if (clusterEnd == start + 1 && sanitizedCodepoints[start] <= 0xFFFF)
			{
				appendGlyphToken(result, sanitizedCodepoints[start]);
				start = clusterEnd;
				continue;
			}

			std::wstring runText;
			auto runEnd = clusterEnd;
			for (auto index = start; index < runEnd; ++index)
			{
				appendUtf16(runText, sanitizedCodepoints[index]);
			}

			// Keep adjacent unsupported clusters together so DirectWrite can shape
			// scripts and emoji sequences, without replacing native game-font text.
			while (runEnd < sanitizedCodepoints.size())
			{
				const auto nextClusterEnd = getClusterEnd(runEnd);
				if (clusterToWindows1252(runEnd, nextClusterEnd, windows1252Text)) break;
				for (auto index = runEnd; index < nextClusterEnd; ++index)
				{
					appendUtf16(runText, sanitizedCodepoints[index]);
				}
				runEnd = nextClusterEnd;
			}

			appendRunToken(result, RegisterUnicodeRun(runText, runEnd - start));
			start = runEnd;
		}

		return result;
	}

	void TextRenderer::StripMaterialTextIcons(const char* in, char* out, std::size_t max)
	{
		if (!in || !out) return;

		--max;
		std::size_t current = 0;
		while (*in != 0 && current < max)
		{
			if (*in == '^' && (in[1] == '\x01' || in[1] == '\x02'))
			{
				in += 2;

				if (*in) // width
				{
					++in;
				}

				if (*in) // height
				{
					++in;
				}

				if (*in) // material name length + material name characters
				{
					const auto materialNameLength = *in;
					++in;
					for (auto i = 0; i < materialNameLength; i++)
					{
						if (*in)
						{
							++in;
						}
					}
				}
			}
			else
			{
				*out = *in;
				++out;
				++current;
				++in;
			}

		}

		*out = '\0';
	}

	std::string TextRenderer::StripMaterialTextIcons(const std::string& in)
	{
		char buffer[1000]{}; // Should be more than enough
		StripMaterialTextIcons(in.data(), buffer, sizeof(buffer));
		return std::string{ buffer };
	}

	void TextRenderer::StripAllTextIcons(const char* in, char* out, std::size_t max)
	{
		if (!in || !out) return;

		--max;
		std::size_t current = 0;
		while (*in != 0 && current < max)
		{
			if (*in == '^' && (in[1] == '\x01' || in[1] == '\x02'))
			{
				in += 2;

				if (*in) // width
				{
					++in;
				}

				if (*in) // height
				{
					++in;
				}

				if (*in) // material name length + material name characters
				{
					const auto materialNameLength = *in;
					++in;
					for (auto i = 0; i < materialNameLength; i++)
					{
						if (*in)
						{
							++in;
						}
					}
				}

				continue;
			}

			if (*in == FONT_ICON_SEPARATOR_CHARACTER)
			{
				const auto* fontIconEndPos = &in[1];
				FontIconInfo fontIcon{};
				if(IsFontIcon(fontIconEndPos, fontIcon))
				{
					in = fontIconEndPos;
					continue;
				}
			}

			*out = *in;
			++out;
			++current;
			++in;
		}

		*out = '\0';
	}

	std::string TextRenderer::StripAllTextIcons(const std::string& in)
	{
		char buffer[1000]{}; // Should be more than enough
		StripAllTextIcons(in.data(), buffer, sizeof(buffer));
		return std::string{ buffer };
	}

	int TextRenderer::SEH_PrintStrlenWithCursor(const char* string, const Game::field_t* field)
	{
		if (!string)
		{
			return 0;
		}

		const auto cursorPos = field->cursor;
		auto len = 0;
		auto lenWithInvisibleTail = 0;
		auto count = 0;
		const auto* curText = string;
		while(*curText)
		{
			const auto c = Game::SEH_ReadCharFromString(&curText, nullptr);
			lenWithInvisibleTail = len;
			if (c == '^')
			{
				const char* unicodeEnd = curText;
				std::uint32_t value{};
				if (ParseUnicodeGlyphEscape(unicodeEnd, value))
				{
					curText = unicodeEnd;
					++len;
					count += static_cast<int>(UNICODE_GLYPH_HEX_LENGTH + 2);
					lenWithInvisibleTail = len;
					continue;
				}

				unicodeEnd = curText;
				if (ParseUnicodeRunEscape(unicodeEnd, value))
				{
					curText = unicodeEnd;
					len += static_cast<int>(GetUnicodeRunCharacterCount(value));
					count += static_cast<int>(UNICODE_RUN_ID_HEX_LENGTH + 2);
					lenWithInvisibleTail = len;
					continue;
				}
			}

			if (c == '^' && *curText >= COLOR_FIRST_CHAR && *curText <= COLOR_LAST_CHAR && !(cursorPos > count && cursorPos < count + 2))
			{
				++curText;
				++count;
			}
			else if(c != '\r' && c != '\n')
			{
				++len;
			}

			++count;
			++lenWithInvisibleTail;
		}

		return lenWithInvisibleTail;
	}

	__declspec(naked) void TextRenderer::Field_AdjustScroll_PrintLen_Stub()
	{
		__asm
		{
			push eax
			pushad

			push esi
			push [esp + 0x8 + 0x24]
			call SEH_PrintStrlenWithCursor
			add esp, 0x8
			mov [esp + 0x20], eax

			popad
			pop eax
			ret
		}
	}

	void TextRenderer::PatchColorLimit(const char limit)
	{
		Utils::Hook::Set<char>(0x535629, limit); // DrawText2d
		Utils::Hook::Set<char>(0x4C1BE4, limit); // SEH_PrintStrlen
		Utils::Hook::Set<char>(0x4863DD, limit); // No idea
		Utils::Hook::Set<char>(0x486429, limit); // No idea
		Utils::Hook::Set<char>(0x49A5A8, limit); // No idea
		Utils::Hook::Set<char>(0x505721, limit); // R_TextWidth
		Utils::Hook::Set<char>(0x505801, limit); // No idea
		Utils::Hook::Set<char>(0x50597F, limit); // No idea
		Utils::Hook::Set<char>(0x5815DB, limit); // No idea
		Utils::Hook::Set<char>(0x592ED0, limit); // No idea
		Utils::Hook::Set<char>(0x5A2E2E, limit); // No idea

		Utils::Hook::Set<char>(0x5A2733, static_cast<char>(ColorIndexForChar(limit))); // No idea
	}

	// Patches team overhead normally
	bool TextRenderer::Dvar_GetUnpackedColorByName(const char* name, float* expandedColor)
	{
		if (r_colorBlind.get<bool>())
		{
			if (std::strcmp(name, "g_TeamColor_EnemyTeam") == 0)
			{
				// Dvar_GetUnpackedColor
				const auto* colorblindEnemy = g_ColorBlind_EnemyTeam->current.color;
				expandedColor[0] = static_cast<float>(colorblindEnemy[0]) / 255.0f;
				expandedColor[1] = static_cast<float>(colorblindEnemy[1]) / 255.0f;
				expandedColor[2] = static_cast<float>(colorblindEnemy[2]) / 255.0f;
				expandedColor[3] = static_cast<float>(colorblindEnemy[3]) / 255.0f;
				return false;
			}

			if (std::strcmp(name, "g_TeamColor_MyTeam") == 0)
			{
				// Dvar_GetUnpackedColor
				const auto* colorblindAlly = g_ColorBlind_MyTeam->current.color;
				expandedColor[0] = static_cast<float>(colorblindAlly[0]) / 255.0f;
				expandedColor[1] = static_cast<float>(colorblindAlly[1]) / 255.0f;
				expandedColor[2] = static_cast<float>(colorblindAlly[2]) / 255.0f;
				expandedColor[3] = static_cast<float>(colorblindAlly[3]) / 255.0f;
				return false;
			}
		}

		return true;
	}

	__declspec(naked) void TextRenderer::GetUnpackedColorByNameStub()
	{
		__asm
		{
			push [esp + 8h]
			push [esp + 8h]
			call TextRenderer::Dvar_GetUnpackedColorByName
			add esp, 8h

			test al, al
			jnz continue

			retn

		continue:
			push edi
			mov edi, [esp + 8h]
			push 406535h
			retn
		}
	}

	void TextRenderer::UpdateColorTable()
	{
		if (cg_newColors.get<bool>())
			currentColorTable = &colorTableNew;
		else
			currentColorTable = &colorTableDefault;

		(*currentColorTable)[TEXT_COLOR_AXIS] = *reinterpret_cast<unsigned*>(0x66E5F70);
		(*currentColorTable)[TEXT_COLOR_ALLIES] = *reinterpret_cast<unsigned*>(0x66E5F74);
		(*currentColorTable)[TEXT_COLOR_RAINBOW] = HsvToRgb({ static_cast<uint8_t>((Game::Sys_Milliseconds() / 200) % 256), 255,255 });
		(*currentColorTable)[TEXT_COLOR_SERVER] = sv_customTextColor->current.unsignedInt;
	}

	void TextRenderer::InitFontIconStrings()
	{
		stringHintAutoComplete.Format("TAB");
		stringHintModifier.Format(Utils::String::VA("%c", FONT_ICON_MODIFIER_SEPARATOR_CHARACTER));
		stringListHeader.Cache();
		stringListFlipHorizontal.Format(Utils::String::VA("%c", FONT_ICON_MODIFIER_FLIP_HORIZONTALLY));
		stringListFlipVertical.Format(Utils::String::VA("%c", FONT_ICON_MODIFIER_FLIP_VERTICALLY));
		stringListBig.Format(Utils::String::VA("%c", FONT_ICON_MODIFIER_BIG));
	}

	void TextRenderer::InitFontIcons()
	{
		InitFontIconStrings();

		fontIconList.clear();
		fontIconLookup.clear();

		const auto fontIconTable = Game::DB_FindXAssetHeader(Game::ASSET_TYPE_STRINGTABLE, "mp/fonticons.csv").stringTable;

		if (fontIconTable->columnCount < 2 || fontIconTable->rowCount <= 0)
		{
			Logger::Error(Game::ERR_FATAL, "\x15" "Failed to load mp/fonticons.csv");
			return;
		}

		fontIconList.reserve(fontIconTable->rowCount);
		for (auto rowIndex = 0; rowIndex < fontIconTable->rowCount; rowIndex++)
		{
			const auto* columns = &fontIconTable->values[rowIndex * fontIconTable->columnCount];

			if(columns[0].string == nullptr || columns[1].string == nullptr)
				continue;

			if (columns[0].string[0] == '\0' || columns[1].string[1] == '\0')
				continue;

			if (columns[0].string[0] == '#')
				continue;

			FontIconTableEntry entry
			{
				columns[0].string,
				columns[1].string,
				nullptr
			};

			fontIconList.emplace_back(entry);
			fontIconLookup.emplace(std::make_pair(entry.iconName, entry));
		}

		std::ranges::sort(fontIconList, [](const FontIconTableEntry& a, const FontIconTableEntry& b) -> bool
		{
			return a.iconName < b.iconName;
		});
	}

	TextRenderer::TextRenderer()
	{
		currentColorTable = &colorTableDefault;

		cg_newColors = Dvar::Register<bool>("cg_newColors", true, Game::DVAR_ARCHIVE, "Use Warfare 2 color code style.");
		cg_fontIconAutocomplete = Dvar::Register<bool>("cg_fontIconAutocomplete", true, Game::DVAR_ARCHIVE, "Show autocomplete for fonticons when typing.");
		cg_fontIconAutocompleteHint = Dvar::Register<bool>("cg_fontIconAutocompleteHint", true, Game::DVAR_ARCHIVE, "Show hint text in autocomplete for fonticons.");
		sv_customTextColor = Game::Dvar_RegisterColor("sv_customTextColor", 1, 0.7f, 0, 1, Game::DVAR_CODINFO, "Color for the extended color code.");

		// Initialize font icons when initializing UI
		Components::Events::AfterUIInit(InitFontIcons);
		Renderer::OnBackendFrame(BuildPendingUnicodeGlyphs);
		Renderer::OnBackendFrame(BuildPendingUnicodeRuns);
		Renderer::OnDeviceRecoveryBegin([]
		{
			std::lock_guard lock(UnicodeRunMutex);
			UnicodeGlyphCache.clear();
			PendingUnicodeGlyphs.clear();
			UnicodeRunCache.clear();
			PendingUnicodeRuns.clear();
		});

		// Replace vanilla text drawing function with a reimplementation with extensions
		Utils::Hook(0x535410, DrawText2D, HOOK_JUMP).install()->quick();

		// Consider material text icons and font icons when calculating text width
		Utils::Hook(0x5056C0, R_TextWidth_Hk, HOOK_JUMP).install()->quick();

		// Patch ColorIndex
		Utils::Hook(0x417770, ColorIndex, HOOK_JUMP).install()->quick();

		// Add a colorblind mode for team colors
		r_colorBlind = Dvar::Register<bool>("r_colorBlind", false, Game::DVAR_ARCHIVE, "Use color-blindness-friendly colors");
		// A dark red
		g_ColorBlind_EnemyTeam = Game::Dvar_RegisterColor("g_ColorBlind_EnemyTeam", 0.659f, 0.088f, 0.145f, 1, Game::DVAR_ARCHIVE, "Enemy team color for colorblind mode");
		// A bright yellow
		g_ColorBlind_MyTeam = Game::Dvar_RegisterColor("g_ColorBlind_MyTeam", 1, 0.859f, 0.125f, 1, Game::DVAR_ARCHIVE, "Ally team color for colorblind mode");

		// Replace team colors with colorblind team colors when colorblind is enabled
		Utils::Hook(0x406530, GetUnpackedColorByNameStub, HOOK_JUMP).install()->quick();

		// Consider the cursor being inside the color escape sequence when getting the print length for a field
		Utils::Hook(0x488CBD, Field_AdjustScroll_PrintLen_Stub, HOOK_CALL).install()->quick();

		// Draw fonticon autocompletion for say field
		Utils::Hook(0x4CA1BD, Field_Draw_Say, HOOK_CALL).install()->quick();

		// Draw fonticon autocompletion for console field
		Utils::Hook(0x5A50A5, Con_DrawInput_Hk, HOOK_CALL).install()->quick();
		Utils::Hook(0x5A50BB, Con_DrawInput_Hk, HOOK_CALL).install()->quick();

		// Handle key inputs for console and chat
		Utils::Hook(0x4F685C, Console_Key_Hk, HOOK_CALL).install()->quick();
		Utils::Hook(0x4F6694, Message_Key_Stub, HOOK_CALL).install()->quick();
		Utils::Hook(0x4F684C, Message_Key_Stub, HOOK_CALL).install()->quick();

		PatchColorLimit(COLOR_LAST_CHAR);
	}
}
