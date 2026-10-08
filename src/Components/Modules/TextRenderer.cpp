#include "STDInclude.hpp"

#include <dwrite.h>
#include <wrl/client.h>

#pragma comment(lib, "dwrite.lib")

#include "TextRenderer.hpp"
#include "Dvar.hpp"
#include "Events.hpp"
#include "Logger.hpp"
#include "Materials.hpp"
#include "Renderer.hpp"
#include "Scheduler.hpp"

namespace Components
{
	unsigned int TextRenderer::colorTableDefault[TEXT_COLOR_COUNT]
	{
		ColorRgb(0, 0, 0),
		ColorRgb(255, 92, 92),
		ColorRgb(0, 255, 0),
		ColorRgb(255, 255, 0),
		ColorRgb(0, 0, 255),
		ColorRgb(0, 255, 255),
		ColorRgb(255, 92, 255),
		ColorRgb(255, 255, 255),
		ColorRgb(255, 255, 255),
		ColorRgb(255, 255, 255),
		ColorRgb(255, 255, 255),
		ColorRgb(255, 255, 255),

		ColorRgb(200, 75, 200),
		ColorRgb(255, 240, 20),
		ColorRgb(128, 0, 128),
		ColorRgb(20, 180, 180),
		ColorRgb(255, 255, 255),
		ColorRgb(60, 75, 35),
		ColorRgb(93, 23, 255),
		ColorRgb(255, 0, 0),
		ColorRgb(0, 255, 0),
		ColorRgb(0, 0, 255),
		ColorRgb(128, 0, 0),
		ColorRgb(255, 105, 180),
		ColorRgb(170, 240, 209),
		ColorRgb(255, 213, 165),
		ColorRgb(187, 231, 151),
		ColorRgb(255, 120, 120),
	};

	unsigned int TextRenderer::colorTableNew[TEXT_COLOR_COUNT]
	{
		ColorRgb(0, 0, 0),
		ColorRgb(255, 49, 49),
		ColorRgb(134, 192, 0),
		ColorRgb(255, 173, 34),
		ColorRgb(0, 135, 193),
		ColorRgb(32, 197, 255),
		ColorRgb(151, 80, 221),
		ColorRgb(255, 255, 255),
		ColorRgb(255, 255, 255),
		ColorRgb(255, 255, 255),
		ColorRgb(255, 255, 255),
		ColorRgb(255, 255, 255),

		ColorRgb(200, 75, 200),
		ColorRgb(255, 240, 20),
		ColorRgb(128, 0, 128),
		ColorRgb(20, 180, 180),
		ColorRgb(255, 255, 255),
		ColorRgb(60, 75, 35),
		ColorRgb(93, 23, 255),
		ColorRgb(255, 0, 0),
		ColorRgb(0, 255, 0),
		ColorRgb(0, 0, 255),
		ColorRgb(128, 0, 0),
		ColorRgb(255, 105, 180),
		ColorRgb(170, 240, 209),
		ColorRgb(255, 213, 165),
		ColorRgb(187, 231, 151),
		ColorRgb(255, 120, 120),
	};

	unsigned int(*TextRenderer::currentColorTable)[TEXT_COLOR_COUNT] = &TextRenderer::colorTableNew;

	Game::dvar_t* TextRenderer::cg_newColors = nullptr;
	Game::dvar_t* TextRenderer::sv_customTextColor = nullptr;
	Game::dvar_t* TextRenderer::r_colorBlind = nullptr;
	Game::dvar_t* TextRenderer::g_ColorBlind_EnemyTeam = nullptr;
	Game::dvar_t* TextRenderer::g_ColorBlind_MyTeam = nullptr;

	constexpr std::uintptr_t ColorIndex_Entry = 0x14028BCC0;

	static const std::uint8_t colorIndexEntry[] = { 0x80, 0xE9, 0x30, 0xB8, 0x07, 0x00, 0x00, 0x00 };

	constexpr std::uintptr_t DrawText2D_ColorLookup = 0x14004E4BF;
	constexpr std::uintptr_t DrawText2D_ColorIndexCall = 0x14004E4C2;
	constexpr std::uintptr_t DrawText2D_AfterColorIndex = 0x14004E4C7;
	constexpr std::uint16_t jumpToColorApplied = 0x37EB;

	static const std::uint8_t colorLookup[] =
	{
		0x0F, 0xB6, 0xCB,
		0xE8, 0xF9, 0xD7, 0x23, 0x00,
		0x0F, 0xB6, 0xC0,
		0x83, 0xF8, 0x08,
		0x73, 0x10,
	};

	constexpr std::uintptr_t teamColorAxis = 0x148CCA440;
	constexpr std::uintptr_t teamColorAllies = 0x148CCA444;

	struct ColorLimitSite
	{
		std::uintptr_t instruction;
		std::uint8_t bytes[3];
		std::size_t length;
	};

	static const ColorLimitSite colorLimitSites[] =
	{
		{ 0x14004E3FC, { 0x80, 0xFA, 0x09 }, 3 },
		{ 0x14024F16F, { 0x3C, 0x09 }, 2 },
		{ 0x14026B006, { 0x3C, 0x09 }, 2 },
		{ 0x14026B07F, { 0x3C, 0x09 }, 2 },
		{ 0x14001AA9E, { 0x3C, 0x09 }, 2 },
		{ 0x14001AC0C, { 0x3C, 0x09 }, 2 },
		{ 0x14001AD8D, { 0x3C, 0x09 }, 2 },
		{ 0x1400E5D12, { 0x80, 0xF9, 0x09 }, 3 },
		{ 0x1400EE550, { 0x80, 0xF9, 0x09 }, 3 },
		{ 0x1400EBB70, { 0x80, 0xF9, 0x09 }, 3 },
	};

	constexpr std::uintptr_t Dvar_GetUnpackedColorByNameCalls[] =
	{
		0x1400A7DA1,
		0x1400A7E18,
		0x1400A8D91,
		0x1400A8DC6,
		0x1400F5A5C,
		0x1400F5A79,
	};

	static const std::uint8_t unpackedColorCallBytes[][5] =
	{
		{ 0xE8, 0x4A, 0xD5, 0x1D, 0x00 },
		{ 0xE8, 0xD3, 0xD4, 0x1D, 0x00 },
		{ 0xE8, 0x5A, 0xC5, 0x1D, 0x00 },
		{ 0xE8, 0x25, 0xC5, 0x1D, 0x00 },
		{ 0xE8, 0x8F, 0xF8, 0x18, 0x00 },
		{ 0xE8, 0x72, 0xF8, 0x18, 0x00 },
	};

	constexpr std::uintptr_t Dvar_RegisterColor = 0x140285D50;

	constexpr std::uintptr_t Sys_Milliseconds = 0x1402A8620;

	static Utils::Hook colorIndexHook;
	static Utils::Hook colorLookupHook;
	static Utils::Hook unpackedColorHooks[std::size(Dvar_GetUnpackedColorByNameCalls)];

	std::map<std::string, TextRenderer::FontIcon> TextRenderer::fontIcons;
	std::mutex TextRenderer::fontIconsMutex;
	std::atomic<bool> TextRenderer::areFontIconsReady = false;

	TextRenderer::FontIconAutocompleteContext TextRenderer::autocompleteContextArray[FONT_ICON_ACI_COUNT];

	TextRenderer::BufferedLocalizedString TextRenderer::stringHintAutoComplete("FONT_ICON_HINT_AUTO_COMPLETE");
	TextRenderer::BufferedLocalizedString TextRenderer::stringHintModifier("FONT_ICON_HINT_MODIFIER");
	TextRenderer::BufferedLocalizedString TextRenderer::stringListHeader("FONT_ICON_MODIFIER_LIST_HEADER");
	TextRenderer::BufferedLocalizedString TextRenderer::stringListFlipHorizontal("FONT_ICON_MODIFIER_LIST_FLIP_HORIZONTAL");
	TextRenderer::BufferedLocalizedString TextRenderer::stringListFlipVertical("FONT_ICON_MODIFIER_LIST_FLIP_VERTICAL");
	TextRenderer::BufferedLocalizedString TextRenderer::stringListBig("FONT_ICON_MODIFIER_LIST_BIG");

	Game::dvar_t* TextRenderer::cg_fontIconAutocomplete = nullptr;
	Game::dvar_t* TextRenderer::cg_fontIconAutocompleteHint = nullptr;

	constexpr char fontIconSeparator = ':';
	constexpr char fontIconModifierSeparator = '+';
	constexpr char fontIconFlipHorizontally = 'h';
	constexpr char fontIconFlipVertically = 'v';
	constexpr char fontIconBig = 'b';
	constexpr float fontIconBigScale = 1.5f;

	constexpr char inlineIconEscape = '^';
	constexpr char inlineIcon = 1;
	constexpr char inlineIconFlipped = 2;
	constexpr int inlineIconUnit = 32;
	constexpr int inlineIconBias = 16;
	constexpr int inlineIconLargest = 127;

	constexpr std::size_t hudIconNameLength = 4;

	constexpr unsigned int assetTypeMaterial = 5;

	constexpr std::uintptr_t DB_FindXAssetEntry = 0x14012D600;
	constexpr std::size_t assetEntryHeader = 8;

	constexpr std::size_t materialAtlasRows = 0x0A;
	constexpr std::size_t materialAtlasColumns = 0x0B;
	constexpr std::size_t materialTextureCount = 80;
	constexpr std::size_t materialTechniqueSet = 88;
	constexpr std::size_t materialTextureTable = 96;
	constexpr std::size_t textureDefSize = 16;
	constexpr std::size_t textureDefImage = 8;
	constexpr unsigned int colorMapHash = 0xA0AB1041;

	constexpr std::size_t imageWidth = 0x18;
	constexpr std::size_t imageHeight = 0x1A;

	constexpr std::size_t fontPixelHeight = 0x08;
	constexpr std::size_t fontGlyphCount = 0x0C;
	constexpr std::size_t fontGlyphs = 0x20;
	constexpr std::size_t glyphSize = 24;
	constexpr std::size_t glyphDx = 4;

	constexpr int glyphDirectCount = 0x60;
	constexpr int glyphFallback = 14;

	constexpr std::uintptr_t SEH_ReadCharFromString = 0x14024F1A0;

	constexpr std::uintptr_t R_TextWidth_Entry = 0x14001AA20;

	static const std::uint8_t textWidthEntry[] = { 0x48, 0x89, 0x4C, 0x24, 0x08, 0x55, 0x56, 0x41, 0x54 };

	constexpr std::uintptr_t DrawText2DCall = 0x14004F8C2;
	constexpr int textRenderFlagCursor = 0x2;
	constexpr int textRenderFlagDropShadow = 0x4;
	constexpr int textRenderFlagDropShadowExtra = 0x8;
	constexpr int textRenderFlagGlow = 0x10;
	constexpr int textRenderFlagGlowForceColor = 0x20;
	constexpr int textRenderFlagFxDecode = 0x40;
	constexpr int textRenderFlagPadding = 0x80;
	constexpr int textRenderFlagSubtitle = 0x100;
	constexpr int textRenderFlagOutline = 0x400;
	constexpr int textRenderFlagOutlineExtra = 0x800;
	constexpr int textRenderFlagForceMonospace = 0x1;

	constexpr std::uintptr_t glyphQuadDraw = 0x1400481C0;
	constexpr std::size_t inlineIconBytes = 12;
	constexpr float styleOffsets[4][2] = { { -1.0f, -1.0f }, { -1.0f, 1.0f }, { 1.0f, -1.0f }, { 1.0f, 1.0f } };

	static const std::uint8_t drawText2DCall[] = { 0xE8, 0x99, 0xE7, 0xFF, 0xFF };

	static Utils::Hook drawText2DHook;
	static Utils::Hook textWidthHook;

	constexpr std::uintptr_t DB_UnloadXZone = 0x14012FDA0;
	constexpr std::uintptr_t DB_UnloadXZoneCalls[] = { 0x14012EDC1, 0x14012F83B };

	static Utils::Hook unloadHooks[std::size(DB_UnloadXZoneCalls)];

	struct TextRenderer::field_t
	{
		int cursor;
		int scroll;
		int drawWidth;
		int widthInPixels;
		float charHeight;
		int fixedSize;
		char buffer[256];
	};

	AssertSize(TextRenderer::field_t, 0x118);
	AssertOffset(TextRenderer::field_t, charHeight, 0x10);
	AssertOffset(TextRenderer::field_t, buffer, 0x18);

	constexpr std::uintptr_t playerKeys = 0x1406C70A0;
	constexpr std::size_t playerKeysStride = 3368;

	struct ConversionArguments
	{
		int argCount;
		const char* args[9];
	};

	AssertOffset(ConversionArguments, args, 0x08);

	constexpr std::uintptr_t UI_SafeTranslateString = 0x140272770;
	constexpr std::uintptr_t UI_ReplaceConversions = 0x140271C90;

	constexpr std::uintptr_t R_AddCmdDrawStretchPic = 0x14001BE90;

	constexpr std::uintptr_t UI_GetFontHandle = 0x14026EE10;

	constexpr std::uintptr_t R_NormalizedTextScale = 0x14001AA00;

