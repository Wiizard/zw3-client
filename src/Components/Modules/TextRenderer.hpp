#pragma once

#include <span>

namespace Components
{
	enum TextColor : int
	{
		TEXT_COLOR_BLACK = 0,
		TEXT_COLOR_RED = 1,
		TEXT_COLOR_GREEN = 2,
		TEXT_COLOR_YELLOW = 3,
		TEXT_COLOR_BLUE = 4,
		TEXT_COLOR_LIGHT_BLUE = 5,
		TEXT_COLOR_PINK = 6,
		TEXT_COLOR_DEFAULT = 7,
		TEXT_COLOR_AXIS = 8,
		TEXT_COLOR_ALLIES = 9,
		TEXT_COLOR_RAINBOW = 10,
		TEXT_COLOR_SERVER = 11,

		TEXT_COLOR_REAL_PINK = 12,
		TEXT_COLOR_REAL_YELLOW = 13,
		TEXT_COLOR_DARK_PURPLE = 14,
		TEXT_COLOR_TEAL = 15,
		TEXT_COLOR_INVALIDCHAR = 16,
		TEXT_COLOR_OLIVE = 17,
		TEXT_COLOR_BLURPLE = 18,
		TEXT_COLOR_PURE_RED = 19,
		TEXT_COLOR_PURE_GREEN = 20,
		TEXT_COLOR_PURE_BLUE = 21,
		TEXT_COLOR_MAROON = 22,
		TEXT_COLOR_HOT_PINK = 23,
		TEXT_COLOR_MINT = 24,
		TEXT_COLOR_PEACH = 25,
		TEXT_COLOR_PASTEL_GREEN = 26,
		TEXT_COLOR_LIGHT_RED = 27,

		TEXT_COLOR_COUNT
	};

	constexpr unsigned int ColorRgba(const std::uint8_t r, const std::uint8_t g, const std::uint8_t b, const std::uint8_t a)
	{
		return r | (g << 8) | (b << 16) | (static_cast<unsigned int>(a) << 24);
	}

	constexpr unsigned int ColorRgb(const std::uint8_t r, const std::uint8_t g, const std::uint8_t b)
	{
		return ColorRgba(r, g, b, 0xFF);
	}

	constexpr int ColorIndexForChar(const char colorChar)
	{
		return colorChar - '0';
	}

	class TextRenderer : public Component
	{
	public:
		enum FontIconAutocompleteInstance : unsigned int
		{
			FONT_ICON_ACI_CONSOLE,
			FONT_ICON_ACI_CHAT,

			FONT_ICON_ACI_COUNT
		};

		struct field_t;

		TextRenderer();

		static void DrawConsoleAutocomplete(std::string_view typed, Game::Font_s* font, float x, float y);
		static bool HandleFontIconAutocompleteKey(FontIconAutocompleteInstance instance, int key, std::span<char> buffer, int& cursor);

		static void StripColors(const char* in, char* out, std::size_t max);
		static std::string StripColors(const std::string& in);
		static void StripMaterialTextIcons(const char* in, char* out, std::size_t max);
		static std::string StripMaterialTextIcons(const std::string& in);
		static void StripAllTextIcons(const char* in, char* out, std::size_t max);
		static std::string StripAllTextIcons(const std::string& in);

		static std::string EncodeUtf8ForGame(std::string_view text, std::size_t maxCharacters);

		static bool TryGetFontIconWidth(const char* text, const char*& end, float& width);

	private:
		struct HsvColor
		{
			unsigned char h;
			unsigned char s;
			unsigned char v;
		};

		struct FontIcon
		{
			std::string materialName;
			void* material;
			float aspect;
			bool isResolved;
		};

		class BufferedLocalizedString
		{
		public:
			explicit BufferedLocalizedString(const char* localizeReference);

			void Cache();
			const char* Format(const char* value);
			const char* GetString() const;
			int GetWidth(FontIconAutocompleteInstance instance, Game::Font_s* font);

		private:
			const char* reference;
			std::string text;
			int width[FONT_ICON_ACI_COUNT];
		};

		struct FontIconAutocompleteResult
		{
			std::string fontIconName;
			std::string iconName;
		};

		struct FontIconAutocompleteContext
		{
			static constexpr std::size_t maxResults = 10;

			bool isActive = false;
			bool isInModifiers = false;
			bool didUserClose = false;
			std::string lastQuery;
			FontIconAutocompleteResult results[maxResults];
			std::size_t resultCount = 0;
			bool hasMoreResults = false;
			std::size_t resultOffset = 0;
			std::size_t lastResultOffset = 0;
			std::size_t selectedOffset = 0;
			float maxFontIconWidth = 0.0f;
			float maxIconNameWidth = 0.0f;
			BufferedLocalizedString stringSearchStartWith{ "FONT_ICON_SEARCH_START_WITH" };
		};