	constexpr std::uintptr_t ScrPlace_ApplyRect = 0x1400F23B0;

	constexpr std::uintptr_t ScrPlace_GetViewPlacement = 0x1400F2BD0;

	constexpr std::uintptr_t Field_AdjustScroll = 0x1400EEB40;

	constexpr std::uintptr_t con_inputBoxColor = 0x1406C7030;

	constexpr std::uintptr_t cls_whiteMaterial = 0x140C5CF08;

	constexpr std::uintptr_t sharedUiInfo_scrollBarArrowUp = 0x1465D0A40;
	constexpr std::uintptr_t sharedUiInfo_scrollBarArrowDown = 0x1465D0A48;

	constexpr std::uintptr_t Con_DrawSay_Field_DrawCall = 0x1400ED087;

	static const std::uint8_t fieldDrawCall[] = { 0xE8, 0x34, 0x1E, 0x00, 0x00 };

	constexpr std::uintptr_t CL_KeyEvent_Message_KeyCalls[] =
	{
		0x1400EE973,
		0x1400EEADB,
	};

	static const std::uint8_t messageKeyCallBytes[][5] =
	{
		{ 0xE8, 0x18, 0x15, 0x00, 0x00 },
		{ 0xE8, 0xB0, 0x13, 0x00, 0x00 },
	};

	constexpr std::uintptr_t Field_AdjustScroll_SEH_PrintStrlenCall = 0x1400EEBD6;
	constexpr std::uintptr_t Field_AdjustScroll_BufferArgument = 0x1400EEBCB;

	static const std::uint8_t printStrlenCall[] =
	{
		0x48, 0x8D, 0x4B, 0x18,
		0xC7, 0x43, 0x04, 0x00, 0x00, 0x00, 0x00,
		0xE8, 0x65, 0x04, 0x16, 0x00,
	};

	constexpr int keyTab = 9;
	constexpr int keyEnter = 13;
	constexpr int keyEscape = 27;
	constexpr int keyUpArrow = 154;
	constexpr int keyDownArrow = 155;
	constexpr int keyPadUpArrow = 183;
	constexpr int keyPadDownArrow = 189;
	constexpr int keyPadEnter = 191;

	constexpr float autocompleteBoxPadding = 6.0f;
	constexpr float autocompleteBoxBorder = 2.0f;
	constexpr float autocompleteColumnSpacing = 12.0f;
	constexpr float autocompleteArrowSize = 12.0f;
	constexpr float autocompleteWhite[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	constexpr float autocompleteTextColor[4] = { 1.0f, 1.0f, 0.8f, 1.0f };
	constexpr float autocompleteHintColor[4] = { 0.6f, 0.6f, 0.6f, 1.0f };

	constexpr char colorFirstChar = '0';
	constexpr char colorLastChar = static_cast<char>('0' + TEXT_COLOR_COUNT - 1);

	static Utils::Hook fieldDrawSayHook;
	static Utils::Hook messageKeyHooks[std::size(CL_KeyEvent_Message_KeyCalls)];
	static Utils::Hook printLenHook;

	constexpr std::size_t encodedCharacterLimit = 1024 / 8;

	static bool IsDroppedCodepoint(const std::uint32_t codepoint)
	{
		return codepoint < 0x20
			|| (codepoint >= 0x7F && codepoint <= 0x9F)
			|| (codepoint >= 0x202A && codepoint <= 0x202E)
			|| (codepoint >= 0x2066 && codepoint <= 0x2069);
	}

	static bool IsUnicodeWhitespace(const std::uint32_t codepoint)
	{
		return codepoint == 0x20 || codepoint == 0xA0 || codepoint == 0x1680
			|| (codepoint >= 0x2000 && codepoint <= 0x200A) || codepoint == 0x2028
			|| codepoint == 0x2029 || codepoint == 0x202F || codepoint == 0x205F
			|| codepoint == 0x3000;
	}

	static void AppendUtf16(std::wstring& output, const std::uint32_t codepoint)
	{
		if (codepoint <= 0xFFFF)
		{
			output.push_back(static_cast<wchar_t>(codepoint));
			return;
		}

		const auto value = codepoint - 0x10000;
		output.push_back(static_cast<wchar_t>(0xD800 + (value >> 10)));
		output.push_back(static_cast<wchar_t>(0xDC00 + (value & 0x3FF)));
	}

	static bool IsGraphemeExtend(const std::uint32_t codepoint)
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
		AppendUtf16(utf16, codepoint);

		WORD types[2]{};

		if (!GetStringTypeW(CT_CTYPE3, utf16.data(), static_cast<int>(utf16.size()), types))
		{
			return false;
		}

		for (std::size_t i = 0; i < utf16.size(); ++i)
		{
			if (types[i] & (C3_NONSPACING | C3_DIACRITIC | C3_VOWELMARK))
			{
				return true;
			}
		}

		return false;
	}

	static bool TryConvertToGameGlyph(const std::uint32_t codepoint, char& converted)
	{
		// Non-ASCII glyph coverage differs between engine fonts. Use the Unicode renderer.
		if (codepoint < 0x20 || codepoint > 0x7E) return false;
		converted = static_cast<char>(codepoint);
		return true;
	}

	static std::size_t ClusterEnd(const std::vector<std::uint32_t>& codepoints, const std::size_t start)
	{
		auto end = start + 1;

		while (end < codepoints.size() && (IsGraphemeExtend(codepoints[end]) || codepoints[end - 1] == 0x200D))
		{
			++end;
		}

		return end;
	}

	static bool TryConvertCluster(const std::vector<std::uint32_t>& codepoints, const std::size_t start, const std::size_t end, std::string& converted)
	{
		converted.clear();

		for (auto i = start; i < end; ++i)
		{
			char character{};

			if (!TryConvertToGameGlyph(codepoints[i], character))
			{
				return false;
			}

			converted.push_back(character);
		}

		return true;
	}

	static int FontPixelHeight(const void* font)
	{
		return *reinterpret_cast<const int*>(static_cast<const std::uint8_t*>(font) + fontPixelHeight);
	}

	static void* FindMaterial(const char* name)
	{
		auto* const entry = reinterpret_cast<std::uint8_t*(*)(unsigned int, const char*)>(
			Utils::Hook::Rebase(DB_FindXAssetEntry))(assetTypeMaterial, name);

		if (!entry)
		{
			return nullptr;
		}

		return *reinterpret_cast<void**>(entry + assetEntryHeader);
	}

	static float ColorMapAspect(const void* material)
	{
		const auto* const bytes = static_cast<const std::uint8_t*>(material);

		if (bytes[materialAtlasRows] > 1 || bytes[materialAtlasColumns] > 1)
		{
			return 0.0f;
		}

		const auto* const table = *reinterpret_cast<const std::uint8_t* const*>(bytes + materialTextureTable);

		for (unsigned int i = 0; i < bytes[materialTextureCount]; ++i)
		{
			const auto* const textureDef = table + i * textureDefSize;

			if (*reinterpret_cast<const unsigned int*>(textureDef) != colorMapHash)
			{
				continue;
			}

			const auto* const image = *reinterpret_cast<const std::uint8_t* const*>(textureDef + textureDefImage);

			if (!image)
			{
				return 0.0f;
			}

			const auto width = *reinterpret_cast<const std::uint16_t*>(image + imageWidth);
			const auto height = *reinterpret_cast<const std::uint16_t*>(image + imageHeight);

			if (!width || !height)
			{
				return 0.0f;
			}

			return static_cast<float>(width) / static_cast<float>(height);
		}

		return 0.0f;
	}

	static int InlineIconSize(int pixelHeight, char size)
	{
		return (pixelHeight * (size - inlineIconBias) + inlineIconBias) / inlineIconUnit;
	}

	static void AppendInlineIcon(std::string& out, void* material, float aspect, bool isFlippedHorizontally, bool isBig)
	{
		const float scale = isBig ? fontIconBigScale : 1.0f;
		const int height = inlineIconBias + static_cast<int>(std::lround(inlineIconUnit * scale));
		const int width = std::clamp(inlineIconBias + static_cast<int>(std::lround(inlineIconUnit * aspect * scale)), inlineIconBias + 1, inlineIconLargest);

		out.push_back(inlineIconEscape);

		if (isFlippedHorizontally)
		{
			out.push_back(inlineIconFlipped);
		}
		else
		{
			out.push_back(inlineIcon);
		}

		out.push_back(static_cast<char>(width));
		out.push_back(static_cast<char>(height));
		out.append(reinterpret_cast<const char*>(&material), sizeof(material));
	}

	static bool IsHudIcon(const char* text)
	{
		return text[0] == inlineIconEscape && (text[1] == inlineIcon || text[1] == inlineIconFlipped);
	}

	static bool HasHudIcon(const char* text)
	{
		for (const char* position = text; *position; ++position)
		{
			if (IsHudIcon(position))
			{
				return true;
			}
		}

		return false;
	}

	static const char* SkipHudIcon(const char* text)
	{
		const char* position = text + 2;

		if (*position)
		{
			++position;
		}

		if (*position)
		{
			++position;
		}

		if (*position)
		{
			const auto nameLength = static_cast<unsigned char>(*position);
			++position;

			for (unsigned int i = 0; i < nameLength && *position; ++i)
			{
				++position;
			}
		}

		return position;
	}

	static void AppendHudIcon(std::string& out, const char* text)
	{
		const char type = text[1];
		const char width = text[2];
		const char height = text[3];

		if (!width || !height || !text[hudIconNameLength])
		{
			return;
		}

		const auto nameLength = static_cast<unsigned char>(text[hudIconNameLength]);
		const char* const name = text + hudIconNameLength + 1;

		if (strnlen(name, nameLength) < nameLength)
		{
			return;
		}

		void* material = FindMaterial(std::string(name, nameLength).c_str());

		if (material)
		{
			const auto* const techniqueSet = *reinterpret_cast<const char* const* const*>(static_cast<const std::uint8_t*>(material) + materialTechniqueSet);

			if (!techniqueSet || !*techniqueSet || std::strcmp(*techniqueSet, "2d") != 0)
			{
				material = nullptr;
			}
		}

		if (!material)
		{
			material = FindMaterial("default");
		}

		if (!material)
		{
			return;
		}

		out.push_back(inlineIconEscape);
		out.push_back(type);
		out.push_back(width);
		out.push_back(height);
		out.append(reinterpret_cast<const char*>(&material), sizeof(material));
	}

	constexpr char unicodeGlyphEscape = 3;
	constexpr std::size_t unicodeGlyphDigits = 6;
	constexpr char unicodeRunEscape = 4;
	constexpr std::size_t unicodeRunDigits = 8;
	constexpr std::uint64_t unicodeRunKey = 1ull << 32;

	constexpr int unicodeEmPixels = 64;
	constexpr int unicodeUnitPixels = unicodeEmPixels / inlineIconUnit;
	constexpr int unicodeBoxUnits = 56;
	constexpr int unicodeBoxPixels = unicodeBoxUnits * unicodeUnitPixels;
	constexpr int unicodeBaselinePixels = (unicodeBoxPixels + unicodeEmPixels) / 2 - unicodeEmPixels / 8;
	constexpr int unicodePieceUnits = inlineIconLargest - inlineIconBias;

	constexpr int unicodeMaxRunPixels = 4096;
	constexpr std::size_t unicodeGlyphsPerFrame = 8;
	constexpr std::size_t unicodeRunsPerFrame = 4;

	constexpr std::size_t unicodeTextureBytes = 32 * 1024 * 1024;

	constexpr unsigned int unicodeImageFlags = 0x1000003;

	struct UnicodeText
	{
		std::vector<Game::Material*> pieces;
		std::vector<std::uint8_t> pieceUnits;
		std::size_t bytes = 0;
		std::uint64_t lastDrawnFrame = 0;
		bool isPending = false;
		bool isFailed = false;
	};

	struct UnicodeBuild
	{
		std::uint64_t key;
		std::wstring text;
		std::vector<Game::Material*> pieces;
		std::vector<std::uint8_t> pieceUnits;
		std::size_t bytes;
	};

	static std::mutex unicodeMutex;
	static std::unordered_map<std::uint32_t, std::wstring> unicodeRunTexts;
	static std::unordered_map<std::uint64_t, UnicodeText> unicodeTexts;
	static std::vector<std::uint64_t> unicodePending;
	static std::size_t unicodeResidentBytes = 0;
	static std::uint64_t unicodeFrame = 1;

	constexpr std::size_t unicodeRunLimit = 4096;

	static std::unordered_map<std::wstring, std::uint32_t> unicodeRunIds;
	static std::uint32_t nextUnicodeRunId = 1;

	static std::atomic<bool> areUnicodeEscapesDrawn = false;

	static int HexDigitValue(const char character)
	{
		if (character >= '0' && character <= '9')
		{
			return character - '0';
		}

		if (character >= 'A' && character <= 'F')
		{
			return character - 'A' + 10;
		}

		if (character >= 'a' && character <= 'f')
		{
			return character - 'a' + 10;
		}

		return -1;
	}