		struct AutocompleteLayout
		{
			float x;
			float y;
			Game::Font_s* font;
			float xScale;
			float yScale;
		};

		static std::map<std::string, FontIcon> fontIcons;
		static std::mutex fontIconsMutex;
		static std::atomic<bool> areFontIconsReady;

		static FontIconAutocompleteContext autocompleteContextArray[FONT_ICON_ACI_COUNT];

		static BufferedLocalizedString stringHintAutoComplete;
		static BufferedLocalizedString stringHintModifier;
		static BufferedLocalizedString stringListHeader;
		static BufferedLocalizedString stringListFlipHorizontal;
		static BufferedLocalizedString stringListFlipVertical;
		static BufferedLocalizedString stringListBig;

		static Game::dvar_t* cg_fontIconAutocomplete;
		static Game::dvar_t* cg_fontIconAutocompleteHint;

		static bool IsAutocompleteEnabled();
		static void InitFontIconStrings();
		static void RegisterAutocompleteDvars();

		static void DrawAutocompleteBox(const FontIconAutocompleteContext& context, const AutocompleteLayout& layout, float width, unsigned int lineCount);
		static void DrawAutocompleteModifiers(FontIconAutocompleteInstance instance, const AutocompleteLayout& layout);
		static void DrawAutocompleteResults(FontIconAutocompleteInstance instance, const AutocompleteLayout& layout);
		static void DrawAutocomplete(FontIconAutocompleteInstance instance, const AutocompleteLayout& layout);
		static void UpdateAutocompleteContextResults(FontIconAutocompleteContext& context, Game::Font_s* font, float textXScale);
		static void UpdateAutocompleteContext(FontIconAutocompleteContext& context, std::string_view typed, Game::Font_s* font, float textXScale);

		static void AutocompleteUp(FontIconAutocompleteContext& context);
		static void AutocompleteDown(FontIconAutocompleteContext& context);
		static void AutocompleteFill(const FontIconAutocompleteContext& context, std::span<char> buffer, int& cursor, bool shouldCloseFontIcon);
		static bool AutocompleteHandleKeyDown(FontIconAutocompleteContext& context, int key, std::span<char> buffer, int& cursor);

		static void Field_Draw_Say(int localClientNum, field_t* edit, int x, int y, int horzAlign, int vertAlign);
		static void Message_Key_Hook(int localClientNum, int key);

		static int SEH_PrintStrlenWithCursor(const char* string, const field_t* field);
		static int Field_AdjustScroll_PrintLen(const char* buffer);

		static bool TryReadFontIcon(const char*& text, FontIcon& icon, bool& isFlippedHorizontally, bool& isBig);
		static bool TranslateText(const char* text, bool isEditing, std::string& translated, std::vector<std::size_t>& unicodeIcons);
		static void InitFontIcons();
		static bool TryResolveFontIcon(FontIcon& icon);
		static void DB_UnloadXZone_Hk(unsigned int zoneIndex, bool shouldCreateDefault);

		static void DrawText2D_Hook(const char* text, float x, float y, void* font, float xScale, float yScale,
			float sinAngle, float cosAngle, unsigned int color, int maxLength, int renderFlags, int cursorPos, char cursorLetter,
			float padding, unsigned int glowForcedColor, int fxBirthTime, int fxLetterTime, int fxDecayStartTime,
			int fxDecayDuration, void* fxMaterial, void* fxMaterialGlow);
		static int R_TextWidth(const char* text, int maxChars, void* font);

		static unsigned int colorTableDefault[TEXT_COLOR_COUNT];
		static unsigned int colorTableNew[TEXT_COLOR_COUNT];
		static unsigned int(*currentColorTable)[TEXT_COLOR_COUNT];

		static Game::dvar_t* cg_newColors;
		static Game::dvar_t* sv_customTextColor;
		static Game::dvar_t* r_colorBlind;
		static Game::dvar_t* g_ColorBlind_EnemyTeam;
		static Game::dvar_t* g_ColorBlind_MyTeam;

		static unsigned int HsvToRgb(HsvColor hsv);
		static void UpdateColorTable();

		static unsigned int ColorIndex(char index);
		static unsigned int DrawText2D_ColorForChar(char colorChar);
		static void Dvar_GetUnpackedColorByName_Hook(const char* name, float* expandedColor);

		static void RegisterDvars();
	};
}