	static bool TryReadUnicodeEscape(const char* text, std::uint64_t& key, const char*& end)
	{
		std::size_t digitCount = 0;

		if (*text == unicodeGlyphEscape)
		{
			digitCount = unicodeGlyphDigits;
		}
		else if (*text == unicodeRunEscape)
		{
			digitCount = unicodeRunDigits;
		}
		else
		{
			return false;
		}

		std::uint32_t value = 0;

		for (std::size_t i = 1; i <= digitCount; ++i)
		{
			const int digit = HexDigitValue(text[i]);

			if (digit < 0)
			{
				return false;
			}

			value = (value << 4) | static_cast<std::uint32_t>(digit);
		}

		if (*text == unicodeGlyphEscape)
		{
			if (value > 0x10FFFF || (value >= 0xD800 && value <= 0xDFFF))
			{
				return false;
			}

			key = value;
		}
		else
		{
			if (value == 0)
			{
				return false;
			}

			key = unicodeRunKey | value;
		}

		end = text + digitCount + 1;
		return true;
	}

	static std::uint32_t RegisterUnicodeRun(const std::wstring& text)
	{
		std::lock_guard _(unicodeMutex);
		const auto found = unicodeRunIds.find(text);

		if (found != unicodeRunIds.end())
		{
			return found->second;
		}

		if (unicodeRunTexts.size() >= unicodeRunLimit)
		{
			return 0;
		}

		const auto id = nextUnicodeRunId;
		++nextUnicodeRunId;

		unicodeRunIds.emplace(text, id);
		unicodeRunTexts.emplace(id, text);

		return id;
	}

	static bool HasUnicodeEscape(const char* text)
	{
		const char* position = text;

		while (*position)
		{
			if (IsHudIcon(position))
			{
				position = SkipHudIcon(position);
				continue;
			}

			if (*position == inlineIconEscape && (position[1] == unicodeGlyphEscape || position[1] == unicodeRunEscape))
			{
				return true;
			}

			++position;
		}

		return false;
	}

	static UnicodeText* FindUnicodeText(const std::uint64_t key)
	{
		const auto value = static_cast<std::uint32_t>(key);

		if (key & unicodeRunKey)
		{
			if (!unicodeRunTexts.contains(value))
			{
				return nullptr;
			}
		}
		else if (value > 0xFFFF || IsDroppedCodepoint(value))
		{
			return nullptr;
		}

		auto& entry = unicodeTexts[key];

		if (entry.pieces.empty() && !entry.isPending && !entry.isFailed)
		{
			entry.isPending = true;
			unicodePending.push_back(key);
		}

		return &entry;
	}

	static void AppendUnicodeText(std::string& out, const std::uint64_t key)
	{
		std::lock_guard _(unicodeMutex);
		auto* const entry = FindUnicodeText(key);

		if (!entry || entry->pieces.empty())
		{
			out.push_back('?');
			return;
		}

		entry->lastDrawnFrame = unicodeFrame;

		for (std::size_t i = 0; i < entry->pieces.size(); ++i)
		{
			const auto* const material = entry->pieces[i];

			out.push_back(inlineIconEscape);
			out.push_back(inlineIcon);
			out.push_back(static_cast<char>(inlineIconBias + entry->pieceUnits[i]));
			out.push_back(static_cast<char>(inlineIconBias + unicodeBoxUnits));
			out.append(reinterpret_cast<const char*>(&material), sizeof(material));
		}
	}

	class UnicodeGlyphRunRenderer final : public IDWriteTextRenderer
	{
	public:
		UnicodeGlyphRunRenderer(IDWriteBitmapRenderTarget* bitmapTarget, IDWriteRenderingParams* params)
			: target(bitmapTarget), renderingParams(params)
		{
		}

		HRESULT STDMETHODCALLTYPE QueryInterface(const IID& iid, void** object) override
		{
			if (!object)
			{
				return E_POINTER;
			}

			if (iid == __uuidof(IUnknown) || iid == __uuidof(IDWritePixelSnapping) || iid == __uuidof(IDWriteTextRenderer))
			{
				*object = this;
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

		HRESULT STDMETHODCALLTYPE IsPixelSnappingDisabled(void*, BOOL* isDisabled) override
		{
			if (!isDisabled)
			{
				return E_POINTER;
			}

			*isDisabled = FALSE;
			return S_OK;
		}

		HRESULT STDMETHODCALLTYPE GetCurrentTransform(void*, DWRITE_MATRIX* transform) override
		{
			if (!transform)
			{
				return E_POINTER;
			}

			*transform = DWRITE_MATRIX{ 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f };
			return S_OK;
		}

		HRESULT STDMETHODCALLTYPE GetPixelsPerDip(void*, FLOAT* pixelsPerDip) override
		{
			if (!pixelsPerDip)
			{
				return E_POINTER;
			}

			*pixelsPerDip = 1.0f;
			return S_OK;
		}

		HRESULT STDMETHODCALLTYPE DrawGlyphRun(void*, FLOAT baselineOriginX, FLOAT baselineOriginY, DWRITE_MEASURING_MODE measuringMode,
			const DWRITE_GLYPH_RUN* glyphRun, const DWRITE_GLYPH_RUN_DESCRIPTION*, IUnknown*) override
		{
			return target->DrawGlyphRun(baselineOriginX, baselineOriginY, measuringMode, glyphRun, renderingParams, RGB(255, 255, 255));
		}

		HRESULT STDMETHODCALLTYPE DrawUnderline(void*, FLOAT, FLOAT, const DWRITE_UNDERLINE*, IUnknown*) override
		{
			return S_OK;
		}

		HRESULT STDMETHODCALLTYPE DrawStrikethrough(void*, FLOAT, FLOAT, const DWRITE_STRIKETHROUGH*, IUnknown*) override
		{
			return S_OK;
		}

		HRESULT STDMETHODCALLTYPE DrawInlineObject(void*, FLOAT, FLOAT, IDWriteInlineObject*, BOOL, BOOL, IUnknown*) override
		{
			return S_OK;
		}

	private:
		IDWriteBitmapRenderTarget* target;
		IDWriteRenderingParams* renderingParams;
	};

	static std::uint8_t InkAt(const DIBSECTION& section, const int x, const int y)
	{
		const auto* const pixel = static_cast<const std::uint8_t*>(section.dsBm.bmBits)
			+ static_cast<std::size_t>(y) * section.dsBm.bmWidthBytes + static_cast<std::size_t>(x) * 4;

		return std::max({ pixel[0], pixel[1], pixel[2] });
	}

	static bool IsColumnEmpty(const DIBSECTION& section, const int x)
	{
		for (int y = 0; y < unicodeBoxPixels; ++y)
		{
			if (InkAt(section, x, y))
			{
				return false;
			}
		}

		return true;
	}

	static void DeleteUnicodeMaterials(const std::vector<Game::Material*>& materials)
	{
		for (auto* const material : materials)
		{
			Materials::Delete(material, true);
		}
	}

	static bool TryCreateUnicodePiece(UnicodeBuild& build, const DIBSECTION& section, const int startUnits, const int units)
	{
		const auto value = static_cast<std::uint32_t>(build.key);
		std::string name;

		if (build.key & unicodeRunKey)
		{
			name = std::format("runtime_unicode_run_{:08X}_{}", value, build.pieces.size());
		}
		else
		{
			name = std::format("runtime_unicode_glyph_{:06X}_{}", value, build.pieces.size());
		}

		const int width = units * unicodeUnitPixels;
		const int startX = startUnits * unicodeUnitPixels;
		auto* const image = Materials::CreateImage(name, width, unicodeBoxPixels, 1, unicodeImageFlags, D3DFMT_A8R8G8B8);

		if (!image->texture.map)
		{
			Materials::DeleteImage(image);
			return false;
		}

		D3DLOCKED_RECT lockedRect{};

		if (FAILED(image->texture.map->LockRect(0, &lockedRect, nullptr, 0)))
		{
			Materials::DeleteImage(image);
			return false;
		}

		for (int y = 0; y < unicodeBoxPixels; ++y)
		{
			auto* const row = static_cast<std::uint8_t*>(lockedRect.pBits) + static_cast<std::size_t>(y) * lockedRect.Pitch;

			for (int x = 0; x < width; ++x)
			{
				row[x * 4 + 0] = 255;
				row[x * 4 + 1] = 255;
				row[x * 4 + 2] = 255;
				row[x * 4 + 3] = InkAt(section, startX + x, y);
			}
		}

		image->texture.map->UnlockRect(0);

		auto* const material = Materials::Create(name, image);

		if (!material)
		{
			Materials::DeleteImage(image);
			return false;
		}

		build.pieces.push_back(material);
		build.pieceUnits.push_back(static_cast<std::uint8_t>(units));
		build.bytes += static_cast<std::size_t>(width) * unicodeBoxPixels * 4;

		return true;
	}

	static bool TryBuildUnicodeText(UnicodeBuild& build)
	{
		using Microsoft::WRL::ComPtr;

		if (build.text.empty())
		{
			return false;
		}

		ComPtr<IDWriteFactory> factory;

		if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(factory.GetAddressOf()))))
		{
			return false;
		}

		ComPtr<IDWriteTextFormat> format;
		const wchar_t* familyName = L"Segoe UI";
		if (build.text.size() == 1 && build.text[0] >= 0x80)
		{
			ComPtr<IDWriteFontCollection> collection;
			if (SUCCEEDED(factory->GetSystemFontCollection(&collection)))
			{
				for (const auto* candidate : { L"Segoe UI", L"Segoe UI Symbol", L"Cambria Math" })
				{
					UINT32 index = 0;
					BOOL exists = FALSE;
					ComPtr<IDWriteFontFamily> family;
					ComPtr<IDWriteFont> font;
					ComPtr<IDWriteFontFace> face;
					const UINT32 codepoint = build.text[0];
					UINT16 glyph = 0;
					if (SUCCEEDED(collection->FindFamilyName(candidate, &index, &exists)) && exists
						&& SUCCEEDED(collection->GetFontFamily(index, &family))
						&& SUCCEEDED(family->GetFirstMatchingFont(DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &font))
						&& SUCCEEDED(font->CreateFontFace(&face)) && SUCCEEDED(face->GetGlyphIndices(&codepoint, 1, &glyph)) && glyph)
					{
						familyName = candidate;
						break;
					}
				}
			}
		}

		if (FAILED(factory->CreateTextFormat(familyName, nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
			static_cast<FLOAT>(unicodeEmPixels), L"", &format)))
		{
			return false;
		}

		format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
		format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
		format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);

		ComPtr<IDWriteTextLayout> layout;

		if (FAILED(factory->CreateTextLayout(build.text.data(), static_cast<UINT32>(build.text.size()), format.Get(),
			static_cast<FLOAT>(unicodeMaxRunPixels), static_cast<FLOAT>(unicodeBoxPixels), &layout)))
		{
			return false;
		}

		DWRITE_TEXT_METRICS textMetrics{};
		DWRITE_LINE_METRICS lineMetrics{};
		UINT32 lineCount = 0;

		if (FAILED(layout->GetMetrics(&textMetrics)) || FAILED(layout->GetLineMetrics(&lineMetrics, 1, &lineCount)) || lineCount != 1)
		{
			return false;
		}

		const int totalUnits = std::max(1, static_cast<int>(std::ceil(textMetrics.widthIncludingTrailingWhitespace / unicodeUnitPixels)));
		const int width = totalUnits * unicodeUnitPixels;

		if (width > unicodeMaxRunPixels)
		{
			return false;
		}

		ComPtr<IDWriteGdiInterop> interop;
		ComPtr<IDWriteBitmapRenderTarget> target;
		ComPtr<IDWriteRenderingParams> renderingParams;

		if (FAILED(factory->GetGdiInterop(&interop)) || FAILED(interop->CreateBitmapRenderTarget(nullptr, width, unicodeBoxPixels, &target))
			|| FAILED(factory->CreateRenderingParams(&renderingParams)))
		{
			return false;
		}
		if (FAILED(target->SetPixelsPerDip(1.0f))) return false;

		const HDC memoryDc = target->GetMemoryDC();

		if (!memoryDc || !PatBlt(memoryDc, 0, 0, width, unicodeBoxPixels, BLACKNESS))
		{
			return false;
		}

		UnicodeGlyphRunRenderer renderer(target.Get(), renderingParams.Get());

		if (FAILED(layout->Draw(nullptr, &renderer, 0.0f, static_cast<FLOAT>(unicodeBaselinePixels) - lineMetrics.baseline)))
		{
			return false;
		}

		const auto bitmap = static_cast<HBITMAP>(GetCurrentObject(memoryDc, OBJ_BITMAP));
		DIBSECTION section{};

		if (!bitmap || GetObjectW(bitmap, sizeof(section), &section) != sizeof(section) || !section.dsBm.bmBits || section.dsBm.bmBitsPixel != 32)
		{
			return false;
		}

		int startUnits = 0;

		while (startUnits < totalUnits)
		{
			int units = std::min(unicodePieceUnits, totalUnits - startUnits);

			if (startUnits + units < totalUnits)
			{
				for (int candidate = units; candidate > unicodePieceUnits / 2; --candidate)
				{
					const int cutX = (startUnits + candidate) * unicodeUnitPixels;

					if (IsColumnEmpty(section, cutX - 1) && IsColumnEmpty(section, cutX))
					{
						units = candidate;
						break;
					}
				}
			}

			if (!TryCreateUnicodePiece(build, section, startUnits, units))
			{
				DeleteUnicodeMaterials(build.pieces);
				build.pieces.clear();
				build.pieceUnits.clear();
				build.bytes = 0;
				return false;
			}

			startUnits += units;
		}

		return true;
	}

	static void RetireUnicodeTexts(const std::uint64_t frame, std::vector<Game::Material*>& retired)
	{
		if (unicodeResidentBytes <= unicodeTextureBytes)
		{
			return;
		}

		std::vector<std::pair<std::uint64_t, std::uint64_t>> candidates;

		for (const auto& [key, entry] : unicodeTexts)
		{
			if (!entry.pieces.empty() && entry.lastDrawnFrame < frame)
			{
				candidates.emplace_back(entry.lastDrawnFrame, key);
			}
		}

		std::ranges::sort(candidates);

		for (const auto& candidate : candidates)
		{
			if (unicodeResidentBytes <= unicodeTextureBytes)
			{
				break;
			}

			auto& entry = unicodeTexts[candidate.second];

			retired.insert(retired.end(), entry.pieces.begin(), entry.pieces.end());
			entry.pieces.clear();
			unicodeResidentBytes -= entry.bytes;
			entry.bytes = 0;
		}
	}

	static void BuildUnicodeTexts(IDirect3DDevice9*)
	{
		std::vector<Game::Material*> retired;
		bool hasPending = false;

		{
			std::lock_guard _(unicodeMutex);
			const auto frame = unicodeFrame;
			++unicodeFrame;

			RetireUnicodeTexts(frame, retired);
			hasPending = !unicodePending.empty() && unicodeResidentBytes < unicodeTextureBytes;
		}

		DeleteUnicodeMaterials(retired);

		if (!hasPending || !FindMaterial("white"))
		{
			return;
		}

		std::vector<UnicodeBuild> builds;

		{
			std::lock_guard _(unicodeMutex);
			std::size_t glyphCount = 0;
			std::size_t runCount = 0;

			for (auto it = unicodePending.begin(); it != unicodePending.end();)
			{
				const auto key = *it;
				const bool isRun = (key & unicodeRunKey) != 0;

				if ((isRun && runCount >= unicodeRunsPerFrame) || (!isRun && glyphCount >= unicodeGlyphsPerFrame))
				{
					++it;
					continue;
				}

				UnicodeBuild build{ key, {}, {}, {}, 0 };

				if (isRun)
				{
					const auto runText = unicodeRunTexts.find(static_cast<std::uint32_t>(key));

					if (runText != unicodeRunTexts.end())
					{
						build.text = runText->second;
					}

					++runCount;
				}
				else
				{
					AppendUtf16(build.text, static_cast<std::uint32_t>(key));
					++glyphCount;
				}

				builds.push_back(std::move(build));
				it = unicodePending.erase(it);
			}
		}

		for (auto& build : builds)
		{
			TryBuildUnicodeText(build);
		}

		std::lock_guard _(unicodeMutex);

		for (auto& build : builds)
		{
			auto& entry = unicodeTexts[build.key];

			entry.isPending = false;
			entry.lastDrawnFrame = unicodeFrame;

			if (build.pieces.empty())
			{
				entry.isFailed = true;
				continue;
			}

			entry.pieces = std::move(build.pieces);
			entry.pieceUnits = std::move(build.pieceUnits);
			entry.bytes = build.bytes;
			unicodeResidentBytes += build.bytes;
		}
	}

	static void DropUnicodeTextures()
	{
		std::vector<Game::Material*> retired;

		{
			std::lock_guard _(unicodeMutex);

			for (auto& item : unicodeTexts)
			{
				auto& entry = item.second;

				retired.insert(retired.end(), entry.pieces.begin(), entry.pieces.end());
				entry.pieces.clear();
				entry.bytes = 0;
			}

			unicodeResidentBytes = 0;
		}

		DeleteUnicodeMaterials(retired);
	}

	void TextRenderer::InitFontIcons()
	{
		InitFontIconStrings();

		void* buffer = nullptr;
		const int length = Game::FS_ReadFile("mp/fonticons.csv", &buffer);

		if (length <= 0 || !buffer)
		{
			if (buffer)
			{
				Game::FS_FreeFile(buffer);
			}

			Logger::Error("textrenderer: mp/fonticons.csv is missing, so there are no font icons\n");
			return;
		}

		const std::string contents(static_cast<const char*>(buffer), static_cast<std::size_t>(length));
		Game::FS_FreeFile(buffer);

		std::map<std::string, FontIcon> table;
		std::istringstream lines(contents);
		std::string line;

		while (std::getline(lines, line))
		{
			if (!line.empty() && line.back() == '\r')
			{
				line.pop_back();
			}

			if (line.empty() || line[0] == '#')
			{
				continue;
			}

			const auto comma = line.find(',');

			if (comma == std::string::npos)
			{
				continue;
			}

			const std::string iconName = line.substr(0, comma);
			std::string materialName = line.substr(comma + 1);
			const auto nextComma = materialName.find(',');

			if (nextComma != std::string::npos)
			{
				materialName.resize(nextComma);
			}

			if (iconName.empty() || materialName.empty())
			{
				continue;
			}

			table.emplace(iconName, FontIcon{ materialName, nullptr, 0.0f, false });
		}

		{
			std::lock_guard _(fontIconsMutex);
			fontIcons = std::move(table);
		}

		areFontIconsReady.store(true, std::memory_order_release);
	}

	bool TextRenderer::TryResolveFontIcon(FontIcon& icon)
	{
		void* material = FindMaterial(icon.materialName.data());

		if (!material)
		{
			return false;
		}

		const auto* const techniqueSet = *reinterpret_cast<const char* const* const*>(static_cast<const std::uint8_t*>(material) + materialTechniqueSet);

		if (!techniqueSet || !*techniqueSet)
		{
			return false;
		}

		if (std::strcmp(*techniqueSet, "2d") != 0)
		{
			material = FindMaterial("default");

			if (!material)
			{
				return false;
			}
		}

		const float aspect = ColorMapAspect(material);
		icon.isResolved = true;
		icon.material = nullptr;
		icon.aspect = aspect;

		if (aspect > 0.0f)
		{
			icon.material = material;
		}

		return icon.material != nullptr;
	}

	void TextRenderer::DB_UnloadXZone_Hk(unsigned int zoneIndex, bool shouldCreateDefault)
	{
		{
			std::lock_guard _(fontIconsMutex);

			for (auto& entry : fontIcons)
			{
				entry.second.isResolved = false;
				entry.second.material = nullptr;
			}
		}

		reinterpret_cast<void(*)(unsigned int, bool)>(Utils::Hook::Rebase(DB_UnloadXZone))(zoneIndex, shouldCreateDefault);
	}

	bool TextRenderer::TryReadFontIcon(const char*& text, FontIcon& icon, bool& isFlippedHorizontally, bool& isBig)
	{
		const char* position = text;

		while (*position != ' ' && *position != fontIconSeparator && *position != 0 && *position != fontIconModifierSeparator)
		{
			++position;
		}

		const char* const nameEnd = position;

		if (*position == fontIconModifierSeparator)
		{
			bool isDone = false;

			while (!isDone)
			{
				++position;

				switch (*position)
				{
				case fontIconFlipHorizontally:
					isFlippedHorizontally = true;
					break;
				case fontIconFlipVertically:
					break;
				case fontIconBig:
					isBig = true;
					break;
				case fontIconSeparator:
					isDone = true;
					break;
				default:
					return false;
				}
			}
		}

		if (*position != fontIconSeparator)
		{
			return false;
		}

		{
			std::lock_guard _(fontIconsMutex);
			const auto found = fontIcons.find(std::string(text, nameEnd));

			if (found == fontIcons.end())
			{
				return false;
			}

			if (!found->second.isResolved && !TryResolveFontIcon(found->second))
			{
				return false;
			}

			if (!found->second.material)
			{
				return false;
			}

			icon.material = found->second.material;
			icon.aspect = found->second.aspect;
		}

		text = position + 1;
		return true;
	}

	bool TextRenderer::TryGetFontIconWidth(const char* text, const char*& end, float& width)
	{
		if (*text != fontIconSeparator || !areFontIconsReady.load(std::memory_order_acquire))
		{
			return false;
		}

		const char* position = text + 1;
		FontIcon icon{};
		bool isFlippedHorizontally = false;
		bool isBig = false;

		if (!TryReadFontIcon(position, icon, isFlippedHorizontally, isBig))
		{
			return false;
		}

		float sizeMultiplier = 1.0f;

		if (isBig)
		{
			sizeMultiplier = 1.5f;
		}

		end = position;
		width = icon.aspect * sizeMultiplier;
		return true;
	}

	static std::string NormalizeUtf8Text(const char* text)
	{
		std::string result;
		if (!text) return result;
		while (*text)
		{
			if (IsHudIcon(text))
			{
				const auto* end = SkipHudIcon(text);
				result.append(text, end);
				text = end;
				continue;
			}
			if (static_cast<unsigned char>(*text) < 0x20 || *text == inlineIconEscape)
			{
				result.push_back(*text++);
				if (result.back() == inlineIconEscape && *text) result.push_back(*text++);
				continue;
			}
			const char* start = text;
			bool hasNonAscii = false;
			while (*text && static_cast<unsigned char>(*text) >= 0x20 && *text != inlineIconEscape)
			{
				hasNonAscii |= static_cast<unsigned char>(*text) >= 0x80;
				++text;
			}
			const std::string_view span(start, text - start);
			if (hasNonAscii && MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, start, static_cast<int>(span.size()), nullptr, 0) > 0)
			{
				std::size_t offset = 0;
				while (offset < span.size())
				{
					const auto begin = offset;
					std::size_t characters = 0;
					while (offset < span.size() && characters < encodedCharacterLimit)
					{
						const auto byte = static_cast<unsigned char>(span[offset]);
						offset += byte < 0x80 ? 1 : byte < 0xE0 ? 2 : byte < 0xF0 ? 3 : 4;
						++characters;
					}
					result += TextRenderer::EncodeUtf8ForGame(span.substr(begin, offset - begin), encodedCharacterLimit);
				}
			}
			else result.append(span);
		}
		return result;
	}

	bool TextRenderer::TranslateText(const char* text, bool isEditing, std::string& translated, std::vector<std::size_t>& unicodeIcons)
	{
		unicodeIcons.clear();

		if (!text)
		{
			return false;
		}
		const auto normalized = isEditing ? std::string(text) : NormalizeUtf8Text(text);
		const bool wasNormalized = normalized != text;
		text = normalized.data();

		const bool hasFontIcons = !isEditing && areFontIconsReady.load(std::memory_order_acquire) && std::strchr(text, fontIconSeparator) != nullptr;
		const bool hasUnicodeEscapes = !isEditing && HasUnicodeEscape(text);

		if (!hasFontIcons && !hasUnicodeEscapes && !HasHudIcon(text))
		{
			if (wasNormalized) translated = normalized;
			return wasNormalized;
		}

		translated.clear();

		bool isTranslated = false;
		const char* position = text;

		while (*position)
		{
			if (IsHudIcon(position))
			{
				AppendHudIcon(translated, position);
				position = SkipHudIcon(position);
				isTranslated = true;
				continue;
			}

			if (hasUnicodeEscapes && *position == inlineIconEscape)
			{
				const char* escapeEnd = nullptr;
				std::uint64_t key = 0;

				if (TryReadUnicodeEscape(position + 1, key, escapeEnd))
				{
					const auto start = translated.size();
					AppendUnicodeText(translated, key);

					for (auto at = start; at + inlineIconBytes <= translated.size(); at += inlineIconBytes)
					{
						unicodeIcons.push_back(at);
					}

					position = escapeEnd;
					isTranslated = true;
					continue;
				}
			}

			if (hasFontIcons && *position == fontIconSeparator)
			{
				const char* iconEnd = position + 1;
				FontIcon icon{};
				bool isFlippedHorizontally = false;
				bool isBig = false;

				if (TryReadFontIcon(iconEnd, icon, isFlippedHorizontally, isBig))
				{
					AppendInlineIcon(translated, icon.material, icon.aspect, isFlippedHorizontally, isBig);
					position = iconEnd;
					isTranslated = true;
					continue;
				}
			}

			translated.push_back(*position);
			++position;
		}

		return isTranslated;
	}

	struct StyledText
	{
		float x;
		float y;
		void* font;
		float xScale;
		float yScale;
		float sinAngle;
		float cosAngle;
		unsigned int color;
		int maxLength;
		int renderFlags;
		float padding;
		unsigned int glowForcedColor;
	};

	static unsigned int GlowColor(const StyledText& draw)
	{
		const unsigned int alpha = draw.color & 0xFF000000;

		if (draw.renderFlags & textRenderFlagGlowForceColor)
		{
			return (draw.glowForcedColor & 0x00FFFFFF) | alpha;
		}

		unsigned int glow = alpha;

		for (int shift = 0; shift < 24; shift += 8)
		{
			const auto channel = static_cast<unsigned int>(std::floor(static_cast<float>((draw.color >> shift) & 0xFF) * 0.06f));
			glow |= channel << shift;
		}

		return glow;
	}

	static void DrawUnicodeStyles(const std::string& text, const std::vector<std::size_t>& unicodeIcons, const StyledText& draw)
	{
		const bool hasShadow = (draw.renderFlags & textRenderFlagDropShadow) != 0;
		const bool hasOutline = (draw.renderFlags & textRenderFlagOutline) != 0;
		const bool hasGlow = (draw.renderFlags & textRenderFlagGlow) != 0 && (draw.renderFlags & textRenderFlagSubtitle) == 0;

		if (unicodeIcons.empty() || (!hasShadow && !hasOutline && !hasGlow))
		{
			return;
		}

		const auto* const fontBytes = static_cast<const std::uint8_t*>(draw.font);
		const int pixelHeight = *reinterpret_cast<const int*>(fontBytes + fontPixelHeight);
		const int glyphCount = *reinterpret_cast<const int*>(fontBytes + fontGlyphCount);
		const auto* const glyphs = *reinterpret_cast<const std::uint8_t* const*>(fontBytes + fontGlyphs);
		const auto readChar = reinterpret_cast<unsigned int(*)(const char**, int*)>(Utils::Hook::Rebase(SEH_ReadCharFromString));
		const auto quad = reinterpret_cast<void(*)(void*, float, float, float, float, float, float, float, float, float, float, unsigned int)>(
			Utils::Hook::Rebase(glyphQuadDraw));

		const unsigned int shadowColor = draw.color & 0xFF000000;
		const unsigned int glowColor = GlowColor(draw);
		const float shadowOffset = (draw.renderFlags & textRenderFlagDropShadowExtra) ? 2.0f : 1.0f;
		const float outlineSize = (draw.renderFlags & textRenderFlagOutlineExtra) ? 1.3f : 1.0f;
		const bool hasPadding = (draw.renderFlags & textRenderFlagPadding) != 0;

		float pen = draw.x;
		int remaining = draw.maxLength;
		std::size_t nextIcon = 0;
		const char* position = text.data();

		while (*position && remaining != 0)
		{
			const auto letterAt = static_cast<std::size_t>(position - text.data());
			unsigned int letter = readChar(&position, nullptr);

			if (letter == '\r' || letter == '\n')
			{
				return;
			}

			if (letter == static_cast<unsigned int>(inlineIconEscape))
			{
				const unsigned int index = static_cast<unsigned char>(*position) - '0';

				if (index < TEXT_COLOR_COUNT)
				{
					++position;
					continue;
				}

				if (*position == inlineIcon || *position == inlineIconFlipped)
				{
					const float w = static_cast<float>(InlineIconSize(pixelHeight, position[1])) * draw.xScale;
					const float h = static_cast<float>(InlineIconSize(pixelHeight, position[2])) * draw.yScale;

					while (nextIcon < unicodeIcons.size() && unicodeIcons[nextIcon] < letterAt)
					{
						++nextIcon;
					}

					if (nextIcon < unicodeIcons.size() && unicodeIcons[nextIcon] == letterAt)
					{
						void* material = nullptr;
						std::memcpy(&material, position + 3, sizeof(material));

						const auto drawAt = [&](float offsetX, float offsetY, unsigned int tint)
						{
							const float along = pen - draw.x + offsetX;
							const float drawX = draw.x + along * draw.cosAngle - offsetY * draw.sinAngle;
							const float drawY = draw.y + offsetY * draw.cosAngle + along * draw.sinAngle
								- 0.5f * (static_cast<float>(pixelHeight) * draw.yScale + h);
							quad(material, drawX, drawY, w, h, 0.0f, 0.0f, 1.0f, 1.0f, draw.sinAngle, draw.cosAngle, tint);
						};

						if (hasGlow)
						{
							for (const auto& offset : styleOffsets)
							{
								drawAt(2.0f * offset[0] * draw.xScale, 2.0f * offset[1] * draw.yScale, glowColor);
							}
						}

						if (hasOutline)
						{
							for (const auto& offset : styleOffsets)
							{
								drawAt(outlineSize * offset[0], outlineSize * offset[1], shadowColor);
							}
						}

						if (hasShadow)
						{
							drawAt(shadowOffset, shadowOffset, shadowColor);
						}
					}

					pen += w;

					if (hasPadding)
					{
						pen += draw.xScale * draw.padding;
					}

					position += inlineIconBytes - 1;
					--remaining;
					continue;
				}
			}

			const std::uint8_t* glyph = nullptr;

			if (letter - 0x20 <= 0x5F)
			{
				glyph = glyphs + (letter - 0x20) * glyphSize;
			}
			else
			{
				int low = glyphDirectCount;
				int high = glyphCount - 1;

				while (low <= high)
				{
					const int middle = (low + high) / 2;
					const auto* const candidate = glyphs + middle * glyphSize;
					const unsigned int candidateLetter = *reinterpret_cast<const std::uint16_t*>(candidate);

					if (candidateLetter == letter)
					{
						glyph = candidate;
						break;
					}

					if (candidateLetter < letter)
					{
						low = middle + 1;
					}
					else
					{
						high = middle - 1;
					}
				}

				if (!glyph)
				{
					glyph = glyphs + glyphFallback * glyphSize;
				}
			}

			pen += static_cast<float>(glyph[glyphDx]) * draw.xScale;

			if (hasPadding)
			{
				pen += draw.xScale * draw.padding;
			}

			--remaining;
		}
	}

	void TextRenderer::DrawText2D_Hook(const char* text, float x, float y, void* font, float xScale, float yScale,
		float sinAngle, float cosAngle, unsigned int color, int maxLength, int renderFlags, int cursorPos, char cursorLetter,
		float padding, unsigned int glowForcedColor, int fxBirthTime, int fxLetterTime, int fxDecayStartTime,
		int fxDecayDuration, void* fxMaterial, void* fxMaterialGlow)
	{
		thread_local std::string translated;
		thread_local std::vector<std::size_t> unicodeIcons;

		const bool isEditing = (renderFlags & textRenderFlagCursor) != 0;

		if (TranslateText(text, isEditing, translated, unicodeIcons))
		{
			text = translated.data();

			const int unstyled = textRenderFlagFxDecode | textRenderFlagForceMonospace;

			if (!(renderFlags & unstyled))
			{
				DrawUnicodeStyles(translated, unicodeIcons, { x, y, font, xScale, yScale, sinAngle, cosAngle, color, maxLength, renderFlags, padding, glowForcedColor });
			}
		}

		reinterpret_cast<void(*)(const char*, float, float, void*, float, float, float, float, unsigned int, int, int, int, char,
			float, unsigned int, int, int, int, int, void*, void*)>(drawText2DHook.GetOriginal())(text, x, y, font, xScale, yScale,
			sinAngle, cosAngle, color, maxLength, renderFlags, cursorPos, cursorLetter, padding, glowForcedColor, fxBirthTime,
			fxLetterTime, fxDecayStartTime, fxDecayDuration, fxMaterial, fxMaterialGlow);
	}

	int TextRenderer::R_TextWidth(const char* text, int maxChars, void* font)
	{
		const auto normalized = NormalizeUtf8Text(text);
		text = normalized.data();
		const auto* const fontBytes = static_cast<const std::uint8_t*>(font);
		const int pixelHeight = *reinterpret_cast<const int*>(fontBytes + fontPixelHeight);
		const int glyphCount = *reinterpret_cast<const int*>(fontBytes + fontGlyphCount);
		const auto* const glyphs = *reinterpret_cast<const std::uint8_t* const*>(fontBytes + fontGlyphs);
		const auto readChar = reinterpret_cast<unsigned int(*)(const char**, int*)>(Utils::Hook::Rebase(SEH_ReadCharFromString));

		int limit = std::numeric_limits<int>::max();

		if (maxChars > 0)
		{
			limit = maxChars;
		}

		int lineWidth = 0;
		int maxWidth = 0;
		int count = 0;

		while (*text && count < limit)
		{
			unsigned int letter = readChar(&text, nullptr);

			if (letter == '\r' || letter == '\n')
			{
				lineWidth = 0;
				continue;
			}

			if (letter == static_cast<unsigned int>(inlineIconEscape))
			{
				const unsigned int index = static_cast<unsigned char>(*text) - '0';

				if (index < TEXT_COLOR_COUNT)
				{
					++text;
					continue;
				}

				if ((*text == inlineIcon || *text == inlineIconFlipped) && text[1] && text[2] && text[3])
				{
					lineWidth += InlineIconSize(pixelHeight, text[1]);
					text = SkipHudIcon(text - 1);
					++count;
					maxWidth = std::max(maxWidth, lineWidth);
					continue;
				}

				const char* escapeEnd = nullptr;
				std::uint64_t key = 0;

				if (TryReadUnicodeEscape(text, key, escapeEnd))
				{
					text = escapeEnd;

					std::lock_guard _(unicodeMutex);
					const auto* const entry = FindUnicodeText(key);

					if (entry && !entry->pieceUnits.empty())
					{
						for (const auto units : entry->pieceUnits)
						{
							if (count >= limit)
							{
								break;
							}

							lineWidth += InlineIconSize(pixelHeight, static_cast<char>(inlineIconBias + units));
							++count;
						}

						maxWidth = std::max(maxWidth, lineWidth);
						continue;
					}

					letter = '?';
				}
			}

			if (letter == static_cast<unsigned int>(fontIconSeparator) && areFontIconsReady.load(std::memory_order_acquire))
			{
				const char* iconEnd = text;
				FontIcon icon{};
				bool isFlippedHorizontally = false;
				bool isBig = false;

				if (TryReadFontIcon(iconEnd, icon, isFlippedHorizontally, isBig))
				{
					std::string escape;
					AppendInlineIcon(escape, icon.material, icon.aspect, isFlippedHorizontally, isBig);

					lineWidth += InlineIconSize(pixelHeight, escape[2]);
					text = iconEnd;
					++count;
					maxWidth = std::max(maxWidth, lineWidth);
					continue;
				}
			}

			const std::uint8_t* glyph = nullptr;

			if (letter - 0x20 <= 0x5F)
			{
				glyph = glyphs + (letter - 0x20) * glyphSize;
			}
			else
			{
				int low = glyphDirectCount;
				int high = glyphCount - 1;

				while (low <= high)
				{
					const int middle = (low + high) / 2;
					const auto* const candidate = glyphs + middle * glyphSize;
					const unsigned int candidateLetter = *reinterpret_cast<const std::uint16_t*>(candidate);

					if (candidateLetter == letter)
					{
						glyph = candidate;
						break;
					}

					if (candidateLetter < letter)
					{
						low = middle + 1;
					}
					else
					{
						high = middle - 1;
					}
				}

				if (!glyph)
				{
					glyph = glyphs + glyphFallback * glyphSize;
				}
			}

			lineWidth += glyph[glyphDx];
			++count;
			maxWidth = std::max(maxWidth, lineWidth);
		}

		return maxWidth;
	}

	TextRenderer::BufferedLocalizedString::BufferedLocalizedString(const char* localizeReference)
		: reference(localizeReference)
	{
		std::ranges::fill(this->width, -1);
	}

	void TextRenderer::BufferedLocalizedString::Cache()
	{
		const char* const translated = reinterpret_cast<const char*(*)(const char*)>(Utils::Hook::Rebase(UI_SafeTranslateString))(this->reference);

		if (!translated)
		{
			return;
		}

		this->text = translated;
		std::ranges::fill(this->width, -1);
	}

	const char* TextRenderer::BufferedLocalizedString::Format(const char* value)
	{
		const char* const formatting = reinterpret_cast<const char*(*)(const char*)>(Utils::Hook::Rebase(UI_SafeTranslateString))(this->reference);

		if (!formatting)
		{
			this->text.clear();
			return this->text.data();
		}

		ConversionArguments arguments{};
		arguments.args[arguments.argCount++] = value;

		char formatted[1024]{};
		reinterpret_cast<void(*)(const char*, ConversionArguments*, char*, int)>(Utils::Hook::Rebase(UI_ReplaceConversions))(
			formatting, &arguments, formatted, sizeof(formatted));

		this->text = formatted;
		std::ranges::fill(this->width, -1);

		return this->text.data();
	}

	const char* TextRenderer::BufferedLocalizedString::GetString() const
	{
		return this->text.data();
	}

	int TextRenderer::BufferedLocalizedString::GetWidth(FontIconAutocompleteInstance instance, Game::Font_s* font)
	{
		if (this->width[instance] < 0)
		{
			this->width[instance] = Game::R_TextWidth(this->GetString(), std::numeric_limits<int>::max(), font);
		}

		return this->width[instance];
	}

	bool TextRenderer::IsAutocompleteEnabled()
	{
		return cg_fontIconAutocomplete && cg_fontIconAutocomplete->current.enabled && areFontIconsReady.load(std::memory_order_acquire);
	}

	void TextRenderer::DrawAutocompleteBox(const FontIconAutocompleteContext& context, const AutocompleteLayout& layout, float width, unsigned int lineCount)
	{
		const auto* const boxColorDvar = Utils::Hook::Get<const Game::dvar_t*>(con_inputBoxColor);
		void* const whiteMaterial = Utils::Hook::Get<void*>(cls_whiteMaterial);

		if (!boxColorDvar || !whiteMaterial)
		{
			return;
		}

		const auto drawStretchPic = reinterpret_cast<void(*)(float, float, float, float, float, float, float, float, const float*, void*)>(
			Utils::Hook::Rebase(R_AddCmdDrawStretchPic));

		const float lineHeight = static_cast<float>(FontPixelHeight(layout.font)) * layout.yScale;
		const float x = layout.x - autocompleteBoxPadding;
		const float y = layout.y - autocompleteBoxPadding;
		const float w = width + autocompleteBoxPadding * 2.0f;
		const float h = static_cast<float>(lineCount) * lineHeight + autocompleteBoxPadding * 2.0f;

		const float* const color = boxColorDvar->current.vector;
		const float borderColor[4] = { color[0] * 0.5f, color[1] * 0.5f, color[2] * 0.5f, color[3] };

		drawStretchPic(x, y, w, h, 0.0f, 0.0f, 0.0f, 0.0f, color, whiteMaterial);
		drawStretchPic(x, y, autocompleteBoxBorder, h, 0.0f, 0.0f, 0.0f, 0.0f, borderColor, whiteMaterial);
		drawStretchPic(x + w - autocompleteBoxBorder, y, autocompleteBoxBorder, h, 0.0f, 0.0f, 0.0f, 0.0f, borderColor, whiteMaterial);
		drawStretchPic(x, y, w, autocompleteBoxBorder, 0.0f, 0.0f, 0.0f, 0.0f, borderColor, whiteMaterial);
		drawStretchPic(x, y + h - autocompleteBoxBorder, w, autocompleteBoxBorder, 0.0f, 0.0f, 0.0f, 0.0f, borderColor, whiteMaterial);

		void* const arrowDown = Utils::Hook::Get<void*>(sharedUiInfo_scrollBarArrowDown);
		void* const arrowUp = Utils::Hook::Get<void*>(sharedUiInfo_scrollBarArrowUp);

		if (context.resultOffset > 0 && arrowDown)
		{
			drawStretchPic(x + w - autocompleteBoxBorder - autocompleteArrowSize, y + autocompleteBoxBorder,
				autocompleteArrowSize, autocompleteArrowSize, 1.0f, 1.0f, 0.0f, 0.0f, autocompleteWhite, arrowDown);
		}

		if (context.hasMoreResults && arrowUp)
		{
			drawStretchPic(x + w - autocompleteBoxBorder - autocompleteArrowSize, y + h - autocompleteBoxBorder - autocompleteArrowSize,
				autocompleteArrowSize, autocompleteArrowSize, 1.0f, 1.0f, 0.0f, 0.0f, autocompleteWhite, arrowUp);
		}
	}

	void TextRenderer::UpdateAutocompleteContextResults(FontIconAutocompleteContext& context, Game::Font_s* font, float textXScale)
	{
		context.resultCount = 0;
		context.hasMoreResults = false;
		context.lastResultOffset = context.resultOffset;

		std::size_t skipCount = context.resultOffset;
		std::unique_lock lock(fontIconsMutex);

		for (auto entry = fontIcons.lower_bound(context.lastQuery); entry != fontIcons.end(); ++entry)
		{
			if (entry->first.compare(0, context.lastQuery.size(), context.lastQuery) != 0)
			{
				break;
			}

			if (skipCount > 0)
			{
				--skipCount;
				continue;
			}

			if (context.resultCount >= FontIconAutocompleteContext::maxResults)
			{
				context.hasMoreResults = true;
				break;
			}

			auto& result = context.results[context.resultCount];
			result.fontIconName = Utils::String::VA("%c%s%c", fontIconSeparator, entry->first.data(), fontIconSeparator);
			result.iconName = entry->first;
			++context.resultCount;
		}

		lock.unlock();

		context.maxFontIconWidth = 0.0f;
		context.maxIconNameWidth = 0.0f;

		for (std::size_t i = 0; i < context.resultCount; ++i)
		{
			const auto& result = context.results[i];
			const float fontIconWidth = static_cast<float>(Game::R_TextWidth(result.fontIconName.data(), std::numeric_limits<int>::max(), font)) * textXScale;
			const float iconNameWidth = static_cast<float>(Game::R_TextWidth(result.iconName.data(), std::numeric_limits<int>::max(), font)) * textXScale;

			context.maxFontIconWidth = std::max(context.maxFontIconWidth, fontIconWidth);
			context.maxIconNameWidth = std::max(context.maxIconNameWidth, iconNameWidth);
		}
	}

	void TextRenderer::UpdateAutocompleteContext(FontIconAutocompleteContext& context, std::string_view typed, Game::Font_s* font, float textXScale)
	{
		int fontIconStart = -1;
		bool isInModifiers = false;

		for (std::size_t i = 0; i < typed.size(); ++i)
		{
			const auto character = static_cast<unsigned char>(typed[i]);

			if (character == fontIconSeparator)
			{
				if (fontIconStart < 0)
				{
					fontIconStart = static_cast<int>(i) + 1;
				}
				else
				{
					fontIconStart = -1;
				}

				isInModifiers = false;
			}
			else if (std::isspace(character))
			{
				fontIconStart = -1;
				isInModifiers = false;
			}
			else if (character == fontIconModifierSeparator)
			{
				if (fontIconStart >= 0 && !isInModifiers)
				{
					isInModifiers = true;
				}
				else
				{
					fontIconStart = -1;
					isInModifiers = false;
				}
			}
		}

		const auto start = static_cast<std::size_t>(fontIconStart);

		if (fontIconStart < 0
			|| start == typed.size()
			|| !std::isalpha(static_cast<unsigned char>(typed[start]))
			|| (start > 1 && std::isalnum(static_cast<unsigned char>(typed[start - 2]))))
		{
			context.isActive = false;
			context.didUserClose = false;
			context.lastQuery.clear();
			context.resultCount = 0;
			return;
		}

		context.isInModifiers = isInModifiers;

		if (context.selectedOffset < context.resultOffset)
		{
			context.resultOffset = context.selectedOffset;
		}
		else if (context.selectedOffset >= context.resultOffset + FontIconAutocompleteContext::maxResults)
		{
			context.resultOffset = context.selectedOffset - (FontIconAutocompleteContext::maxResults - 1);
		}

		if (context.didUserClose)
		{
			return;
		}

		context.isActive = true;

		if (context.isInModifiers)
		{
			return;
		}

		const std::string_view query = typed.substr(start);
		const bool isNewQuery = query != context.lastQuery;

		if (!isNewQuery && context.lastResultOffset == context.resultOffset)
		{
			return;
		}

		if (isNewQuery)
		{
			context.resultOffset = 0;
			context.selectedOffset = 0;
		}

		context.lastQuery = query;
		context.stringSearchStartWith.Format(context.lastQuery.data());
		UpdateAutocompleteContextResults(context, font, textXScale);
	}

	void TextRenderer::DrawAutocompleteModifiers(FontIconAutocompleteInstance instance, const AutocompleteLayout& layout)
	{
		const auto& context = autocompleteContextArray[instance];

		const int longestWidth = std::max({
			stringListHeader.GetWidth(instance, layout.font),
			stringListFlipHorizontal.GetWidth(instance, layout.font),
			stringListFlipVertical.GetWidth(instance, layout.font),
			stringListBig.GetWidth(instance, layout.font),
		});

		DrawAutocompleteBox(context, layout, static_cast<float>(longestWidth) * layout.xScale, 4);

		const float lineHeight = static_cast<float>(FontPixelHeight(layout.font)) * layout.yScale;
		float currentY = layout.y + lineHeight;

		Game::R_AddCmdDrawText(stringListHeader.GetString(), std::numeric_limits<int>::max(), layout.font, layout.x, currentY, layout.xScale, layout.yScale, 0.0f, autocompleteTextColor, 0);
		currentY += lineHeight;

		Game::R_AddCmdDrawText(stringListFlipHorizontal.GetString(), std::numeric_limits<int>::max(), layout.font, layout.x, currentY, layout.xScale, layout.yScale, 0.0f, autocompleteTextColor, 0);
		currentY += lineHeight;
		Game::R_AddCmdDrawText(stringListFlipVertical.GetString(), std::numeric_limits<int>::max(), layout.font, layout.x, currentY, layout.xScale, layout.yScale, 0.0f, autocompleteTextColor, 0);
		currentY += lineHeight;
		Game::R_AddCmdDrawText(stringListBig.GetString(), std::numeric_limits<int>::max(), layout.font, layout.x, currentY, layout.xScale, layout.yScale, 0.0f, autocompleteTextColor, 0);
	}

	void TextRenderer::DrawAutocompleteResults(FontIconAutocompleteInstance instance, const AutocompleteLayout& layout)
	{
		auto& context = autocompleteContextArray[instance];

		const bool isHintEnabled = cg_fontIconAutocompleteHint && cg_fontIconAutocompleteHint->current.enabled;

		int longestWidth = context.stringSearchStartWith.GetWidth(instance, layout.font);

		if (isHintEnabled)
		{
			longestWidth = std::max({ longestWidth, stringHintAutoComplete.GetWidth(instance, layout.font), stringHintModifier.GetWidth(instance, layout.font) });
		}

		const float columnSpacing = autocompleteColumnSpacing * layout.xScale;
		const float boxWidth = std::max(context.maxFontIconWidth + context.maxIconNameWidth + columnSpacing, static_cast<float>(longestWidth) * layout.xScale);
		const float lineHeight = static_cast<float>(FontPixelHeight(layout.font)) * layout.yScale;

		unsigned int totalLines = 1 + static_cast<unsigned int>(context.resultCount);

		if (isHintEnabled)
		{
			totalLines += 2;
		}

		float arrowPadding = 0.0f;

		if (context.resultOffset > 0 || context.hasMoreResults)
		{
			arrowPadding = autocompleteArrowSize;
		}

		DrawAutocompleteBox(context, layout, boxWidth + arrowPadding, totalLines);

		float currentY = layout.y + lineHeight;
		Game::R_AddCmdDrawText(context.stringSearchStartWith.GetString(), std::numeric_limits<int>::max(), layout.font, layout.x, currentY, layout.xScale, layout.yScale, 0.0f, autocompleteTextColor, 0);
		currentY += lineHeight;

		const std::size_t selectedIndex = context.selectedOffset - context.resultOffset;
		const float iconNameX = layout.x + context.maxFontIconWidth + columnSpacing;

		for (std::size_t i = 0; i < context.resultCount; ++i)
		{
			const auto& result = context.results[i];
			Game::R_AddCmdDrawText(result.fontIconName.data(), std::numeric_limits<int>::max(), layout.font, layout.x, currentY, layout.xScale, layout.yScale, 0.0f, autocompleteTextColor, 0);

			if (i == selectedIndex)
			{
				Game::R_AddCmdDrawText(Utils::String::VA("^2%s", result.iconName.data()), std::numeric_limits<int>::max(), layout.font, iconNameX, currentY, layout.xScale, layout.yScale, 0.0f, autocompleteTextColor, 0);
			}
			else
			{
				Game::R_AddCmdDrawText(result.iconName.data(), std::numeric_limits<int>::max(), layout.font, iconNameX, currentY, layout.xScale, layout.yScale, 0.0f, autocompleteTextColor, 0);
			}

			currentY += lineHeight;
		}

		if (isHintEnabled)
		{
			Game::R_AddCmdDrawText(stringHintAutoComplete.GetString(), std::numeric_limits<int>::max(), layout.font, layout.x, currentY, layout.xScale, layout.yScale, 0.0f, autocompleteHintColor, 0);
			currentY += lineHeight;
			Game::R_AddCmdDrawText(stringHintModifier.GetString(), std::numeric_limits<int>::max(), layout.font, layout.x, currentY, layout.xScale, layout.yScale, 0.0f, autocompleteHintColor, 0);
		}
	}

	void TextRenderer::DrawAutocomplete(FontIconAutocompleteInstance instance, const AutocompleteLayout& layout)
	{
		if (autocompleteContextArray[instance].isInModifiers)
		{
			DrawAutocompleteModifiers(instance, layout);
		}
		else
		{
			DrawAutocompleteResults(instance, layout);
		}
	}

	void TextRenderer::DrawConsoleAutocomplete(std::string_view typed, Game::Font_s* font, float x, float y)
	{
		auto& context = autocompleteContextArray[FONT_ICON_ACI_CONSOLE];

		if (!IsAutocompleteEnabled() || !font)
		{
			context.isActive = false;
			return;
		}

		UpdateAutocompleteContext(context, typed, font, 1.0f);

		if (context.isActive)
		{
			DrawAutocomplete(FONT_ICON_ACI_CONSOLE, { x, y, font, 1.0f, 1.0f });
		}
	}

	void TextRenderer::Field_Draw_Say(int localClientNum, field_t* edit, int x, int y, int horzAlign, int vertAlign)
	{
		reinterpret_cast<void(*)(int, field_t*, int, int, int, int)>(fieldDrawSayHook.GetOriginal())(localClientNum, edit, x, y, horzAlign, vertAlign);

		auto& context = autocompleteContextArray[FONT_ICON_ACI_CHAT];

		if (!IsAutocompleteEnabled())
		{
			context.isActive = false;
			return;
		}

		const float* const placement = Game::ScrPlace_GetActivePlacement(localClientNum);
		const float scale = edit->charHeight / 48.0f;

		auto* const font = reinterpret_cast<Game::Font_s*(*)(const float*, int, float)>(Utils::Hook::Rebase(UI_GetFontHandle))(placement, 0, scale);
		const float normalizedScale = reinterpret_cast<float(*)(Game::Font_s*, float)>(Utils::Hook::Rebase(R_NormalizedTextScale))(font, scale);

		float drawX = static_cast<float>(x);
		float drawY = static_cast<float>(y) + static_cast<float>(Game::R_TextHeight(font)) * normalizedScale * 1.5f;
		float xScale = normalizedScale;
		float yScale = normalizedScale;

		reinterpret_cast<void(*)(const float*, float*, float*, float*, float*, int, int)>(Utils::Hook::Rebase(ScrPlace_ApplyRect))(
			placement, &drawX, &drawY, &xScale, &yScale, horzAlign, vertAlign);

		const int cursor = std::clamp(edit->cursor, 0, static_cast<int>(sizeof(edit->buffer)) - 1);
		UpdateAutocompleteContext(context, std::string_view(edit->buffer, static_cast<std::size_t>(cursor)), font, xScale);

		if (context.isActive)
		{
			DrawAutocomplete(FONT_ICON_ACI_CHAT, { std::floor(drawX), std::floor(drawY), font, xScale, yScale });
		}
	}

	void TextRenderer::AutocompleteUp(FontIconAutocompleteContext& context)
	{
		if (context.selectedOffset > 0)
		{
			--context.selectedOffset;
		}
	}

	void TextRenderer::AutocompleteDown(FontIconAutocompleteContext& context)
	{
		if (context.resultCount < FontIconAutocompleteContext::maxResults)
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
			++context.selectedOffset;
		}
	}

	void TextRenderer::AutocompleteFill(const FontIconAutocompleteContext& context, std::span<char> buffer, int& cursor, bool shouldCloseFontIcon)
	{
		if (context.selectedOffset >= context.resultOffset + context.resultCount)
		{
			return;
		}

		if (cursor < 0 || static_cast<std::size_t>(cursor) >= buffer.size())
		{
			return;
		}

		const std::size_t start = static_cast<std::size_t>(cursor);
		const std::size_t afterLength = strnlen(buffer.data() + start, buffer.size() - start);

		if (start + afterLength + 1 > buffer.size())
		{
			return;
		}

		const auto& result = context.results[context.selectedOffset - context.resultOffset];
		std::string fill = result.iconName.substr(context.lastQuery.size());

		if (shouldCloseFontIcon)
		{
			fill.push_back(fontIconSeparator);
		}

		const std::size_t room = buffer.size() - start - afterLength - 1;

		if (fill.size() > room)
		{
			fill.resize(room);
		}

		if (fill.empty())
		{
			return;
		}

		const std::string after(buffer.data() + start, afterLength);

		std::memcpy(buffer.data() + start, fill.data(), fill.size());
		std::memcpy(buffer.data() + start + fill.size(), after.data(), after.size());
		buffer[start + fill.size() + after.size()] = '\0';

		cursor += static_cast<int>(fill.size());
	}

	bool TextRenderer::AutocompleteHandleKeyDown(FontIconAutocompleteContext& context, int key, std::span<char> buffer, int& cursor)
	{
		switch (key)
		{
		case keyUpArrow:
		case keyPadUpArrow:
			AutocompleteUp(context);
			return true;

		case keyDownArrow:
		case keyPadDownArrow:
			AutocompleteDown(context);
			return true;

		case keyEnter:
		case keyPadEnter:
			if (context.resultCount > 0)
			{
				AutocompleteFill(context, buffer, cursor, true);
				return true;
			}
			return false;

		case keyTab:
			AutocompleteFill(context, buffer, cursor, false);
			return true;

		case keyEscape:
			if (!context.didUserClose)
			{
				context.isActive = false;
				context.didUserClose = true;
				return true;
			}
			return false;

		default:
			return false;
		}
	}

	bool TextRenderer::HandleFontIconAutocompleteKey(FontIconAutocompleteInstance instance, int key, std::span<char> buffer, int& cursor)
	{
		if (instance >= FONT_ICON_ACI_COUNT)
		{
			return false;
		}

		auto& context = autocompleteContextArray[instance];

		if (!context.isActive)
		{
			return false;
		}

		return AutocompleteHandleKeyDown(context, key, buffer, cursor);
	}

	void TextRenderer::Message_Key_Hook(int localClientNum, int key)
	{
		auto* const chatField = reinterpret_cast<field_t*>(Utils::Hook::Rebase(playerKeys) + playerKeysStride * static_cast<std::size_t>(localClientNum));
		const int cursorBefore = chatField->cursor;

		if (HandleFontIconAutocompleteKey(FONT_ICON_ACI_CHAT, key, chatField->buffer, chatField->cursor))
		{
			if (chatField->cursor != cursorBefore)
			{
				auto* const placement = reinterpret_cast<const float*(*)(int)>(Utils::Hook::Rebase(ScrPlace_GetViewPlacement))(localClientNum);
				reinterpret_cast<void(*)(const float*, field_t*)>(Utils::Hook::Rebase(Field_AdjustScroll))(placement, chatField);
			}

			return;
		}

		reinterpret_cast<void(*)(int, int)>(messageKeyHooks[0].GetOriginal())(localClientNum, key);
	}

	int TextRenderer::SEH_PrintStrlenWithCursor(const char* string, const field_t* field)
	{
		if (!string)
		{
			return 0;
		}

		const auto readChar = reinterpret_cast<unsigned int(*)(const char**, int*)>(Utils::Hook::Rebase(SEH_ReadCharFromString));
		const int cursorPos = field->cursor;

		int length = 0;
		int lengthWithInvisibleTail = 0;
		int count = 0;
		const char* current = string;

		while (*current)
		{
			const unsigned int letter = readChar(&current, nullptr);
			lengthWithInvisibleTail = length;

			const bool isCursorInside = cursorPos > count && cursorPos < count + 2;

			if (letter == '^' && *current >= colorFirstChar && *current <= colorLastChar && !isCursorInside)
			{
				++current;
				++count;
			}
			else if (letter != '\r' && letter != '\n')
			{
				++length;
			}

			++count;
			++lengthWithInvisibleTail;
		}

		return lengthWithInvisibleTail;
	}

	int TextRenderer::Field_AdjustScroll_PrintLen(const char* buffer)
	{
		const auto* const field = reinterpret_cast<const field_t*>(buffer - offsetof(field_t, buffer));

		return SEH_PrintStrlenWithCursor(buffer, field);
	}

	unsigned int TextRenderer::HsvToRgb(HsvColor hsv)
	{
		if (hsv.s == 0)
		{
			return ColorRgb(hsv.v, hsv.v, hsv.v);
		}

		const unsigned int h = hsv.h;
		const unsigned int s = hsv.s;
		const unsigned int v = hsv.v;

		const auto region = static_cast<std::uint8_t>(h / 43);
		const unsigned int remainder = (h - (region * 43)) * 6;

		const auto p = static_cast<std::uint8_t>((v * (255 - s)) >> 8);
		const auto q = static_cast<std::uint8_t>((v * (255 - ((s * remainder) >> 8))) >> 8);
		const auto t = static_cast<std::uint8_t>((v * (255 - ((s * (255 - remainder)) >> 8))) >> 8);

		switch (region)
		{
		case 0:
			return ColorRgb(static_cast<std::uint8_t>(v), t, p);
		case 1:
			return ColorRgb(q, static_cast<std::uint8_t>(v), p);
		case 2:
			return ColorRgb(p, static_cast<std::uint8_t>(v), t);
		case 3:
			return ColorRgb(p, q, static_cast<std::uint8_t>(v));
		case 4:
			return ColorRgb(t, p, static_cast<std::uint8_t>(v));
		default:
			return ColorRgb(static_cast<std::uint8_t>(v), p, q);
		}
	}

	void TextRenderer::UpdateColorTable()
	{
		if (cg_newColors && !cg_newColors->current.enabled)
		{
			currentColorTable = &colorTableDefault;
		}
		else
		{
			currentColorTable = &colorTableNew;
		}

		const int milliseconds = reinterpret_cast<int(*)()>(Utils::Hook::Rebase(Sys_Milliseconds))();

		(*currentColorTable)[TEXT_COLOR_AXIS] = Utils::Hook::Get<unsigned int>(teamColorAxis);
		(*currentColorTable)[TEXT_COLOR_ALLIES] = Utils::Hook::Get<unsigned int>(teamColorAllies);
		(*currentColorTable)[TEXT_COLOR_RAINBOW] = HsvToRgb({ static_cast<std::uint8_t>((milliseconds / 200) % 256), 255, 255 });

		if (sv_customTextColor)
		{
			(*currentColorTable)[TEXT_COLOR_SERVER] = sv_customTextColor->current.unsignedInt;
		}
	}

	unsigned int TextRenderer::ColorIndex(const char index)
	{
		const int result = index - '0';

		if (result < 0 || result >= TEXT_COLOR_COUNT)
		{
			return TEXT_COLOR_DEFAULT;
		}

		return static_cast<unsigned int>(result);
	}

	void TextRenderer::StripColors(const char* in, char* out, std::size_t max)
	{
		if (!in || !out)
		{
			return;
		}

		--max;
		std::size_t current = 0;

		while (*in != 0 && current < max)
		{
			const char index = in[1];

			if (*in == '^' && (ColorIndex(index) != TEXT_COLOR_DEFAULT || index == '7'))
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
		char buffer[1024]{};
		StripColors(in.data(), buffer, sizeof(buffer));

		return std::string(buffer);
	}

	void TextRenderer::StripMaterialTextIcons(const char* in, char* out, std::size_t max)
	{
		if (!in || !out)
		{
			return;
		}

		--max;
		std::size_t current = 0;

		while (*in != 0 && current < max)
		{
			if (IsHudIcon(in))
			{
				in = SkipHudIcon(in);
				continue;
			}

			*out = *in;
			++out;
			++current;
			++in;
		}

		*out = '\0';
	}

	std::string TextRenderer::StripMaterialTextIcons(const std::string& in)
	{
		char buffer[1000]{};
		StripMaterialTextIcons(in.data(), buffer, sizeof(buffer));

		return std::string(buffer);
	}

	void TextRenderer::StripAllTextIcons(const char* in, char* out, std::size_t max)
	{
		if (!in || !out)
		{
			return;
		}

		--max;
		std::size_t current = 0;

		while (*in != 0 && current < max)
		{
			if (IsHudIcon(in))
			{
				in = SkipHudIcon(in);
				continue;
			}

			if (*in == fontIconSeparator && areFontIconsReady.load(std::memory_order_acquire))
			{
				const char* iconEnd = in + 1;
				FontIcon icon{};
				bool isFlippedHorizontally = false;
				bool isBig = false;

				if (TryReadFontIcon(iconEnd, icon, isFlippedHorizontally, isBig))
				{
					in = iconEnd;
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
		char buffer[1000]{};
		StripAllTextIcons(in.data(), buffer, sizeof(buffer));

		return std::string(buffer);
	}

	unsigned int TextRenderer::DrawText2D_ColorForChar(const char colorChar)
	{
		UpdateColorTable();

		return (*currentColorTable)[ColorIndex(colorChar)];
	}

	void TextRenderer::Dvar_GetUnpackedColorByName_Hook(const char* name, float* expandedColor)
	{
		if (r_colorBlind && r_colorBlind->current.enabled && g_ColorBlind_EnemyTeam && g_ColorBlind_MyTeam)
		{
			const Game::dvar_t* replacement = nullptr;

			if (std::strcmp(name, "g_TeamColor_EnemyTeam") == 0)
			{
				replacement = g_ColorBlind_EnemyTeam;
			}
			else if (std::strcmp(name, "g_TeamColor_MyTeam") == 0)
			{
				replacement = g_ColorBlind_MyTeam;
			}

			if (replacement)
			{
				for (int i = 0; i < 4; ++i)
				{
					expandedColor[i] = static_cast<float>(replacement->current.color[i]) / 255.0f;
				}

				return;
			}
		}

		reinterpret_cast<void(*)(const char*, float*)>(unpackedColorHooks[0].GetOriginal())(name, expandedColor);
	}

	void TextRenderer::RegisterDvars()
	{
		const auto registerColor = reinterpret_cast<Game::dvar_t*(*)(const char*, float, float, float, float, unsigned int, const char*)>(
			Utils::Hook::Rebase(Dvar_RegisterColor));

		cg_newColors = Dvar::Register("cg_newColors", true, Game::DVAR_ARCHIVE, "Use Warfare 2 color code style.").Get();
		sv_customTextColor = registerColor("sv_customTextColor", 1.0f, 0.7f, 0.0f, 1.0f, Game::DVAR_CODINFO, "Color for the extended color code.");
		r_colorBlind = Dvar::Register("r_colorBlind", false, Game::DVAR_ARCHIVE, "Use color-blindness-friendly colors").Get();
		g_ColorBlind_EnemyTeam = registerColor("g_ColorBlind_EnemyTeam", 0.659f, 0.088f, 0.145f, 1.0f, Game::DVAR_ARCHIVE, "Enemy team color for colorblind mode");
		g_ColorBlind_MyTeam = registerColor("g_ColorBlind_MyTeam", 1.0f, 0.859f, 0.125f, 1.0f, Game::DVAR_ARCHIVE, "Ally team color for colorblind mode");
	}

	void TextRenderer::RegisterAutocompleteDvars()
	{
		cg_fontIconAutocomplete = Dvar::Register("cg_fontIconAutocomplete", true, Game::DVAR_ARCHIVE, "Show autocomplete for fonticons when typing.").Get();
		cg_fontIconAutocompleteHint = Dvar::Register("cg_fontIconAutocompleteHint", true, Game::DVAR_ARCHIVE, "Show hint text in autocomplete for fonticons.").Get();
	}

	void TextRenderer::InitFontIconStrings()
	{
		const char modifierSeparator[] = { fontIconModifierSeparator, '\0' };
		const char flipHorizontally[] = { fontIconFlipHorizontally, '\0' };
		const char flipVertically[] = { fontIconFlipVertically, '\0' };
		const char big[] = { fontIconBig, '\0' };

		stringHintAutoComplete.Format("TAB");
		stringHintModifier.Format(modifierSeparator);
		stringListHeader.Cache();
		stringListFlipHorizontal.Format(flipHorizontally);
		stringListFlipVertical.Format(flipVertically);
		stringListBig.Format(big);
	}

	std::string TextRenderer::EncodeUtf8ForGame(const std::string_view text, const std::size_t maxCharacters)
	{
		if (text.empty() || maxCharacters == 0 || text.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
		{
			return {};
		}

		const auto textLength = static_cast<int>(text.size());
		const auto wideLength = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), textLength, nullptr, 0);

		if (wideLength <= 0)
		{
			return {};
		}

		std::wstring wideText(static_cast<std::size_t>(wideLength), L'\0');

		if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), textLength, wideText.data(), wideLength) != wideLength)
		{
			return {};
		}

		const auto characterLimit = std::min(maxCharacters, encodedCharacterLimit);

		std::vector<std::uint32_t> codepoints;
		codepoints.reserve(std::min(wideText.size(), characterLimit));

		for (std::size_t i = 0; i < wideText.size() && codepoints.size() < characterLimit;)
		{
			std::uint32_t codepoint = static_cast<std::uint16_t>(wideText[i]);
			std::size_t unitCount = 1;

			if (codepoint >= 0xD800 && codepoint <= 0xDBFF && i + 1 < wideText.size())
			{
				const auto trailing = static_cast<std::uint32_t>(static_cast<std::uint16_t>(wideText[i + 1]));

				if (trailing >= 0xDC00 && trailing <= 0xDFFF)
				{
					codepoint = 0x10000 + ((codepoint - 0xD800) << 10) + (trailing - 0xDC00);
					unitCount = 2;
				}
			}

			i += unitCount;

			if (!IsDroppedCodepoint(codepoint))
			{
				codepoints.push_back(codepoint);
			}
		}

		std::vector<std::uint32_t> clean;
		clean.reserve(codepoints.size());

		for (std::size_t i = 0; i < codepoints.size(); ++i)
		{
			const bool isColorCode = codepoints[i] == '^' && i + 1 < codepoints.size()
				&& codepoints[i + 1] >= static_cast<std::uint32_t>(colorFirstChar)
				&& codepoints[i + 1] <= static_cast<std::uint32_t>(colorLastChar);

			if (isColorCode)
			{
				++i;
				continue;
			}

			if (IsUnicodeWhitespace(codepoints[i]))
			{
				clean.push_back(0x20);
			}
			else
			{
				clean.push_back(codepoints[i]);
			}
		}

		std::string result;
		const bool canEscape = areUnicodeEscapesDrawn.load(std::memory_order_acquire);

		if (canEscape)
		{
			std::wstring wholeText;

			for (const auto codepoint : clean)
			{
				AppendUtf16(wholeText, codepoint);
			}

			std::vector<WORD> types(wholeText.size());
			bool isRightToLeft = false;

			if (!wholeText.empty() && GetStringTypeW(CT_CTYPE2, wholeText.data(), static_cast<int>(wholeText.size()), types.data()))
			{
				isRightToLeft = std::ranges::find(types, static_cast<WORD>(C2_RIGHTTOLEFT)) != types.end();
			}

			std::uint32_t runId = 0;

			if (isRightToLeft)
			{
				runId = RegisterUnicodeRun(wholeText);
			}

			if (runId)
			{
				result.push_back(inlineIconEscape);
				result.push_back(unicodeRunEscape);
				result.append(std::format("{:08X}", runId));
				return result;
			}
		}

		result.reserve(clean.size());

		std::string cluster;

		for (std::size_t start = 0; start < clean.size();)
		{
			const auto end = ClusterEnd(clean, start);

			if (TryConvertCluster(clean, start, end, cluster))
			{
				result.append(cluster);
				start = end;
				continue;
			}

			if (!canEscape)
			{
				start = end;
				continue;
			}

			if (end == start + 1 && clean[start] <= 0xFFFF)
			{
				result.push_back(inlineIconEscape);
				result.push_back(unicodeGlyphEscape);
				result.append(std::format("{:06X}", clean[start]));
				start = end;
				continue;
			}

			auto runEnd = end;

			while (runEnd < clean.size())
			{
				const auto nextEnd = ClusterEnd(clean, runEnd);

				if (TryConvertCluster(clean, runEnd, nextEnd, cluster))
				{
					break;
				}

				runEnd = nextEnd;
			}

			std::wstring runText;

			for (auto i = start; i < runEnd; ++i)
			{
				AppendUtf16(runText, clean[i]);
			}

			const auto runId = RegisterUnicodeRun(runText);

			if (runId)
			{
				result.push_back(inlineIconEscape);
				result.push_back(unicodeRunEscape);
				result.append(std::format("{:08X}", runId));
			}
			else
			{
				result.push_back('?');
			}

			start = runEnd;
		}

		return result;
	}

	TextRenderer::TextRenderer()
	{
		bool areFontIconsOn = false;

		bool areUnloadCallsIntact = true;

		for (const auto call : DB_UnloadXZoneCalls)
		{
			areUnloadCallsIntact = areUnloadCallsIntact && Utils::Hook::BranchesTo(call, DB_UnloadXZone, HOOK_CALL);
		}

		if (Utils::Hook::MatchesBytes(R_TextWidth_Entry, textWidthEntry, sizeof(textWidthEntry))
			&& Utils::Hook::MatchesBytes(DrawText2DCall, drawText2DCall, sizeof(drawText2DCall))
			&& areUnloadCallsIntact)
		{
			bool isSeated = textWidthHook.Initialize(R_TextWidth_Entry, reinterpret_cast<void*>(R_TextWidth), HOOK_JUMP)->Install()->IsInstalled();
			isSeated = drawText2DHook.Initialize(DrawText2DCall, reinterpret_cast<void*>(DrawText2D_Hook), HOOK_CALL)->Install()->IsInstalled() && isSeated;

			for (std::size_t i = 0; i < std::size(DB_UnloadXZoneCalls); ++i)
			{
				isSeated = unloadHooks[i].Initialize(DB_UnloadXZoneCalls[i], reinterpret_cast<void*>(DB_UnloadXZone_Hk), HOOK_CALL)->Install()->IsInstalled() && isSeated;
			}

			if (isSeated)
			{
				textWidthHook.Quick();
				drawText2DHook.Quick();

				for (auto& hook : unloadHooks)
				{
					hook.Quick();
				}

				areFontIconsOn = true;
				Events::AfterUIInit(InitFontIcons);

				Renderer::OnBackendFrame(BuildUnicodeTexts);
				Renderer::OnDeviceRecoveryBegin(DropUnicodeTextures);
				areUnicodeEscapesDrawn.store(true, std::memory_order_release);
			}
			else
			{
				textWidthHook.Uninstall();
				drawText2DHook.Uninstall();

				for (auto& hook : unloadHooks)
				{
					hook.Uninstall();
				}

				Logger::Error("textrenderer: could not seat the font icon hooks, font icons are off\n");
			}
		}
		else
		{
			Logger::Error("textrenderer: the text drawing code does not read as expected, font icons are off\n");
		}

		if (areFontIconsOn)
		{
			Scheduler::Once(RegisterAutocompleteDvars, Scheduler::Pipeline::MAIN);

			bool isChatExpected = Utils::Hook::MatchesBytes(Con_DrawSay_Field_DrawCall, fieldDrawCall, sizeof(fieldDrawCall));

			for (std::size_t i = 0; i < std::size(CL_KeyEvent_Message_KeyCalls); ++i)
			{
				isChatExpected = isChatExpected && Utils::Hook::MatchesBytes(CL_KeyEvent_Message_KeyCalls[i], messageKeyCallBytes[i], sizeof(messageKeyCallBytes[i]));
			}

			if (isChatExpected)
			{
				bool isChatSeated = fieldDrawSayHook.Initialize(Con_DrawSay_Field_DrawCall, reinterpret_cast<void*>(Field_Draw_Say), HOOK_CALL)->Install()->IsInstalled();

				for (std::size_t i = 0; i < std::size(CL_KeyEvent_Message_KeyCalls); ++i)
				{
					isChatSeated = messageKeyHooks[i].Initialize(CL_KeyEvent_Message_KeyCalls[i], reinterpret_cast<void*>(Message_Key_Hook), HOOK_CALL)
						->Install()->IsInstalled() && isChatSeated;
				}

				if (isChatSeated)
				{
					fieldDrawSayHook.Quick();

					for (auto& hook : messageKeyHooks)
					{
						hook.Quick();
					}
				}
				else
				{
					fieldDrawSayHook.Uninstall();

					for (auto& hook : messageKeyHooks)
					{
						hook.Uninstall();
					}

					Logger::Error("textrenderer: could not seat the chat hooks, no font icon autocomplete in chat\n");
				}
			}
			else
			{
				Logger::Error("textrenderer: the chat code does not read as expected, no font icon autocomplete in chat\n");
			}
		}

		bool isExpected = Utils::Hook::MatchesBytes(ColorIndex_Entry, colorIndexEntry, sizeof(colorIndexEntry))
			&& Utils::Hook::MatchesBytes(DrawText2D_ColorLookup, colorLookup, sizeof(colorLookup))
			&& Utils::Hook::MatchesBytes(Field_AdjustScroll_BufferArgument, printStrlenCall, sizeof(printStrlenCall));

		for (const auto& site : colorLimitSites)
		{
			isExpected = isExpected && Utils::Hook::MatchesBytes(site.instruction, site.bytes, site.length);
		}

		for (std::size_t i = 0; i < std::size(Dvar_GetUnpackedColorByNameCalls); ++i)
		{
			isExpected = isExpected && Utils::Hook::MatchesBytes(Dvar_GetUnpackedColorByNameCalls[i], unpackedColorCallBytes[i], sizeof(unpackedColorCallBytes[i]));
		}

		if (!isExpected)
		{
			Logger::Error("textrenderer: the text code does not read as expected, colour codes stay at ^9\n");
			return;
		}

		bool isSeated = colorIndexHook.Initialize(ColorIndex_Entry, reinterpret_cast<void*>(ColorIndex), HOOK_JUMP)->Install()->IsInstalled();
		isSeated = colorLookupHook.Initialize(DrawText2D_ColorIndexCall, reinterpret_cast<void*>(DrawText2D_ColorForChar), HOOK_CALL)->Install()->IsInstalled() && isSeated;
		isSeated = printLenHook.Initialize(Field_AdjustScroll_SEH_PrintStrlenCall, reinterpret_cast<void*>(Field_AdjustScroll_PrintLen), HOOK_CALL)->Install()->IsInstalled() && isSeated;

		for (std::size_t i = 0; i < std::size(Dvar_GetUnpackedColorByNameCalls); ++i)
		{
			isSeated = unpackedColorHooks[i].Initialize(Dvar_GetUnpackedColorByNameCalls[i], reinterpret_cast<void*>(Dvar_GetUnpackedColorByName_Hook), HOOK_CALL)
				->Install()->IsInstalled() && isSeated;
		}

		if (!isSeated)
		{
			colorIndexHook.Uninstall();
			colorLookupHook.Uninstall();
			printLenHook.Uninstall();

			for (auto& hook : unpackedColorHooks)
			{
				hook.Uninstall();
			}

			Logger::Error("textrenderer: could not seat the colour hooks, colour codes stay at ^9\n");
			return;
		}

		colorIndexHook.Quick();
		colorLookupHook.Quick();
		printLenHook.Quick();

		for (auto& hook : unpackedColorHooks)
		{
			hook.Quick();
		}

		Utils::Hook::Set<std::uint16_t>(DrawText2D_AfterColorIndex, jumpToColorApplied);

		for (const auto& site : colorLimitSites)
		{
			Utils::Hook::Set<std::uint8_t>(site.instruction + site.length - 1, static_cast<std::uint8_t>(TEXT_COLOR_COUNT - 1));
		}

		Scheduler::Once(RegisterDvars, Scheduler::Pipeline::MAIN);
	}
}
