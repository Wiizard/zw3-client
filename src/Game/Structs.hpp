#pragma once

#define NUM_CUSTOM_CLASSES 15

namespace Game
{

	struct XNKID
	{
		unsigned char ab[8];
	};

	struct XNADDR
	{
		in_addr ina;
		in_addr inaOnline;
		unsigned short wPortOnline;
		unsigned char abEnet[6];
		unsigned char abOnline[20];
	};

	struct XNKEY
	{
		unsigned char ab[16];
	};

	struct _XSESSION_INFO
	{
		XNKID sessionID;
		XNADDR hostAddress;
		XNKEY keyExchangeKey;
	};

	static_assert(sizeof(XNADDR) == 36, "XNADDR must be 36 bytes");
	static_assert(sizeof(_XSESSION_INFO) == 60, "_XSESSION_INFO must be 60 bytes");
	typedef float vec2_t[2];
	typedef float vec3_t[3];
	typedef float vec4_t[4];

	struct cmd_function_s
	{
		cmd_function_s* next;
		const char* name;
		void(*function)();
	};

	AssertSize(cmd_function_s, 0x18);

	struct CmdArgs
	{
		int nesting;
		int localClientNum[8];
		int controllerIndex[8];
		int argc[8];
		int pad;
		const char** argv[8];
	};

	AssertSize(CmdArgs, 0xA8);
	AssertOffset(CmdArgs, argc, 0x44);
	AssertOffset(CmdArgs, argv, 0x68);

	enum dvar_type : unsigned char
	{
		DVAR_TYPE_BOOL = 0,
		DVAR_TYPE_FLOAT = 1,
		DVAR_TYPE_FLOAT_2 = 2,
		DVAR_TYPE_FLOAT_3 = 3,
		DVAR_TYPE_FLOAT_4 = 4,
		DVAR_TYPE_INT = 5,
		DVAR_TYPE_ENUM = 6,
		DVAR_TYPE_STRING = 7,
		DVAR_TYPE_COLOR = 8,
		DVAR_TYPE_FLOAT_3_COLOR = 9,
	};

	enum dvar_flag : unsigned int
	{
		DVAR_NONE = 0x0,
		DVAR_ARCHIVE = 0x1,
		DVAR_LATCH = 0x2,
		DVAR_CHEAT = 0x4,
		DVAR_CODINFO = 0x8,
		DVAR_SCRIPTINFO = 0x10,
		DVAR_INTERNAL = 0x80,
		DVAR_EXTERNAL = 0x100,
		DVAR_USERINFO = 0x200,
		DVAR_SERVERINFO = 0x400,
		DVAR_INIT = 0x800,
		DVAR_SYSTEMINFO = 0x1000,
		DVAR_ROM = 0x2000,
	};

	union DvarValue
	{
		bool enabled;
		int integer;
		unsigned int unsignedInt;
		float value;
		vec4_t vector;
		const char* string;
		unsigned char color[4];
	};

	AssertSize(DvarValue, 0x10);

	union DvarLimits
	{
		struct
		{
			int stringCount;
			const char** strings;
		} enumeration;

		struct
		{
			int min;
			int max;
		} integer;

		struct
		{
			float min;
			float max;
		} value;

		unsigned char raw[16];
	};

	AssertSize(DvarLimits, 0x10);

	struct dvar_t
	{
		const char* name;
		unsigned int flags;
		dvar_type type;
		bool modified;
		unsigned char pad[2];
		DvarValue current;
		DvarValue latched;
		DvarValue reset;
		DvarLimits domain;
		bool(*domainFunc)(const dvar_t*, DvarValue*);
		dvar_t* hashNext;
	};

	AssertSize(dvar_t, 0x60);
	AssertOffset(dvar_t, current, 0x10);
	AssertOffset(dvar_t, latched, 0x20);
	AssertOffset(dvar_t, reset, 0x30);
	AssertOffset(dvar_t, domain, 0x40);
	AssertOffset(dvar_t, hashNext, 0x58);

	enum errorParm_t
	{
		ERR_FATAL = 0x0,
		ERR_DROP = 0x1,
		ERR_SERVERDISCONNECT = 0x2,
		ERR_DISCONNECT = 0x3,
		ERR_SCRIPT = 0x4,
		ERR_SCRIPT_DROP = 0x5,
		ERR_LOCALIZATION = 0x6,
		ERR_MAPLOADERRORSUMMARY = 0x7,
	};

	enum netadrtype_t : int
	{
		NA_BOT = 0,
		NA_BAD = 1,
		NA_LOOPBACK = 2,
		NA_BROADCAST = 3,
		NA_IP = 4,
		NA_IPX = 5,
		NA_BROADCAST_IPX = 6,
	};

	enum netsrc_t : int
	{
		NS_CLIENT1 = 0,
		NS_SERVER = 1,
		NS_PACKET = 2,
	};

	union netIP_t
	{
		unsigned char bytes[4];
		std::uint32_t full;
	};

	struct netadr_t
	{
		netadrtype_t type;
		netIP_t ip;
		unsigned short port;
		char ipx[10];
	};

	AssertSize(netadr_t, 20);
	AssertOffset(netadr_t, ip, 4);
	AssertOffset(netadr_t, port, 8);

	struct msg_t
	{
		int overflowed;
		int readOnly;
		unsigned char* data;
		unsigned char* splitData;
		int maxsize;
		int cursize;
		int splitSize;
		int readcount;
		int bit;
		int lastEntityRef;
	};

	AssertSize(msg_t, 48);
	AssertOffset(msg_t, data, 8);
	AssertOffset(msg_t, maxsize, 24);
	AssertOffset(msg_t, readcount, 36);
	AssertOffset(msg_t, bit, 40);

	struct usercmd_s
	{
		int serverTime;
		int buttons;
		int angles[3];
		unsigned short weapon;
		unsigned short primaryWeaponForAltMode;
		unsigned short offHandIndex;
		char forwardmove;
		char rightmove;
		float meleeChargeYaw;
		char meleeChargeDist;
		char selectedLoc[2];
		char selectedLocAngle;
		char remoteControlAngles[2];
	};

	AssertSize(usercmd_s, 40);
	AssertOffset(usercmd_s, forwardmove, 26);
	AssertOffset(usercmd_s, rightmove, 27);

	struct Material;
	struct ScreenPlacement;
	struct XModel;
	struct FxEffectDef;
	struct snd_alias_list_t;
	struct PhysCollmap;
	struct TracerDef;

	struct Font_s
	{
		const char* fontName;
		int pixelHeight;
		int glyphCount;
		Material* material;
		Material* glowMaterial;
		const void* glyphs;
	};

	AssertSize(Font_s, 0x28);
	AssertOffset(Font_s, fontName, 0x0);
	AssertOffset(Font_s, pixelHeight, 0x8);
	AssertOffset(Font_s, glyphCount, 0xC);
	AssertOffset(Font_s, material, 0x10);
	AssertOffset(Font_s, glowMaterial, 0x18);
	AssertOffset(Font_s, glyphs, 0x20);

	struct Statement_s;
	struct MenuEventHandlerSet;
	struct menuDef_t;
	struct snd_alias_list_t;

	struct rectDef_s
	{
		float x;
		float y;
		float w;
		float h;
		unsigned char horzAlign;
		unsigned char vertAlign;
	};

	AssertSize(rectDef_s, 20);

	struct windowDef_t
	{
		const char* name;
		rectDef_s rect;
		rectDef_s rectClient;
		const char* group;
		int style;
		int border;
		int ownerDraw;
		int ownerDrawFlags;
		float borderSize;
		int staticFlags;
		int dynamicFlags[1];
		int nextTime;
		float foreColor[4];
		float backColor[4];
		float borderColor[4];
		float outlineColor[4];
		float disableColor[4];
		Material* background;
	};

	AssertSize(windowDef_t, 176);
	AssertOffset(windowDef_t, rect, 8);
	AssertOffset(windowDef_t, rectClient, 28);
	AssertOffset(windowDef_t, group, 48);
	AssertOffset(windowDef_t, style, 56);
	AssertOffset(windowDef_t, ownerDraw, 64);
	AssertOffset(windowDef_t, staticFlags, 76);
	AssertOffset(windowDef_t, dynamicFlags, 80);
	AssertOffset(windowDef_t, foreColor, 88);
	AssertOffset(windowDef_t, backColor, 104);
	AssertOffset(windowDef_t, borderColor, 120);
	AssertOffset(windowDef_t, outlineColor, 136);
	AssertOffset(windowDef_t, disableColor, 152);
	AssertOffset(windowDef_t, background, 168);

	enum WindowStaticFlags : unsigned int
	{
		WINDOW_STATIC_DECORATION = 0x100000,
		WINDOW_STATIC_HORIZONTAL_SCROLL = 0x200000,
		WINDOW_STATIC_SCREEN_SPACE = 0x400000,
		WINDOW_STATIC_AUTO_WRAPPED = 0x800000,
		WINDOW_STATIC_POPUP = 0x1000000,
		WINDOW_STATIC_OUT_OF_BOUNDS_CLICK = 0x2000000,
		WINDOW_STATIC_LEGACY_SPLITSCREEN_SCALE = 0x4000000,
		WINDOW_STATIC_HIDDEN_DURING_FLASHBANG = 0x10000000,
		WINDOW_STATIC_HIDDEN_DURING_SCOPE = 0x20000000,
		WINDOW_STATIC_HIDDEN_DURING_UI = 0x40000000,
		WINDOW_STATIC_TEXT_ONLY_FOCUS = 0x80000000,
	};

	enum WindowDynamicFlags : unsigned int
	{
		WINDOW_DYNAMIC_VISIBLE = 0x4,
		WINDOW_DYNAMIC_FORECOLOR_SET = 0x10000,
	};

	enum expDataType : int
	{
		VAL_INT = 0,
		VAL_FLOAT = 1,
		VAL_STRING = 2,
		VAL_FUNCTION = 3,
	};

	struct ExpressionString
	{
		const char* string;
	};

	AssertSize(ExpressionString, 0x8);

	union operandInternalDataUnion
	{
		int intVal;
		float floatVal;
		ExpressionString stringVal;
		Statement_s* function;
	};

	AssertSize(operandInternalDataUnion, 0x8);

	struct Operand
	{
		expDataType dataType;
		operandInternalDataUnion internals;
	};

	AssertSize(Operand, 16);
	AssertOffset(Operand, internals, 8);

	enum operationEnum : int
	{
		OP_NOOP = 0,
		OP_RIGHTPAREN = 1,
		OP_SUBTRACT = 6,
		OP_LEFTPAREN = 16,
		OP_LAST_SYMBOL = 22,
		OP_COUNT = 196,
	};

	enum expressionEntryType : int
	{
		EET_OPERATOR = 0,
		EET_OPERAND = 1,
	};

	union entryInternalData
	{
		int op;
		Operand operand;
	};

	AssertSize(entryInternalData, 0x10);

	struct expressionEntry
	{
		int type;
		entryInternalData data;
	};

	AssertSize(expressionEntry, 24);
	AssertOffset(expressionEntry, data, 8);

	struct UIFunctionList
	{
		int totalFunctions;
		Statement_s** functions;
	};

	AssertSize(UIFunctionList, 0x10);

	struct StaticDvar
	{
		dvar_t* dvar;
		char* dvarName;
	};

	AssertSize(StaticDvar, 0x10);

	struct StaticDvarList
	{
		int numStaticDvars;
		StaticDvar** staticDvars;
	};

	AssertSize(StaticDvarList, 0x10);

	struct StringList
	{
		int totalStrings;
		const char** strings;
	};

	AssertSize(StringList, 0x10);

	struct ExpressionSupportingData
	{
		UIFunctionList uifunctions;
		StaticDvarList staticDvarList;
		StringList uiStrings;
	};

	AssertSize(ExpressionSupportingData, 48);

	struct Statement_s
	{
		int numEntries;
		expressionEntry* entries;
		ExpressionSupportingData* supportingData;
		int lastExecuteTime;
		Operand lastResult;
	};

	AssertSize(Statement_s, 48);
	AssertOffset(Statement_s, entries, 8);
	AssertOffset(Statement_s, supportingData, 16);
	AssertOffset(Statement_s, lastExecuteTime, 24);

	enum EventType : int
	{
		EVENT_UNCONDITIONAL = 0,
		EVENT_IF = 1,
		EVENT_ELSE = 2,
		EVENT_SET_LOCAL_VAR_BOOL = 3,
		EVENT_SET_LOCAL_VAR_INT = 4,
		EVENT_SET_LOCAL_VAR_FLOAT = 5,
		EVENT_SET_LOCAL_VAR_STRING = 6,
	};

	struct ConditionalScript
	{
		MenuEventHandlerSet* eventHandlerSet;
		Statement_s* eventExpression;
	};

	AssertSize(ConditionalScript, 0x10);

	struct SetLocalVarData
	{
		const char* localVarName;
		Statement_s* expression;
	};

	AssertSize(SetLocalVarData, 0x10);

	union EventData
	{
		const char* unconditionalScript;
		ConditionalScript* conditionalScript;
		MenuEventHandlerSet* elseScript;
		SetLocalVarData* setLocalVarData;
	};

	AssertSize(EventData, 0x8);

	struct MenuEventHandler
	{
		EventData eventData;
		char eventType;
	};

	AssertSize(MenuEventHandler, 16);
	AssertOffset(MenuEventHandler, eventType, 8);

	struct MenuEventHandlerSet
	{
		int eventHandlerCount;
		MenuEventHandler** eventHandlers;
	};

	AssertSize(MenuEventHandlerSet, 16);

	struct ItemKeyHandler
	{
		int key;
		MenuEventHandlerSet* action;
		ItemKeyHandler* next;
	};

	AssertSize(ItemKeyHandler, 24);
	AssertOffset(ItemKeyHandler, next, 16);

	struct columnInfo_s
	{
		int pos;
		int width;
		int maxChars;
		int alignment;
	};

	AssertSize(columnInfo_s, 0x10);

	struct listBoxDef_s
	{
		int mousePos;
		int startPos[1];
		int endPos[1];
		int drawPadding;
		float elementWidth;
		float elementHeight;
		int elementStyle;
		int numColumns;
		columnInfo_s columnInfo[16];
		MenuEventHandlerSet* onDoubleClick;
		int notselectable;
		int noScrollBars;
		int usePaging;
		float selectBorder[4];
		Material* selectIcon;
	};

	AssertSize(listBoxDef_s, 336);
	AssertOffset(listBoxDef_s, elementWidth, 16);
	AssertOffset(listBoxDef_s, numColumns, 28);
	AssertOffset(listBoxDef_s, columnInfo, 32);
	AssertOffset(listBoxDef_s, onDoubleClick, 288);
	AssertOffset(listBoxDef_s, noScrollBars, 300);
	AssertOffset(listBoxDef_s, selectBorder, 308);
	AssertOffset(listBoxDef_s, selectIcon, 328);

	struct editFieldDef_s
	{
		float minVal;
		float maxVal;
		float defVal;
		float range;
		int maxChars;
		int maxCharsGotoNext;
		int maxPaintChars;
		int paintOffset;
	};

	AssertSize(editFieldDef_s, 32);

	struct multiDef_s
	{
		const char* dvarList[32];
		const char* dvarStr[32];
		float dvarValue[32];
		int count;
		int strDef;
	};

	AssertSize(multiDef_s, 648);

	struct newsTickerDef_s
	{
		int feedId;
		int speed;
		int spacing;
		int lastTime;
		int start;
		int end;
		float x;
	};

	AssertSize(newsTickerDef_s, 28);

	struct textScrollDef_s
	{
		int startTime;
	};

	AssertSize(textScrollDef_s, 0x4);

	union itemDefData_t
	{
		listBoxDef_s* listBox;
		editFieldDef_s* editField;
		multiDef_s* multi;
		const char* enumDvarName;
		newsTickerDef_s* ticker;
		textScrollDef_s* scroll;
		void* data;
	};

	AssertSize(itemDefData_t, 0x8);

	struct ItemFloatExpression
	{
		int target;
		Statement_s* expression;
	};

	AssertSize(ItemFloatExpression, 16);

	enum ItemDefType : int
	{
		ITEM_TYPE_TEXT = 0,
		ITEM_TYPE_BUTTON = 1,
		ITEM_TYPE_RADIOBUTTON = 2,
		ITEM_TYPE_CHECKBOX = 3,
		ITEM_TYPE_EDITFIELD = 4,
		ITEM_TYPE_COMBO = 5,
		ITEM_TYPE_LISTBOX = 6,
		ITEM_TYPE_MODEL = 7,
		ITEM_TYPE_OWNERDRAW = 8,
		ITEM_TYPE_NUMERICFIELD = 9,
		ITEM_TYPE_SLIDER = 10,
		ITEM_TYPE_YESNO = 11,
		ITEM_TYPE_MULTI = 12,
		ITEM_TYPE_DVARENUM = 13,
		ITEM_TYPE_BIND = 14,
		ITEM_TYPE_MENUMODEL = 15,
		ITEM_TYPE_VALIDFILEFIELD = 16,
		ITEM_TYPE_DECIMALFIELD = 17,
		ITEM_TYPE_UPREDITFIELD = 18,
		ITEM_TYPE_GAME_MESSAGE_WINDOW = 19,
		ITEM_TYPE_NEWS_TICKER = 20,
		ITEM_TYPE_TEXT_SCROLL = 21,
		ITEM_TYPE_EMAILFIELD = 22,
		ITEM_TYPE_PASSWORDFIELD = 23,
	};

	constexpr unsigned int EDIT_FIELD_TYPE_MASK = 0xC74E11;

	struct itemDef_s
	{
		windowDef_t window;
		rectDef_s textRect[1];
		int type;
		int dataType;
		int alignment;
		int fontEnum;
		int textAlignMode;
		float textalignx;
		float textaligny;
		float textscale;
		int textStyle;
		int gameMsgWindowIndex;
		int gameMsgWindowMode;
		const char* text;
		int itemFlags;
		menuDef_t* parent;
		MenuEventHandlerSet* mouseEnterText;
		MenuEventHandlerSet* mouseExitText;
		MenuEventHandlerSet* mouseEnter;
		MenuEventHandlerSet* mouseExit;
		MenuEventHandlerSet* action;
		MenuEventHandlerSet* accept;
		MenuEventHandlerSet* onFocus;
		MenuEventHandlerSet* leaveFocus;
		const char* dvar;
		const char* dvarTest;
		ItemKeyHandler* onKey;
		const char* enableDvar;
		const char* localVar;
		int dvarFlags;
		snd_alias_list_t* focusSound;
		float special;
		int cursorPos[1];
		itemDefData_t typeData;
		int imageTrack;
		int floatExpressionCount;
		ItemFloatExpression* floatExpressions;
		Statement_s* visibleExp;
		Statement_s* disabledExp;
		Statement_s* textExp;
		Statement_s* materialExp;
		float glowColor[4];
		bool decayActive;
		int fxBirthTime;
		int fxLetterTime;
		int fxDecayStartTime;
		int fxDecayDuration;
		int lastSoundPlayedTime;
	};

	AssertSize(itemDef_s, 488);
	AssertOffset(itemDef_s, textRect, 176);
	AssertOffset(itemDef_s, type, 196);
	AssertOffset(itemDef_s, dataType, 200);
	AssertOffset(itemDef_s, fontEnum, 208);
	AssertOffset(itemDef_s, textscale, 224);
	AssertOffset(itemDef_s, text, 240);
	AssertOffset(itemDef_s, parent, 256);
	AssertOffset(itemDef_s, mouseEnterText, 264);
	AssertOffset(itemDef_s, action, 296);
	AssertOffset(itemDef_s, onFocus, 312);
	AssertOffset(itemDef_s, leaveFocus, 320);
	AssertOffset(itemDef_s, dvar, 328);
	AssertOffset(itemDef_s, dvarTest, 336);
	AssertOffset(itemDef_s, onKey, 344);
	AssertOffset(itemDef_s, enableDvar, 352);
	AssertOffset(itemDef_s, localVar, 360);
	AssertOffset(itemDef_s, dvarFlags, 368);
	AssertOffset(itemDef_s, focusSound, 376);
	AssertOffset(itemDef_s, special, 384);
	AssertOffset(itemDef_s, cursorPos, 388);
	AssertOffset(itemDef_s, typeData, 392);
	AssertOffset(itemDef_s, floatExpressionCount, 404);
	AssertOffset(itemDef_s, floatExpressions, 408);
	AssertOffset(itemDef_s, visibleExp, 416);
	AssertOffset(itemDef_s, disabledExp, 424);
	AssertOffset(itemDef_s, textExp, 432);
	AssertOffset(itemDef_s, materialExp, 440);

	enum ItemDvarFlags : unsigned int
	{
		ITEM_DVAR_FLAG_ENABLE = 0x1,
		ITEM_DVAR_FLAG_DISABLE = 0x2,
		ITEM_DVAR_FLAG_SHOW = 0x4,
		ITEM_DVAR_FLAG_HIDE = 0x8,
	};

	struct menuTransition
	{
		int transitionType;
		int targetField;
		int startTime;
		float startVal;
		float endVal;
		float time;
		int endTriggerType;
	};

	AssertSize(menuTransition, 28);

	struct menuDef_t
	{
		windowDef_t window;
		const char* font;
		int fullScreen;
		int itemCount;
		int fontIndex;
		int cursorItem[1];
		int fadeCycle;
		float fadeClamp;
		float fadeAmount;
		float fadeInAmount;
		float blurRadius;
		MenuEventHandlerSet* onOpen;
		MenuEventHandlerSet* onCloseRequest;
		MenuEventHandlerSet* onClose;
		MenuEventHandlerSet* onESC;
		ItemKeyHandler* onKey;
		Statement_s* visibleExp;
		const char* allowedBinding;
		const char* soundName;
		int imageTrack;
		float focusColor[4];
		Statement_s* rectXExp;
		Statement_s* rectYExp;
		Statement_s* rectWExp;
		Statement_s* rectHExp;
		Statement_s* openSoundExp;
		Statement_s* closeSoundExp;
		itemDef_s** items;
		menuTransition scaleTransition[1];
		menuTransition alphaTransition[1];
		menuTransition xTransition[1];
		menuTransition yTransition[1];
		ExpressionSupportingData* expressionData;
	};

	AssertSize(menuDef_t, 488);
	AssertOffset(menuDef_t, fullScreen, 184);
	AssertOffset(menuDef_t, itemCount, 188);
	AssertOffset(menuDef_t, onOpen, 224);
	AssertOffset(menuDef_t, onCloseRequest, 232);
	AssertOffset(menuDef_t, onClose, 240);
	AssertOffset(menuDef_t, onESC, 248);
	AssertOffset(menuDef_t, onKey, 256);
	AssertOffset(menuDef_t, visibleExp, 264);
	AssertOffset(menuDef_t, allowedBinding, 272);
	AssertOffset(menuDef_t, soundName, 280);
	AssertOffset(menuDef_t, focusColor, 292);
	AssertOffset(menuDef_t, rectXExp, 312);
	AssertOffset(menuDef_t, rectYExp, 320);
	AssertOffset(menuDef_t, rectWExp, 328);
	AssertOffset(menuDef_t, rectHExp, 336);
	AssertOffset(menuDef_t, openSoundExp, 344);
	AssertOffset(menuDef_t, closeSoundExp, 352);
	AssertOffset(menuDef_t, items, 360);
	AssertOffset(menuDef_t, scaleTransition, 368);
	AssertOffset(menuDef_t, alphaTransition, 396);
	AssertOffset(menuDef_t, xTransition, 424);
	AssertOffset(menuDef_t, yTransition, 452);
	AssertOffset(menuDef_t, expressionData, 480);

	struct MenuList
	{
		const char* name;
		int menuCount;
		menuDef_t** menus;
	};

	AssertSize(MenuList, 24);
	AssertOffset(MenuList, menuCount, 8);
	AssertOffset(MenuList, menus, 16);

	enum UILocalVarType : int
	{
		UILOCALVAR_INT = 0,
		UILOCALVAR_FLOAT = 1,
		UILOCALVAR_STRING = 2,
	};

	struct UILocalVar
	{
		UILocalVarType type;
		const char* name;
		union
		{
			int integer;
			float value;
			const char* string;
		} u;
	};

	struct UILocalVarContext
	{
		UILocalVar table[256];
	};

	struct UiContext
	{
		int localClientNum;
		float bias;
		int realTime;
		int frameTime;
		struct
		{
			float x;
			float y;
			int lastMoveTime;
		} cursor;
		int isCursorVisible;
		int paintFull;
		int screenWidth;
		int screenHeight;
		float screenAspect;
		float FPS;
		float blurRadiusOut;
		menuDef_t* Menus[640];
		int menuCount;
		menuDef_t* menuStack[16];
		int openMenuCount;
		UILocalVarContext localVars;
	};

	AssertSize(UiContext, 11464);
	AssertOffset(UiContext, Menus, 56);
	AssertOffset(UiContext, menuCount, 5176);
	AssertOffset(UiContext, menuStack, 5184);

	struct gameTypeName_t
	{
		char gameType[12];
		char uiName[32];
	};

	AssertSize(gameTypeName_t, 44);

	struct NetField
	{
		const char* name;
		std::intptr_t offset;
		int bits;
	};

	AssertSize(NetField, 24);
	AssertOffset(NetField, bits, 16);

	struct WeaponDef;

	struct WeaponCompleteDef
	{
		const char* szInternalName;
		WeaponDef* weapDef;
		const char* szDisplayName;
		unsigned short* hideTags;
		const char** szXAnims;
		float fAdsZoomFov;
		int iAdsTransInTime;
		int iAdsTransOutTime;
		int iClipSize;
		int impactType;
		int iFireTime;
		int dpadIconRatio;
		float penetrateMultiplier;
		float fAdsViewKickCenterSpeed;
		float fHipViewKickCenterSpeed;
		const char* szAltWeaponName;
		unsigned int altWeaponIndex;
		int iAltRaiseTime;
		Material* killIcon;
		Material* dpadIcon;
		int fireAnimLength;
		int iFirstRaiseTime;
		int ammoDropStockMax;
		float adsDofStart;
		float adsDofEnd;
		unsigned short accuracyGraphKnotCount[2];
		float(*accuracyGraphKnots[2])[2];
		bool motionTracker;
		bool enhanced;
		bool dpadIconShowsAmmo;
	};

	struct WeaponDef
	{
		const char* szOverlayName;
		XModel** gunXModel;
		XModel* handXModel;
		const char** szXAnimsRightHanded;
		const char** szXAnimsLeftHanded;
		const char* szModeName;
		unsigned short* notetrackSoundMapKeys;
		unsigned short* notetrackSoundMapValues;
		unsigned short* notetrackRumbleMapKeys;
		unsigned short* notetrackRumbleMapValues;
		int playerAnimType;
		int weapType;
		int weapClass;
		int penetrateType;
		int inventoryType;
		int fireType;
		int offhandClass;
		int stance;
		FxEffectDef* viewFlashEffect;
		FxEffectDef* worldFlashEffect;
		snd_alias_list_t* pickupSound;
		snd_alias_list_t* pickupSoundPlayer;
		snd_alias_list_t* ammoPickupSound;
		snd_alias_list_t* ammoPickupSoundPlayer;
		snd_alias_list_t* projectileSound;
		snd_alias_list_t* pullbackSound;
		snd_alias_list_t* pullbackSoundPlayer;
		snd_alias_list_t* fireSound;
		snd_alias_list_t* fireSoundPlayer;
		snd_alias_list_t* fireSoundPlayerAkimbo;
		snd_alias_list_t* fireLoopSound;
		snd_alias_list_t* fireLoopSoundPlayer;
		snd_alias_list_t* fireStopSound;
		snd_alias_list_t* fireStopSoundPlayer;
		snd_alias_list_t* fireLastSound;
		snd_alias_list_t* fireLastSoundPlayer;
		snd_alias_list_t* emptyFireSound;
		snd_alias_list_t* emptyFireSoundPlayer;
		snd_alias_list_t* meleeSwipeSound;
		snd_alias_list_t* meleeSwipeSoundPlayer;
		snd_alias_list_t* meleeHitSound;
		snd_alias_list_t* meleeMissSound;
		snd_alias_list_t* rechamberSound;
		snd_alias_list_t* rechamberSoundPlayer;
		snd_alias_list_t* reloadSound;
		snd_alias_list_t* reloadSoundPlayer;
		snd_alias_list_t* reloadEmptySound;
		snd_alias_list_t* reloadEmptySoundPlayer;
		snd_alias_list_t* reloadStartSound;
		snd_alias_list_t* reloadStartSoundPlayer;
		snd_alias_list_t* reloadEndSound;
		snd_alias_list_t* reloadEndSoundPlayer;
		snd_alias_list_t* detonateSound;
		snd_alias_list_t* detonateSoundPlayer;
		snd_alias_list_t* nightVisionWearSound;
		snd_alias_list_t* nightVisionWearSoundPlayer;
		snd_alias_list_t* nightVisionRemoveSound;
		snd_alias_list_t* nightVisionRemoveSoundPlayer;
		snd_alias_list_t* altSwitchSound;
		snd_alias_list_t* altSwitchSoundPlayer;
		snd_alias_list_t* raiseSound;
		snd_alias_list_t* raiseSoundPlayer;
		snd_alias_list_t* firstRaiseSound;
		snd_alias_list_t* firstRaiseSoundPlayer;
		snd_alias_list_t* putawaySound;
		snd_alias_list_t* putawaySoundPlayer;
		snd_alias_list_t* scanSound;
		snd_alias_list_t** bounceSound;
		FxEffectDef* viewShellEjectEffect;
		FxEffectDef* worldShellEjectEffect;
		FxEffectDef* viewLastShotEjectEffect;
		FxEffectDef* worldLastShotEjectEffect;
		Material* reticleCenter;
		Material* reticleSide;
		int iReticleCenterSize;
		int iReticleSideSize;
		int iReticleMinOfs;
		int activeReticleType;
		float vStandMove[3];
		float vStandRot[3];
		float strafeMove[3];
		float strafeRot[3];
		float vDuckedOfs[3];
		float vDuckedMove[3];
		float vDuckedRot[3];
		float vProneOfs[3];
		float vProneMove[3];
		float vProneRot[3];
		float fPosMoveRate;
		float fPosProneMoveRate;
		float fStandMoveMinSpeed;
		float fDuckedMoveMinSpeed;
		float fProneMoveMinSpeed;
		float fPosRotRate;
		float fPosProneRotRate;
		float fStandRotMinSpeed;
		float fDuckedRotMinSpeed;
		float fProneRotMinSpeed;
		XModel** worldModel;
		XModel* worldClipModel;
		XModel* rocketModel;
		XModel* knifeModel;
		XModel* worldKnifeModel;
		Material* hudIcon;
		int hudIconRatio;
		Material* pickupIcon;
		int pickupIconRatio;
		Material* ammoCounterIcon;
		int ammoCounterIconRatio;
		int ammoCounterClip;
		int iStartAmmo;
		const char* szAmmoName;
		int iAmmoIndex;
		const char* szClipName;
		int iClipIndex;
		int iMaxAmmo;
		int shotCount;
		const char* szSharedAmmoCapName;
		int iSharedAmmoCapIndex;
		int iSharedAmmoCap;
		int damage;
		int playerDamage;
		int iMeleeDamage;
		int iDamageType;
		int iFireDelay;
		int iMeleeDelay;
		int meleeChargeDelay;
		int iDetonateDelay;
		int iRechamberTime;
		int rechamberTimeOneHanded;
		int iRechamberBoltTime;
		int iHoldFireTime;
		int iDetonateTime;
		int iMeleeTime;
		int meleeChargeTime;
		int iReloadTime;
		int reloadShowRocketTime;
		int iReloadEmptyTime;
		int iReloadAddTime;
		int iReloadStartTime;
		int iReloadStartAddTime;
		int iReloadEndTime;
		int iDropTime;
		int iRaiseTime;
		int iAltDropTime;
		int quickDropTime;
		int quickRaiseTime;
		int iBreachRaiseTime;
		int iEmptyRaiseTime;
		int iEmptyDropTime;
		int sprintInTime;
		int sprintLoopTime;
		int sprintOutTime;
		int stunnedTimeBegin;
		int stunnedTimeLoop;
		int stunnedTimeEnd;
		int nightVisionWearTime;
		int nightVisionWearTimeFadeOutEnd;
		int nightVisionWearTimePowerUp;
		int nightVisionRemoveTime;
		int nightVisionRemoveTimePowerDown;
		int nightVisionRemoveTimeFadeInStart;
		int fuseTime;
		int aiFuseTime;
		float autoAimRange;
		float aimAssistRange;
		float aimAssistRangeAds;
		float aimPadding;
		float enemyCrosshairRange;
		float moveSpeedScale;
		float adsMoveSpeedScale;
		float sprintDurationScale;
		float fAdsZoomInFrac;
		float fAdsZoomOutFrac;
		Material* overlayMaterial;
		Material* overlayMaterialLowRes;
		Material* overlayMaterialEMP;
		Material* overlayMaterialEMPLowRes;
		int overlayReticle;
		int overlayInterface;
		float overlayWidth;
		float overlayHeight;
		float overlayWidthSplitscreen;
		float overlayHeightSplitscreen;
		float fAdsBobFactor;
		float fAdsViewBobMult;
		float fHipSpreadStandMin;
		float fHipSpreadDuckedMin;
		float fHipSpreadProneMin;
		float hipSpreadStandMax;
		float hipSpreadDuckedMax;
		float hipSpreadProneMax;
		float fHipSpreadDecayRate;
		float fHipSpreadFireAdd;
		float fHipSpreadTurnAdd;
		float fHipSpreadMoveAdd;
		float fHipSpreadDuckedDecay;
		float fHipSpreadProneDecay;
		float fHipReticleSidePos;
		float fAdsIdleAmount;
		float fHipIdleAmount;
		float adsIdleSpeed;
		float hipIdleSpeed;
		float fIdleCrouchFactor;
		float fIdleProneFactor;
		float fGunMaxPitch;
		float fGunMaxYaw;
		float swayMaxAngle;
		float swayLerpSpeed;
		float swayPitchScale;
		float swayYawScale;
		float swayHorizScale;
		float swayVertScale;
		float swayShellShockScale;
		float adsSwayMaxAngle;
		float adsSwayLerpSpeed;
		float adsSwayPitchScale;
		float adsSwayYawScale;
		float adsSwayHorizScale;
		float adsSwayVertScale;
		float adsViewErrorMin;
		float adsViewErrorMax;
		PhysCollmap* physCollmap;
		float dualWieldViewModelOffset;
		int killIconRatio;
		int iReloadAmmoAdd;
		int iReloadStartAdd;
		int ammoDropStockMin;
		int ammoDropClipPercentMin;
		int ammoDropClipPercentMax;
		int iExplosionRadius;
		int iExplosionRadiusMin;
		int iExplosionInnerDamage;
		int iExplosionOuterDamage;
		float damageConeAngle;
		float bulletExplDmgMult;
		float bulletExplRadiusMult;
		int iProjectileSpeed;
		int iProjectileSpeedUp;
		int iProjectileSpeedForward;
		int iProjectileActivateDist;
		float projLifetime;
		float timeToAccelerate;
		float projectileCurvature;
		XModel* projectileModel;
		int projExplosion;
		FxEffectDef* projExplosionEffect;
		FxEffectDef* projDudEffect;
		snd_alias_list_t* projExplosionSound;
		snd_alias_list_t* projDudSound;
		int stickiness;
		float lowAmmoWarningThreshold;
		float ricochetChance;
		float* parallelBounce;
		float* perpendicularBounce;
		FxEffectDef* projTrailEffect;
		FxEffectDef* projBeaconEffect;
		float vProjectileColor[3];
		int guidedMissileType;
		float maxSteeringAccel;
		int projIgnitionDelay;
		FxEffectDef* projIgnitionEffect;
		snd_alias_list_t* projIgnitionSound;
		float fAdsAimPitch;
		float fAdsCrosshairInFrac;
		float fAdsCrosshairOutFrac;
		int adsGunKickReducedKickBullets;
		float adsGunKickReducedKickPercent;
		float fAdsGunKickPitchMin;
		float fAdsGunKickPitchMax;
		float fAdsGunKickYawMin;
		float fAdsGunKickYawMax;
		float fAdsGunKickAccel;
		float fAdsGunKickSpeedMax;
		float fAdsGunKickSpeedDecay;
		float fAdsGunKickStaticDecay;
		float fAdsViewKickPitchMin;
		float fAdsViewKickPitchMax;
		float fAdsViewKickYawMin;
		float fAdsViewKickYawMax;
		float fAdsViewScatterMin;
		float fAdsViewScatterMax;
		float fAdsSpread;
		int hipGunKickReducedKickBullets;
		float hipGunKickReducedKickPercent;
		float fHipGunKickPitchMin;
		float fHipGunKickPitchMax;
		float fHipGunKickYawMin;
		float fHipGunKickYawMax;
		float fHipGunKickAccel;
		float fHipGunKickSpeedMax;
		float fHipGunKickSpeedDecay;
		float fHipGunKickStaticDecay;
		float fHipViewKickPitchMin;
		float fHipViewKickPitchMax;
		float fHipViewKickYawMin;
		float fHipViewKickYawMax;
		float fHipViewScatterMin;
		float fHipViewScatterMax;
		float fightDist;
		float maxDist;
		const char* accuracyGraphName[2];
		float(*originalAccuracyGraphKnots[2])[2];
		unsigned short originalAccuracyGraphKnotCount[2];
		int iPositionReloadTransTime;
		float leftArc;
		float rightArc;
		float topArc;
		float bottomArc;
		float accuracy;
		float aiSpread;
		float playerSpread;
		float minTurnSpeed[2];
		float maxTurnSpeed[2];
		float pitchConvergenceTime;
		float yawConvergenceTime;
		float suppressTime;
		float maxRange;
		float fAnimHorRotateInc;
		float fPlayerPositionDist;
		const char* szUseHintString;
		const char* dropHintString;
		int iUseHintStringIndex;
		int dropHintStringIndex;
		float horizViewJitter;
		float vertViewJitter;
		float scanSpeed;
		float scanAccel;
		int scanPauseTime;
		const char* szScript;
		float fOOPosAnimLength[2];
		int minDamage;
		int minPlayerDamage;
		float fMaxDamageRange;
		float fMinDamageRange;
		float destabilizationRateTime;
		float destabilizationCurvatureMax;
		int destabilizeDistance;
		float* locationDamageMultipliers;
		const char* fireRumble;
		const char* meleeImpactRumble;
		TracerDef* tracerType;
		float turretScopeZoomRate;
		float turretScopeZoomMin;
		float turretScopeZoomMax;
		float turretOverheatUpRate;
		float turretOverheatDownRate;
		float turretOverheatPenalty;
		snd_alias_list_t* turretOverheatSound;
		FxEffectDef* turretOverheatEffect;
		const char* turretBarrelSpinRumble;
		float turretBarrelSpinSpeed;
		float turretBarrelSpinUpTime;
		float turretBarrelSpinDownTime;
		snd_alias_list_t* turretBarrelSpinMaxSnd;
		snd_alias_list_t* turretBarrelSpinUpSnd[4];
		snd_alias_list_t* turretBarrelSpinDownSnd[4];
		snd_alias_list_t* missileConeSoundAlias;
		snd_alias_list_t* missileConeSoundAliasAtBase;
		float missileConeSoundRadiusAtTop;
		float missileConeSoundRadiusAtBase;
		float missileConeSoundHeight;
		float missileConeSoundOriginOffset;
		float missileConeSoundVolumescaleAtCore;
		float missileConeSoundVolumescaleAtEdge;
		float missileConeSoundVolumescaleCoreSize;
		float missileConeSoundPitchAtTop;
		float missileConeSoundPitchAtBottom;
		float missileConeSoundPitchTopSize;
		float missileConeSoundPitchBottomSize;
		float missileConeSoundCrossfadeTopSize;
		float missileConeSoundCrossfadeBottomSize;
		bool sharedAmmo;
		bool lockonSupported;
		bool requireLockonToFire;
		bool bigExplosion;
		bool noAdsWhenMagEmpty;
		bool avoidDropCleanup;
		bool inheritsPerks;
		bool crosshairColorChange;
		bool bRifleBullet;
		bool armorPiercing;
		bool bBoltAction;
		bool aimDownSight;
		bool bRechamberWhileAds;
		bool bBulletExplosiveDamage;
		bool bCookOffHold;
		bool bClipOnly;
		bool noAmmoPickup;
		bool adsFireOnly;
		bool cancelAutoHolsterWhenEmpty;
		bool disableSwitchToWhenEmpty;
		bool suppressAmmoReserveDisplay;
		bool laserSightDuringNightvision;
		bool markableViewmodel;
		bool noDualWield;
		bool flipKillIcon;
		bool bNoPartialReload;
		bool bSegmentedReload;
		bool blocksProne;
		bool silenced;
		bool isRollingGrenade;
		bool projExplosionEffectForceNormalUp;
		bool bProjImpactExplode;
		bool stickToPlayers;
		bool hasDetonator;
		bool disableFiring;
		bool timedDetonation;
		bool rotate;
		bool holdButtonToThrow;
		bool freezeMovementWhenFiring;
		bool thermalScope;
		bool altModeSameWeapon;
		bool turretBarrelSpinEnabled;
		bool missileConeSoundEnabled;
		bool missileConeSoundPitchshiftEnabled;
		bool missileConeSoundCrossfadeEnabled;
		bool offhandHoldIsCancelable;
	};

	struct WeaponFullDef
	{
		WeaponCompleteDef complete;
		WeaponDef weapDef;
		unsigned short hideTags[32];
		const char* szXAnims[37];
		XModel* gunXModel[16];
		const char* szXAnimsRightHanded[37];
		const char* szXAnimsLeftHanded[37];
		unsigned short notetrackSoundMapKeys[16];
		unsigned short notetrackSoundMapValues[16];
		unsigned short notetrackRumbleMapKeys[16];
		unsigned short notetrackRumbleMapValues[16];
		XModel* worldModel[16];
		float parallelBounce[31];
		float perpendicularBounce[31];
		float locationDamageMultipliers[20];
	};

	AssertSize(WeaponCompleteDef, 0xA0);
	AssertSize(WeaponDef, 0x890);

	AssertOffset(WeaponCompleteDef, weapDef, 8);
	AssertOffset(WeaponCompleteDef, hideTags, 24);
	AssertOffset(WeaponCompleteDef, szXAnims, 32);
	AssertOffset(WeaponCompleteDef, szAltWeaponName, 80);
	AssertOffset(WeaponCompleteDef, altWeaponIndex, 88);
	AssertOffset(WeaponCompleteDef, killIcon, 96);
	AssertOffset(WeaponDef, gunXModel, 8);
	AssertOffset(WeaponDef, szXAnimsRightHanded, 24);
	AssertOffset(WeaponDef, szXAnimsLeftHanded, 32);
	AssertOffset(WeaponDef, notetrackSoundMapKeys, 48);
	AssertOffset(WeaponDef, notetrackSoundMapValues, 56);
	AssertOffset(WeaponDef, fireSoundPlayer, 192);
	AssertOffset(WeaponDef, fireLastSound, 240);
	AssertOffset(WeaponDef, fireLastSoundPlayer, 248);
	AssertOffset(WeaponDef, bounceSound, 504);
	AssertOffset(WeaponDef, worldModel, 736);
	AssertOffset(WeaponDef, szAmmoName, 832);
	AssertOffset(WeaponDef, iAmmoIndex, 840);
	AssertOffset(WeaponDef, szClipName, 848);
	AssertOffset(WeaponDef, iClipIndex, 856);
	AssertOffset(WeaponDef, szSharedAmmoCapName, 872);
	AssertOffset(WeaponDef, iSharedAmmoCapIndex, 880);
	AssertOffset(WeaponDef, iSharedAmmoCap, 884);
	AssertOffset(WeaponDef, parallelBounce, 1472);
	AssertOffset(WeaponDef, perpendicularBounce, 1480);
	AssertOffset(WeaponDef, locationDamageMultipliers, 1904);

	AssertOffset(WeaponDef, weapClass, 0x58);
	AssertOffset(WeaponDef, offhandClass, 0x68);
	AssertOffset(WeaponDef, aimAssistRange, 0x42C);
	AssertOffset(WeaponDef, aimAssistRangeAds, 0x430);
	AssertOffset(WeaponDef, requireLockonToFire, 0x85E);
	AssertOffset(WeaponDef, missileConeSoundAlias, 2072);
	AssertOffset(WeaponDef, sharedAmmo, 2140);

	AssertOffset(WeaponDef, iMaxAmmo, 860);
	AssertOffset(WeaponDef, bClipOnly, 2155);

	AssertOffset(WeaponDef, inventoryType, 0x60);

	AssertOffset(WeaponDef, notetrackRumbleMapKeys, 0x40);
	AssertOffset(WeaponDef, notetrackRumbleMapValues, 0x48);
	AssertOffset(WeaponDef, fireRumble, 0x778);
	AssertOffset(WeaponDef, meleeImpactRumble, 0x780);
	AssertOffset(WeaponDef, turretBarrelSpinRumble, 0x7B8);
	AssertOffset(WeaponDef, turretBarrelSpinEnabled, 0x885);

	struct weaponParms
	{
		float forward[3];
		float right[3];
		float up[3];
		float muzzleTrace[3];
		float gunForward[3];
		unsigned int weaponIndex;
		const WeaponDef* weapDef;
		const WeaponCompleteDef* weapCompleteDef;
	};

	AssertSize(weaponParms, 0x50);
	AssertOffset(weaponParms, muzzleTrace, 0x24);
	AssertOffset(weaponParms, weaponIndex, 0x3C);
	AssertOffset(weaponParms, weapDef, 0x40);

	struct XZoneInfo
	{
		const char* name;
		int allocFlags;
		int freeFlags;
	};

	AssertSize(XZoneInfo, 16);

	struct StringTableCell
	{
		const char* string;
		int hash;
	};

	AssertSize(StringTableCell, 16);

	struct StringTable
	{
		const char* name;
		int columnCount;
		int rowCount;
		StringTableCell* values;
	};

	AssertSize(StringTable, 24);
	AssertOffset(StringTable, values, 16);

	enum StructuredDataTypeCategory
	{
		DATA_INT = 0x0,
		DATA_BYTE = 0x1,
		DATA_BOOL = 0x2,
		DATA_STRING = 0x3,
		DATA_ENUM = 0x4,
		DATA_STRUCT = 0x5,
		DATA_INDEXED_ARRAY = 0x6,
		DATA_ENUM_ARRAY = 0x7,
		DATA_FLOAT = 0x8,
		DATA_SHORT = 0x9,
		DATA_COUNT = 0xA,
	};

	union StructuredDataTypeUnion
	{
		unsigned int stringDataLength;
		int enumIndex;
		int structIndex;
		int indexedArrayIndex;
		int enumedArrayIndex;
	};

	AssertSize(StructuredDataTypeUnion, 0x4);

	struct StructuredDataType
	{
		StructuredDataTypeCategory type;
		StructuredDataTypeUnion u;
	};

	struct StructuredDataEnumEntry
	{
		const char* string;
		unsigned short index;
	};

	struct StructuredDataEnum
	{
		int entryCount;
		int reservedEntryCount;
		StructuredDataEnumEntry* entries;
	};

	struct StructuredDataStructProperty
	{
		const char* name;
		StructuredDataType type;
		unsigned int offset;
	};

	struct StructuredDataStruct
	{
		int propertyCount;
		StructuredDataStructProperty* properties;
		int size;
		unsigned int bitOffset;
	};

	struct StructuredDataIndexedArray
	{
		int arraySize;
		StructuredDataType elementType;
		unsigned int elementSize;
	};

	struct StructuredDataEnumedArray
	{
		int enumIndex;
		StructuredDataType elementType;
		unsigned int elementSize;
	};

	struct StructuredDataDef
	{
		int version;
		unsigned int formatChecksum;
		int enumCount;
		StructuredDataEnum* enums;
		int structCount;
		StructuredDataStruct* structs;
		int indexedArrayCount;
		StructuredDataIndexedArray* indexedArrays;
		int enumedArrayCount;
		StructuredDataEnumedArray* enumedArrays;
		StructuredDataType rootType;
		unsigned int size;
	};

	struct StructuredDataDefSet
	{
		const char* name;
		unsigned int defCount;
		StructuredDataDef* defs;
	};

	struct StructuredDataBuffer
	{
		char* data;
		unsigned int size;
	};

	AssertSize(StructuredDataType, 8);
	AssertSize(StructuredDataEnumEntry, 16);
	AssertSize(StructuredDataEnum, 16);
	AssertSize(StructuredDataStructProperty, 24);
	AssertSize(StructuredDataStruct, 24);
	AssertSize(StructuredDataIndexedArray, 16);
	AssertSize(StructuredDataEnumedArray, 16);
	AssertSize(StructuredDataDef, 88);
	AssertSize(StructuredDataDefSet, 24);
	AssertSize(StructuredDataBuffer, 16);

	enum LookupError
	{
		LOOKUP_ERROR_NONE = 0x0,
		LOOKUP_ERROR_WRONG_DATA_TYPE = 0x1,
		LOOKUP_ERROR_INDEX_OUTSIDE_BOUNDS = 0x2,
		LOOKUP_ERROR_INVALID_STRUCT_PROPERTY = 0x3,
		LOOKUP_ERROR_INVALID_ENUM_VALUE = 0x4,
		LOOKUP_ERROR_COUNT = 0x5,
	};

	struct StructuredDataLookup
	{
		StructuredDataDef* def;
		StructuredDataType* type;
		unsigned int offset;
		LookupError error;
	};

	AssertOffset(StructuredDataLookup, offset, 0x10);
	AssertOffset(StructuredDataLookup, error, 0x14);
	AssertOffset(StructuredDataEnumEntry, index, 8);
	AssertOffset(StructuredDataStructProperty, offset, 16);
	AssertOffset(StructuredDataStruct, size, 16);
	AssertOffset(StructuredDataIndexedArray, elementSize, 12);
	AssertOffset(StructuredDataDef, enums, 16);
	AssertOffset(StructuredDataDef, structs, 32);
	AssertOffset(StructuredDataDef, indexedArrays, 48);
	AssertOffset(StructuredDataDef, enumedArrays, 64);
	AssertOffset(StructuredDataDef, rootType, 72);
	AssertOffset(StructuredDataDef, size, 80);
	AssertOffset(StructuredDataDefSet, defCount, 8);
	AssertOffset(StructuredDataDefSet, defs, 16);

	constexpr std::size_t MAX_GENTITIES = 2048;

	enum
	{
		ENTFIELD_ENTITY = 0x0,
		ENTFIELD_CLIENT = 0x6000,
		ENTFIELD_MASK = 0xE000,
	};

	enum ClassNum : unsigned int
	{
		CLASS_NUM_ENTITY = 0x0,
	};

	enum usercmdButtonBits
	{
		CMD_BUTTON_ATTACK = 1 << 0,
		CMD_BUTTON_SPRINT = 1 << 1,
		CMD_BUTTON_MELEE = 1 << 2,
		CMD_BUTTON_ACTIVATE = 1 << 3,
		CMD_BUTTON_RELOAD = 1 << 4,
		CMD_BUTTON_USE_RELOAD = 1 << 5,
		CMD_BUTTON_LEAN_LEFT = 1 << 6,
		CMD_BUTTON_LEAN_RIGHT = 1 << 7,
		CMD_BUTTON_PRONE = 1 << 8,
		CMD_BUTTON_CROUCH = 1 << 9,
		CMD_BUTTON_UP = 1 << 10,
		CMD_BUTTON_ADS = 1 << 11,
		CMD_BUTTON_DOWN = 1 << 12,
		CMD_BUTTON_BREATH = 1 << 13,
		CMD_BUTTON_FRAG = 1 << 14,
		CMD_BUTTON_OFFHAND_SECONDARY = 1 << 15,
		CMD_BUTTON_THROW = 1 << 19,
		CMD_BUTTON_REMOTE = 1 << 20,
	};

	enum weapInventoryType_t
	{
		WEAPINVENTORY_PRIMARY = 0x0,
		WEAPINVENTORY_OFFHAND = 0x1,
		WEAPINVENTORY_ITEM = 0x2,
		WEAPINVENTORY_ALTMODE = 0x3,
		WEAPINVENTORY_EXCLUSIVE = 0x4,
		WEAPINVENTORY_SCAVENGER = 0x5,
		WEAPINVENTORYCOUNT = 0x6,
	};

	enum OffhandClass
	{
		OFFHAND_CLASS_NONE = 0x0,
		OFFHAND_CLASS_FRAG_GRENADE = 0x1,
		OFFHAND_CLASS_SMOKE_GRENADE = 0x2,
		OFFHAND_CLASS_FLASH_GRENADE = 0x3,
		OFFHAND_CLASS_THROWINGKNIFE = 0x4,
		OFFHAND_CLASS_OTHER = 0x5,
		OFFHAND_CLASS_COUNT = 0x6,
	};

	struct GlobalAmmo
	{
		int ammoType;
		int ammoCount;
	};

	struct ClipAmmo
	{
		int clipIndex;
		int ammoCount[2];
	};

	struct PlayerWeaponCommonState
	{
		int offHandIndex;
		OffhandClass offhandPrimary;
		OffhandClass offhandSecondary;
		unsigned int weapon;
		unsigned int primaryWeaponForAltMode;
		int weapFlags;
		float fWeaponPosFrac;
		float aimSpreadScale;
		int adsDelayTime;
		int spreadOverride;
		int spreadOverrideState;
		int lastWeaponHand;
		GlobalAmmo ammoNotInClip[15];
		ClipAmmo ammoInClip[15];
		int weapLockFlags;
	};

	AssertOffset(PlayerWeaponCommonState, weapFlags, 0x14);
	AssertOffset(PlayerWeaponCommonState, lastWeaponHand, 724 - 0x2A8);
	AssertOffset(PlayerWeaponCommonState, ammoNotInClip, 728 - 0x2A8);
	AssertOffset(PlayerWeaponCommonState, ammoInClip, 848 - 0x2A8);
	AssertOffset(PlayerWeaponCommonState, weapLockFlags, 1028 - 0x2A8);

	enum
	{
		PWF_DISABLE_WEAPONS = 1 << 7,
		PWF_DISABLE_WEAPON_PICKUP = 1 << 16,
	};

	struct PlayerActiveWeaponState
	{
		int weapAnim;
		int weaponTime;
		int weaponDelay;
		int weaponRestrictKickTime;
		int weaponState;
		int weapHandFlags;
		unsigned int weaponShotCount;
	};

	AssertSize(PlayerActiveWeaponState, 28);

	struct PlayerEquippedWeaponState
	{
		bool usedBefore;
		bool dualWielding;
		char weaponModel;
		bool needsRechamber[2];
	};

	AssertSize(PlayerEquippedWeaponState, 5);

	enum
	{
		WEAPON_RAISING = 0x1,
		WEAPON_DROPPING = 0x3,
		WEAPON_DROPPING_QUICK = 0x4,
		WEAPON_DROPPING_ALT = 0x5,
	};

	enum
	{
		WEAP_ANIM_QUICK_DROP = 0x14,
	};

	enum
	{
		WEAPON_HAND_RIGHT = 0x0,
		WEAPON_HAND_LEFT = 0x1,
		NUM_WEAPON_HANDS = 0x2,
	};

	struct playerState_s
	{
		int commandTime;
		int pm_type;
		int pm_time;
		int pm_flags;
		int otherFlags;
		int linkFlags;
		int bobCycle;
		float origin[3];
		float velocity[3];
		char pad1[0x2C];
		float delta_angles[3];
		char pad2[0x40];
		int movementDir;
		int eFlags;
		char pad3[0x50];
		int clientNum;
		int viewmodelIndex;
		float viewangles[3];
		char pad4[0x38];
		int stats[4];
		char pad5[0x10];
		int viewlocked_entNum;
		char pad8[0x40];
		int locationSelectionInfo;
		char pad6[0x30];
		PlayerActiveWeaponState weapState[NUM_WEAPON_HANDS];
		unsigned int weaponsEquipped[15];
		PlayerEquippedWeaponState weapEquippedData[15];
		char pad7[0x1];
		PlayerWeaponCommonState weapCommon;
	};

	AssertOffset(playerState_s, pm_type, 0x4);
	AssertOffset(playerState_s, pm_time, 0x8);
	AssertOffset(playerState_s, pm_flags, 0xC);
	AssertOffset(playerState_s, bobCycle, 0x18);
	AssertOffset(playerState_s, origin, 0x1C);
	AssertOffset(playerState_s, velocity, 0x28);
	AssertOffset(playerState_s, delta_angles, 0x60);
	AssertOffset(playerState_s, movementDir, 0xAC);

	AssertOffset(playerState_s, otherFlags, 0x10);
	AssertOffset(playerState_s, linkFlags, 0x14);
	AssertOffset(playerState_s, eFlags, 0xB0);
	AssertOffset(playerState_s, viewangles, 0x10C);
	AssertOffset(playerState_s, stats, 0x150);

	AssertOffset(playerState_s, clientNum, 0x104);
	AssertOffset(playerState_s, viewlocked_entNum, 0x170);
	AssertOffset(playerState_s, locationSelectionInfo, 0x1B4);
	AssertOffset(playerState_s, weapState, 0x1E8);
	AssertOffset(playerState_s, weaponsEquipped, 0x220);
	AssertOffset(playerState_s, weapEquippedData, 0x25C);
	AssertOffset(playerState_s, weapCommon, 0x2A8);

	struct snapshot_s
	{
		playerState_s ps;
		char pad0[0x311C - sizeof(playerState_s)];
		int snapFlags;
		int ping;
		int serverTime;
		int numEntities;
		int numClients;
	};

	AssertOffset(snapshot_s, snapFlags, 0x311C);
	AssertOffset(snapshot_s, serverTime, 0x3124);
	AssertOffset(snapshot_s, numClients, 0x312C);

	struct RefdefView
	{
		float tanHalfFovX;
		float tanHalfFovY;
		float org[3];
		float axis[3][3];
		float zNear;
	};

	struct refdef_t
	{
		unsigned int x;
		unsigned int y;
		unsigned int width;
		unsigned int height;
		RefdefView view;
		float viewOffset[3];
		int time;
	};

	AssertOffset(refdef_t, view, 0x10);
	AssertOffset(refdef_t, viewOffset, 0x4C);
	AssertOffset(refdef_t, time, 0x58);

	struct cg_s
	{
		playerState_s predictedPlayerState;
		char pad0[0x3370 - sizeof(playerState_s)];
		int clientNum;
		int localClientNum;
		char pad1[0x3390 - 0x3378];
		snapshot_s* snap;
		snapshot_s* nextSnap;
		char pad2[0x6A780 - 0x33A0];
		int time;
		char pad3[0x6A7A0 - 0x6A784];
		refdef_t refdef;
	};

	AssertOffset(cg_s, clientNum, 0x3370);
	AssertOffset(cg_s, localClientNum, 0x3374);
	AssertOffset(cg_s, nextSnap, 0x3398);
	AssertOffset(cg_s, time, 0x6A780);
	AssertOffset(cg_s, refdef, 0x6A7A0);

	enum CompassType
	{
		COMPASS_TYPE_PARTIAL = 0x0,
		COMPASS_TYPE_FULL = 0x1,
	};

	enum sessionState_t
	{
		SESS_STATE_PLAYING = 0x0,
		SESS_STATE_DEAD = 0x1,
		SESS_STATE_SPECTATOR = 0x2,
		SESS_STATE_INTERMISSION = 0x3,
	};

	enum clientConnected_t
	{
		CON_DISCONNECTED = 0x0,
		CON_CONNECTING = 0x1,
		CON_CONNECTED = 0x2,
	};

	enum team_t
	{
		TEAM_FREE = 0x0,
		TEAM_SPECTATOR = 0x3,
	};

	struct clientState_s
	{
		int clientIndex;
		team_t team;
		char pad0[0x3C];
		char name[16];
	};

	AssertOffset(clientState_s, name, 0x44);

	struct clientSession_t
	{
		sessionState_t sessionState;
		char pad0[0x14];
		int score;
		int deaths;
		int kills;
		char pad1[0x8];
		clientConnected_t connected;
		char pad2[0x8C];
		clientState_s cs;
	};

	AssertOffset(clientSession_t, score, 0x18);
	AssertOffset(clientSession_t, deaths, 0x1C);
	AssertOffset(clientSession_t, kills, 0x20);
	AssertOffset(clientSession_t, connected, 0x2C);
	AssertOffset(clientSession_t, cs, 0xBC);

	enum ClientFlags
	{
		CF_BIT_NOCLIP = (1 << 0),
		CF_BIT_UFO = (1 << 1),
		CF_BIT_FROZEN = (1 << 2),
		CF_BIT_DISABLE_USABILITY = (1 << 3),
	};

	struct gclient_s
	{
		playerState_s ps;
		char pad0[0x311C - sizeof(playerState_s)];
		clientSession_t sess;
		char pad1[0x3394 - 0x311C - sizeof(clientSession_t)];
		int flags;
		char pad2[0x8];
		int buttons;
		int oldbuttons;
		int latched_buttons;
		char pad3[0xC8];
		unsigned short useHoldEntity;
		char pad4[0x2];
		int useHoldTime;
		char pad5[0x1FC];
	};

	AssertSize(gclient_s, 0x3678);
	AssertOffset(gclient_s, sess, 0x311C);
	AssertOffset(gclient_s, flags, 0x3394);
	AssertOffset(gclient_s, buttons, 0x33A0);
	AssertOffset(gclient_s, oldbuttons, 0x33A4);
	AssertOffset(gclient_s, latched_buttons, 0x33A8);
	AssertOffset(gclient_s, useHoldEntity, 0x3474);
	AssertOffset(gclient_s, useHoldTime, 0x3478);

	struct trajectory_t
	{
		int trType;
		int trTime;
		int trDuration;
		float trBase[3];
		float trDelta[3];
	};

	struct LerpEntityStateTurret
	{
		float gunAngles[3];
		int lastBarrelRotChangeTime;
		int lastBarrelRotChangeRate;
		int lastHeatChangeLevel;
		int lastHeatChangeTime;
		bool isBarrelRotating;
		bool isOverheat;
		bool isHeatingUp;
		bool isBeingCarried;
	};

	union LerpEntityStateTypeUnion
	{
		LerpEntityStateTurret turret;
		char pad0[0x24];
	};

	struct LerpEntityState
	{
		int eFlags;
		trajectory_t pos;
		trajectory_t apos;
		LerpEntityStateTypeUnion u;
	};

	AssertSize(LerpEntityState, 0x70);

	struct entityState_s
	{
		int number;
		int eType;
		LerpEntityState lerp;
		int time2;
		int otherEntityNum;
		int attackerEntityNum;
		int groundEntityNum;
		int loopSound;
		int surfType;
		int index;
		int clientNum;
		int iHeadIcon;
		int iHeadIconTeam;
		int solid;
		unsigned int eventParm;
		int eventSequence;
		int events[4];
		unsigned int eventParms[4];
		unsigned short weapon;
		char pad0[0x32];
	};

	AssertSize(entityState_s, 0x100);
	AssertOffset(entityState_s, lerp, 0x8);
	AssertOffset(entityState_s, otherEntityNum, 0x7C);
	AssertOffset(entityState_s, clientNum, 0x94);
	AssertOffset(entityState_s, eventParm, 0xA4);
	AssertOffset(entityState_s, weapon, 0xCC);
	static_assert(offsetof(entityState_s, lerp.u.turret) == 0x54);

	enum entityType_t
	{
		ET_PLAYER = 0x1,
		ET_MISSILE = 0x4,
		ET_HELICOPTER = 0xC,
	};

	struct entityShared_t
	{
		char isLinked;
		char modelType;
		char svFlags;
		char isInUse;
		char pad0[0x34];
		float currentOrigin[3];
		float currentAngles[3];
		char pad1[0x8];
	};

	AssertSize(entityShared_t, 0x58);
	AssertOffset(entityShared_t, currentOrigin, 0x38);

	enum entityFlag
	{
		FL_GODMODE = 1 << 0,
		FL_DEMI_GODMODE = 1 << 1,
		FL_NOTARGET = 1 << 2,
	};

	struct gentity_s
	{
		entityState_s s;
		entityShared_t r;
		gclient_s* client;
		char pad1[0x18];
		unsigned short model;
		char pad1a[0x2];
		unsigned char active;
		char pad2[0x5];
		unsigned short script_classname;
		unsigned short classname;
		char pad3[0xE];
		int flags;
		char pad4[0x14];
		int health;
		char pad5[0xE8];
	};

	AssertSize(gentity_s, 0x298);
	AssertOffset(gentity_s, r, 0x100);
	AssertOffset(gentity_s, client, 0x158);
	AssertOffset(gentity_s, model, 0x178);
	AssertOffset(gentity_s, active, 0x17C);
	AssertOffset(gentity_s, script_classname, 0x182);
	AssertOffset(gentity_s, classname, 0x184);
	AssertOffset(gentity_s, flags, 0x194);
	AssertOffset(gentity_s, health, 0x1AC);

	struct CEntTurretInfo
	{
		char pad0[0x8];
		float barrelPitch;
		bool playerUsing;
	};

	struct cpose_t
	{
		char pad0[0x20];
		float origin[3];
		char pad1[0x48 - 0x2C];
		CEntTurretInfo turret;
		char pad2[0x78 - 0x48 - sizeof(CEntTurretInfo)];
	};

	AssertSize(cpose_t, 0x78);
	AssertOffset(cpose_t, origin, 0x20);
	static_assert(offsetof(cpose_t, turret.playerUsing) == 0x54);

	struct centity_s
	{
		cpose_t pose;
		LerpEntityState prevState;
		entityState_s nextState;
		char nextValid;
		char pad0[0x220 - 0x1E9];
	};

	AssertSize(centity_s, 0x220);
	AssertOffset(centity_s, prevState, 0x78);
	AssertOffset(centity_s, nextState, 0xE8);
	AssertOffset(centity_s, nextValid, 0x1E8);

	enum pmtype_t
	{
		PM_SPECTATOR = 0x5,
	};

	struct netchan_t
	{
		int outgoingSequence;
		netsrc_t sock;
		int dropped;
		int incomingSequence;
		netadr_t remoteAddress;
		int qport;
		char pad0[0x14];
		int unsentFragments;
	};

	AssertOffset(netchan_t, remoteAddress, 16);
	AssertOffset(netchan_t, qport, 0x24);
	AssertOffset(netchan_t, unsentFragments, 0x3C);

	struct clientHeader_t
	{
		int state;
		char pad0[20];
		netchan_t netchan;
	};

	AssertOffset(clientHeader_t, netchan, 24);

	enum clientState_t
	{
		CS_FREE = 0x0,
		CS_ZOMBIE = 0x1,
		CS_CONNECTED = 0x3,
		CS_ACTIVE = 0x5,
	};

	enum svscmd_type
	{
		SV_CMD_CAN_IGNORE = 0x0,
		SV_CMD_RELIABLE = 0x1,
	};

#pragma pack(push, 1)
	struct clientStats_t
	{
		unsigned int checksum;
		unsigned char binary[2000];
		int data[1547];
	};
#pragma pack(pop)

	AssertSize(clientStats_t, 0x2000);

	struct client_s
	{
		clientHeader_t header;
		char pad0[0x618];
		char userinfo[1024];
		char pad1[0x20848];
		gentity_s* gentity;
		char name[16];
		char pad2[0x4];
		int lastPacketTime;
		int lastConnectTime;
		int nextSnapshotTime;
		char pad7[0x4];
		int ping;
		char pad3[0x20824];
		int bIsTestClient;
		char pad4[0x5];
		clientStats_t stats;
		char pad8[0x40B];
		std::uint64_t steamID;
		char pad5[0x62888];
	};

	AssertSize(client_s, 681904);
	AssertOffset(client_s, userinfo, 0x670);
	AssertOffset(client_s, gentity, 0x212B8);
	AssertOffset(client_s, name, 0x212C0);
	AssertOffset(client_s, lastPacketTime, 0x212D4);
	AssertOffset(client_s, lastConnectTime, 0x212D8);
	AssertOffset(client_s, nextSnapshotTime, 0x212DC);
	AssertOffset(client_s, ping, 0x212E4);
	AssertOffset(client_s, bIsTestClient, 0x41B0C);
	AssertOffset(client_s, stats, 0x41B15);
	AssertOffset(client_s, steamID, 0x43F20);

	enum XAssetType
	{
		ASSET_TYPE_PHYSPRESET = 0x0,
		ASSET_TYPE_PHYSCOLLMAP = 0x1,
		ASSET_TYPE_XANIMPARTS = 0x2,
		ASSET_TYPE_XMODEL_SURFS = 0x3,
		ASSET_TYPE_XMODEL = 0x4,
		ASSET_TYPE_MATERIAL = 0x5,
		ASSET_TYPE_PIXELSHADER = 0x6,
		ASSET_TYPE_VERTEXSHADER = 0x7,
		ASSET_TYPE_VERTEXDECL = 0x8,
		ASSET_TYPE_TECHNIQUE_SET = 0x9,
		ASSET_TYPE_IMAGE = 0xA,
		ASSET_TYPE_SOUND = 0xB,
		ASSET_TYPE_SOUND_CURVE = 0xC,
		ASSET_TYPE_LOADED_SOUND = 0xD,
		ASSET_TYPE_CLIPMAP_SP = 0xE,
		ASSET_TYPE_CLIPMAP_MP = 0xF,
		ASSET_TYPE_COMWORLD = 0x10,
		ASSET_TYPE_GAMEWORLD_SP = 0x11,
		ASSET_TYPE_GAMEWORLD_MP = 0x12,
		ASSET_TYPE_MAP_ENTS = 0x13,
		ASSET_TYPE_FXWORLD = 0x14,
		ASSET_TYPE_GFXWORLD = 0x15,
		ASSET_TYPE_LIGHT_DEF = 0x16,
		ASSET_TYPE_UI_MAP = 0x17,
		ASSET_TYPE_FONT = 0x18,
		ASSET_TYPE_MENULIST = 0x19,
		ASSET_TYPE_MENU = 0x1A,
		ASSET_TYPE_LOCALIZE_ENTRY = 0x1B,
		ASSET_TYPE_WEAPON = 0x1C,
		ASSET_TYPE_SNDDRIVER_GLOBALS = 0x1D,
		ASSET_TYPE_FX = 0x1E,
		ASSET_TYPE_IMPACT_FX = 0x1F,
		ASSET_TYPE_AITYPE = 0x20,
		ASSET_TYPE_MPTYPE = 0x21,
		ASSET_TYPE_CHARACTER = 0x22,
		ASSET_TYPE_XMODELALIAS = 0x23,
		ASSET_TYPE_RAWFILE = 0x24,
		ASSET_TYPE_STRINGTABLE = 0x25,
		ASSET_TYPE_LEADERBOARD = 0x26,
		ASSET_TYPE_STRUCTURED_DATA_DEF = 0x27,
		ASSET_TYPE_TRACER = 0x28,
		ASSET_TYPE_VEHICLE = 0x29,
		ASSET_TYPE_ADDON_MAP_ENTS = 0x2A,
		ASSET_TYPE_COUNT = 0x2B,
	};

	enum ImageCategory : char
	{
		IMG_CATEGORY_UNKNOWN = 0x0,
		IMG_CATEGORY_AUTO_GENERATED = 0x1,
		IMG_CATEGORY_LIGHTMAP = 0x2,
		IMG_CATEGORY_LOAD_FROM_FILE = 0x3,
		IMG_CATEGORY_RAW = 0x4,
		IMG_CATEGORY_FIRST_UNMANAGED = 0x5,
		IMG_CATEGORY_WATER = 0x5,
		IMG_CATEGORY_RENDERTARGET = 0x6,
		IMG_CATEGORY_TEMP = 0x7,
	};

	struct PhysGeomInfo;
	struct BrushWrapper;
	struct cbrushside_t;
	struct cplane_s;
	struct XAnimNotifyInfo;
	struct XAnimDeltaPart;
	struct XAnimPartTrans;
	struct XAnimDeltaPartQuat2;
	struct XAnimDeltaPartQuat;
	struct XSurface;
	struct GfxPackedVertex;
	struct XRigidVertList;
	struct XSurfaceCollisionTree;
	struct XSurfaceCollisionNode;
	struct XSurfaceCollisionLeaf;
	struct DObjAnimMat;
	struct XModelCollSurf_s;
	struct XBoneInfo;
	struct MaterialTechniqueSet;
	struct MaterialTextureDef;
	struct MaterialConstantDef;
	struct GfxStateBits;
	struct MaterialTechnique;
	struct MaterialVertexDeclaration;
	struct MaterialVertexShader;
	struct MaterialPixelShader;
	struct MaterialShaderArgument;
	struct GfxImage;
	struct water_t;
	struct GfxImageLoadDef;
	struct complex_s;
	struct XModelCollTri_s;
	struct snd_alias_t;
	struct SoundFile;
	struct SndCurve;
	struct SpeakerMap;
	struct LoadedSound;
	struct cStaticModel_s;
	struct ClipMaterial;
	struct cNode_t;
	struct cLeaf_t;
	struct cLeafBrushNode_s;
	struct CollisionBorder;
	struct CollisionPartition;
	struct CollisionAabbTree;
	struct cmodel_t;
	struct MapEnts;
	struct SModelAabbNode;
	struct DynEntityDef;
	struct DynEntityPose;
	struct DynEntityClient;
	struct DynEntityColl;
	struct TriggerModel;
	struct TriggerHull;
	struct TriggerSlab;
	struct Stage;
	struct FxElemDef;
	struct FxElemMarkVisuals;
	struct FxTrailDef;
	struct FxSparkFountainDef;
	struct FxElemVelStateSample;
	struct FxElemVisStateSample;
	struct FxTrailVertex;
	struct ComPrimaryLight;
	struct pathnode_t;
	struct pathbasenode_t;
	struct pathnode_tree_t;
	struct VehicleTrackSegment;
	struct G_GlassData;
	struct pathlink_s;
	struct VehicleTrackSector;
	struct VehicleTrackObstacle;
	struct G_GlassPiece;
	struct G_GlassName;
	struct FxGlassDef;
	union FxGlassPiecePlace;
	struct FxGlassPieceState;
	struct FxGlassPieceDynamics;
	union FxGlassGeometryData;
	struct FxGlassInitPieceState;
	struct GfxWorldVertex;
	struct GfxReflectionProbe;
	struct GfxLightmapArray;
	struct GfxLightGridEntry;
	struct GfxLightGridColors;
	struct GfxStaticModelInst;
	struct GfxSurface;
	struct GfxSurfaceBounds;
	struct GfxStaticModelDrawInst;
	struct GfxSky;
	struct GfxCellTreeCount;
	struct GfxCellTree;
	struct GfxCell;
	struct GfxBrushModel;
	struct MaterialMemory;
	struct GfxSceneDynModel;
	struct GfxSceneDynBrush;
	struct GfxShadowGeometry;
	struct GfxLightRegion;
	struct GfxHeroOnlyLight;
	struct GfxAabbTree;
	struct GfxPortal;
	struct GfxLightRegionHull;
	struct GfxLightRegionAxis;
	struct FxImpactEntry;
	struct LbColumnDef;
	struct XAsset;
	struct AddonMapEnts;

	enum DynEntityType
	{
		DYNENT_TYPE_INVALID = 0x0,
		DYNENT_TYPE_CLUTTER = 0x1,
		DYNENT_TYPE_DESTRUCT = 0x2,
		DYNENT_TYPE_COUNT = 0x3,
	};

	enum LbAggType
	{
		LBAGG_TYPE_MIN = 0x0,
		LBAGG_TYPE_MAX = 0x1,
		LBAGG_TYPE_SUM = 0x2,
		LBAGG_TYPE_LAST = 0x3,
		LBAGG_TYPE_COUNT = 0x4,
	};

	enum LbColType
	{
		LBCOL_TYPE_NUMBER = 0x0,
		LBCOL_TYPE_TIME = 0x1,
		LBCOL_TYPE_LEVELXP = 0x2,
		LBCOL_TYPE_PRESTIGE = 0x3,
		LBCOL_TYPE_BIGNUMBER = 0x4,
		LBCOL_TYPE_PERCENT = 0x5,
		LBCOL_TYPE_COUNT = 0x6,
	};

	enum MaterialShaderArgumentType : unsigned __int16
	{
		MTL_ARG_MATERIAL_VERTEX_CONST = 0x0,
		MTL_ARG_LITERAL_VERTEX_CONST = 0x1,
		MTL_ARG_MATERIAL_PIXEL_SAMPLER = 0x2,
		MTL_ARG_CODE_PRIM_BEGIN = 0x3,
		MTL_ARG_CODE_VERTEX_CONST = 0x3,
		MTL_ARG_CODE_PIXEL_SAMPLER = 0x4,
		MTL_ARG_CODE_PIXEL_CONST = 0x5,
		MTL_ARG_CODE_PRIM_END = 0x6,
		MTL_ARG_MATERIAL_PIXEL_CONST = 0x6,
		MTL_ARG_LITERAL_PIXEL_CONST = 0x7,
		MTL_ARG_COUNT = 0x8,
	};

	enum PathNodeErrorCode
	{
		PNERR_NONE = 0x0,
		PNERR_INSOLID = 0x1,
		PNERR_FLOATING = 0x2,
		PNERR_NOLINK = 0x3,
		PNERR_DUPLICATE = 0x4,
		PNERR_NOSTANCE = 0x5,
		PNERR_INVALIDDOOR = 0x6,
		PNERR_NOANGLES = 0x7,
		PNERR_BADPLACEMENT = 0x8,
		NUM_PATH_NODE_ERRORS = 0x9,
	};

	enum VehicleAxleType
	{
		VEH_AXLE_FRONT = 0x0,
		VEH_AXLE_REAR = 0x1,
		VEH_AXLE_ALL = 0x2,
		VEH_AXLE_COUNT = 0x3,
	};

	enum VehicleType
	{
		VEH_WHEELS_4 = 0x0,
		VEH_TANK = 0x1,
		VEH_PLANE = 0x2,
		VEH_BOAT = 0x3,
		VEH_ARTILLERY = 0x4,
		VEH_HELICOPTER = 0x5,
		VEH_SNOWMOBILE = 0x6,
		VEH_TYPE_COUNT = 0x7,
	};

	enum nodeType
	{
		NODE_ERROR = 0x0,
		NODE_PATHNODE = 0x1,
		NODE_COVER_STAND = 0x2,
		NODE_COVER_CROUCH = 0x3,
		NODE_COVER_CROUCH_WINDOW = 0x4,
		NODE_COVER_PRONE = 0x5,
		NODE_COVER_RIGHT = 0x6,
		NODE_COVER_LEFT = 0x7,
		NODE_AMBUSH = 0x8,
		NODE_EXPOSED = 0x9,
		NODE_CONCEALMENT_STAND = 0xA,
		NODE_CONCEALMENT_CROUCH = 0xB,
		NODE_CONCEALMENT_PRONE = 0xC,
		NODE_DOOR = 0xD,
		NODE_DOOR_INTERIOR = 0xE,
		NODE_SCRIPTED = 0xF,
		NODE_NEGOTIATION_BEGIN = 0x10,
		NODE_NEGOTIATION_END = 0x11,
		NODE_TURRET = 0x12,
		NODE_GUARD = 0x13,
		NODE_NUMTYPES = 0x14,
		NODE_DONTLINK = 0x14,
	};

	struct PhysPreset
	{
		const char* name;
		int type;
		float mass;
		float bounce;
		float friction;
		float bulletForceScale;
		float explosiveForceScale;
		const char* sndAliasPrefix;
		float piecesSpreadFraction;
		float piecesUpwardVelocity;
		bool tempDefaultToCylinder;
		bool perSurfaceSndAlias;
	};

	AssertSize(PhysPreset, 0x38);
	AssertOffset(PhysPreset, name, 0x0);
	AssertOffset(PhysPreset, type, 0x8);
	AssertOffset(PhysPreset, mass, 0xC);
	AssertOffset(PhysPreset, bounce, 0x10);
	AssertOffset(PhysPreset, friction, 0x14);
	AssertOffset(PhysPreset, bulletForceScale, 0x18);
	AssertOffset(PhysPreset, explosiveForceScale, 0x1C);
	AssertOffset(PhysPreset, sndAliasPrefix, 0x20);
	AssertOffset(PhysPreset, piecesSpreadFraction, 0x28);
	AssertOffset(PhysPreset, piecesUpwardVelocity, 0x2C);
	AssertOffset(PhysPreset, tempDefaultToCylinder, 0x30);
	AssertOffset(PhysPreset, perSurfaceSndAlias, 0x31);

	struct PhysMass
	{
		float centerOfMass[3];
		float momentsOfInertia[3];
		float productsOfInertia[3];
	};

	AssertSize(PhysMass, 0x24);
	AssertOffset(PhysMass, centerOfMass, 0x0);
	AssertOffset(PhysMass, momentsOfInertia, 0xC);
	AssertOffset(PhysMass, productsOfInertia, 0x18);

	struct Bounds
	{
		float midPoint[3];
		float halfSize[3];
	};

	AssertSize(Bounds, 0x18);
	AssertOffset(Bounds, midPoint, 0x0);
	AssertOffset(Bounds, halfSize, 0xC);

	struct PhysCollmap
	{
		const char* name;
		unsigned int count;
		PhysGeomInfo* geoms;
		PhysMass mass;
		Bounds bounds;
	};

	AssertSize(PhysCollmap, 0x58);
	AssertOffset(PhysCollmap, name, 0x0);
	AssertOffset(PhysCollmap, count, 0x8);
	AssertOffset(PhysCollmap, geoms, 0x10);
	AssertOffset(PhysCollmap, mass, 0x18);
	AssertOffset(PhysCollmap, bounds, 0x3C);

	struct PhysGeomInfo
	{
		BrushWrapper* brushWrapper;
		int type;
		float orientation[3][3];
		Bounds bounds;
	};

	AssertSize(PhysGeomInfo, 0x48);
	AssertOffset(PhysGeomInfo, brushWrapper, 0x0);
	AssertOffset(PhysGeomInfo, type, 0x8);
	AssertOffset(PhysGeomInfo, orientation, 0xC);
	AssertOffset(PhysGeomInfo, bounds, 0x30);

	struct cbrush_t
	{
		unsigned short numsides;
		unsigned short glassPieceIndex;
		cbrushside_t* sides;
		unsigned char* baseAdjacentSide;
		unsigned short axialMaterialNum[2][3];
		unsigned char firstAdjacentSideOffsets[2][3];
		unsigned char edgeCount[2][3];
	};

	AssertSize(cbrush_t, 0x30);
	AssertOffset(cbrush_t, numsides, 0x0);
	AssertOffset(cbrush_t, glassPieceIndex, 0x2);
	AssertOffset(cbrush_t, sides, 0x8);
	AssertOffset(cbrush_t, baseAdjacentSide, 0x10);
	AssertOffset(cbrush_t, axialMaterialNum, 0x18);
	AssertOffset(cbrush_t, firstAdjacentSideOffsets, 0x24);
	AssertOffset(cbrush_t, edgeCount, 0x2A);

	struct BrushWrapper
	{
		Bounds bounds;
		cbrush_t brush;
		int totalEdgeCount;
		cplane_s* planes;
	};

	AssertSize(BrushWrapper, 0x58);
	AssertOffset(BrushWrapper, bounds, 0x0);
	AssertOffset(BrushWrapper, brush, 0x18);
	AssertOffset(BrushWrapper, totalEdgeCount, 0x48);
	AssertOffset(BrushWrapper, planes, 0x50);

	struct cbrushside_t
	{
		cplane_s* plane;
		unsigned __int16 materialNum;
		char firstAdjacentSideOffset;
		char edgeCount;
	};

	AssertSize(cbrushside_t, 0x10);
	AssertOffset(cbrushside_t, plane, 0x0);
	AssertOffset(cbrushside_t, materialNum, 0x8);
	AssertOffset(cbrushside_t, firstAdjacentSideOffset, 0xA);
	AssertOffset(cbrushside_t, edgeCount, 0xB);

	struct cplane_s
	{
		float normal[3];
		float dist;
		unsigned char type;
		unsigned char pad[3];
	};

	AssertSize(cplane_s, 0x14);
	AssertOffset(cplane_s, normal, 0x0);
	AssertOffset(cplane_s, dist, 0xC);
	AssertOffset(cplane_s, type, 0x10);
	AssertOffset(cplane_s, pad, 0x11);

	enum XAnimPartType
	{
		PART_TYPE_NO_QUAT = 0x0,
		PART_TYPE_HALF_QUAT = 0x1,
		PART_TYPE_FULL_QUAT = 0x2,
		PART_TYPE_HALF_QUAT_NO_SIZE = 0x3,
		PART_TYPE_FULL_QUAT_NO_SIZE = 0x4,
		PART_TYPE_SMALL_TRANS = 0x5,
		PART_TYPE_TRANS = 0x6,
		PART_TYPE_TRANS_NO_SIZE = 0x7,
		PART_TYPE_NO_TRANS = 0x8,
		PART_TYPE_ALL = 0x9,
		PART_TYPE_COUNT = 0xA,
	};

	union XAnimIndices
	{
		char* _1;
		unsigned __int16* _2;
		void* data;
	};

	AssertSize(XAnimIndices, 0x8);
	AssertOffset(XAnimIndices, _1, 0x0);
	AssertOffset(XAnimIndices, _2, 0x0);
	AssertOffset(XAnimIndices, data, 0x0);

	struct XAnimParts
	{
		const char* name;
		unsigned __int16 dataByteCount;
		unsigned __int16 dataShortCount;
		unsigned __int16 dataIntCount;
		unsigned __int16 randomDataByteCount;
		unsigned __int16 randomDataIntCount;
		unsigned __int16 numframes;
		char flags;
		unsigned char boneCount[10];
		unsigned char notifyCount;
		char assetType;
		bool isDefault;
		unsigned int randomDataShortCount;
		unsigned int indexCount;
		float framerate;
		float frequency;
		unsigned __int16* names;
		char* dataByte;
		__int16* dataShort;
		int* dataInt;
		__int16* randomDataShort;
		char* randomDataByte;
		int* randomDataInt;
		XAnimIndices indices;
		XAnimNotifyInfo* notify;
		XAnimDeltaPart* deltaPart;
	};

	AssertSize(XAnimParts, 0x88);
	AssertOffset(XAnimParts, name, 0x0);
	AssertOffset(XAnimParts, dataByteCount, 0x8);
	AssertOffset(XAnimParts, dataShortCount, 0xA);
	AssertOffset(XAnimParts, dataIntCount, 0xC);
	AssertOffset(XAnimParts, randomDataByteCount, 0xE);
	AssertOffset(XAnimParts, randomDataIntCount, 0x10);
	AssertOffset(XAnimParts, numframes, 0x12);
	AssertOffset(XAnimParts, flags, 0x14);
	AssertOffset(XAnimParts, boneCount, 0x15);
	AssertOffset(XAnimParts, notifyCount, 0x1F);
	AssertOffset(XAnimParts, assetType, 0x20);
	AssertOffset(XAnimParts, isDefault, 0x21);
	AssertOffset(XAnimParts, randomDataShortCount, 0x24);
	AssertOffset(XAnimParts, indexCount, 0x28);
	AssertOffset(XAnimParts, framerate, 0x2C);
	AssertOffset(XAnimParts, frequency, 0x30);
	AssertOffset(XAnimParts, names, 0x38);
	AssertOffset(XAnimParts, dataByte, 0x40);
	AssertOffset(XAnimParts, dataShort, 0x48);
	AssertOffset(XAnimParts, dataInt, 0x50);
	AssertOffset(XAnimParts, randomDataShort, 0x58);
	AssertOffset(XAnimParts, randomDataByte, 0x60);
	AssertOffset(XAnimParts, randomDataInt, 0x68);
	AssertOffset(XAnimParts, indices, 0x70);
	AssertOffset(XAnimParts, notify, 0x78);
	AssertOffset(XAnimParts, deltaPart, 0x80);

	struct XAnimNotifyInfo
	{
		unsigned __int16 name;
		float time;
	};

	AssertSize(XAnimNotifyInfo, 0x8);
	AssertOffset(XAnimNotifyInfo, name, 0x0);
	AssertOffset(XAnimNotifyInfo, time, 0x4);

	struct XAnimDeltaPart
	{
		XAnimPartTrans* trans;
		XAnimDeltaPartQuat2* quat2;
		XAnimDeltaPartQuat* quat;
	};

	AssertSize(XAnimDeltaPart, 0x18);
	AssertOffset(XAnimDeltaPart, trans, 0x0);
	AssertOffset(XAnimDeltaPart, quat2, 0x8);
	AssertOffset(XAnimDeltaPart, quat, 0x10);

	union XAnimDynamicFrames
	{
		uint8_t(*_1)[3];
		unsigned __int16(*_2)[3];
	};

	AssertSize(XAnimDynamicFrames, 0x8);
	AssertOffset(XAnimDynamicFrames, _1, 0x0);
	AssertOffset(XAnimDynamicFrames, _2, 0x0);

	union XAnimDynamicIndices
	{
		char _1[1];
		unsigned __int16 _2[1];
	};

	AssertSize(XAnimDynamicIndices, 0x2);
	AssertOffset(XAnimDynamicIndices, _1, 0x0);
	AssertOffset(XAnimDynamicIndices, _2, 0x0);

	struct XAnimPartTransFrames
	{
		float mins[3];
		float size[3];
		XAnimDynamicFrames frames;
		XAnimDynamicIndices indices;
	};

	AssertSize(XAnimPartTransFrames, 0x28);
	AssertOffset(XAnimPartTransFrames, mins, 0x0);
	AssertOffset(XAnimPartTransFrames, size, 0xC);
	AssertOffset(XAnimPartTransFrames, frames, 0x18);
	AssertOffset(XAnimPartTransFrames, indices, 0x20);

	union XAnimPartTransData
	{
		XAnimPartTransFrames frames;
		float frame0[3];
	};

	AssertSize(XAnimPartTransData, 0x28);
	AssertOffset(XAnimPartTransData, frames, 0x0);
	AssertOffset(XAnimPartTransData, frame0, 0x0);

	struct XAnimPartTrans
	{
		unsigned __int16 size;
		char smallTrans;
		XAnimPartTransData u;
	};

	AssertSize(XAnimPartTrans, 0x30);
	AssertOffset(XAnimPartTrans, size, 0x0);
	AssertOffset(XAnimPartTrans, smallTrans, 0x2);
	AssertOffset(XAnimPartTrans, u, 0x8);

	struct XAnimDeltaPartQuatDataFrames2
	{
		__int16(*frames)[2];
		XAnimDynamicIndices indices;
	};

	AssertSize(XAnimDeltaPartQuatDataFrames2, 0x10);
	AssertOffset(XAnimDeltaPartQuatDataFrames2, frames, 0x0);
	AssertOffset(XAnimDeltaPartQuatDataFrames2, indices, 0x8);

	union XAnimDeltaPartQuatData2
	{
		XAnimDeltaPartQuatDataFrames2 frames;
		__int16 frame0[2];
	};

	AssertSize(XAnimDeltaPartQuatData2, 0x10);
	AssertOffset(XAnimDeltaPartQuatData2, frames, 0x0);
	AssertOffset(XAnimDeltaPartQuatData2, frame0, 0x0);

	struct XAnimDeltaPartQuat2
	{
		unsigned __int16 size;
		XAnimDeltaPartQuatData2 u;
	};

	AssertSize(XAnimDeltaPartQuat2, 0x18);
	AssertOffset(XAnimDeltaPartQuat2, size, 0x0);
	AssertOffset(XAnimDeltaPartQuat2, u, 0x8);

	struct XAnimDeltaPartQuatDataFrames
	{
		__int16(*frames)[4];
		XAnimDynamicIndices indices;
	};

	AssertSize(XAnimDeltaPartQuatDataFrames, 0x10);
	AssertOffset(XAnimDeltaPartQuatDataFrames, frames, 0x0);
	AssertOffset(XAnimDeltaPartQuatDataFrames, indices, 0x8);

	union XAnimDeltaPartQuatData
	{
		XAnimDeltaPartQuatDataFrames frames;
		__int16 frame0[4];
	};

	AssertSize(XAnimDeltaPartQuatData, 0x10);
	AssertOffset(XAnimDeltaPartQuatData, frames, 0x0);
	AssertOffset(XAnimDeltaPartQuatData, frame0, 0x0);

	struct XAnimDeltaPartQuat
	{
		unsigned __int16 size;
		XAnimDeltaPartQuatData u;
	};

	AssertSize(XAnimDeltaPartQuat, 0x18);
	AssertOffset(XAnimDeltaPartQuat, size, 0x0);
	AssertOffset(XAnimDeltaPartQuat, u, 0x8);

	struct XModelSurfs
	{
		const char* name;
		XSurface* surfs;
		unsigned short numsurfs;
		int partBits[6];
	};

	AssertSize(XModelSurfs, 0x30);
	AssertOffset(XModelSurfs, name, 0x0);
	AssertOffset(XModelSurfs, surfs, 0x8);
	AssertOffset(XModelSurfs, numsurfs, 0x10);
	AssertOffset(XModelSurfs, partBits, 0x14);

	struct XSurfaceVertexInfo
	{
		short vertCount[4];
		unsigned short* vertsBlend;
	};

	AssertSize(XSurfaceVertexInfo, 0x10);
	AssertOffset(XSurfaceVertexInfo, vertCount, 0x0);
	AssertOffset(XSurfaceVertexInfo, vertsBlend, 0x8);

	struct XSurface
	{
		unsigned char tileMode;
		bool deformed;
		unsigned short vertCount;
		unsigned short triCount;
		unsigned char zoneHandle;
		unsigned short baseTriIndex;
		unsigned short baseVertIndex;
		unsigned short* triIndices;
		XSurfaceVertexInfo vertInfo;
		GfxPackedVertex* verts0;
		unsigned int vertListCount;
		XRigidVertList* vertList;
		int partBits[6];
	};

	AssertSize(XSurface, 0x58);
	AssertOffset(XSurface, tileMode, 0x0);
	AssertOffset(XSurface, deformed, 0x1);
	AssertOffset(XSurface, vertCount, 0x2);
	AssertOffset(XSurface, triCount, 0x4);
	AssertOffset(XSurface, zoneHandle, 0x6);
	AssertOffset(XSurface, baseTriIndex, 0x8);
	AssertOffset(XSurface, baseVertIndex, 0xA);
	AssertOffset(XSurface, triIndices, 0x10);
	AssertOffset(XSurface, vertInfo, 0x18);
	AssertOffset(XSurface, verts0, 0x28);
	AssertOffset(XSurface, vertListCount, 0x30);
	AssertOffset(XSurface, vertList, 0x38);
	AssertOffset(XSurface, partBits, 0x40);

	union GfxColor
	{
		unsigned int packed;
		unsigned char array[4];
	};

	AssertSize(GfxColor, 0x4);
	AssertOffset(GfxColor, packed, 0x0);
	AssertOffset(GfxColor, array, 0x0);

	union PackedTexCoords
	{
		unsigned int packed;
	};

	AssertSize(PackedTexCoords, 0x4);
	AssertOffset(PackedTexCoords, packed, 0x0);

	union PackedUnitVec
	{
		unsigned int packed;
		unsigned char array[4];
	};

	AssertSize(PackedUnitVec, 0x4);
	AssertOffset(PackedUnitVec, packed, 0x0);
	AssertOffset(PackedUnitVec, array, 0x0);

	struct GfxPackedVertex
	{
		float xyz[3];
		float binormalSign;
		GfxColor color;
		PackedTexCoords texCoord;
		PackedUnitVec normal;
		PackedUnitVec tangent;
	};

	AssertSize(GfxPackedVertex, 0x20);
	AssertOffset(GfxPackedVertex, xyz, 0x0);
	AssertOffset(GfxPackedVertex, binormalSign, 0xC);
	AssertOffset(GfxPackedVertex, color, 0x10);
	AssertOffset(GfxPackedVertex, texCoord, 0x14);
	AssertOffset(GfxPackedVertex, normal, 0x18);
	AssertOffset(GfxPackedVertex, tangent, 0x1C);

	struct XRigidVertList
	{
		unsigned short boneOffset;
		unsigned short vertCount;
		unsigned short triOffset;
		unsigned short triCount;
		XSurfaceCollisionTree* collisionTree;
	};

	AssertSize(XRigidVertList, 0x10);
	AssertOffset(XRigidVertList, boneOffset, 0x0);
	AssertOffset(XRigidVertList, vertCount, 0x2);
	AssertOffset(XRigidVertList, triOffset, 0x4);
	AssertOffset(XRigidVertList, triCount, 0x6);
	AssertOffset(XRigidVertList, collisionTree, 0x8);

	struct XSurfaceCollisionTree
	{
		float trans[3];
		float scale[3];
		unsigned int nodeCount;
		XSurfaceCollisionNode* nodes;
		unsigned int leafCount;
		XSurfaceCollisionLeaf* leafs;
	};

	AssertSize(XSurfaceCollisionTree, 0x38);
	AssertOffset(XSurfaceCollisionTree, trans, 0x0);
	AssertOffset(XSurfaceCollisionTree, scale, 0xC);
	AssertOffset(XSurfaceCollisionTree, nodeCount, 0x18);
	AssertOffset(XSurfaceCollisionTree, nodes, 0x20);
	AssertOffset(XSurfaceCollisionTree, leafCount, 0x28);
	AssertOffset(XSurfaceCollisionTree, leafs, 0x30);

	struct XSurfaceCollisionAabb
	{
		unsigned short mins[3];
		unsigned short maxs[3];
	};

	AssertSize(XSurfaceCollisionAabb, 0xC);
	AssertOffset(XSurfaceCollisionAabb, mins, 0x0);
	AssertOffset(XSurfaceCollisionAabb, maxs, 0x6);

	struct XSurfaceCollisionNode
	{
		XSurfaceCollisionAabb aabb;
		unsigned short childBeginIndex;
		unsigned short childCount;
	};

	AssertSize(XSurfaceCollisionNode, 0x10);
	AssertOffset(XSurfaceCollisionNode, aabb, 0x0);
	AssertOffset(XSurfaceCollisionNode, childBeginIndex, 0xC);
	AssertOffset(XSurfaceCollisionNode, childCount, 0xE);

	struct XSurfaceCollisionLeaf
	{
		unsigned short triangleBeginIndex;
	};

	AssertSize(XSurfaceCollisionLeaf, 0x2);
	AssertOffset(XSurfaceCollisionLeaf, triangleBeginIndex, 0x0);

	struct XModelLodInfo
	{
		float dist;
		unsigned short numsurfs;
		unsigned short surfIndex;
		XModelSurfs* modelSurfs;
		int partBits[6];
		XSurface* surfs;
		char lod;
		char smcBaseIndexPlusOne;
		char smcSubIndexMask;
		char smcBucket;
	};

	AssertSize(XModelLodInfo, 0x38);
	AssertOffset(XModelLodInfo, dist, 0x0);
	AssertOffset(XModelLodInfo, numsurfs, 0x4);
	AssertOffset(XModelLodInfo, surfIndex, 0x6);
	AssertOffset(XModelLodInfo, modelSurfs, 0x8);
	AssertOffset(XModelLodInfo, partBits, 0x10);
	AssertOffset(XModelLodInfo, surfs, 0x28);
	AssertOffset(XModelLodInfo, lod, 0x30);
	AssertOffset(XModelLodInfo, smcBaseIndexPlusOne, 0x31);
	AssertOffset(XModelLodInfo, smcSubIndexMask, 0x32);
	AssertOffset(XModelLodInfo, smcBucket, 0x33);

	struct XModel
	{
		const char* name;
		unsigned char numBones;
		unsigned char numRootBones;
		unsigned char numsurfs;
		unsigned char lodRampType;
		float scale;
		unsigned int noScalePartBits[6];
		unsigned short* boneNames;
		unsigned char* parentList;
		short* quats;
		float* trans;
		unsigned char* partClassification;
		DObjAnimMat* baseMat;
		Material** materialHandles;
		XModelLodInfo lodInfo[4];
		unsigned char maxLoadedLod;
		unsigned char numLods;
		unsigned char collLod;
		unsigned char flags;
		XModelCollSurf_s* collSurfs;
		int numCollSurfs;
		int contents;
		XBoneInfo* boneInfo;
		float radius;
		Bounds bounds;
		int memUsage;
		bool bad;
		PhysPreset* physPreset;
		PhysCollmap* physCollmap;
	};

	AssertSize(XModel, 0x198);
	AssertOffset(XModel, name, 0x0);
	AssertOffset(XModel, numBones, 0x8);
	AssertOffset(XModel, numRootBones, 0x9);
	AssertOffset(XModel, numsurfs, 0xA);
	AssertOffset(XModel, lodRampType, 0xB);
	AssertOffset(XModel, scale, 0xC);
	AssertOffset(XModel, noScalePartBits, 0x10);
	AssertOffset(XModel, boneNames, 0x28);
	AssertOffset(XModel, parentList, 0x30);
	AssertOffset(XModel, quats, 0x38);
	AssertOffset(XModel, trans, 0x40);
	AssertOffset(XModel, partClassification, 0x48);
	AssertOffset(XModel, baseMat, 0x50);
	AssertOffset(XModel, materialHandles, 0x58);
	AssertOffset(XModel, lodInfo, 0x60);
	AssertOffset(XModel, maxLoadedLod, 0x140);
	AssertOffset(XModel, numLods, 0x141);
	AssertOffset(XModel, collLod, 0x142);
	AssertOffset(XModel, flags, 0x143);
	AssertOffset(XModel, collSurfs, 0x148);
	AssertOffset(XModel, numCollSurfs, 0x150);
	AssertOffset(XModel, contents, 0x154);
	AssertOffset(XModel, boneInfo, 0x158);
	AssertOffset(XModel, radius, 0x160);
	AssertOffset(XModel, bounds, 0x164);
	AssertOffset(XModel, memUsage, 0x17C);
	AssertOffset(XModel, bad, 0x180);
	AssertOffset(XModel, physPreset, 0x188);
	AssertOffset(XModel, physCollmap, 0x190);

	struct DObjAnimMat
	{
		float quat[4];
		float trans[3];
		float transWeight;
	};

	AssertSize(DObjAnimMat, 0x20);
	AssertOffset(DObjAnimMat, quat, 0x0);
	AssertOffset(DObjAnimMat, trans, 0x10);
	AssertOffset(DObjAnimMat, transWeight, 0x1C);

	struct DObjSkelMat
	{
		float axis[3][4];
		float origin[4];
	};

	AssertSize(DObjSkelMat, 0x40);
	AssertOffset(DObjSkelMat, axis, 0x0);
	AssertOffset(DObjSkelMat, origin, 0x30);

	struct GfxDrawSurfFields
	{
		unsigned __int64 objectId : 16;
		unsigned __int64 reflectionProbeIndex : 8;
		unsigned __int64 hasGfxEntIndex : 1;
		unsigned __int64 customIndex : 5;
		unsigned __int64 materialSortedIndex : 12;
		unsigned __int64 prepass : 2;
		unsigned __int64 useHeroLighting : 1;
		unsigned __int64 sceneLightIndex : 8;
		unsigned __int64 surfType : 4;
		unsigned __int64 primarySortKey : 6;
		unsigned __int64 unused : 1;
	};

	AssertSize(GfxDrawSurfFields, 0x8);

	union GfxDrawSurf
	{
		GfxDrawSurfFields fields;
		unsigned __int64 packed;
	};

	AssertSize(GfxDrawSurf, 0x8);
	AssertOffset(GfxDrawSurf, fields, 0x0);
	AssertOffset(GfxDrawSurf, packed, 0x0);

	struct MaterialInfo
	{
		const char* name;
		unsigned char gameFlags;
		unsigned char sortKey;
		unsigned char textureAtlasRowCount;
		unsigned char textureAtlasColumnCount;
		GfxDrawSurf drawSurf;
		unsigned int surfaceTypeBits;
		unsigned short hashIndex;
	};

	AssertSize(MaterialInfo, 0x20);
	AssertOffset(MaterialInfo, name, 0x0);
	AssertOffset(MaterialInfo, gameFlags, 0x8);
	AssertOffset(MaterialInfo, sortKey, 0x9);
	AssertOffset(MaterialInfo, textureAtlasRowCount, 0xA);
	AssertOffset(MaterialInfo, textureAtlasColumnCount, 0xB);
	AssertOffset(MaterialInfo, drawSurf, 0x10);
	AssertOffset(MaterialInfo, surfaceTypeBits, 0x18);
	AssertOffset(MaterialInfo, hashIndex, 0x1C);

	struct Material
	{
		MaterialInfo info;
		unsigned char stateBitsEntry[48];
		unsigned char textureCount;
		unsigned char constantCount;
		unsigned char stateBitsCount;
		unsigned char stateFlags;
		unsigned char cameraRegion;
		MaterialTechniqueSet* techniqueSet;
		MaterialTextureDef* textureTable;
		MaterialConstantDef* constantTable;
		GfxStateBits* stateBitsTable;
	};

	AssertSize(Material, 0x78);
	AssertOffset(Material, info, 0x0);
	AssertOffset(Material, stateBitsEntry, 0x20);
	AssertOffset(Material, textureCount, 0x50);
	AssertOffset(Material, constantCount, 0x51);
	AssertOffset(Material, stateBitsCount, 0x52);
	AssertOffset(Material, stateFlags, 0x53);
	AssertOffset(Material, cameraRegion, 0x54);
	AssertOffset(Material, techniqueSet, 0x58);
	AssertOffset(Material, textureTable, 0x60);
	AssertOffset(Material, constantTable, 0x68);
	AssertOffset(Material, stateBitsTable, 0x70);

	struct MaterialTechniqueSet
	{
		const char* name;
		char worldVertFormat;
		bool hasBeenUploaded;
		char unused[1];
		MaterialTechniqueSet* remappedTechniqueSet;
		MaterialTechnique* techniques[48];
	};

	AssertSize(MaterialTechniqueSet, 0x198);
	AssertOffset(MaterialTechniqueSet, name, 0x0);
	AssertOffset(MaterialTechniqueSet, worldVertFormat, 0x8);
	AssertOffset(MaterialTechniqueSet, hasBeenUploaded, 0x9);
	AssertOffset(MaterialTechniqueSet, unused, 0xA);
	AssertOffset(MaterialTechniqueSet, remappedTechniqueSet, 0x10);
	AssertOffset(MaterialTechniqueSet, techniques, 0x18);

	struct MaterialPass
	{
		MaterialVertexDeclaration* vertexDecl;
		MaterialVertexShader* vertexShader;
		MaterialPixelShader* pixelShader;
		char perPrimArgCount;
		char perObjArgCount;
		char stableArgCount;
		char customSamplerFlags;
		MaterialShaderArgument* args;
	};

	AssertSize(MaterialPass, 0x28);
	AssertOffset(MaterialPass, vertexDecl, 0x0);
	AssertOffset(MaterialPass, vertexShader, 0x8);
	AssertOffset(MaterialPass, pixelShader, 0x10);
	AssertOffset(MaterialPass, perPrimArgCount, 0x18);
	AssertOffset(MaterialPass, perObjArgCount, 0x19);
	AssertOffset(MaterialPass, stableArgCount, 0x1A);
	AssertOffset(MaterialPass, customSamplerFlags, 0x1B);
	AssertOffset(MaterialPass, args, 0x20);

	struct MaterialTechnique
	{
		const char* name;
		unsigned __int16 flags;
		unsigned __int16 passCount;
		MaterialPass passArray[1];
	};

	AssertSize(MaterialTechnique, 0x38);
	AssertOffset(MaterialTechnique, name, 0x0);
	AssertOffset(MaterialTechnique, flags, 0x8);
	AssertOffset(MaterialTechnique, passCount, 0xA);
	AssertOffset(MaterialTechnique, passArray, 0x10);

	struct MaterialStreamRouting
	{
		char source;
		char dest;
	};

	AssertSize(MaterialStreamRouting, 0x2);
	AssertOffset(MaterialStreamRouting, source, 0x0);
	AssertOffset(MaterialStreamRouting, dest, 0x1);

	struct MaterialVertexStreamRouting
	{
		MaterialStreamRouting data[13];
		IDirect3DVertexDeclaration9* decl[16];
	};

	AssertSize(MaterialVertexStreamRouting, 0xA0);
	AssertOffset(MaterialVertexStreamRouting, data, 0x0);
	AssertOffset(MaterialVertexStreamRouting, decl, 0x20);

	struct MaterialVertexDeclaration
	{
		const char* name;
		char streamCount;
		bool hasOptionalSource;
		MaterialVertexStreamRouting routing;
	};

	AssertSize(MaterialVertexDeclaration, 0xB0);
	AssertOffset(MaterialVertexDeclaration, name, 0x0);
	AssertOffset(MaterialVertexDeclaration, streamCount, 0x8);
	AssertOffset(MaterialVertexDeclaration, hasOptionalSource, 0x9);
	AssertOffset(MaterialVertexDeclaration, routing, 0x10);

	struct GfxVertexShaderLoadDef
	{
		unsigned int* program;
		unsigned __int16 programSize;
		unsigned __int16 loadForRenderer;
	};

	AssertSize(GfxVertexShaderLoadDef, 0x10);
	AssertOffset(GfxVertexShaderLoadDef, program, 0x0);
	AssertOffset(GfxVertexShaderLoadDef, programSize, 0x8);
	AssertOffset(GfxVertexShaderLoadDef, loadForRenderer, 0xA);

	struct MaterialVertexShaderProgram
	{
		IDirect3DVertexShader9* vs;
		GfxVertexShaderLoadDef loadDef;
	};

	AssertSize(MaterialVertexShaderProgram, 0x18);
	AssertOffset(MaterialVertexShaderProgram, vs, 0x0);
	AssertOffset(MaterialVertexShaderProgram, loadDef, 0x8);

	struct MaterialVertexShader
	{
		const char* name;
		MaterialVertexShaderProgram prog;
	};

	AssertSize(MaterialVertexShader, 0x20);
	AssertOffset(MaterialVertexShader, name, 0x0);
	AssertOffset(MaterialVertexShader, prog, 0x8);

	struct GfxPixelShaderLoadDef
	{
		unsigned int* program;
		unsigned __int16 programSize;
		unsigned __int16 loadForRenderer;
	};

	AssertSize(GfxPixelShaderLoadDef, 0x10);
	AssertOffset(GfxPixelShaderLoadDef, program, 0x0);
	AssertOffset(GfxPixelShaderLoadDef, programSize, 0x8);
	AssertOffset(GfxPixelShaderLoadDef, loadForRenderer, 0xA);

	struct MaterialPixelShaderProgram
	{
		IDirect3DPixelShader9* ps;
		GfxPixelShaderLoadDef loadDef;
	};

	AssertSize(MaterialPixelShaderProgram, 0x18);
	AssertOffset(MaterialPixelShaderProgram, ps, 0x0);
	AssertOffset(MaterialPixelShaderProgram, loadDef, 0x8);

	struct MaterialPixelShader
	{
		const char* name;
		MaterialPixelShaderProgram prog;
	};

	AssertSize(MaterialPixelShader, 0x20);
	AssertOffset(MaterialPixelShader, name, 0x0);
	AssertOffset(MaterialPixelShader, prog, 0x8);

	struct MaterialArgumentCodeConst
	{
		unsigned __int16 index;
		char firstRow;
		char rowCount;
	};

	AssertSize(MaterialArgumentCodeConst, 0x4);
	AssertOffset(MaterialArgumentCodeConst, index, 0x0);
	AssertOffset(MaterialArgumentCodeConst, firstRow, 0x2);
	AssertOffset(MaterialArgumentCodeConst, rowCount, 0x3);

	union MaterialArgumentDef
	{
		float* literalConst;
		MaterialArgumentCodeConst codeConst;
		unsigned int codeSampler;
		unsigned int nameHash;
	};

	AssertSize(MaterialArgumentDef, 0x8);
	AssertOffset(MaterialArgumentDef, literalConst, 0x0);
	AssertOffset(MaterialArgumentDef, codeConst, 0x0);
	AssertOffset(MaterialArgumentDef, codeSampler, 0x0);
	AssertOffset(MaterialArgumentDef, nameHash, 0x0);

	struct MaterialShaderArgument
	{
		MaterialShaderArgumentType type;
		unsigned __int16 dest;
		MaterialArgumentDef u;
	};

	AssertSize(MaterialShaderArgument, 0x10);
	AssertOffset(MaterialShaderArgument, type, 0x0);
	AssertOffset(MaterialShaderArgument, dest, 0x2);
	AssertOffset(MaterialShaderArgument, u, 0x8);

	union MaterialTextureDefInfo
	{
		GfxImage* image;
		water_t* water;
	};

	AssertSize(MaterialTextureDefInfo, 0x8);
	AssertOffset(MaterialTextureDefInfo, image, 0x0);
	AssertOffset(MaterialTextureDefInfo, water, 0x0);

	struct MaterialTextureDef
	{
		unsigned int nameHash;
		char nameStart;
		char nameEnd;
		char samplerState;
		char semantic;
		MaterialTextureDefInfo u;
	};

	AssertSize(MaterialTextureDef, 0x10);
	AssertOffset(MaterialTextureDef, nameHash, 0x0);
	AssertOffset(MaterialTextureDef, nameStart, 0x4);
	AssertOffset(MaterialTextureDef, nameEnd, 0x5);
	AssertOffset(MaterialTextureDef, samplerState, 0x6);
	AssertOffset(MaterialTextureDef, semantic, 0x7);
	AssertOffset(MaterialTextureDef, u, 0x8);

	union GfxTexture
	{
		IDirect3DBaseTexture9* basemap;
		IDirect3DTexture9* map;
		IDirect3DVolumeTexture9* volmap;
		IDirect3DCubeTexture9* cubemap;
		GfxImageLoadDef* loadDef;
	};

	AssertSize(GfxTexture, 0x8);
	AssertOffset(GfxTexture, basemap, 0x0);
	AssertOffset(GfxTexture, map, 0x0);
	AssertOffset(GfxTexture, volmap, 0x0);
	AssertOffset(GfxTexture, cubemap, 0x0);
	AssertOffset(GfxTexture, loadDef, 0x0);

	struct Picmip
	{
		char platform[2];
	};

	AssertSize(Picmip, 0x2);
	AssertOffset(Picmip, platform, 0x0);

	struct CardMemory
	{
		int platform[2];
	};

	AssertSize(CardMemory, 0x8);
	AssertOffset(CardMemory, platform, 0x0);

	struct GfxImage
	{
		GfxTexture texture;
		unsigned char mapType;
		unsigned char semantic;
		unsigned char category;
		bool useSrgbReads;
		Picmip picmip;
		bool noPicmip;
		unsigned char track;
		CardMemory cardMemory;
		unsigned short width;
		unsigned short height;
		unsigned short depth;
		bool delayLoadPixels;
		const char* name;
	};

	AssertSize(GfxImage, 0x28);
	AssertOffset(GfxImage, texture, 0x0);
	AssertOffset(GfxImage, mapType, 0x8);
	AssertOffset(GfxImage, semantic, 0x9);
	AssertOffset(GfxImage, category, 0xA);
	AssertOffset(GfxImage, useSrgbReads, 0xB);
	AssertOffset(GfxImage, picmip, 0xC);
	AssertOffset(GfxImage, noPicmip, 0xE);
	AssertOffset(GfxImage, track, 0xF);
	AssertOffset(GfxImage, cardMemory, 0x10);
	AssertOffset(GfxImage, width, 0x18);
	AssertOffset(GfxImage, height, 0x1A);
	AssertOffset(GfxImage, depth, 0x1C);
	AssertOffset(GfxImage, delayLoadPixels, 0x1E);
	AssertOffset(GfxImage, name, 0x20);

	struct GfxImageLoadDef
	{
		char levelCount;
		char pad[3];
		int flags;
		int format;
		int resourceSize;
		unsigned char data[1];
	};

	AssertSize(GfxImageLoadDef, 0x14);
	AssertOffset(GfxImageLoadDef, levelCount, 0x0);
	AssertOffset(GfxImageLoadDef, pad, 0x1);
	AssertOffset(GfxImageLoadDef, flags, 0x4);
	AssertOffset(GfxImageLoadDef, format, 0x8);
	AssertOffset(GfxImageLoadDef, resourceSize, 0xC);
	AssertOffset(GfxImageLoadDef, data, 0x10);

	enum GfxImageFileFormat
	{
		IMG_FORMAT_INVALID = 0x0,
		IMG_FORMAT_BITMAP_RGBA = 0x1,
		IMG_FORMAT_BITMAP_RGB = 0x2,
		IMG_FORMAT_BITMAP_LUMINANCE_ALPHA = 0x3,
		IMG_FORMAT_BITMAP_LUMINANCE = 0x4,
		IMG_FORMAT_BITMAP_ALPHA = 0x5,
		IMG_FORMAT_WAVELET_RGBA = 0x6,
		IMG_FORMAT_WAVELET_RGB = 0x7,
		IMG_FORMAT_WAVELET_LUMINANCE_ALPHA = 0x8,
		IMG_FORMAT_WAVELET_LUMINANCE = 0x9,
		IMG_FORMAT_WAVELET_ALPHA = 0xA,
		IMG_FORMAT_DXT1 = 0xB,
		IMG_FORMAT_DXT3 = 0xC,
		IMG_FORMAT_DXT5 = 0xD,
		IMG_FORMAT_DXN = 0xE,
		IMG_FORMAT_DXT3A_AS_LUMINANCE = 0xF,
		IMG_FORMAT_DXT5A_AS_LUMINANCE = 0x10,
		IMG_FORMAT_DXT3A_AS_ALPHA = 0x11,
		IMG_FORMAT_DXT5A_AS_ALPHA = 0x12,
		IMG_FORMAT_DXT1_AS_LUMINANCE_ALPHA = 0x13,
		IMG_FORMAT_DXN_AS_LUMINANCE_ALPHA = 0x14,
		IMG_FORMAT_DXT1_AS_LUMINANCE = 0x15,
		IMG_FORMAT_DXT1_AS_ALPHA = 0x16,
		IMG_FORMAT_COUNT = 0x17,
	};

	struct GfxImageFileHeader
	{
		char tag[3];
		char version;
		unsigned int flags;
		char format;
		char unused;
		__int16 dimensions[3];
		int fileSizeForPicmip[4];
	};

	AssertSize(GfxImageFileHeader, 0x20);
	AssertOffset(GfxImageFileHeader, flags, 0x4);
	AssertOffset(GfxImageFileHeader, format, 0x8);
	AssertOffset(GfxImageFileHeader, dimensions, 0xA);
	AssertOffset(GfxImageFileHeader, fileSizeForPicmip, 0x10);

	struct WaterWritable
	{
		float floatTime;
	};

	AssertSize(WaterWritable, 0x4);
	AssertOffset(WaterWritable, floatTime, 0x0);

	struct water_t
	{
		WaterWritable writable;
		float* H0Real;
		float* H0Imag;
		float* wTerm;
		int M;
		int N;
		float Lx;
		float Lz;
		float gravity;
		float windvel;
		float winddir[2];
		float amplitude;
		float codeConstant[4];
		GfxImage* image;
	};

	AssertSize(water_t, 0x60);
	AssertOffset(water_t, writable, 0x0);
	AssertOffset(water_t, H0Real, 0x8);
	AssertOffset(water_t, H0Imag, 0x10);
	AssertOffset(water_t, wTerm, 0x18);
	AssertOffset(water_t, M, 0x20);
	AssertOffset(water_t, N, 0x24);
	AssertOffset(water_t, Lx, 0x28);
	AssertOffset(water_t, Lz, 0x2C);
	AssertOffset(water_t, gravity, 0x30);
	AssertOffset(water_t, windvel, 0x34);
	AssertOffset(water_t, winddir, 0x38);
	AssertOffset(water_t, amplitude, 0x40);
	AssertOffset(water_t, codeConstant, 0x44);
	AssertOffset(water_t, image, 0x58);

	struct complex_s
	{
		float real;
		float imag;
	};

	AssertSize(complex_s, 0x8);
	AssertOffset(complex_s, real, 0x0);
	AssertOffset(complex_s, imag, 0x4);

	struct MaterialConstantDef
	{
		unsigned int nameHash;
		char name[12];
		float literal[4];
	};

	AssertSize(MaterialConstantDef, 0x20);
	AssertOffset(MaterialConstantDef, nameHash, 0x0);
	AssertOffset(MaterialConstantDef, name, 0x4);
	AssertOffset(MaterialConstantDef, literal, 0x10);

	struct GfxStateBits
	{
		unsigned int loadBits[2];
	};

	AssertSize(GfxStateBits, 0x8);
	AssertOffset(GfxStateBits, loadBits, 0x0);

	struct XModelCollSurf_s
	{
		XModelCollTri_s* collTris;
		int numCollTris;
		Bounds bounds;
		int boneIdx;
		int contents;
		int surfFlags;
	};

	AssertSize(XModelCollSurf_s, 0x30);
	AssertOffset(XModelCollSurf_s, collTris, 0x0);
	AssertOffset(XModelCollSurf_s, numCollTris, 0x8);
	AssertOffset(XModelCollSurf_s, bounds, 0xC);
	AssertOffset(XModelCollSurf_s, boneIdx, 0x24);
	AssertOffset(XModelCollSurf_s, contents, 0x28);
	AssertOffset(XModelCollSurf_s, surfFlags, 0x2C);

	struct XModelCollTri_s
	{
		float plane[4];
		float svec[4];
		float tvec[4];
	};

	AssertSize(XModelCollTri_s, 0x30);
	AssertOffset(XModelCollTri_s, plane, 0x0);
	AssertOffset(XModelCollTri_s, svec, 0x10);
	AssertOffset(XModelCollTri_s, tvec, 0x20);

	struct XBoneInfo
	{
		Bounds bounds;
		float radiusSquared;
	};

	AssertSize(XBoneInfo, 0x1C);
	AssertOffset(XBoneInfo, bounds, 0x0);
	AssertOffset(XBoneInfo, radiusSquared, 0x18);

	struct snd_alias_list_t
	{
		const char* aliasName;
		snd_alias_t* head;
		unsigned int count;
	};

	AssertSize(snd_alias_list_t, 0x18);
	AssertOffset(snd_alias_list_t, aliasName, 0x0);
	AssertOffset(snd_alias_list_t, head, 0x8);
	AssertOffset(snd_alias_list_t, count, 0x10);

	union SoundAliasFlags
	{
#pragma warning(push)
#pragma warning(disable: 4201)
		struct
		{
			unsigned int looping : 1;
			unsigned int isMaster : 1;
			unsigned int isSlave : 1;
			unsigned int fullDryLevel : 1;
			unsigned int noWetLevel : 1;
			unsigned int unknown : 1;
			unsigned int unk_is3D : 1;
			unsigned int type : 2;
			unsigned int channel : 6;
		};
#pragma warning(pop)
		unsigned int intValue;
	};

	AssertSize(SoundAliasFlags, 0x4);
	AssertOffset(SoundAliasFlags, intValue, 0x0);

	struct snd_alias_t
	{
		const char* aliasName;
		const char* subtitle;
		const char* secondaryAliasName;
		const char* chainAliasName;
		const char* mixerGroup;
		SoundFile* soundFile;
		int sequence;
		float volMin;
		float volMax;
		float pitchMin;
		float pitchMax;
		float distMin;
		float distMax;
		float velocityMin;
		SoundAliasFlags flags;
		union
		{
			float slavePercentage;
			float masterPercentage;
		} ___u15;
		float probability;
		float lfePercentage;
		float centerPercentage;
		int startDelay;
		SndCurve* volumeFalloffCurve;
		float envelopMin;
		float envelopMax;
		float envelopPercentage;
		SpeakerMap* speakerMap;
	};

	AssertSize(snd_alias_t, 0x88);
	AssertOffset(snd_alias_t, aliasName, 0x0);
	AssertOffset(snd_alias_t, subtitle, 0x8);
	AssertOffset(snd_alias_t, secondaryAliasName, 0x10);
	AssertOffset(snd_alias_t, chainAliasName, 0x18);
	AssertOffset(snd_alias_t, mixerGroup, 0x20);
	AssertOffset(snd_alias_t, soundFile, 0x28);
	AssertOffset(snd_alias_t, sequence, 0x30);
	AssertOffset(snd_alias_t, volMin, 0x34);
	AssertOffset(snd_alias_t, volMax, 0x38);
	AssertOffset(snd_alias_t, pitchMin, 0x3C);
	AssertOffset(snd_alias_t, pitchMax, 0x40);
	AssertOffset(snd_alias_t, distMin, 0x44);
	AssertOffset(snd_alias_t, distMax, 0x48);
	AssertOffset(snd_alias_t, velocityMin, 0x4C);
	AssertOffset(snd_alias_t, flags, 0x50);
	AssertOffset(snd_alias_t, ___u15, 0x54);
	AssertOffset(snd_alias_t, probability, 0x58);
	AssertOffset(snd_alias_t, lfePercentage, 0x5C);
	AssertOffset(snd_alias_t, centerPercentage, 0x60);
	AssertOffset(snd_alias_t, startDelay, 0x64);
	AssertOffset(snd_alias_t, volumeFalloffCurve, 0x68);
	AssertOffset(snd_alias_t, envelopMin, 0x70);
	AssertOffset(snd_alias_t, envelopMax, 0x74);
	AssertOffset(snd_alias_t, envelopPercentage, 0x78);
	AssertOffset(snd_alias_t, speakerMap, 0x80);

	struct StreamFileNameRaw
	{
		const char* dir;
		const char* name;
	};

	AssertSize(StreamFileNameRaw, 0x10);
	AssertOffset(StreamFileNameRaw, dir, 0x0);
	AssertOffset(StreamFileNameRaw, name, 0x8);

	union StreamFileInfo
	{
		StreamFileNameRaw raw;
	};

	AssertSize(StreamFileInfo, 0x10);
	AssertOffset(StreamFileInfo, raw, 0x0);

	struct StreamFileName
	{
		StreamFileInfo info;
	};

	AssertSize(StreamFileName, 0x10);
	AssertOffset(StreamFileName, info, 0x0);

	struct StreamedSound
	{
		StreamFileName filename;
	};

	AssertSize(StreamedSound, 0x10);
	AssertOffset(StreamedSound, filename, 0x0);

	union SoundFileRef
	{
		LoadedSound* loadSnd;
		StreamedSound streamSnd;
	};

	AssertSize(SoundFileRef, 0x10);
	AssertOffset(SoundFileRef, loadSnd, 0x0);
	AssertOffset(SoundFileRef, streamSnd, 0x0);

	struct SoundFile
	{
		char type;
		char exists;
		SoundFileRef u;
	};

	AssertSize(SoundFile, 0x18);
	AssertOffset(SoundFile, type, 0x0);
	AssertOffset(SoundFile, exists, 0x1);
	AssertOffset(SoundFile, u, 0x8);

	struct MssSound
	{
		unsigned short formatTag;
		unsigned short channels;
		unsigned int rate;
		unsigned int averageBytes;
		unsigned short blockAlign;
		unsigned short bits;
		char pad0[8];
		unsigned int dataLength;
		char pad1[20];
		char* data;
	};

	AssertSize(MssSound, 0x38);
	AssertOffset(MssSound, formatTag, 0x0);
	AssertOffset(MssSound, channels, 0x2);
	AssertOffset(MssSound, rate, 0x4);
	AssertOffset(MssSound, averageBytes, 0x8);
	AssertOffset(MssSound, blockAlign, 0xC);
	AssertOffset(MssSound, bits, 0xE);
	AssertOffset(MssSound, dataLength, 0x18);
	AssertOffset(MssSound, data, 0x30);

	struct LoadedSound
	{
		const char* name;
		MssSound sound;
	};

	AssertSize(LoadedSound, 0x40);
	AssertOffset(LoadedSound, name, 0x0);
	AssertOffset(LoadedSound, sound, 0x8);

	struct SndCurve
	{
		const char* filename;
		unsigned __int16 knotCount;
		float knots[16][2];
	};

	AssertSize(SndCurve, 0x90);
	AssertOffset(SndCurve, filename, 0x0);
	AssertOffset(SndCurve, knotCount, 0x8);
	AssertOffset(SndCurve, knots, 0xC);

	struct SpeakerMapEntry
	{
		unsigned char level;
		unsigned char speaker;
		char pad0[2];
		float value;
	};

	AssertSize(SpeakerMapEntry, 0x8);
	AssertOffset(SpeakerMapEntry, level, 0x0);
	AssertOffset(SpeakerMapEntry, speaker, 0x1);
	AssertOffset(SpeakerMapEntry, value, 0x4);

	struct SpeakerMapChannel
	{
		unsigned char entryCount;
		SpeakerMapEntry* entries;
	};

	AssertSize(SpeakerMapChannel, 0x10);
	AssertOffset(SpeakerMapChannel, entryCount, 0x0);
	AssertOffset(SpeakerMapChannel, entries, 0x8);

	struct SpeakerMap
	{
		bool isDefault;
		const char* name;
		SpeakerMapChannel channelMaps[2][2];
	};

	AssertSize(SpeakerMap, 0x50);
	AssertOffset(SpeakerMap, isDefault, 0x0);
	AssertOffset(SpeakerMap, name, 0x8);
	AssertOffset(SpeakerMap, channelMaps, 0x10);

	struct alignas(64) clipMap_t
	{
		const char* name;
		int isInUse;
		unsigned int planeCount;
		cplane_s* planes;
		unsigned int numStaticModels;
		cStaticModel_s* staticModelList;
		unsigned int numMaterials;
		ClipMaterial* materials;
		unsigned int numBrushSides;
		cbrushside_t* brushsides;
		unsigned int numBrushEdges;
		unsigned char* brushEdges;
		unsigned int numNodes;
		cNode_t* nodes;
		unsigned int numLeafs;
		cLeaf_t* leafs;
		unsigned int leafbrushNodesCount;
		cLeafBrushNode_s* leafbrushNodes;
		unsigned int numLeafBrushes;
		unsigned __int16* leafbrushes;
		unsigned int numLeafSurfaces;
		unsigned int* leafsurfaces;
		unsigned int vertCount;
		vec3_t* verts;
		unsigned int triCount;
		unsigned __int16* triIndices;
		unsigned char* triEdgeIsWalkable;
		unsigned int borderCount;
		CollisionBorder* borders;
		unsigned int partitionCount;
		CollisionPartition* partitions;
		unsigned int aabbTreeCount;
		CollisionAabbTree* aabbTrees;
		unsigned int numSubModels;
		cmodel_t* cmodels;
		unsigned __int16 numBrushes;
		cbrush_t* brushes;
		Bounds* brushBounds;
		int* brushContents;
		MapEnts* mapEnts;
		unsigned __int16 smodelNodeCount;
		SModelAabbNode* smodelNodes;
		unsigned __int16 dynEntCount[2];
		DynEntityDef* dynEntDefList[2];
		DynEntityPose* dynEntPoseList[2];
		DynEntityClient* dynEntClientList[2];
		DynEntityColl* dynEntCollList[2];
		unsigned int checksum;
		char pad[0x6C];
	};

	AssertSize(clipMap_t, 0x200);
	AssertOffset(clipMap_t, name, 0x0);
	AssertOffset(clipMap_t, isInUse, 0x8);
	AssertOffset(clipMap_t, planeCount, 0xC);
	AssertOffset(clipMap_t, planes, 0x10);
	AssertOffset(clipMap_t, numStaticModels, 0x18);
	AssertOffset(clipMap_t, staticModelList, 0x20);
	AssertOffset(clipMap_t, numMaterials, 0x28);
	AssertOffset(clipMap_t, materials, 0x30);
	AssertOffset(clipMap_t, numBrushSides, 0x38);
	AssertOffset(clipMap_t, brushsides, 0x40);
	AssertOffset(clipMap_t, numBrushEdges, 0x48);
	AssertOffset(clipMap_t, brushEdges, 0x50);
	AssertOffset(clipMap_t, numNodes, 0x58);
	AssertOffset(clipMap_t, nodes, 0x60);
	AssertOffset(clipMap_t, numLeafs, 0x68);
	AssertOffset(clipMap_t, leafs, 0x70);
	AssertOffset(clipMap_t, leafbrushNodesCount, 0x78);
	AssertOffset(clipMap_t, leafbrushNodes, 0x80);
	AssertOffset(clipMap_t, numLeafBrushes, 0x88);
	AssertOffset(clipMap_t, leafbrushes, 0x90);
	AssertOffset(clipMap_t, numLeafSurfaces, 0x98);
	AssertOffset(clipMap_t, leafsurfaces, 0xA0);
	AssertOffset(clipMap_t, vertCount, 0xA8);
	AssertOffset(clipMap_t, verts, 0xB0);
	AssertOffset(clipMap_t, triCount, 0xB8);
	AssertOffset(clipMap_t, triIndices, 0xC0);
	AssertOffset(clipMap_t, triEdgeIsWalkable, 0xC8);
	AssertOffset(clipMap_t, borderCount, 0xD0);
	AssertOffset(clipMap_t, borders, 0xD8);
	AssertOffset(clipMap_t, partitionCount, 0xE0);
	AssertOffset(clipMap_t, partitions, 0xE8);
	AssertOffset(clipMap_t, aabbTreeCount, 0xF0);
	AssertOffset(clipMap_t, aabbTrees, 0xF8);
	AssertOffset(clipMap_t, numSubModels, 0x100);
	AssertOffset(clipMap_t, cmodels, 0x108);
	AssertOffset(clipMap_t, numBrushes, 0x110);
	AssertOffset(clipMap_t, brushes, 0x118);
	AssertOffset(clipMap_t, brushBounds, 0x120);
	AssertOffset(clipMap_t, brushContents, 0x128);
	AssertOffset(clipMap_t, mapEnts, 0x130);
	AssertOffset(clipMap_t, smodelNodeCount, 0x138);
	AssertOffset(clipMap_t, smodelNodes, 0x140);
	AssertOffset(clipMap_t, dynEntCount, 0x148);
	AssertOffset(clipMap_t, dynEntDefList, 0x150);
	AssertOffset(clipMap_t, dynEntPoseList, 0x160);
	AssertOffset(clipMap_t, dynEntClientList, 0x170);
	AssertOffset(clipMap_t, dynEntCollList, 0x180);
	AssertOffset(clipMap_t, checksum, 0x190);
	AssertOffset(clipMap_t, pad, 0x194);

	struct cStaticModel_s
	{
		XModel* xmodel;
		float origin[3];
		float invScaledAxis[3][3];
		Bounds absBounds;
	};

	AssertSize(cStaticModel_s, 0x50);
	AssertOffset(cStaticModel_s, xmodel, 0x0);
	AssertOffset(cStaticModel_s, origin, 0x8);
	AssertOffset(cStaticModel_s, invScaledAxis, 0x14);
	AssertOffset(cStaticModel_s, absBounds, 0x38);

	struct ClipMaterial
	{
		const char* name;
		int surfaceFlags;
		int contents;
	};

	AssertSize(ClipMaterial, 0x10);
	AssertOffset(ClipMaterial, name, 0x0);
	AssertOffset(ClipMaterial, surfaceFlags, 0x8);
	AssertOffset(ClipMaterial, contents, 0xC);

	struct cNode_t
	{
		cplane_s* plane;
		__int16 children[2];
	};

	AssertSize(cNode_t, 0x10);
	AssertOffset(cNode_t, plane, 0x0);
	AssertOffset(cNode_t, children, 0x8);

	struct cLeaf_t
	{
		unsigned __int16 firstCollAabbIndex;
		unsigned __int16 collAabbCount;
		int brushContents;
		int terrainContents;
		Bounds bounds;
		int leafBrushNode;
	};

	AssertSize(cLeaf_t, 0x28);
	AssertOffset(cLeaf_t, firstCollAabbIndex, 0x0);
	AssertOffset(cLeaf_t, collAabbCount, 0x2);
	AssertOffset(cLeaf_t, brushContents, 0x4);
	AssertOffset(cLeaf_t, terrainContents, 0x8);
	AssertOffset(cLeaf_t, bounds, 0xC);
	AssertOffset(cLeaf_t, leafBrushNode, 0x24);

	struct cLeafBrushNodeLeaf_t
	{
		unsigned __int16* brushes;
	};

	AssertSize(cLeafBrushNodeLeaf_t, 0x8);
	AssertOffset(cLeafBrushNodeLeaf_t, brushes, 0x0);

	struct cLeafBrushNodeChildren_t
	{
		float dist;
		float range;
		unsigned __int16 childOffset[2];
	};

	AssertSize(cLeafBrushNodeChildren_t, 0xC);
	AssertOffset(cLeafBrushNodeChildren_t, dist, 0x0);
	AssertOffset(cLeafBrushNodeChildren_t, range, 0x4);
	AssertOffset(cLeafBrushNodeChildren_t, childOffset, 0x8);

	union cLeafBrushNodeData_t
	{
		cLeafBrushNodeLeaf_t leaf;
		cLeafBrushNodeChildren_t children;
	};

	AssertSize(cLeafBrushNodeData_t, 0x10);
	AssertOffset(cLeafBrushNodeData_t, leaf, 0x0);
	AssertOffset(cLeafBrushNodeData_t, children, 0x0);

	struct cLeafBrushNode_s
	{
		unsigned char axis;
		__int16 leafBrushCount;
		int contents;
		cLeafBrushNodeData_t data;
	};

	AssertSize(cLeafBrushNode_s, 0x18);
	AssertOffset(cLeafBrushNode_s, axis, 0x0);
	AssertOffset(cLeafBrushNode_s, leafBrushCount, 0x2);
	AssertOffset(cLeafBrushNode_s, contents, 0x4);
	AssertOffset(cLeafBrushNode_s, data, 0x8);

	struct CollisionBorder
	{
		float distEq[3];
		float zBase;
		float zSlope;
		float start;
		float length;
	};

	AssertSize(CollisionBorder, 0x1C);
	AssertOffset(CollisionBorder, distEq, 0x0);
	AssertOffset(CollisionBorder, zBase, 0xC);
	AssertOffset(CollisionBorder, zSlope, 0x10);
	AssertOffset(CollisionBorder, start, 0x14);
	AssertOffset(CollisionBorder, length, 0x18);

	struct CollisionPartition
	{
		unsigned char triCount;
		unsigned char borderCount;
		unsigned char firstVertSegment;
		int firstTri;
		CollisionBorder* borders;
	};

	AssertSize(CollisionPartition, 0x10);
	AssertOffset(CollisionPartition, triCount, 0x0);
	AssertOffset(CollisionPartition, borderCount, 0x1);
	AssertOffset(CollisionPartition, firstVertSegment, 0x2);
	AssertOffset(CollisionPartition, firstTri, 0x4);
	AssertOffset(CollisionPartition, borders, 0x8);

	union CollisionAabbTreeIndex
	{
		int firstChildIndex;
		int partitionIndex;
	};

	AssertSize(CollisionAabbTreeIndex, 0x4);
	AssertOffset(CollisionAabbTreeIndex, firstChildIndex, 0x0);
	AssertOffset(CollisionAabbTreeIndex, partitionIndex, 0x0);

	struct CollisionAabbTree
	{
		float midPoint[3];
		unsigned __int16 materialIndex;
		unsigned __int16 childCount;
		float halfSize[3];
		CollisionAabbTreeIndex u;
	};

	AssertSize(CollisionAabbTree, 0x20);
	AssertOffset(CollisionAabbTree, midPoint, 0x0);
	AssertOffset(CollisionAabbTree, materialIndex, 0xC);
	AssertOffset(CollisionAabbTree, childCount, 0xE);
	AssertOffset(CollisionAabbTree, halfSize, 0x10);
	AssertOffset(CollisionAabbTree, u, 0x1C);

	struct cmodel_t
	{
		Bounds bounds;
		float radius;
		cLeaf_t leaf;
	};

	AssertSize(cmodel_t, 0x44);
	AssertOffset(cmodel_t, bounds, 0x0);
	AssertOffset(cmodel_t, radius, 0x18);
	AssertOffset(cmodel_t, leaf, 0x1C);

	struct MapTriggers
	{
		unsigned int count;
		TriggerModel* models;
		unsigned int hullCount;
		TriggerHull* hulls;
		unsigned int slabCount;
		TriggerSlab* slabs;
	};

	AssertSize(MapTriggers, 0x30);
	AssertOffset(MapTriggers, count, 0x0);
	AssertOffset(MapTriggers, models, 0x8);
	AssertOffset(MapTriggers, hullCount, 0x10);
	AssertOffset(MapTriggers, hulls, 0x18);
	AssertOffset(MapTriggers, slabCount, 0x20);
	AssertOffset(MapTriggers, slabs, 0x28);

	struct MapEnts
	{
		const char* name;
		char* entityString;
		int numEntityChars;
		MapTriggers trigger;
		Stage* stages;
		char stageCount;
	};

	AssertSize(MapEnts, 0x58);
	AssertOffset(MapEnts, name, 0x0);
	AssertOffset(MapEnts, entityString, 0x8);
	AssertOffset(MapEnts, numEntityChars, 0x10);
	AssertOffset(MapEnts, trigger, 0x18);
	AssertOffset(MapEnts, stages, 0x48);
	AssertOffset(MapEnts, stageCount, 0x50);

	struct TriggerModel
	{
		int contents;
		unsigned __int16 hullCount;
		unsigned __int16 firstHull;
	};

	AssertSize(TriggerModel, 0x8);
	AssertOffset(TriggerModel, contents, 0x0);
	AssertOffset(TriggerModel, hullCount, 0x4);
	AssertOffset(TriggerModel, firstHull, 0x6);

	struct TriggerHull
	{
		Bounds bounds;
		int contents;
		unsigned __int16 slabCount;
		unsigned __int16 firstSlab;
	};

	AssertSize(TriggerHull, 0x20);
	AssertOffset(TriggerHull, bounds, 0x0);
	AssertOffset(TriggerHull, contents, 0x18);
	AssertOffset(TriggerHull, slabCount, 0x1C);
	AssertOffset(TriggerHull, firstSlab, 0x1E);

	struct TriggerSlab
	{
		float dir[3];
		float midPoint;
		float halfSize;
	};

	AssertSize(TriggerSlab, 0x14);
	AssertOffset(TriggerSlab, dir, 0x0);
	AssertOffset(TriggerSlab, midPoint, 0xC);
	AssertOffset(TriggerSlab, halfSize, 0x10);

	struct Stage
	{
		const char* name;
		float origin[3];
		unsigned __int16 triggerIndex;
		char sunPrimaryLightIndex;
	};

	AssertSize(Stage, 0x18);
	AssertOffset(Stage, name, 0x0);
	AssertOffset(Stage, origin, 0x8);
	AssertOffset(Stage, triggerIndex, 0x14);
	AssertOffset(Stage, sunPrimaryLightIndex, 0x16);

	struct SModelAabbNode
	{
		Bounds bounds;
		unsigned __int16 firstChild;
		unsigned __int16 childCount;
	};

	AssertSize(SModelAabbNode, 0x1C);
	AssertOffset(SModelAabbNode, bounds, 0x0);
	AssertOffset(SModelAabbNode, firstChild, 0x18);
	AssertOffset(SModelAabbNode, childCount, 0x1A);

	struct GfxPlacement
	{
		float quat[4];
		float origin[3];
	};

	AssertSize(GfxPlacement, 0x1C);
	AssertOffset(GfxPlacement, quat, 0x0);
	AssertOffset(GfxPlacement, origin, 0x10);

	struct DynEntityDef
	{
		DynEntityType type;
		GfxPlacement pose;
		XModel* xModel;
		unsigned __int16 brushModel;
		unsigned __int16 physicsBrushModel;
		FxEffectDef* destroyFx;
		PhysPreset* physPreset;
		int health;
		PhysMass mass;
		int contents;
	};

	AssertSize(DynEntityDef, 0x70);
	AssertOffset(DynEntityDef, type, 0x0);
	AssertOffset(DynEntityDef, pose, 0x4);
	AssertOffset(DynEntityDef, xModel, 0x20);
	AssertOffset(DynEntityDef, brushModel, 0x28);
	AssertOffset(DynEntityDef, physicsBrushModel, 0x2A);
	AssertOffset(DynEntityDef, destroyFx, 0x30);
	AssertOffset(DynEntityDef, physPreset, 0x38);
	AssertOffset(DynEntityDef, health, 0x40);
	AssertOffset(DynEntityDef, mass, 0x44);
	AssertOffset(DynEntityDef, contents, 0x68);

	enum FxElemType
	{
		FX_ELEM_TYPE_SPRITE_BILLBOARD = 0x0,
		FX_ELEM_TYPE_SPRITE_ORIENTED = 0x1,
		FX_ELEM_TYPE_TAIL = 0x2,
		FX_ELEM_TYPE_TRAIL = 0x3,
		FX_ELEM_TYPE_CLOUD = 0x4,
		FX_ELEM_TYPE_SPARK_CLOUD = 0x5,
		FX_ELEM_TYPE_SPARK_FOUNTAIN = 0x6,
		FX_ELEM_TYPE_MODEL = 0x7,
		FX_ELEM_TYPE_OMNI_LIGHT = 0x8,
		FX_ELEM_TYPE_SPOT_LIGHT = 0x9,
		FX_ELEM_TYPE_SOUND = 0xA,
		FX_ELEM_TYPE_DECAL = 0xB,
		FX_ELEM_TYPE_RUNNER = 0xC,
		FX_ELEM_TYPE_COUNT = 0xD,
		FX_ELEM_TYPE_LAST_SPRITE = 0x3,
		FX_ELEM_TYPE_LAST_DRAWN = 0x9,
	};

	struct FxEffectDef
	{
		const char* name;
		int flags;
		int totalSize;
		int msecLoopingLife;
		int elemDefCountLooping;
		int elemDefCountOneShot;
		int elemDefCountEmission;
		FxElemDef* elemDefs;
	};

	AssertSize(FxEffectDef, 0x28);
	AssertOffset(FxEffectDef, name, 0x0);
	AssertOffset(FxEffectDef, flags, 0x8);
	AssertOffset(FxEffectDef, totalSize, 0xC);
	AssertOffset(FxEffectDef, msecLoopingLife, 0x10);
	AssertOffset(FxEffectDef, elemDefCountLooping, 0x14);
	AssertOffset(FxEffectDef, elemDefCountOneShot, 0x18);
	AssertOffset(FxEffectDef, elemDefCountEmission, 0x1C);
	AssertOffset(FxEffectDef, elemDefs, 0x20);

	struct FxSpawnDefLooping
	{
		int intervalMsec;
		int count;
	};

	AssertSize(FxSpawnDefLooping, 0x8);
	AssertOffset(FxSpawnDefLooping, intervalMsec, 0x0);
	AssertOffset(FxSpawnDefLooping, count, 0x4);

	struct FxIntRange
	{
		int base;
		int amplitude;
	};

	AssertSize(FxIntRange, 0x8);
	AssertOffset(FxIntRange, base, 0x0);
	AssertOffset(FxIntRange, amplitude, 0x4);

	struct FxSpawnDefOneShot
	{
		FxIntRange count;
	};

	AssertSize(FxSpawnDefOneShot, 0x8);
	AssertOffset(FxSpawnDefOneShot, count, 0x0);

	union FxSpawnDef
	{
		FxSpawnDefLooping looping;
		FxSpawnDefOneShot oneShot;
	};

	AssertSize(FxSpawnDef, 0x8);
	AssertOffset(FxSpawnDef, looping, 0x0);
	AssertOffset(FxSpawnDef, oneShot, 0x0);

	struct FxFloatRange
	{
		float base;
		float amplitude;
	};

	AssertSize(FxFloatRange, 0x8);
	AssertOffset(FxFloatRange, base, 0x0);
	AssertOffset(FxFloatRange, amplitude, 0x4);

	struct FxElemAtlas
	{
		char behavior;
		char index;
		char fps;
		char loopCount;
		char colIndexBits;
		char rowIndexBits;
		__int16 entryCount;
	};

	AssertSize(FxElemAtlas, 0x8);
	AssertOffset(FxElemAtlas, behavior, 0x0);
	AssertOffset(FxElemAtlas, index, 0x1);
	AssertOffset(FxElemAtlas, fps, 0x2);
	AssertOffset(FxElemAtlas, loopCount, 0x3);
	AssertOffset(FxElemAtlas, colIndexBits, 0x4);
	AssertOffset(FxElemAtlas, rowIndexBits, 0x5);
	AssertOffset(FxElemAtlas, entryCount, 0x6);

	union FxEffectDefRef
	{
		FxEffectDef* handle;
		const char* name;
	};

	AssertSize(FxEffectDefRef, 0x8);
	AssertOffset(FxEffectDefRef, handle, 0x0);
	AssertOffset(FxEffectDefRef, name, 0x0);

	union FxElemVisuals
	{
		const void* anonymous;
		Material* material;
		XModel* model;
		FxEffectDefRef effectDef;
		const char* soundName;
	};

	AssertSize(FxElemVisuals, 0x8);
	AssertOffset(FxElemVisuals, anonymous, 0x0);
	AssertOffset(FxElemVisuals, material, 0x0);
	AssertOffset(FxElemVisuals, model, 0x0);
	AssertOffset(FxElemVisuals, effectDef, 0x0);
	AssertOffset(FxElemVisuals, soundName, 0x0);

	union FxElemDefVisuals
	{
		FxElemMarkVisuals* markArray;
		FxElemVisuals* array;
		FxElemVisuals instance;
	};

	AssertSize(FxElemDefVisuals, 0x8);
	AssertOffset(FxElemDefVisuals, markArray, 0x0);
	AssertOffset(FxElemDefVisuals, array, 0x0);
	AssertOffset(FxElemDefVisuals, instance, 0x0);

	union FxElemExtendedDefPtr
	{
		FxTrailDef* trailDef;
		FxSparkFountainDef* sparkFountainDef;
		void* unknownDef;
	};

	AssertSize(FxElemExtendedDefPtr, 0x8);
	AssertOffset(FxElemExtendedDefPtr, trailDef, 0x0);
	AssertOffset(FxElemExtendedDefPtr, sparkFountainDef, 0x0);
	AssertOffset(FxElemExtendedDefPtr, unknownDef, 0x0);

	struct FxElemDef
	{
		int flags;
		FxSpawnDef spawn;
		FxFloatRange spawnRange;
		FxFloatRange fadeInRange;
		FxFloatRange fadeOutRange;
		float spawnFrustumCullRadius;
		FxIntRange spawnDelayMsec;
		FxIntRange lifeSpanMsec;
		FxFloatRange spawnOrigin[3];
		FxFloatRange spawnOffsetRadius;
		FxFloatRange spawnOffsetHeight;
		FxFloatRange spawnAngles[3];
		FxFloatRange angularVelocity[3];
		FxFloatRange initialRotation;
		FxFloatRange gravity;
		FxFloatRange reflectionFactor;
		FxElemAtlas atlas;
		char elemType;
		char visualCount;
		char velIntervalCount;
		char visStateIntervalCount;
		FxElemVelStateSample* velSamples;
		FxElemVisStateSample* visSamples;
		FxElemDefVisuals visuals;
		Bounds collBounds;
		FxEffectDefRef effectOnImpact;
		FxEffectDefRef effectOnDeath;
		FxEffectDefRef effectEmitted;
		FxFloatRange emitDist;
		FxFloatRange emitDistVariance;
		FxElemExtendedDefPtr extended;
		char sortOrder;
		char lightingFrac;
		char useItemClip;
		char fadeInfo;
	};

	AssertSize(FxElemDef, 0x120);
	AssertOffset(FxElemDef, flags, 0x0);
	AssertOffset(FxElemDef, spawn, 0x4);
	AssertOffset(FxElemDef, spawnRange, 0xC);
	AssertOffset(FxElemDef, fadeInRange, 0x14);
	AssertOffset(FxElemDef, fadeOutRange, 0x1C);
	AssertOffset(FxElemDef, spawnFrustumCullRadius, 0x24);
	AssertOffset(FxElemDef, spawnDelayMsec, 0x28);
	AssertOffset(FxElemDef, lifeSpanMsec, 0x30);
	AssertOffset(FxElemDef, spawnOrigin, 0x38);
	AssertOffset(FxElemDef, spawnOffsetRadius, 0x50);
	AssertOffset(FxElemDef, spawnOffsetHeight, 0x58);
	AssertOffset(FxElemDef, spawnAngles, 0x60);
	AssertOffset(FxElemDef, angularVelocity, 0x78);
	AssertOffset(FxElemDef, initialRotation, 0x90);
	AssertOffset(FxElemDef, gravity, 0x98);
	AssertOffset(FxElemDef, reflectionFactor, 0xA0);
	AssertOffset(FxElemDef, atlas, 0xA8);
	AssertOffset(FxElemDef, elemType, 0xB0);
	AssertOffset(FxElemDef, visualCount, 0xB1);
	AssertOffset(FxElemDef, velIntervalCount, 0xB2);
	AssertOffset(FxElemDef, visStateIntervalCount, 0xB3);
	AssertOffset(FxElemDef, velSamples, 0xB8);
	AssertOffset(FxElemDef, visSamples, 0xC0);
	AssertOffset(FxElemDef, visuals, 0xC8);
	AssertOffset(FxElemDef, collBounds, 0xD0);
	AssertOffset(FxElemDef, effectOnImpact, 0xE8);
	AssertOffset(FxElemDef, effectOnDeath, 0xF0);
	AssertOffset(FxElemDef, effectEmitted, 0xF8);
	AssertOffset(FxElemDef, emitDist, 0x100);
	AssertOffset(FxElemDef, emitDistVariance, 0x108);
	AssertOffset(FxElemDef, extended, 0x110);
	AssertOffset(FxElemDef, sortOrder, 0x118);
	AssertOffset(FxElemDef, lightingFrac, 0x119);
	AssertOffset(FxElemDef, useItemClip, 0x11A);
	AssertOffset(FxElemDef, fadeInfo, 0x11B);

	struct FxElemVec3Range
	{
		float base[3];
		float amplitude[3];
	};

	AssertSize(FxElemVec3Range, 0x18);
	AssertOffset(FxElemVec3Range, base, 0x0);
	AssertOffset(FxElemVec3Range, amplitude, 0xC);

	struct FxElemVelStateInFrame
	{
		FxElemVec3Range velocity;
		FxElemVec3Range totalDelta;
	};

	AssertSize(FxElemVelStateInFrame, 0x30);
	AssertOffset(FxElemVelStateInFrame, velocity, 0x0);
	AssertOffset(FxElemVelStateInFrame, totalDelta, 0x18);

	struct FxElemVelStateSample
	{
		FxElemVelStateInFrame local;
		FxElemVelStateInFrame world;
	};

	AssertSize(FxElemVelStateSample, 0x60);
	AssertOffset(FxElemVelStateSample, local, 0x0);
	AssertOffset(FxElemVelStateSample, world, 0x30);

	struct FxElemVisualState
	{
		unsigned char color[4];
		float rotationDelta;
		float rotationTotal;
		float size[2];
		float scale;
	};

	AssertSize(FxElemVisualState, 0x18);
	AssertOffset(FxElemVisualState, color, 0x0);
	AssertOffset(FxElemVisualState, rotationDelta, 0x4);
	AssertOffset(FxElemVisualState, rotationTotal, 0x8);
	AssertOffset(FxElemVisualState, size, 0xC);
	AssertOffset(FxElemVisualState, scale, 0x14);

	struct FxElemVisStateSample
	{
		FxElemVisualState base;
		FxElemVisualState amplitude;
	};

	AssertSize(FxElemVisStateSample, 0x30);
	AssertOffset(FxElemVisStateSample, base, 0x0);
	AssertOffset(FxElemVisStateSample, amplitude, 0x18);

	struct FxElemMarkVisuals
	{
		Material* materials[2];
	};

	AssertSize(FxElemMarkVisuals, 0x10);
	AssertOffset(FxElemMarkVisuals, materials, 0x0);

	struct FxTrailDef
	{
		int scrollTimeMsec;
		int repeatDist;
		float invSplitDist;
		float invSplitArcDist;
		float invSplitTime;
		int vertCount;
		FxTrailVertex* verts;
		int indCount;
		unsigned __int16* inds;
	};

	AssertSize(FxTrailDef, 0x30);
	AssertOffset(FxTrailDef, scrollTimeMsec, 0x0);
	AssertOffset(FxTrailDef, repeatDist, 0x4);
	AssertOffset(FxTrailDef, invSplitDist, 0x8);
	AssertOffset(FxTrailDef, invSplitArcDist, 0xC);
	AssertOffset(FxTrailDef, invSplitTime, 0x10);
	AssertOffset(FxTrailDef, vertCount, 0x14);
	AssertOffset(FxTrailDef, verts, 0x18);
	AssertOffset(FxTrailDef, indCount, 0x20);
	AssertOffset(FxTrailDef, inds, 0x28);

	struct FxTrailVertex
	{
		float pos[2];
		float normal[2];
		float texCoord;
	};

	AssertSize(FxTrailVertex, 0x14);
	AssertOffset(FxTrailVertex, pos, 0x0);
	AssertOffset(FxTrailVertex, normal, 0x8);
	AssertOffset(FxTrailVertex, texCoord, 0x10);

	struct FxSparkFountainDef
	{
		float gravity;
		float bounceFrac;
		float bounceRand;
		float sparkSpacing;
		float sparkLength;
		int sparkCount;
		float loopTime;
		float velMin;
		float velMax;
		float velConeFrac;
		float restSpeed;
		float boostTime;
		float boostFactor;
	};

	AssertSize(FxSparkFountainDef, 0x34);
	AssertOffset(FxSparkFountainDef, gravity, 0x0);
	AssertOffset(FxSparkFountainDef, bounceFrac, 0x4);
	AssertOffset(FxSparkFountainDef, bounceRand, 0x8);
	AssertOffset(FxSparkFountainDef, sparkSpacing, 0xC);
	AssertOffset(FxSparkFountainDef, sparkLength, 0x10);
	AssertOffset(FxSparkFountainDef, sparkCount, 0x14);
	AssertOffset(FxSparkFountainDef, loopTime, 0x18);
	AssertOffset(FxSparkFountainDef, velMin, 0x1C);
	AssertOffset(FxSparkFountainDef, velMax, 0x20);
	AssertOffset(FxSparkFountainDef, velConeFrac, 0x24);
	AssertOffset(FxSparkFountainDef, restSpeed, 0x28);
	AssertOffset(FxSparkFountainDef, boostTime, 0x2C);
	AssertOffset(FxSparkFountainDef, boostFactor, 0x30);

	struct DynEntityPose
	{
		GfxPlacement pose;
		float radius;
	};

	AssertSize(DynEntityPose, 0x20);
	AssertOffset(DynEntityPose, pose, 0x0);
	AssertOffset(DynEntityPose, radius, 0x1C);

	struct DynEntityClient
	{
		int physObjId;
		unsigned __int16 flags;
		unsigned __int16 lightingHandle;
		int health;
	};

	AssertSize(DynEntityClient, 0xC);
	AssertOffset(DynEntityClient, physObjId, 0x0);
	AssertOffset(DynEntityClient, flags, 0x4);
	AssertOffset(DynEntityClient, lightingHandle, 0x6);
	AssertOffset(DynEntityClient, health, 0x8);

	struct DynEntityColl
	{
		unsigned __int16 sector;
		unsigned __int16 nextEntInSector;
		float linkMins[2];
		float linkMaxs[2];
	};

	AssertSize(DynEntityColl, 0x14);
	AssertOffset(DynEntityColl, sector, 0x0);
	AssertOffset(DynEntityColl, nextEntInSector, 0x2);
	AssertOffset(DynEntityColl, linkMins, 0x4);
	AssertOffset(DynEntityColl, linkMaxs, 0xC);

	struct ComWorld
	{
		const char* name;
		int isInUse;
		unsigned int primaryLightCount;
		ComPrimaryLight* primaryLights;
	};

	AssertSize(ComWorld, 0x18);
	AssertOffset(ComWorld, name, 0x0);
	AssertOffset(ComWorld, isInUse, 0x8);
	AssertOffset(ComWorld, primaryLightCount, 0xC);
	AssertOffset(ComWorld, primaryLights, 0x10);

	struct ComPrimaryLight
	{
		char type;
		char canUseShadowMap;
		char exponent;
		char unused;
		float color[3];
		float dir[3];
		float origin[3];
		float radius;
		float cosHalfFovOuter;
		float cosHalfFovInner;
		float cosHalfFovExpanded;
		float rotationLimit;
		float translationLimit;
		const char* defName;
	};

	AssertSize(ComPrimaryLight, 0x48);
	AssertOffset(ComPrimaryLight, type, 0x0);
	AssertOffset(ComPrimaryLight, canUseShadowMap, 0x1);
	AssertOffset(ComPrimaryLight, exponent, 0x2);
	AssertOffset(ComPrimaryLight, unused, 0x3);
	AssertOffset(ComPrimaryLight, color, 0x4);
	AssertOffset(ComPrimaryLight, dir, 0x10);
	AssertOffset(ComPrimaryLight, origin, 0x1C);
	AssertOffset(ComPrimaryLight, radius, 0x28);
	AssertOffset(ComPrimaryLight, cosHalfFovOuter, 0x2C);
	AssertOffset(ComPrimaryLight, cosHalfFovInner, 0x30);
	AssertOffset(ComPrimaryLight, cosHalfFovExpanded, 0x34);
	AssertOffset(ComPrimaryLight, rotationLimit, 0x38);
	AssertOffset(ComPrimaryLight, translationLimit, 0x3C);
	AssertOffset(ComPrimaryLight, defName, 0x40);

	struct PathData
	{
		unsigned int nodeCount;
		pathnode_t* nodes;
		pathbasenode_t* basenodes;
		unsigned int chainNodeCount;
		unsigned __int16* chainNodeForNode;
		unsigned __int16* nodeForChainNode;
		int visBytes;
		char* pathVis;
		int nodeTreeCount;
		pathnode_tree_t* nodeTree;
	};

	AssertSize(PathData, 0x50);
	AssertOffset(PathData, nodeCount, 0x0);
	AssertOffset(PathData, nodes, 0x8);
	AssertOffset(PathData, basenodes, 0x10);
	AssertOffset(PathData, chainNodeCount, 0x18);
	AssertOffset(PathData, chainNodeForNode, 0x20);
	AssertOffset(PathData, nodeForChainNode, 0x28);
	AssertOffset(PathData, visBytes, 0x30);
	AssertOffset(PathData, pathVis, 0x38);
	AssertOffset(PathData, nodeTreeCount, 0x40);
	AssertOffset(PathData, nodeTree, 0x48);

	struct VehicleTrack
	{
		VehicleTrackSegment* segments;
		unsigned int segmentCount;
	};

	AssertSize(VehicleTrack, 0x10);
	AssertOffset(VehicleTrack, segments, 0x0);
	AssertOffset(VehicleTrack, segmentCount, 0x8);

	struct GameWorldSp
	{
		const char* name;
		PathData path;
		VehicleTrack vehicleTrack;
		G_GlassData* g_glassData;
	};

	AssertSize(GameWorldSp, 0x70);
	AssertOffset(GameWorldSp, name, 0x0);
	AssertOffset(GameWorldSp, path, 0x8);
	AssertOffset(GameWorldSp, vehicleTrack, 0x58);
	AssertOffset(GameWorldSp, g_glassData, 0x68);

	union $23305223CFD097B6F79557BDD2047E6C
	{
		float minUseDistSq;
		PathNodeErrorCode error;
	};

	AssertSize($23305223CFD097B6F79557BDD2047E6C, 0x4);
	AssertOffset($23305223CFD097B6F79557BDD2047E6C, minUseDistSq, 0x0);
	AssertOffset($23305223CFD097B6F79557BDD2047E6C, error, 0x0);

	struct pathnode_constant_t
	{
		nodeType type;
		unsigned __int16 spawnflags;
		unsigned __int16 targetname;
		unsigned __int16 script_linkName;
		unsigned __int16 script_noteworthy;
		unsigned __int16 target;
		unsigned __int16 animscript;
		int animscriptfunc;
		float vOrigin[3];
		float fAngle;
		float forward[2];
		float fRadius;
		$23305223CFD097B6F79557BDD2047E6C ___u12;
		__int16 wOverlapNode[2];
		unsigned __int16 totalLinkCount;
		pathlink_s* Links;
	};

	AssertSize(pathnode_constant_t, 0x48);
	AssertOffset(pathnode_constant_t, type, 0x0);
	AssertOffset(pathnode_constant_t, spawnflags, 0x4);
	AssertOffset(pathnode_constant_t, targetname, 0x6);
	AssertOffset(pathnode_constant_t, script_linkName, 0x8);
	AssertOffset(pathnode_constant_t, script_noteworthy, 0xA);
	AssertOffset(pathnode_constant_t, target, 0xC);
	AssertOffset(pathnode_constant_t, animscript, 0xE);
	AssertOffset(pathnode_constant_t, animscriptfunc, 0x10);
	AssertOffset(pathnode_constant_t, vOrigin, 0x14);
	AssertOffset(pathnode_constant_t, fAngle, 0x20);
	AssertOffset(pathnode_constant_t, forward, 0x24);
	AssertOffset(pathnode_constant_t, fRadius, 0x2C);
	AssertOffset(pathnode_constant_t, ___u12, 0x30);
	AssertOffset(pathnode_constant_t, wOverlapNode, 0x34);
	AssertOffset(pathnode_constant_t, totalLinkCount, 0x38);
	AssertOffset(pathnode_constant_t, Links, 0x40);

	struct pathnode_dynamic_t
	{
		void* pOwner;
		int iFreeTime;
		int iValidTime[3];
		int dangerousNodeTime[3];
		int inPlayerLOSTime;
		__int16 wLinkCount;
		__int16 wOverlapCount;
		__int16 turretEntNumber;
		char userCount;
		bool hasBadPlaceLink;
	};

	AssertSize(pathnode_dynamic_t, 0x30);
	AssertOffset(pathnode_dynamic_t, pOwner, 0x0);
	AssertOffset(pathnode_dynamic_t, iFreeTime, 0x8);
	AssertOffset(pathnode_dynamic_t, iValidTime, 0xC);
	AssertOffset(pathnode_dynamic_t, dangerousNodeTime, 0x18);
	AssertOffset(pathnode_dynamic_t, inPlayerLOSTime, 0x24);
	AssertOffset(pathnode_dynamic_t, wLinkCount, 0x28);
	AssertOffset(pathnode_dynamic_t, wOverlapCount, 0x2A);
	AssertOffset(pathnode_dynamic_t, turretEntNumber, 0x2C);
	AssertOffset(pathnode_dynamic_t, userCount, 0x2E);
	AssertOffset(pathnode_dynamic_t, hasBadPlaceLink, 0x2F);

	union $73F238679C0419BE2C31C6559E8604FC
	{
		float nodeCost;
		int linkIndex;
	};

	AssertSize($73F238679C0419BE2C31C6559E8604FC, 0x4);
	AssertOffset($73F238679C0419BE2C31C6559E8604FC, nodeCost, 0x0);
	AssertOffset($73F238679C0419BE2C31C6559E8604FC, linkIndex, 0x0);

	struct pathnode_transient_t
	{
		int iSearchFrame;
		pathnode_t* pNextOpen;
		pathnode_t* pPrevOpen;
		pathnode_t* pParent;
		float fCost;
		float fHeuristic;
		$73F238679C0419BE2C31C6559E8604FC ___u6;
	};

	AssertSize(pathnode_transient_t, 0x30);
	AssertOffset(pathnode_transient_t, iSearchFrame, 0x0);
	AssertOffset(pathnode_transient_t, pNextOpen, 0x8);
	AssertOffset(pathnode_transient_t, pPrevOpen, 0x10);
	AssertOffset(pathnode_transient_t, pParent, 0x18);
	AssertOffset(pathnode_transient_t, fCost, 0x20);
	AssertOffset(pathnode_transient_t, fHeuristic, 0x24);
	AssertOffset(pathnode_transient_t, ___u6, 0x28);

	struct pathnode_t
	{
		pathnode_constant_t constant;
		pathnode_dynamic_t dynamic;
		pathnode_transient_t transient;
	};

	AssertSize(pathnode_t, 0xA8);
	AssertOffset(pathnode_t, constant, 0x0);
	AssertOffset(pathnode_t, dynamic, 0x48);
	AssertOffset(pathnode_t, transient, 0x78);

	struct pathlink_s
	{
		float fDist;
		unsigned __int16 nodeNum;
		char disconnectCount;
		char negotiationLink;
		char flags;
		char ubBadPlaceCount[3];
	};

	AssertSize(pathlink_s, 0xC);
	AssertOffset(pathlink_s, fDist, 0x0);
	AssertOffset(pathlink_s, nodeNum, 0x4);
	AssertOffset(pathlink_s, disconnectCount, 0x6);
	AssertOffset(pathlink_s, negotiationLink, 0x7);
	AssertOffset(pathlink_s, flags, 0x8);
	AssertOffset(pathlink_s, ubBadPlaceCount, 0x9);

	struct pathbasenode_t
	{
		float vOrigin[3];
		unsigned int type;
	};

	AssertSize(pathbasenode_t, 0x10);
	AssertOffset(pathbasenode_t, vOrigin, 0x0);
	AssertOffset(pathbasenode_t, type, 0xC);

	struct pathnode_tree_nodes_t
	{
		int nodeCount;
		unsigned __int16* nodes;
	};

	AssertSize(pathnode_tree_nodes_t, 0x10);
	AssertOffset(pathnode_tree_nodes_t, nodeCount, 0x0);
	AssertOffset(pathnode_tree_nodes_t, nodes, 0x8);

	union pathnode_tree_info_t
	{
		pathnode_tree_t* child[2];
		pathnode_tree_nodes_t s;
	};

	AssertSize(pathnode_tree_info_t, 0x10);
	AssertOffset(pathnode_tree_info_t, child, 0x0);
	AssertOffset(pathnode_tree_info_t, s, 0x0);

	struct pathnode_tree_t
	{
		int axis;
		float dist;
		pathnode_tree_info_t u;
	};

	AssertSize(pathnode_tree_t, 0x18);
	AssertOffset(pathnode_tree_t, axis, 0x0);
	AssertOffset(pathnode_tree_t, dist, 0x4);
	AssertOffset(pathnode_tree_t, u, 0x8);

	struct VehicleTrackSegment
	{
		const char* targetName;
		VehicleTrackSector* sectors;
		unsigned int sectorCount;
		VehicleTrackSegment** nextBranches;
		unsigned int nextBranchesCount;
		VehicleTrackSegment** prevBranches;
		unsigned int prevBranchesCount;
		float endEdgeDir[2];
		float endEdgeDist;
		float totalLength;
	};

	AssertSize(VehicleTrackSegment, 0x48);
	AssertOffset(VehicleTrackSegment, targetName, 0x0);
	AssertOffset(VehicleTrackSegment, sectors, 0x8);
	AssertOffset(VehicleTrackSegment, sectorCount, 0x10);
	AssertOffset(VehicleTrackSegment, nextBranches, 0x18);
	AssertOffset(VehicleTrackSegment, nextBranchesCount, 0x20);
	AssertOffset(VehicleTrackSegment, prevBranches, 0x28);
	AssertOffset(VehicleTrackSegment, prevBranchesCount, 0x30);
	AssertOffset(VehicleTrackSegment, endEdgeDir, 0x34);
	AssertOffset(VehicleTrackSegment, endEdgeDist, 0x3C);
	AssertOffset(VehicleTrackSegment, totalLength, 0x40);

	struct VehicleTrackSector
	{
		float startEdgeDir[2];
		float startEdgeDist;
		float leftEdgeDir[2];
		float leftEdgeDist;
		float rightEdgeDir[2];
		float rightEdgeDist;
		float sectorLength;
		float sectorWidth;
		float totalPriorLength;
		float totalFollowingLength;
		VehicleTrackObstacle* obstacles;
		unsigned int obstacleCount;
	};

	AssertSize(VehicleTrackSector, 0x48);
	AssertOffset(VehicleTrackSector, startEdgeDir, 0x0);
	AssertOffset(VehicleTrackSector, startEdgeDist, 0x8);
	AssertOffset(VehicleTrackSector, leftEdgeDir, 0xC);
	AssertOffset(VehicleTrackSector, leftEdgeDist, 0x14);
	AssertOffset(VehicleTrackSector, rightEdgeDir, 0x18);
	AssertOffset(VehicleTrackSector, rightEdgeDist, 0x20);
	AssertOffset(VehicleTrackSector, sectorLength, 0x24);
	AssertOffset(VehicleTrackSector, sectorWidth, 0x28);
	AssertOffset(VehicleTrackSector, totalPriorLength, 0x2C);
	AssertOffset(VehicleTrackSector, totalFollowingLength, 0x30);
	AssertOffset(VehicleTrackSector, obstacles, 0x38);
	AssertOffset(VehicleTrackSector, obstacleCount, 0x40);

	struct VehicleTrackObstacle
	{
		float origin[2];
		float radius;
	};

	AssertSize(VehicleTrackObstacle, 0xC);
	AssertOffset(VehicleTrackObstacle, origin, 0x0);
	AssertOffset(VehicleTrackObstacle, radius, 0x8);

	struct G_GlassData
	{
		G_GlassPiece* glassPieces;
		unsigned int pieceCount;
		unsigned __int16 damageToWeaken;
		unsigned __int16 damageToDestroy;
		unsigned int glassNameCount;
		G_GlassName* glassNames;
		char pad[108];
	};

	AssertSize(G_GlassData, 0x90);
	AssertOffset(G_GlassData, glassPieces, 0x0);
	AssertOffset(G_GlassData, pieceCount, 0x8);
	AssertOffset(G_GlassData, damageToWeaken, 0xC);
	AssertOffset(G_GlassData, damageToDestroy, 0xE);
	AssertOffset(G_GlassData, glassNameCount, 0x10);
	AssertOffset(G_GlassData, glassNames, 0x18);
	AssertOffset(G_GlassData, pad, 0x20);

	struct G_GlassPiece
	{
		unsigned __int16 damageTaken;
		unsigned __int16 collapseTime;
		int lastStateChangeTime;
		char impactDir;
		char impactPos[2];
	};

	AssertSize(G_GlassPiece, 0xC);
	AssertOffset(G_GlassPiece, damageTaken, 0x0);
	AssertOffset(G_GlassPiece, collapseTime, 0x2);
	AssertOffset(G_GlassPiece, lastStateChangeTime, 0x4);
	AssertOffset(G_GlassPiece, impactDir, 0x8);
	AssertOffset(G_GlassPiece, impactPos, 0x9);

	struct G_GlassName
	{
		char* nameStr;
		unsigned __int16 name;
		unsigned __int16 pieceCount;
		unsigned __int16* pieceIndices;
	};

	AssertSize(G_GlassName, 0x18);
	AssertOffset(G_GlassName, nameStr, 0x0);
	AssertOffset(G_GlassName, name, 0x8);
	AssertOffset(G_GlassName, pieceCount, 0xA);
	AssertOffset(G_GlassName, pieceIndices, 0x10);

	struct GameWorldMp
	{
		const char* name;
		G_GlassData* g_glassData;
	};

	AssertSize(GameWorldMp, 0x10);
	AssertOffset(GameWorldMp, name, 0x0);
	AssertOffset(GameWorldMp, g_glassData, 0x8);

	struct FxGlassSystem
	{
		int time;
		int prevTime;
		unsigned int defCount;
		unsigned int pieceLimit;
		unsigned int pieceWordCount;
		unsigned int initPieceCount;
		unsigned int cellCount;
		unsigned int activePieceCount;
		unsigned int firstFreePiece;
		unsigned int geoDataLimit;
		unsigned int geoDataCount;
		unsigned int initGeoDataCount;
		FxGlassDef* defs;
		FxGlassPiecePlace* piecePlaces;
		FxGlassPieceState* pieceStates;
		FxGlassPieceDynamics* pieceDynamics;
		FxGlassGeometryData* geoData;
		unsigned int* isInUse;
		unsigned int* cellBits;
		char* visData;
		float(*linkOrg)[3];
		float* halfThickness;
		unsigned __int16* lightingHandles;
		FxGlassInitPieceState* initPieceStates;
		FxGlassGeometryData* initGeoData;
		bool needToCompactData;
		char initCount;
		float effectChanceAccum;
		int lastPieceDeletionTime;
	};

	AssertSize(FxGlassSystem, 0xA8);
	AssertOffset(FxGlassSystem, time, 0x0);
	AssertOffset(FxGlassSystem, prevTime, 0x4);
	AssertOffset(FxGlassSystem, defCount, 0x8);
	AssertOffset(FxGlassSystem, pieceLimit, 0xC);
	AssertOffset(FxGlassSystem, pieceWordCount, 0x10);
	AssertOffset(FxGlassSystem, initPieceCount, 0x14);
	AssertOffset(FxGlassSystem, cellCount, 0x18);
	AssertOffset(FxGlassSystem, activePieceCount, 0x1C);
	AssertOffset(FxGlassSystem, firstFreePiece, 0x20);
	AssertOffset(FxGlassSystem, geoDataLimit, 0x24);
	AssertOffset(FxGlassSystem, geoDataCount, 0x28);
	AssertOffset(FxGlassSystem, initGeoDataCount, 0x2C);
	AssertOffset(FxGlassSystem, defs, 0x30);
	AssertOffset(FxGlassSystem, piecePlaces, 0x38);
	AssertOffset(FxGlassSystem, pieceStates, 0x40);
	AssertOffset(FxGlassSystem, pieceDynamics, 0x48);
	AssertOffset(FxGlassSystem, geoData, 0x50);
	AssertOffset(FxGlassSystem, isInUse, 0x58);
	AssertOffset(FxGlassSystem, cellBits, 0x60);
	AssertOffset(FxGlassSystem, visData, 0x68);
	AssertOffset(FxGlassSystem, linkOrg, 0x70);
	AssertOffset(FxGlassSystem, halfThickness, 0x78);
	AssertOffset(FxGlassSystem, lightingHandles, 0x80);
	AssertOffset(FxGlassSystem, initPieceStates, 0x88);
	AssertOffset(FxGlassSystem, initGeoData, 0x90);
	AssertOffset(FxGlassSystem, needToCompactData, 0x98);
	AssertOffset(FxGlassSystem, initCount, 0x99);
	AssertOffset(FxGlassSystem, effectChanceAccum, 0x9C);
	AssertOffset(FxGlassSystem, lastPieceDeletionTime, 0xA0);

	struct FxWorld
	{
		const char* name;
		FxGlassSystem glassSys;
	};

	AssertSize(FxWorld, 0xB0);
	AssertOffset(FxWorld, name, 0x0);
	AssertOffset(FxWorld, glassSys, 0x8);

	struct FxGlassDef
	{
		float halfThickness;
		float texVecs[2][2];
		GfxColor color;
		Material* material;
		Material* materialShattered;
		PhysPreset* physPreset;
	};

	AssertSize(FxGlassDef, 0x30);
	AssertOffset(FxGlassDef, halfThickness, 0x0);
	AssertOffset(FxGlassDef, texVecs, 0x4);
	AssertOffset(FxGlassDef, color, 0x14);
	AssertOffset(FxGlassDef, material, 0x18);
	AssertOffset(FxGlassDef, materialShattered, 0x20);
	AssertOffset(FxGlassDef, physPreset, 0x28);

	struct FxSpatialFrame
	{
		float quat[4];
		float origin[3];
	};

	AssertSize(FxSpatialFrame, 0x1C);
	AssertOffset(FxSpatialFrame, quat, 0x0);
	AssertOffset(FxSpatialFrame, origin, 0x10);

	struct $E43DBA5037697D705289B74D87E76C70
	{
		FxSpatialFrame frame;
		float radius;
	};

	AssertSize($E43DBA5037697D705289B74D87E76C70, 0x20);
	AssertOffset($E43DBA5037697D705289B74D87E76C70, frame, 0x0);
	AssertOffset($E43DBA5037697D705289B74D87E76C70, radius, 0x1C);

	union FxGlassPiecePlace
	{
		$E43DBA5037697D705289B74D87E76C70 __s0;
		unsigned int nextFree;
	};

	AssertSize(FxGlassPiecePlace, 0x20);
	AssertOffset(FxGlassPiecePlace, __s0, 0x0);
	AssertOffset(FxGlassPiecePlace, nextFree, 0x0);

	struct FxGlassPieceState
	{
		float texCoordOrigin[2];
		unsigned int supportMask;
		unsigned __int16 initIndex;
		unsigned __int16 geoDataStart;
		char defIndex;
		char pad[5];
		char vertCount;
		char holeDataCount;
		char crackDataCount;
		char fanDataCount;
		unsigned __int16 flags;
		float areaX2;
	};

	AssertSize(FxGlassPieceState, 0x20);
	AssertOffset(FxGlassPieceState, texCoordOrigin, 0x0);
	AssertOffset(FxGlassPieceState, supportMask, 0x8);
	AssertOffset(FxGlassPieceState, initIndex, 0xC);
	AssertOffset(FxGlassPieceState, geoDataStart, 0xE);
	AssertOffset(FxGlassPieceState, defIndex, 0x10);
	AssertOffset(FxGlassPieceState, pad, 0x11);
	AssertOffset(FxGlassPieceState, vertCount, 0x16);
	AssertOffset(FxGlassPieceState, holeDataCount, 0x17);
	AssertOffset(FxGlassPieceState, crackDataCount, 0x18);
	AssertOffset(FxGlassPieceState, fanDataCount, 0x19);
	AssertOffset(FxGlassPieceState, flags, 0x1A);
	AssertOffset(FxGlassPieceState, areaX2, 0x1C);

	struct FxGlassPieceDynamics
	{
		int fallTime;
		int physObjId;
		int physJointId;
		float vel[3];
		float avel[3];
	};

	AssertSize(FxGlassPieceDynamics, 0x24);
	AssertOffset(FxGlassPieceDynamics, fallTime, 0x0);
	AssertOffset(FxGlassPieceDynamics, physObjId, 0x4);
	AssertOffset(FxGlassPieceDynamics, physJointId, 0x8);
	AssertOffset(FxGlassPieceDynamics, vel, 0xC);
	AssertOffset(FxGlassPieceDynamics, avel, 0x18);

	struct FxGlassVertex
	{
		__int16 x;
		__int16 y;
	};

	AssertSize(FxGlassVertex, 0x4);
	AssertOffset(FxGlassVertex, x, 0x0);
	AssertOffset(FxGlassVertex, y, 0x2);

	struct FxGlassHoleHeader
	{
		unsigned __int16 uniqueVertCount;
		char touchVert;
		char pad[1];
	};

	AssertSize(FxGlassHoleHeader, 0x4);
	AssertOffset(FxGlassHoleHeader, uniqueVertCount, 0x0);
	AssertOffset(FxGlassHoleHeader, touchVert, 0x2);
	AssertOffset(FxGlassHoleHeader, pad, 0x3);

	struct FxGlassCrackHeader
	{
		unsigned __int16 uniqueVertCount;
		char beginVertIndex;
		char endVertIndex;
	};

	AssertSize(FxGlassCrackHeader, 0x4);
	AssertOffset(FxGlassCrackHeader, uniqueVertCount, 0x0);
	AssertOffset(FxGlassCrackHeader, beginVertIndex, 0x2);
	AssertOffset(FxGlassCrackHeader, endVertIndex, 0x3);

	union FxGlassGeometryData
	{
		FxGlassVertex vert;
		FxGlassHoleHeader hole;
		FxGlassCrackHeader crack;
		char asBytes[4];
		__int16 anonymous[2];
	};

	AssertSize(FxGlassGeometryData, 0x4);
	AssertOffset(FxGlassGeometryData, vert, 0x0);
	AssertOffset(FxGlassGeometryData, hole, 0x0);
	AssertOffset(FxGlassGeometryData, crack, 0x0);
	AssertOffset(FxGlassGeometryData, asBytes, 0x0);
	AssertOffset(FxGlassGeometryData, anonymous, 0x0);

	struct FxGlassInitPieceState
	{
		FxSpatialFrame frame;
		float radius;
		float texCoordOrigin[2];
		unsigned int supportMask;
		float areaX2;
		unsigned char defIndex;
		unsigned char vertCount;
		unsigned char fanDataCount;
		char pad[1];
	};

	AssertSize(FxGlassInitPieceState, 0x34);
	AssertOffset(FxGlassInitPieceState, frame, 0x0);
	AssertOffset(FxGlassInitPieceState, radius, 0x1C);
	AssertOffset(FxGlassInitPieceState, texCoordOrigin, 0x20);
	AssertOffset(FxGlassInitPieceState, supportMask, 0x28);
	AssertOffset(FxGlassInitPieceState, areaX2, 0x2C);
	AssertOffset(FxGlassInitPieceState, defIndex, 0x30);
	AssertOffset(FxGlassInitPieceState, vertCount, 0x31);
	AssertOffset(FxGlassInitPieceState, fanDataCount, 0x32);
	AssertOffset(FxGlassInitPieceState, pad, 0x33);

	struct GfxWorldDpvsPlanes
	{
		int cellCount;
		cplane_s* planes;
		unsigned __int16* nodes;
		unsigned int* sceneEntCellBits;
	};

	AssertSize(GfxWorldDpvsPlanes, 0x20);
	AssertOffset(GfxWorldDpvsPlanes, cellCount, 0x0);
	AssertOffset(GfxWorldDpvsPlanes, planes, 0x8);
	AssertOffset(GfxWorldDpvsPlanes, nodes, 0x10);
	AssertOffset(GfxWorldDpvsPlanes, sceneEntCellBits, 0x18);

	struct GfxWorldVertexData
	{
		GfxWorldVertex* vertices;
		void* worldVb;
	};

	AssertSize(GfxWorldVertexData, 0x10);
	AssertOffset(GfxWorldVertexData, vertices, 0x0);
	AssertOffset(GfxWorldVertexData, worldVb, 0x8);

	struct GfxWorldVertexLayerData
	{
		char* data;
		IDirect3DVertexBuffer9* layerVb;
	};

	AssertSize(GfxWorldVertexLayerData, 0x10);
	AssertOffset(GfxWorldVertexLayerData, data, 0x0);
	AssertOffset(GfxWorldVertexLayerData, layerVb, 0x8);

	struct GfxWorldDraw
	{
		unsigned int reflectionProbeCount;
		GfxImage** reflectionProbes;
		GfxReflectionProbe* reflectionProbeOrigins;
		GfxTexture* reflectionProbeTextures;
		int lightmapCount;
		GfxLightmapArray* lightmaps;
		GfxTexture* lightmapPrimaryTextures;
		GfxTexture* lightmapSecondaryTextures;
		GfxImage* lightmapOverridePrimary;
		GfxImage* lightmapOverrideSecondary;
		unsigned int vertexCount;
		GfxWorldVertexData vd;
		unsigned int vertexLayerDataSize;
		GfxWorldVertexLayerData vld;
		int indexCount;
		unsigned short* indices;
	};

	AssertSize(GfxWorldDraw, 0x90);
	AssertOffset(GfxWorldDraw, reflectionProbeCount, 0x0);
	AssertOffset(GfxWorldDraw, reflectionProbes, 0x8);
	AssertOffset(GfxWorldDraw, reflectionProbeOrigins, 0x10);
	AssertOffset(GfxWorldDraw, reflectionProbeTextures, 0x18);
	AssertOffset(GfxWorldDraw, lightmapCount, 0x20);
	AssertOffset(GfxWorldDraw, lightmaps, 0x28);
	AssertOffset(GfxWorldDraw, lightmapPrimaryTextures, 0x30);
	AssertOffset(GfxWorldDraw, lightmapSecondaryTextures, 0x38);
	AssertOffset(GfxWorldDraw, lightmapOverridePrimary, 0x40);
	AssertOffset(GfxWorldDraw, lightmapOverrideSecondary, 0x48);
	AssertOffset(GfxWorldDraw, vertexCount, 0x50);
	AssertOffset(GfxWorldDraw, vd, 0x58);
	AssertOffset(GfxWorldDraw, vertexLayerDataSize, 0x68);
	AssertOffset(GfxWorldDraw, vld, 0x70);
	AssertOffset(GfxWorldDraw, indexCount, 0x80);
	AssertOffset(GfxWorldDraw, indices, 0x88);

	struct GfxLightGrid
	{
		bool hasLightRegions;
		unsigned int lastSunPrimaryLightIndex;
		unsigned __int16 mins[3];
		unsigned __int16 maxs[3];
		unsigned int rowAxis;
		unsigned int colAxis;
		unsigned __int16* rowDataStart;
		unsigned int rawRowDataSize;
		char* rawRowData;
		unsigned int entryCount;
		GfxLightGridEntry* entries;
		unsigned int colorCount;
		GfxLightGridColors* colors;
	};

	AssertSize(GfxLightGrid, 0x58);
	AssertOffset(GfxLightGrid, hasLightRegions, 0x0);
	AssertOffset(GfxLightGrid, lastSunPrimaryLightIndex, 0x4);
	AssertOffset(GfxLightGrid, mins, 0x8);
	AssertOffset(GfxLightGrid, maxs, 0xE);
	AssertOffset(GfxLightGrid, rowAxis, 0x14);
	AssertOffset(GfxLightGrid, colAxis, 0x18);
	AssertOffset(GfxLightGrid, rowDataStart, 0x20);
	AssertOffset(GfxLightGrid, rawRowDataSize, 0x28);
	AssertOffset(GfxLightGrid, rawRowData, 0x30);
	AssertOffset(GfxLightGrid, entryCount, 0x38);
	AssertOffset(GfxLightGrid, entries, 0x40);
	AssertOffset(GfxLightGrid, colorCount, 0x48);
	AssertOffset(GfxLightGrid, colors, 0x50);

	struct sunflare_t
	{
		bool hasValidData;
		Material* spriteMaterial;
		Material* flareMaterial;
		float spriteSize;
		float flareMinSize;
		float flareMinDot;
		float flareMaxSize;
		float flareMaxDot;
		float flareMaxAlpha;
		int flareFadeInTime;
		int flareFadeOutTime;
		float blindMinDot;
		float blindMaxDot;
		float blindMaxDarken;
		int blindFadeInTime;
		int blindFadeOutTime;
		float glareMinDot;
		float glareMaxDot;
		float glareMaxLighten;
		int glareFadeInTime;
		int glareFadeOutTime;
		float sunFxPosition[3];
	};

	AssertSize(sunflare_t, 0x70);
	AssertOffset(sunflare_t, hasValidData, 0x0);
	AssertOffset(sunflare_t, spriteMaterial, 0x8);
	AssertOffset(sunflare_t, flareMaterial, 0x10);
	AssertOffset(sunflare_t, spriteSize, 0x18);
	AssertOffset(sunflare_t, flareMinSize, 0x1C);
	AssertOffset(sunflare_t, flareMinDot, 0x20);
	AssertOffset(sunflare_t, flareMaxSize, 0x24);
	AssertOffset(sunflare_t, flareMaxDot, 0x28);
	AssertOffset(sunflare_t, flareMaxAlpha, 0x2C);
	AssertOffset(sunflare_t, flareFadeInTime, 0x30);
	AssertOffset(sunflare_t, flareFadeOutTime, 0x34);
	AssertOffset(sunflare_t, blindMinDot, 0x38);
	AssertOffset(sunflare_t, blindMaxDot, 0x3C);
	AssertOffset(sunflare_t, blindMaxDarken, 0x40);
	AssertOffset(sunflare_t, blindFadeInTime, 0x44);
	AssertOffset(sunflare_t, blindFadeOutTime, 0x48);
	AssertOffset(sunflare_t, glareMinDot, 0x4C);
	AssertOffset(sunflare_t, glareMaxDot, 0x50);
	AssertOffset(sunflare_t, glareMaxLighten, 0x54);
	AssertOffset(sunflare_t, glareFadeInTime, 0x58);
	AssertOffset(sunflare_t, glareFadeOutTime, 0x5C);
	AssertOffset(sunflare_t, sunFxPosition, 0x60);

	struct GfxWorldDpvsStatic
	{
		unsigned int smodelCount;
		unsigned int staticSurfaceCount;
		unsigned int staticSurfaceCountNoDecal;
		unsigned int litOpaqueSurfsBegin;
		unsigned int litOpaqueSurfsEnd;
		unsigned int litTransSurfsBegin;
		unsigned int litTransSurfsEnd;
		unsigned int shadowCasterSurfsBegin;
		unsigned int shadowCasterSurfsEnd;
		unsigned int emissiveSurfsBegin;
		unsigned int emissiveSurfsEnd;
		unsigned int smodelVisDataCount;
		unsigned int surfaceVisDataCount;
		unsigned char* smodelVisData[3];
		unsigned char* surfaceVisData[3];
		unsigned __int16* sortedSurfIndex;
		GfxStaticModelInst* smodelInsts;
		GfxSurface* surfaces;
		GfxSurfaceBounds* surfacesBounds;
		GfxStaticModelDrawInst* smodelDrawInsts;
		GfxDrawSurf* surfaceMaterials;
		unsigned int* surfaceCastsSunShadow;
		volatile int usageCount;
	};

	AssertSize(GfxWorldDpvsStatic, 0xA8);
	AssertOffset(GfxWorldDpvsStatic, smodelCount, 0x0);
	AssertOffset(GfxWorldDpvsStatic, staticSurfaceCount, 0x4);
	AssertOffset(GfxWorldDpvsStatic, staticSurfaceCountNoDecal, 0x8);
	AssertOffset(GfxWorldDpvsStatic, litOpaqueSurfsBegin, 0xC);
	AssertOffset(GfxWorldDpvsStatic, litOpaqueSurfsEnd, 0x10);
	AssertOffset(GfxWorldDpvsStatic, litTransSurfsBegin, 0x14);
	AssertOffset(GfxWorldDpvsStatic, litTransSurfsEnd, 0x18);
	AssertOffset(GfxWorldDpvsStatic, shadowCasterSurfsBegin, 0x1C);
	AssertOffset(GfxWorldDpvsStatic, shadowCasterSurfsEnd, 0x20);
	AssertOffset(GfxWorldDpvsStatic, emissiveSurfsBegin, 0x24);
	AssertOffset(GfxWorldDpvsStatic, emissiveSurfsEnd, 0x28);
	AssertOffset(GfxWorldDpvsStatic, smodelVisDataCount, 0x2C);
	AssertOffset(GfxWorldDpvsStatic, surfaceVisDataCount, 0x30);
	AssertOffset(GfxWorldDpvsStatic, smodelVisData, 0x38);
	AssertOffset(GfxWorldDpvsStatic, surfaceVisData, 0x50);
	AssertOffset(GfxWorldDpvsStatic, sortedSurfIndex, 0x68);
	AssertOffset(GfxWorldDpvsStatic, smodelInsts, 0x70);
	AssertOffset(GfxWorldDpvsStatic, surfaces, 0x78);
	AssertOffset(GfxWorldDpvsStatic, surfacesBounds, 0x80);
	AssertOffset(GfxWorldDpvsStatic, smodelDrawInsts, 0x88);
	AssertOffset(GfxWorldDpvsStatic, surfaceMaterials, 0x90);
	AssertOffset(GfxWorldDpvsStatic, surfaceCastsSunShadow, 0x98);
	AssertOffset(GfxWorldDpvsStatic, usageCount, 0xA0);

	struct GfxWorldDpvsDynamic
	{
		unsigned int dynEntClientWordCount[2];
		unsigned int dynEntClientCount[2];
		unsigned int* dynEntCellBits[2];
		char* dynEntVisData[2][3];
	};

	AssertSize(GfxWorldDpvsDynamic, 0x50);
	AssertOffset(GfxWorldDpvsDynamic, dynEntClientWordCount, 0x0);
	AssertOffset(GfxWorldDpvsDynamic, dynEntClientCount, 0x8);
	AssertOffset(GfxWorldDpvsDynamic, dynEntCellBits, 0x10);
	AssertOffset(GfxWorldDpvsDynamic, dynEntVisData, 0x20);

	struct GfxWorld
	{
		const char* name;
		const char* baseName;
		int planeCount;
		int nodeCount;
		unsigned int surfaceCount;
		int skyCount;
		GfxSky* skies;
		unsigned int lastSunPrimaryLightIndex;
		unsigned int primaryLightCount;
		unsigned int sortKeyLitDecal;
		unsigned int sortKeyEffectDecal;
		unsigned int sortKeyEffectAuto;
		unsigned int sortKeyDistortion;
		GfxWorldDpvsPlanes dpvsPlanes;
		GfxCellTreeCount* aabbTreeCounts;
		GfxCellTree* aabbTrees;
		GfxCell* cells;
		GfxWorldDraw draw;
		GfxLightGrid lightGrid;
		int modelCount;
		GfxBrushModel* models;
		Bounds bounds;
		unsigned int checksum;
		int materialMemoryCount;
		MaterialMemory* materialMemory;
		sunflare_t sun;
		float outdoorLookupMatrix[4][4];
		GfxImage* outdoorImage;
		unsigned int* cellCasterBits;
		unsigned int* cellHasSunLitSurfsBits;
		GfxSceneDynModel* sceneDynModel;
		GfxSceneDynBrush* sceneDynBrush;
		unsigned int* primaryLightEntityShadowVis;
		unsigned int* primaryLightDynEntShadowVis[2];
		char* nonSunPrimaryLightForModelDynEnt;
		GfxShadowGeometry* shadowGeom;
		GfxLightRegion* lightRegion;
		GfxWorldDpvsStatic dpvs;
		GfxWorldDpvsDynamic dpvsDyn;
		unsigned int mapVtxChecksum;
		unsigned int heroOnlyLightCount;
		GfxHeroOnlyLight* heroOnlyLights;
		char fogTypesAllowed;
	};

	AssertSize(GfxWorld, 0x3B0);
	AssertOffset(GfxWorld, name, 0x0);
	AssertOffset(GfxWorld, baseName, 0x8);
	AssertOffset(GfxWorld, planeCount, 0x10);
	AssertOffset(GfxWorld, nodeCount, 0x14);
	AssertOffset(GfxWorld, surfaceCount, 0x18);
	AssertOffset(GfxWorld, skyCount, 0x1C);
	AssertOffset(GfxWorld, skies, 0x20);
	AssertOffset(GfxWorld, lastSunPrimaryLightIndex, 0x28);
	AssertOffset(GfxWorld, primaryLightCount, 0x2C);
	AssertOffset(GfxWorld, sortKeyLitDecal, 0x30);
	AssertOffset(GfxWorld, sortKeyEffectDecal, 0x34);
	AssertOffset(GfxWorld, sortKeyEffectAuto, 0x38);
	AssertOffset(GfxWorld, sortKeyDistortion, 0x3C);
	AssertOffset(GfxWorld, dpvsPlanes, 0x40);
	AssertOffset(GfxWorld, aabbTreeCounts, 0x60);
	AssertOffset(GfxWorld, aabbTrees, 0x68);
	AssertOffset(GfxWorld, cells, 0x70);
	AssertOffset(GfxWorld, draw, 0x78);
	AssertOffset(GfxWorld, lightGrid, 0x108);
	AssertOffset(GfxWorld, modelCount, 0x160);
	AssertOffset(GfxWorld, models, 0x168);
	AssertOffset(GfxWorld, bounds, 0x170);
	AssertOffset(GfxWorld, checksum, 0x188);
	AssertOffset(GfxWorld, materialMemoryCount, 0x18C);
	AssertOffset(GfxWorld, materialMemory, 0x190);
	AssertOffset(GfxWorld, sun, 0x198);
	AssertOffset(GfxWorld, outdoorLookupMatrix, 0x208);
	AssertOffset(GfxWorld, outdoorImage, 0x248);
	AssertOffset(GfxWorld, cellCasterBits, 0x250);
	AssertOffset(GfxWorld, cellHasSunLitSurfsBits, 0x258);
	AssertOffset(GfxWorld, sceneDynModel, 0x260);
	AssertOffset(GfxWorld, sceneDynBrush, 0x268);
	AssertOffset(GfxWorld, primaryLightEntityShadowVis, 0x270);
	AssertOffset(GfxWorld, primaryLightDynEntShadowVis, 0x278);
	AssertOffset(GfxWorld, nonSunPrimaryLightForModelDynEnt, 0x288);
	AssertOffset(GfxWorld, shadowGeom, 0x290);
	AssertOffset(GfxWorld, lightRegion, 0x298);
	AssertOffset(GfxWorld, dpvs, 0x2A0);
	AssertOffset(GfxWorld, dpvsDyn, 0x348);
	AssertOffset(GfxWorld, mapVtxChecksum, 0x398);
	AssertOffset(GfxWorld, heroOnlyLightCount, 0x39C);
	AssertOffset(GfxWorld, heroOnlyLights, 0x3A0);
	AssertOffset(GfxWorld, fogTypesAllowed, 0x3A8);

	struct GfxSky
	{
		int skySurfCount;
		int* skyStartSurfs;
		GfxImage* skyImage;
		char skySamplerState;
	};

	AssertSize(GfxSky, 0x20);
	AssertOffset(GfxSky, skySurfCount, 0x0);
	AssertOffset(GfxSky, skyStartSurfs, 0x8);
	AssertOffset(GfxSky, skyImage, 0x10);
	AssertOffset(GfxSky, skySamplerState, 0x18);

	struct GfxCellTreeCount
	{
		int aabbTreeCount;
	};

	AssertSize(GfxCellTreeCount, 0x4);
	AssertOffset(GfxCellTreeCount, aabbTreeCount, 0x0);

	struct GfxCellTree
	{
		GfxAabbTree* aabbTree;
	};

	AssertSize(GfxCellTree, 0x8);
	AssertOffset(GfxCellTree, aabbTree, 0x0);

	struct GfxAabbTree
	{
		Bounds bounds;
		unsigned __int16 childCount;
		unsigned __int16 surfaceCount;
		unsigned __int16 startSurfIndex;
		unsigned __int16 surfaceCountNoDecal;
		unsigned __int16 startSurfIndexNoDecal;
		unsigned __int16 smodelIndexCount;
		unsigned __int16* smodelIndexes;
		int childrenOffset;
	};

	AssertSize(GfxAabbTree, 0x38);
	AssertOffset(GfxAabbTree, bounds, 0x0);
	AssertOffset(GfxAabbTree, childCount, 0x18);
	AssertOffset(GfxAabbTree, surfaceCount, 0x1A);
	AssertOffset(GfxAabbTree, startSurfIndex, 0x1C);
	AssertOffset(GfxAabbTree, surfaceCountNoDecal, 0x1E);
	AssertOffset(GfxAabbTree, startSurfIndexNoDecal, 0x20);
	AssertOffset(GfxAabbTree, smodelIndexCount, 0x22);
	AssertOffset(GfxAabbTree, smodelIndexes, 0x28);
	AssertOffset(GfxAabbTree, childrenOffset, 0x30);

	struct GfxCell
	{
		Bounds bounds;
		int portalCount;
		GfxPortal* portals;
		char reflectionProbeCount;
		char* reflectionProbes;
	};

	AssertSize(GfxCell, 0x38);
	AssertOffset(GfxCell, bounds, 0x0);
	AssertOffset(GfxCell, portalCount, 0x18);
	AssertOffset(GfxCell, portals, 0x20);
	AssertOffset(GfxCell, reflectionProbeCount, 0x28);
	AssertOffset(GfxCell, reflectionProbes, 0x30);

	struct GfxPortalWritable
	{
		bool isQueued;
		bool isAncestor;
		char recursionDepth;
		char hullPointCount;
		float(*hullPoints)[2];
		GfxPortal* queuedParent;
	};

	AssertSize(GfxPortalWritable, 0x18);
	AssertOffset(GfxPortalWritable, isQueued, 0x0);
	AssertOffset(GfxPortalWritable, isAncestor, 0x1);
	AssertOffset(GfxPortalWritable, recursionDepth, 0x2);
	AssertOffset(GfxPortalWritable, hullPointCount, 0x3);
	AssertOffset(GfxPortalWritable, hullPoints, 0x8);
	AssertOffset(GfxPortalWritable, queuedParent, 0x10);

	struct DpvsPlane
	{
		float coeffs[4];
	};

	AssertSize(DpvsPlane, 0x10);
	AssertOffset(DpvsPlane, coeffs, 0x0);

	struct GfxPortal
	{
		GfxPortalWritable writable;
		DpvsPlane plane;
		float(*vertices)[3];
		unsigned __int16 cellIndex;
		char vertexCount;
		float hullAxis[2][3];
	};

	AssertSize(GfxPortal, 0x50);
	AssertOffset(GfxPortal, writable, 0x0);
	AssertOffset(GfxPortal, plane, 0x18);
	AssertOffset(GfxPortal, vertices, 0x28);
	AssertOffset(GfxPortal, cellIndex, 0x30);
	AssertOffset(GfxPortal, vertexCount, 0x32);
	AssertOffset(GfxPortal, hullAxis, 0x34);

	struct GfxReflectionProbe
	{
		float origin[3];
	};

	AssertSize(GfxReflectionProbe, 0xC);
	AssertOffset(GfxReflectionProbe, origin, 0x0);

	struct GfxLightmapArray
	{
		GfxImage* primary;
		GfxImage* secondary;
	};

	AssertSize(GfxLightmapArray, 0x10);
	AssertOffset(GfxLightmapArray, primary, 0x0);
	AssertOffset(GfxLightmapArray, secondary, 0x8);

	struct GfxWorldVertex
	{
		float xyz[3];
		float binormalSign;
		GfxColor color;
		float texCoord[2];
		float lmapCoord[2];
		PackedUnitVec normal;
		PackedUnitVec tangent;
	};

	AssertSize(GfxWorldVertex, 0x2C);
	AssertOffset(GfxWorldVertex, xyz, 0x0);
	AssertOffset(GfxWorldVertex, binormalSign, 0xC);
	AssertOffset(GfxWorldVertex, color, 0x10);
	AssertOffset(GfxWorldVertex, texCoord, 0x14);
	AssertOffset(GfxWorldVertex, lmapCoord, 0x1C);
	AssertOffset(GfxWorldVertex, normal, 0x24);
	AssertOffset(GfxWorldVertex, tangent, 0x28);

	struct GfxLightGridEntry
	{
		unsigned __int16 colorsIndex;
		char primaryLightIndex;
		char needsTrace;
	};

	AssertSize(GfxLightGridEntry, 0x4);
	AssertOffset(GfxLightGridEntry, colorsIndex, 0x0);
	AssertOffset(GfxLightGridEntry, primaryLightIndex, 0x2);
	AssertOffset(GfxLightGridEntry, needsTrace, 0x3);

	struct GfxLightGridColors
	{
		unsigned char rgb[56][3];
	};

	AssertSize(GfxLightGridColors, 0xA8);
	AssertOffset(GfxLightGridColors, rgb, 0x0);

	struct GfxBrushModelWritable
	{
		Bounds bounds;
	};

	AssertSize(GfxBrushModelWritable, 0x18);
	AssertOffset(GfxBrushModelWritable, bounds, 0x0);

	struct GfxBrushModel
	{
		GfxBrushModelWritable writable;
		Bounds bounds;
		float radius;
		unsigned __int16 surfaceCount;
		unsigned __int16 startSurfIndex;
		unsigned __int16 surfaceCountNoDecal;
	};

	AssertSize(GfxBrushModel, 0x3C);
	AssertOffset(GfxBrushModel, writable, 0x0);
	AssertOffset(GfxBrushModel, bounds, 0x18);
	AssertOffset(GfxBrushModel, radius, 0x30);
	AssertOffset(GfxBrushModel, surfaceCount, 0x34);
	AssertOffset(GfxBrushModel, startSurfIndex, 0x36);
	AssertOffset(GfxBrushModel, surfaceCountNoDecal, 0x38);

	struct MaterialMemory
	{
		Material* material;
		int memory;
	};

	AssertSize(MaterialMemory, 0x10);
	AssertOffset(MaterialMemory, material, 0x0);
	AssertOffset(MaterialMemory, memory, 0x8);

	struct XModelDrawInfo
	{
		char hasGfxEntIndex;
		char lod;
		unsigned __int16 surfId;
	};

	AssertSize(XModelDrawInfo, 0x4);
	AssertOffset(XModelDrawInfo, hasGfxEntIndex, 0x0);
	AssertOffset(XModelDrawInfo, lod, 0x1);
	AssertOffset(XModelDrawInfo, surfId, 0x2);

	struct GfxSceneDynModel
	{
		XModelDrawInfo info;
		unsigned __int16 dynEntId;
	};

	AssertSize(GfxSceneDynModel, 0x6);
	AssertOffset(GfxSceneDynModel, info, 0x0);
	AssertOffset(GfxSceneDynModel, dynEntId, 0x4);

	struct BModelDrawInfo
	{
		unsigned __int16 surfId;
	};

	AssertSize(BModelDrawInfo, 0x2);
	AssertOffset(BModelDrawInfo, surfId, 0x0);

	struct GfxSceneDynBrush
	{
		BModelDrawInfo info;
		unsigned __int16 dynEntId;
	};

	AssertSize(GfxSceneDynBrush, 0x4);
	AssertOffset(GfxSceneDynBrush, info, 0x0);
	AssertOffset(GfxSceneDynBrush, dynEntId, 0x2);

	struct GfxShadowGeometry
	{
		unsigned __int16 surfaceCount;
		unsigned __int16 smodelCount;
		unsigned __int16* sortedSurfIndex;
		unsigned __int16* smodelIndex;
	};

	AssertSize(GfxShadowGeometry, 0x18);
	AssertOffset(GfxShadowGeometry, surfaceCount, 0x0);
	AssertOffset(GfxShadowGeometry, smodelCount, 0x2);
	AssertOffset(GfxShadowGeometry, sortedSurfIndex, 0x8);
	AssertOffset(GfxShadowGeometry, smodelIndex, 0x10);

	struct GfxLightRegion
	{
		unsigned int hullCount;
		GfxLightRegionHull* hulls;
	};

	AssertSize(GfxLightRegion, 0x10);
	AssertOffset(GfxLightRegion, hullCount, 0x0);
	AssertOffset(GfxLightRegion, hulls, 0x8);

	struct GfxLightRegionHull
	{
		float kdopMidPoint[9];
		float kdopHalfSize[9];
		unsigned int axisCount;
		GfxLightRegionAxis* axis;
	};

	AssertSize(GfxLightRegionHull, 0x58);
	AssertOffset(GfxLightRegionHull, kdopMidPoint, 0x0);
	AssertOffset(GfxLightRegionHull, kdopHalfSize, 0x24);
	AssertOffset(GfxLightRegionHull, axisCount, 0x48);
	AssertOffset(GfxLightRegionHull, axis, 0x50);

	struct GfxLightRegionAxis
	{
		float dir[3];
		float midPoint;
		float halfSize;
	};

	AssertSize(GfxLightRegionAxis, 0x14);
	AssertOffset(GfxLightRegionAxis, dir, 0x0);
	AssertOffset(GfxLightRegionAxis, midPoint, 0xC);
	AssertOffset(GfxLightRegionAxis, halfSize, 0x10);

	struct GfxStaticModelInst
	{
		Bounds bounds;
		float lightingOrigin[3];
	};

	AssertSize(GfxStaticModelInst, 0x24);
	AssertOffset(GfxStaticModelInst, bounds, 0x0);
	AssertOffset(GfxStaticModelInst, lightingOrigin, 0x18);

	struct srfTriangles_t
	{
		int vertexLayerData;
		int firstVertex;
		unsigned short vertexCount;
		unsigned short triCount;
		int baseIndex;
	};

	AssertSize(srfTriangles_t, 0x10);
	AssertOffset(srfTriangles_t, vertexLayerData, 0x0);
	AssertOffset(srfTriangles_t, firstVertex, 0x4);
	AssertOffset(srfTriangles_t, vertexCount, 0x8);
	AssertOffset(srfTriangles_t, triCount, 0xA);
	AssertOffset(srfTriangles_t, baseIndex, 0xC);

	struct GfxSurfaceLightingAndFlagsFields
	{
		unsigned char lightmapIndex;
		unsigned char reflectionProbeIndex;
		unsigned char primaryLightIndex;
		unsigned char flags;
	};

	AssertSize(GfxSurfaceLightingAndFlagsFields, 0x4);
	AssertOffset(GfxSurfaceLightingAndFlagsFields, lightmapIndex, 0x0);
	AssertOffset(GfxSurfaceLightingAndFlagsFields, reflectionProbeIndex, 0x1);
	AssertOffset(GfxSurfaceLightingAndFlagsFields, primaryLightIndex, 0x2);
	AssertOffset(GfxSurfaceLightingAndFlagsFields, flags, 0x3);

	union GfxSurfaceLightingAndFlags
	{
		GfxSurfaceLightingAndFlagsFields fields;
		unsigned int packed;
	};

	AssertSize(GfxSurfaceLightingAndFlags, 0x4);
	AssertOffset(GfxSurfaceLightingAndFlags, fields, 0x0);
	AssertOffset(GfxSurfaceLightingAndFlags, packed, 0x0);

	struct GfxSurface
	{
		srfTriangles_t tris;
		Material* material;
		GfxSurfaceLightingAndFlags laf;
	};

	AssertSize(GfxSurface, 0x20);
	AssertOffset(GfxSurface, tris, 0x0);
	AssertOffset(GfxSurface, material, 0x10);
	AssertOffset(GfxSurface, laf, 0x18);

	struct GfxSurfaceBounds
	{
		Bounds bounds;
	};

	AssertSize(GfxSurfaceBounds, 0x18);
	AssertOffset(GfxSurfaceBounds, bounds, 0x0);

	struct GfxPackedPlacement
	{
		float origin[3];
		float axis[3][3];
		float scale;
	};

	AssertSize(GfxPackedPlacement, 0x34);
	AssertOffset(GfxPackedPlacement, origin, 0x0);
	AssertOffset(GfxPackedPlacement, axis, 0xC);
	AssertOffset(GfxPackedPlacement, scale, 0x30);

	struct GfxStaticModelDrawInst
	{
		GfxPackedPlacement placement;
		XModel* model;
		unsigned __int16 cullDist;
		unsigned __int16 lightingHandle;
		unsigned char reflectionProbeIndex;
		unsigned char primaryLightIndex;
		unsigned char flags;
		unsigned char firstMtlSkinIndex;
		GfxColor groundLighting;
		unsigned __int16 cacheId[4];
	};

	AssertSize(GfxStaticModelDrawInst, 0x58);
	AssertOffset(GfxStaticModelDrawInst, placement, 0x0);
	AssertOffset(GfxStaticModelDrawInst, model, 0x38);
	AssertOffset(GfxStaticModelDrawInst, cullDist, 0x40);
	AssertOffset(GfxStaticModelDrawInst, lightingHandle, 0x42);
	AssertOffset(GfxStaticModelDrawInst, reflectionProbeIndex, 0x44);
	AssertOffset(GfxStaticModelDrawInst, primaryLightIndex, 0x45);
	AssertOffset(GfxStaticModelDrawInst, flags, 0x46);
	AssertOffset(GfxStaticModelDrawInst, firstMtlSkinIndex, 0x47);
	AssertOffset(GfxStaticModelDrawInst, groundLighting, 0x48);
	AssertOffset(GfxStaticModelDrawInst, cacheId, 0x4C);

	struct GfxHeroOnlyLight
	{
		char type;
		char unused[3];
		float color[3];
		float dir[3];
		float origin[3];
		float radius;
		float cosHalfFovOuter;
		float cosHalfFovInner;
		int exponent;
	};

	AssertSize(GfxHeroOnlyLight, 0x38);
	AssertOffset(GfxHeroOnlyLight, type, 0x0);
	AssertOffset(GfxHeroOnlyLight, unused, 0x1);
	AssertOffset(GfxHeroOnlyLight, color, 0x4);
	AssertOffset(GfxHeroOnlyLight, dir, 0x10);
	AssertOffset(GfxHeroOnlyLight, origin, 0x1C);
	AssertOffset(GfxHeroOnlyLight, radius, 0x28);
	AssertOffset(GfxHeroOnlyLight, cosHalfFovOuter, 0x2C);
	AssertOffset(GfxHeroOnlyLight, cosHalfFovInner, 0x30);
	AssertOffset(GfxHeroOnlyLight, exponent, 0x34);

	struct GfxLightImage
	{
		GfxImage* image;
		char samplerState;
	};

	AssertSize(GfxLightImage, 0x10);
	AssertOffset(GfxLightImage, image, 0x0);
	AssertOffset(GfxLightImage, samplerState, 0x8);

	struct GfxLightDef
	{
		const char* name;
		GfxLightImage attenuation;
		int lmapLookupStart;
	};

	AssertSize(GfxLightDef, 0x20);
	AssertOffset(GfxLightDef, name, 0x0);
	AssertOffset(GfxLightDef, attenuation, 0x8);
	AssertOffset(GfxLightDef, lmapLookupStart, 0x18);

	struct Glyph
	{
		unsigned __int16 letter;
		char x0;
		char y0;
		char dx;
		char pixelWidth;
		char pixelHeight;
		float s0;
		float t0;
		float s1;
		float t1;
	};

	AssertSize(Glyph, 0x18);
	AssertOffset(Glyph, letter, 0x0);
	AssertOffset(Glyph, x0, 0x2);
	AssertOffset(Glyph, y0, 0x3);
	AssertOffset(Glyph, dx, 0x4);
	AssertOffset(Glyph, pixelWidth, 0x5);
	AssertOffset(Glyph, pixelHeight, 0x6);
	AssertOffset(Glyph, s0, 0x8);
	AssertOffset(Glyph, t0, 0xC);
	AssertOffset(Glyph, s1, 0x10);
	AssertOffset(Glyph, t1, 0x14);

	struct LocalizeEntry
	{
		const char* value;
		const char* name;
	};

	AssertSize(LocalizeEntry, 0x10);
	AssertOffset(LocalizeEntry, value, 0x0);
	AssertOffset(LocalizeEntry, name, 0x8);

	struct TracerDef
	{
		const char* name;
		Material* material;
		unsigned int drawInterval;
		float speed;
		float beamLength;
		float beamWidth;
		float screwRadius;
		float screwDist;
		float colors[5][4];
	};

	AssertSize(TracerDef, 0x78);
	AssertOffset(TracerDef, name, 0x0);
	AssertOffset(TracerDef, material, 0x8);
	AssertOffset(TracerDef, drawInterval, 0x10);
	AssertOffset(TracerDef, speed, 0x14);
	AssertOffset(TracerDef, beamLength, 0x18);
	AssertOffset(TracerDef, beamWidth, 0x1C);
	AssertOffset(TracerDef, screwRadius, 0x20);
	AssertOffset(TracerDef, screwDist, 0x24);
	AssertOffset(TracerDef, colors, 0x28);

	struct SndDriverGlobals
	{
		const char* name;
	};

	AssertSize(SndDriverGlobals, 0x8);
	AssertOffset(SndDriverGlobals, name, 0x0);

	struct FxImpactTable
	{
		const char* name;
		FxImpactEntry* table;
	};

	AssertSize(FxImpactTable, 0x10);
	AssertOffset(FxImpactTable, name, 0x0);
	AssertOffset(FxImpactTable, table, 0x8);

	struct FxImpactEntry
	{
		FxEffectDef* nonflesh[31];
		FxEffectDef* flesh[4];
	};

	AssertSize(FxImpactEntry, 0x118);
	AssertOffset(FxImpactEntry, nonflesh, 0x0);
	AssertOffset(FxImpactEntry, flesh, 0xF8);

	struct RawFile
	{
		const char* name;
		int compressedLen;
		int len;
		const char* buffer;
	};

	AssertSize(RawFile, 0x18);
	AssertOffset(RawFile, name, 0x0);
	AssertOffset(RawFile, compressedLen, 0x8);
	AssertOffset(RawFile, len, 0xC);
	AssertOffset(RawFile, buffer, 0x10);

	struct LeaderboardDef
	{
		const char* name;
		int id;
		int columnCount;
		int xpColId;
		int prestigeColId;
		LbColumnDef* columns;
	};

	AssertSize(LeaderboardDef, 0x20);
	AssertOffset(LeaderboardDef, name, 0x0);
	AssertOffset(LeaderboardDef, id, 0x8);
	AssertOffset(LeaderboardDef, columnCount, 0xC);
	AssertOffset(LeaderboardDef, xpColId, 0x10);
	AssertOffset(LeaderboardDef, prestigeColId, 0x14);
	AssertOffset(LeaderboardDef, columns, 0x18);

	struct LbColumnDef
	{
		const char* name;
		int id;
		int propertyId;
		bool hidden;
		const char* statName;
		LbColType type;
		int precision;
		LbAggType agg;
	};

	AssertSize(LbColumnDef, 0x30);
	AssertOffset(LbColumnDef, name, 0x0);
	AssertOffset(LbColumnDef, id, 0x8);
	AssertOffset(LbColumnDef, propertyId, 0xC);
	AssertOffset(LbColumnDef, hidden, 0x10);
	AssertOffset(LbColumnDef, statName, 0x18);
	AssertOffset(LbColumnDef, type, 0x20);
	AssertOffset(LbColumnDef, precision, 0x24);
	AssertOffset(LbColumnDef, agg, 0x28);

	struct VehiclePhysDef
	{
		int physicsEnabled;
		const char* physPresetName;
		PhysPreset* physPreset;
		const char* accelGraphName;
		VehicleAxleType steeringAxle;
		VehicleAxleType powerAxle;
		VehicleAxleType brakingAxle;
		float topSpeed;
		float reverseSpeed;
		float maxVelocity;
		float maxPitch;
		float maxRoll;
		float suspensionTravelFront;
		float suspensionTravelRear;
		float suspensionStrengthFront;
		float suspensionDampingFront;
		float suspensionStrengthRear;
		float suspensionDampingRear;
		float frictionBraking;
		float frictionCoasting;
		float frictionTopSpeed;
		float frictionSide;
		float frictionSideRear;
		float velocityDependentSlip;
		float rollStability;
		float rollResistance;
		float pitchResistance;
		float yawResistance;
		float uprightStrengthPitch;
		float uprightStrengthRoll;
		float targetAirPitch;
		float airYawTorque;
		float airPitchTorque;
		float minimumMomentumForCollision;
		float collisionLaunchForceScale;
		float wreckedMassScale;
		float wreckedBodyFriction;
		float minimumJoltForNotify;
		float slipThresholdFront;
		float slipThresholdRear;
		float slipFricScaleFront;
		float slipFricScaleRear;
		float slipFricRateFront;
		float slipFricRateRear;
		float slipYawTorque;
	};

	AssertSize(VehiclePhysDef, 0xC8);
	AssertOffset(VehiclePhysDef, physicsEnabled, 0x0);
	AssertOffset(VehiclePhysDef, physPresetName, 0x8);
	AssertOffset(VehiclePhysDef, physPreset, 0x10);
	AssertOffset(VehiclePhysDef, accelGraphName, 0x18);
	AssertOffset(VehiclePhysDef, steeringAxle, 0x20);
	AssertOffset(VehiclePhysDef, powerAxle, 0x24);
	AssertOffset(VehiclePhysDef, brakingAxle, 0x28);
	AssertOffset(VehiclePhysDef, topSpeed, 0x2C);
	AssertOffset(VehiclePhysDef, reverseSpeed, 0x30);
	AssertOffset(VehiclePhysDef, maxVelocity, 0x34);
	AssertOffset(VehiclePhysDef, maxPitch, 0x38);
	AssertOffset(VehiclePhysDef, maxRoll, 0x3C);
	AssertOffset(VehiclePhysDef, suspensionTravelFront, 0x40);
	AssertOffset(VehiclePhysDef, suspensionTravelRear, 0x44);
	AssertOffset(VehiclePhysDef, suspensionStrengthFront, 0x48);
	AssertOffset(VehiclePhysDef, suspensionDampingFront, 0x4C);
	AssertOffset(VehiclePhysDef, suspensionStrengthRear, 0x50);
	AssertOffset(VehiclePhysDef, suspensionDampingRear, 0x54);
	AssertOffset(VehiclePhysDef, frictionBraking, 0x58);
	AssertOffset(VehiclePhysDef, frictionCoasting, 0x5C);
	AssertOffset(VehiclePhysDef, frictionTopSpeed, 0x60);
	AssertOffset(VehiclePhysDef, frictionSide, 0x64);
	AssertOffset(VehiclePhysDef, frictionSideRear, 0x68);
	AssertOffset(VehiclePhysDef, velocityDependentSlip, 0x6C);
	AssertOffset(VehiclePhysDef, rollStability, 0x70);
	AssertOffset(VehiclePhysDef, rollResistance, 0x74);
	AssertOffset(VehiclePhysDef, pitchResistance, 0x78);
	AssertOffset(VehiclePhysDef, yawResistance, 0x7C);
	AssertOffset(VehiclePhysDef, uprightStrengthPitch, 0x80);
	AssertOffset(VehiclePhysDef, uprightStrengthRoll, 0x84);
	AssertOffset(VehiclePhysDef, targetAirPitch, 0x88);
	AssertOffset(VehiclePhysDef, airYawTorque, 0x8C);
	AssertOffset(VehiclePhysDef, airPitchTorque, 0x90);
	AssertOffset(VehiclePhysDef, minimumMomentumForCollision, 0x94);
	AssertOffset(VehiclePhysDef, collisionLaunchForceScale, 0x98);
	AssertOffset(VehiclePhysDef, wreckedMassScale, 0x9C);
	AssertOffset(VehiclePhysDef, wreckedBodyFriction, 0xA0);
	AssertOffset(VehiclePhysDef, minimumJoltForNotify, 0xA4);
	AssertOffset(VehiclePhysDef, slipThresholdFront, 0xA8);
	AssertOffset(VehiclePhysDef, slipThresholdRear, 0xAC);
	AssertOffset(VehiclePhysDef, slipFricScaleFront, 0xB0);
	AssertOffset(VehiclePhysDef, slipFricScaleRear, 0xB4);
	AssertOffset(VehiclePhysDef, slipFricRateFront, 0xB8);
	AssertOffset(VehiclePhysDef, slipFricRateRear, 0xBC);
	AssertOffset(VehiclePhysDef, slipYawTorque, 0xC0);

	struct VehicleDef
	{
		const char* name;
		VehicleType type;
		const char* useHintString;
		int health;
		int quadBarrel;
		float texScrollScale;
		float topSpeed;
		float accel;
		float rotRate;
		float rotAccel;
		float maxBodyPitch;
		float maxBodyRoll;
		float fakeBodyAccelPitch;
		float fakeBodyAccelRoll;
		float fakeBodyVelPitch;
		float fakeBodyVelRoll;
		float fakeBodySideVelPitch;
		float fakeBodyPitchStrength;
		float fakeBodyRollStrength;
		float fakeBodyPitchDampening;
		float fakeBodyRollDampening;
		float fakeBodyBoatRockingAmplitude;
		float fakeBodyBoatRockingPeriod;
		float fakeBodyBoatRockingRotationPeriod;
		float fakeBodyBoatRockingFadeoutSpeed;
		float boatBouncingMinForce;
		float boatBouncingMaxForce;
		float boatBouncingRate;
		float boatBouncingFadeinSpeed;
		float boatBouncingFadeoutSteeringAngle;
		float collisionDamage;
		float collisionSpeed;
		float killcamOffset[3];
		int playerProtected;
		int bulletDamage;
		int armorPiercingDamage;
		int grenadeDamage;
		int projectileDamage;
		int projectileSplashDamage;
		int heavyExplosiveDamage;
		VehiclePhysDef vehPhysDef;
		float boostDuration;
		float boostRechargeTime;
		float boostAcceleration;
		float suspensionTravel;
		float maxSteeringAngle;
		float steeringLerp;
		float minSteeringScale;
		float minSteeringSpeed;
		int camLookEnabled;
		float camLerp;
		float camPitchInfluence;
		float camRollInfluence;
		float camFovIncrease;
		float camFovOffset;
		float camFovSpeed;
		const char* turretWeaponName;
		WeaponCompleteDef* turretWeapon;
		float turretHorizSpanLeft;
		float turretHorizSpanRight;
		float turretVertSpanUp;
		float turretVertSpanDown;
		float turretRotRate;
		snd_alias_list_t* turretSpinSnd;
		snd_alias_list_t* turretStopSnd;
		int trophyEnabled;
		float trophyRadius;
		float trophyInactiveRadius;
		int trophyAmmoCount;
		float trophyReloadTime;
		unsigned __int16 trophyTags[4];
		Material* compassFriendlyIcon;
		Material* compassEnemyIcon;
		int compassIconWidth;
		int compassIconHeight;
		snd_alias_list_t* idleLowSnd;
		snd_alias_list_t* idleHighSnd;
		snd_alias_list_t* engineLowSnd;
		snd_alias_list_t* engineHighSnd;
		float engineSndSpeed;
		snd_alias_list_t* engineStartUpSnd;
		int engineStartUpLength;
		snd_alias_list_t* engineShutdownSnd;
		snd_alias_list_t* engineIdleSnd;
		snd_alias_list_t* engineSustainSnd;
		snd_alias_list_t* engineRampUpSnd;
		int engineRampUpLength;
		snd_alias_list_t* engineRampDownSnd;
		int engineRampDownLength;
		snd_alias_list_t* suspensionSoftSnd;
		float suspensionSoftCompression;
		snd_alias_list_t* suspensionHardSnd;
		float suspensionHardCompression;
		snd_alias_list_t* collisionSnd;
		float collisionBlendSpeed;
		snd_alias_list_t* speedSnd;
		float speedSndBlendSpeed;
		const char* surfaceSndPrefix;
		snd_alias_list_t* surfaceSnds[31];
		float surfaceSndBlendSpeed;
		float slideVolume;
		float slideBlendSpeed;
		float inAirPitch;
	};

	AssertSize(VehicleDef, 0x3F0);
	AssertOffset(VehicleDef, name, 0x0);
	AssertOffset(VehicleDef, type, 0x8);
	AssertOffset(VehicleDef, useHintString, 0x10);
	AssertOffset(VehicleDef, health, 0x18);
	AssertOffset(VehicleDef, quadBarrel, 0x1C);
	AssertOffset(VehicleDef, texScrollScale, 0x20);
	AssertOffset(VehicleDef, topSpeed, 0x24);
	AssertOffset(VehicleDef, accel, 0x28);
	AssertOffset(VehicleDef, rotRate, 0x2C);
	AssertOffset(VehicleDef, rotAccel, 0x30);
	AssertOffset(VehicleDef, maxBodyPitch, 0x34);
	AssertOffset(VehicleDef, maxBodyRoll, 0x38);
	AssertOffset(VehicleDef, fakeBodyAccelPitch, 0x3C);
	AssertOffset(VehicleDef, fakeBodyAccelRoll, 0x40);
	AssertOffset(VehicleDef, fakeBodyVelPitch, 0x44);
	AssertOffset(VehicleDef, fakeBodyVelRoll, 0x48);
	AssertOffset(VehicleDef, fakeBodySideVelPitch, 0x4C);
	AssertOffset(VehicleDef, fakeBodyPitchStrength, 0x50);
	AssertOffset(VehicleDef, fakeBodyRollStrength, 0x54);
	AssertOffset(VehicleDef, fakeBodyPitchDampening, 0x58);
	AssertOffset(VehicleDef, fakeBodyRollDampening, 0x5C);
	AssertOffset(VehicleDef, fakeBodyBoatRockingAmplitude, 0x60);
	AssertOffset(VehicleDef, fakeBodyBoatRockingPeriod, 0x64);
	AssertOffset(VehicleDef, fakeBodyBoatRockingRotationPeriod, 0x68);
	AssertOffset(VehicleDef, fakeBodyBoatRockingFadeoutSpeed, 0x6C);
	AssertOffset(VehicleDef, boatBouncingMinForce, 0x70);
	AssertOffset(VehicleDef, boatBouncingMaxForce, 0x74);
	AssertOffset(VehicleDef, boatBouncingRate, 0x78);
	AssertOffset(VehicleDef, boatBouncingFadeinSpeed, 0x7C);
	AssertOffset(VehicleDef, boatBouncingFadeoutSteeringAngle, 0x80);
	AssertOffset(VehicleDef, collisionDamage, 0x84);
	AssertOffset(VehicleDef, collisionSpeed, 0x88);
	AssertOffset(VehicleDef, killcamOffset, 0x8C);
	AssertOffset(VehicleDef, playerProtected, 0x98);
	AssertOffset(VehicleDef, bulletDamage, 0x9C);
	AssertOffset(VehicleDef, armorPiercingDamage, 0xA0);
	AssertOffset(VehicleDef, grenadeDamage, 0xA4);
	AssertOffset(VehicleDef, projectileDamage, 0xA8);
	AssertOffset(VehicleDef, projectileSplashDamage, 0xAC);
	AssertOffset(VehicleDef, heavyExplosiveDamage, 0xB0);
	AssertOffset(VehicleDef, vehPhysDef, 0xB8);
	AssertOffset(VehicleDef, boostDuration, 0x180);
	AssertOffset(VehicleDef, boostRechargeTime, 0x184);
	AssertOffset(VehicleDef, boostAcceleration, 0x188);
	AssertOffset(VehicleDef, suspensionTravel, 0x18C);
	AssertOffset(VehicleDef, maxSteeringAngle, 0x190);
	AssertOffset(VehicleDef, steeringLerp, 0x194);
	AssertOffset(VehicleDef, minSteeringScale, 0x198);
	AssertOffset(VehicleDef, minSteeringSpeed, 0x19C);
	AssertOffset(VehicleDef, camLookEnabled, 0x1A0);
	AssertOffset(VehicleDef, camLerp, 0x1A4);
	AssertOffset(VehicleDef, camPitchInfluence, 0x1A8);
	AssertOffset(VehicleDef, camRollInfluence, 0x1AC);
	AssertOffset(VehicleDef, camFovIncrease, 0x1B0);
	AssertOffset(VehicleDef, camFovOffset, 0x1B4);
	AssertOffset(VehicleDef, camFovSpeed, 0x1B8);
	AssertOffset(VehicleDef, turretWeaponName, 0x1C0);
	AssertOffset(VehicleDef, turretWeapon, 0x1C8);
	AssertOffset(VehicleDef, turretHorizSpanLeft, 0x1D0);
	AssertOffset(VehicleDef, turretHorizSpanRight, 0x1D4);
	AssertOffset(VehicleDef, turretVertSpanUp, 0x1D8);
	AssertOffset(VehicleDef, turretVertSpanDown, 0x1DC);
	AssertOffset(VehicleDef, turretRotRate, 0x1E0);
	AssertOffset(VehicleDef, turretSpinSnd, 0x1E8);
	AssertOffset(VehicleDef, turretStopSnd, 0x1F0);
	AssertOffset(VehicleDef, trophyEnabled, 0x1F8);
	AssertOffset(VehicleDef, trophyRadius, 0x1FC);
	AssertOffset(VehicleDef, trophyInactiveRadius, 0x200);
	AssertOffset(VehicleDef, trophyAmmoCount, 0x204);
	AssertOffset(VehicleDef, trophyReloadTime, 0x208);
	AssertOffset(VehicleDef, trophyTags, 0x20C);
	AssertOffset(VehicleDef, compassFriendlyIcon, 0x218);
	AssertOffset(VehicleDef, compassEnemyIcon, 0x220);
	AssertOffset(VehicleDef, compassIconWidth, 0x228);
	AssertOffset(VehicleDef, compassIconHeight, 0x22C);
	AssertOffset(VehicleDef, idleLowSnd, 0x230);
	AssertOffset(VehicleDef, idleHighSnd, 0x238);
	AssertOffset(VehicleDef, engineLowSnd, 0x240);
	AssertOffset(VehicleDef, engineHighSnd, 0x248);
	AssertOffset(VehicleDef, engineSndSpeed, 0x250);
	AssertOffset(VehicleDef, engineStartUpSnd, 0x258);
	AssertOffset(VehicleDef, engineStartUpLength, 0x260);
	AssertOffset(VehicleDef, engineShutdownSnd, 0x268);
	AssertOffset(VehicleDef, engineIdleSnd, 0x270);
	AssertOffset(VehicleDef, engineSustainSnd, 0x278);
	AssertOffset(VehicleDef, engineRampUpSnd, 0x280);
	AssertOffset(VehicleDef, engineRampUpLength, 0x288);
	AssertOffset(VehicleDef, engineRampDownSnd, 0x290);
	AssertOffset(VehicleDef, engineRampDownLength, 0x298);
	AssertOffset(VehicleDef, suspensionSoftSnd, 0x2A0);
	AssertOffset(VehicleDef, suspensionSoftCompression, 0x2A8);
	AssertOffset(VehicleDef, suspensionHardSnd, 0x2B0);
	AssertOffset(VehicleDef, suspensionHardCompression, 0x2B8);
	AssertOffset(VehicleDef, collisionSnd, 0x2C0);
	AssertOffset(VehicleDef, collisionBlendSpeed, 0x2C8);
	AssertOffset(VehicleDef, speedSnd, 0x2D0);
	AssertOffset(VehicleDef, speedSndBlendSpeed, 0x2D8);
	AssertOffset(VehicleDef, surfaceSndPrefix, 0x2E0);
	AssertOffset(VehicleDef, surfaceSnds, 0x2E8);
	AssertOffset(VehicleDef, surfaceSndBlendSpeed, 0x3E0);
	AssertOffset(VehicleDef, slideVolume, 0x3E4);
	AssertOffset(VehicleDef, slideBlendSpeed, 0x3E8);
	AssertOffset(VehicleDef, inAirPitch, 0x3EC);

	struct ScriptStringList
	{
		int count;
		const char** strings;
	};

	AssertSize(ScriptStringList, 0x10);
	AssertOffset(ScriptStringList, count, 0x0);
	AssertOffset(ScriptStringList, strings, 0x8);

	struct XAssetList
	{
		ScriptStringList stringList;
		int assetCount;
		XAsset* assets;
	};

	AssertSize(XAssetList, 0x20);
	AssertOffset(XAssetList, stringList, 0x0);
	AssertOffset(XAssetList, assetCount, 0x10);
	AssertOffset(XAssetList, assets, 0x18);

	union XAssetHeader
	{
		void* data;
		PhysPreset* physPreset;
		PhysCollmap* physCollmap;
		XAnimParts* parts;
		XModelSurfs* modelSurfs;
		XModel* model;
		Material* material;
		MaterialPixelShader* pixelShader;
		MaterialVertexShader* vertexShader;
		MaterialVertexDeclaration* vertexDecl;
		MaterialTechniqueSet* techniqueSet;
		GfxImage* image;
		snd_alias_list_t* sound;
		SndCurve* sndCurve;
		LoadedSound* loadSnd;
		clipMap_t* clipMap;
		ComWorld* comWorld;
		GameWorldSp* gameWorldSp;
		GameWorldMp* gameWorldMp;
		MapEnts* mapEnts;
		FxWorld* fxWorld;
		GfxWorld* gfxWorld;
		GfxLightDef* lightDef;
		Font_s* font;
		MenuList* menuList;
		menuDef_t* menu;
		LocalizeEntry* localize;
		WeaponCompleteDef* weapon;
		SndDriverGlobals* sndDriverGlobals;
		FxEffectDef* fx;
		FxImpactTable* impactFx;
		RawFile* rawfile;
		StringTable* stringTable;
		LeaderboardDef* leaderboardDef;
		StructuredDataDefSet* structuredDataDefSet;
		TracerDef* tracerDef;
		VehicleDef* vehDef;
		AddonMapEnts* addonMapEnts;
	};

	AssertSize(XAssetHeader, 0x8);
	AssertOffset(XAssetHeader, data, 0x0);
	AssertOffset(XAssetHeader, physPreset, 0x0);
	AssertOffset(XAssetHeader, physCollmap, 0x0);
	AssertOffset(XAssetHeader, parts, 0x0);
	AssertOffset(XAssetHeader, modelSurfs, 0x0);
	AssertOffset(XAssetHeader, model, 0x0);
	AssertOffset(XAssetHeader, material, 0x0);
	AssertOffset(XAssetHeader, pixelShader, 0x0);
	AssertOffset(XAssetHeader, vertexShader, 0x0);
	AssertOffset(XAssetHeader, vertexDecl, 0x0);
	AssertOffset(XAssetHeader, techniqueSet, 0x0);
	AssertOffset(XAssetHeader, image, 0x0);
	AssertOffset(XAssetHeader, sound, 0x0);
	AssertOffset(XAssetHeader, sndCurve, 0x0);
	AssertOffset(XAssetHeader, loadSnd, 0x0);
	AssertOffset(XAssetHeader, clipMap, 0x0);
	AssertOffset(XAssetHeader, comWorld, 0x0);
	AssertOffset(XAssetHeader, gameWorldSp, 0x0);
	AssertOffset(XAssetHeader, gameWorldMp, 0x0);
	AssertOffset(XAssetHeader, mapEnts, 0x0);
	AssertOffset(XAssetHeader, fxWorld, 0x0);
	AssertOffset(XAssetHeader, gfxWorld, 0x0);
	AssertOffset(XAssetHeader, lightDef, 0x0);
	AssertOffset(XAssetHeader, font, 0x0);
	AssertOffset(XAssetHeader, menuList, 0x0);
	AssertOffset(XAssetHeader, menu, 0x0);
	AssertOffset(XAssetHeader, localize, 0x0);
	AssertOffset(XAssetHeader, weapon, 0x0);
	AssertOffset(XAssetHeader, sndDriverGlobals, 0x0);
	AssertOffset(XAssetHeader, fx, 0x0);
	AssertOffset(XAssetHeader, impactFx, 0x0);
	AssertOffset(XAssetHeader, rawfile, 0x0);
	AssertOffset(XAssetHeader, stringTable, 0x0);
	AssertOffset(XAssetHeader, leaderboardDef, 0x0);
	AssertOffset(XAssetHeader, structuredDataDefSet, 0x0);
	AssertOffset(XAssetHeader, tracerDef, 0x0);
	AssertOffset(XAssetHeader, vehDef, 0x0);
	AssertOffset(XAssetHeader, addonMapEnts, 0x0);

	struct XAsset
	{
		unsigned int type;
		XAssetHeader header;
	};

	AssertSize(XAsset, 0x10);
	AssertOffset(XAsset, type, 0x0);
	AssertOffset(XAsset, header, 0x8);

	struct XAssetEntry
	{
		XAsset asset;
	};

	enum XFILE_BLOCK_TYPES
	{
		XFILE_BLOCK_TEMP = 0x0,
		XFILE_BLOCK_PHYSICAL = 0x1,
		XFILE_BLOCK_RUNTIME = 0x2,
		XFILE_BLOCK_VIRTUAL = 0x3,
		XFILE_BLOCK_LARGE = 0x4,
		XFILE_BLOCK_CALLBACK = 0x5,
		XFILE_BLOCK_VERTEX = 0x6,
		XFILE_BLOCK_INDEX = 0x7,
		MAX_XFILE_COUNT = 0x8,

		XFILE_BLOCK_INVALID = -1,
	};

	typedef std::int64_t fileHandle_t;

	enum FsThread
	{
		FS_THREAD_MAIN = 0x0,
		FS_THREAD_STREAM = 0x1,
		FS_THREAD_DATABASE = 0x2,
		FS_THREAD_BACKEND = 0x3,
		FS_THREAD_SERVER = 0x4,
		FS_THREAD_COUNT = 0x5,
		FS_THREAD_INVALID = 0x6,
	};

	enum fsMode_t
	{
		FS_READ = 0x0,
		FS_WRITE = 0x1,
		FS_APPEND = 0x2,
		FS_APPEND_SYNC = 0x3,
	};

	enum fsOrigin_t
	{
		FS_SEEK_CUR = 0x0,
		FS_SEEK_END = 0x1,
		FS_SEEK_SET = 0x2,
	};

	struct scr_entref_t
	{
		unsigned short entnum;
		unsigned short classnum;
	};

	static_assert(sizeof(scr_entref_t) == 4);

	typedef void(*BuiltinFunction)();
	typedef void(*BuiltinMethod)(scr_entref_t);

	enum VariableType
	{
		VAR_UNDEFINED = 0x0,
		VAR_BEGIN_REF = 0x1,
		VAR_POINTER = 0x1,
		VAR_STRING = 0x2,
		VAR_ISTRING = 0x3,
		VAR_VECTOR = 0x4,
		VAR_END_REF = 0x5,
		VAR_FLOAT = 0x5,
		VAR_INTEGER = 0x6,
		VAR_CODEPOS = 0x7,
		VAR_PRECODEPOS = 0x8,
		VAR_FUNCTION = 0x9,
		VAR_BUILTIN_FUNCTION = 0xA,
		VAR_BUILTIN_METHOD = 0xB,
		VAR_STACK = 0xC,
		VAR_ANIMATION = 0xD,
		VAR_DEVELOPER_CODEPOS = 0xE,
		VAR_PRE_ANIMATION = 0xF,
		VAR_THREAD = 0x10,
		VAR_NOTIFY_THREAD = 0x11,
		VAR_TIME_THREAD = 0x12,
		VAR_CHILD_THREAD = 0x13,
		VAR_OBJECT = 0x14,
		VAR_DEAD_ENTITY = 0x15,
		VAR_ENTITY = 0x16,
		VAR_ARRAY = 0x17,
		VAR_DEAD_THREAD = 0x18,
		VAR_COUNT = 0x19,
		VAR_THREAD_LIST = 0x1A,
		VAR_ENDON_LIST = 0x1B,
	};

	constexpr int FIRST_OBJECT = VAR_THREAD;

	union VariableUnion
	{
		int intValue;
		float floatValue;
		unsigned int stringValue;
		const float* vectorValue;
		const char* codePosValue;
		unsigned int pointerValue;
	};

	struct VariableValue
	{
		VariableUnion u;
		int type;
	};

	static_assert(sizeof(VariableValue) == 16);

	struct scr_const_t
	{
		unsigned short _;
		char pad0[0x9A];
		unsigned short script_model;
	};

	static_assert(offsetof(scr_const_t, script_model) == 0x9C);

	struct TempPriority
	{
		void* threadHandle;
		int oldPriority;
	};

	AssertSize(TempPriority, 0x10);
	AssertOffset(TempPriority, oldPriority, 0x8);

	struct FastCriticalSection
	{
		volatile long readCount;
		volatile long writeCount;
		TempPriority tempPriority;
	};

	AssertSize(FastCriticalSection, 0x18);
	AssertOffset(FastCriticalSection, writeCount, 0x4);
	AssertOffset(FastCriticalSection, tempPriority, 0x8);

	struct HunkUser
	{
		HunkUser* current;
		HunkUser* next;
		int maxSize;
		std::intptr_t end;
		std::intptr_t pos;
		const char* name;
		bool fixed;
		int type;
		char pad0[8];
		char buf[1];
	};

	AssertOffset(HunkUser, next, 0x8);
	AssertOffset(HunkUser, maxSize, 0x10);
	AssertOffset(HunkUser, end, 0x18);
	AssertOffset(HunkUser, pos, 0x20);
	AssertOffset(HunkUser, name, 0x28);
	AssertOffset(HunkUser, fixed, 0x30);
	AssertOffset(HunkUser, type, 0x34);
	AssertOffset(HunkUser, buf, 0x40);

	enum CriticalSection
	{
		CRITSECT_CONSOLE = 0x0,
		CRITSECT_COM_ERROR = 0x2,
		CRITSECT_SCRIPT_STRING = 0x10,
		CRITSECT_SYS_EVENT_QUEUE = 0x12,
		CRITSECT_FATAL_ERROR = 0x14,
		CRITSECT_CBUF = 0x24,

		CRITSECT_COUNT = 0x2B,
	};

	struct AddonMapEnts
	{
		const char* name;
		char* entityString;
		int numEntityChars;
		MapTriggers trigger;
	};

	AssertSize(AddonMapEnts, 0x48);
	AssertOffset(AddonMapEnts, name, 0x0);
	AssertOffset(AddonMapEnts, entityString, 0x8);
	AssertOffset(AddonMapEnts, numEntityChars, 0x10);
	AssertOffset(AddonMapEnts, trigger, 0x18);

	struct XFile
	{
		unsigned int size;
		unsigned int externalSize;
		unsigned int blockSize[8];
	};

	AssertSize(XFile, 0x28);
	AssertOffset(XFile, size, 0x0);
	AssertOffset(XFile, externalSize, 0x4);
	AssertOffset(XFile, blockSize, 0x8);

	enum TraceHitType
	{
		TRACE_HITTYPE_NONE = 0x0,
		TRACE_HITTYPE_ENTITY = 0x1,
		TRACE_HITTYPE_DYNENT_MODEL = 0x2,
		TRACE_HITTYPE_DYNENT_BRUSH = 0x3,
		TRACE_HITTYPE_GLASS = 0x4,
	};

	constexpr unsigned char MAPTYPE_CUBE = 5;

	struct ImageList
	{
		unsigned int count;
		GfxImage* image[8192];
	};

	AssertOffset(ImageList, image, 0x8);

	struct CModelSectionHeader
	{
		int size;
		int offset;
		int fixupStart;
		int fixupCount;
		std::uint32_t buffer;
	};

	AssertSize(CModelSectionHeader, 20);

	enum CModelSection
	{
		SECTION_MAIN = 0,
		SECTION_INDEX = 1,
		SECTION_VERTEX = 2,
		SECTION_FIXUP = 3,
	};

	struct CModelHeader
	{
		int version;
		unsigned int signature;
		CModelSectionHeader sectionHeader[4];
	};

	AssertSize(CModelHeader, 88);

	enum
	{
		ITEM_TEXTSTYLE_NORMAL = 0,
		ITEM_TEXTSTYLE_SHADOWED = 3,
	};

	struct visField_t
	{
		const char* name;
		int offset;
		int fieldType;
	};

	AssertSize(visField_t, 0x10);
	AssertOffset(visField_t, offset, 0x8);
	AssertOffset(visField_t, fieldType, 0xC);

	struct visionSetVars_t;

	enum DvarSetSource
	{
		DVAR_SOURCE_INTERNAL = 0,
	};

	struct DObj
	{
		void* tree;
		unsigned short duplicateParts;
		unsigned short entnum;
		unsigned char duplicatePartsSize;
		unsigned char numModels;
		unsigned char numBones;
		char pad0[0x79];
		float radius;
		char pad1[0x1C];
		XModel** models;
	};

	AssertSize(DObj, 0xB0);
	AssertOffset(DObj, duplicateParts, 0x8);
	AssertOffset(DObj, entnum, 0xA);
	AssertOffset(DObj, numModels, 0xD);
	AssertOffset(DObj, numBones, 0xE);
	AssertOffset(DObj, radius, 0x88);
	AssertOffset(DObj, models, 0xA8);

	struct trace_t
	{
		float fraction;
		float normal[3];
		int surfaceFlags;
		int contents;
		const char* material;
		TraceHitType hitType;
		unsigned short hitId;
		unsigned short modelIndex;
		unsigned short partName;
		unsigned short partGroup;
		bool allsolid;
		bool startsolid;
		bool walkable;
	};

	AssertSize(trace_t, 0x30);
	AssertOffset(trace_t, surfaceFlags, 0x10);
	AssertOffset(trace_t, contents, 0x14);
	AssertOffset(trace_t, hitType, 0x20);
	AssertOffset(trace_t, hitId, 0x24);
	AssertOffset(trace_t, allsolid, 0x2C);
	AssertOffset(trace_t, startsolid, 0x2D);
	AssertOffset(trace_t, walkable, 0x2E);

	enum
	{
		PMF_LADDER = 1 << 3,
	};

	struct pml_t;

	struct WinMouseVars_t
	{
		int oldButtonState;
		POINT oldPos;
		bool mouseActive;
		bool mouseInitialized;
	};

	AssertSize(WinMouseVars_t, 0x10);

	struct WinVars_t
	{
		HINSTANCE reflib_library;
		int reflib_active;
		HWND hWnd;
		HINSTANCE hInstance;
		int activeApp;
		int isMinimized;
		int recenterMouse;
		HHOOK lowLevelKeyboardHook;
		unsigned int sysMsgTime;
	};

	AssertOffset(WinVars_t, hWnd, 0x10);
	AssertOffset(WinVars_t, recenterMouse, 0x28);
	AssertOffset(WinVars_t, sysMsgTime, 0x38);

	struct pmove_s
	{
		playerState_s* ps;
		usercmd_s cmd;
		usercmd_s oldcmd;
		int tracemask;
		int numtouch;
		int touchents[32];
		Bounds bounds;
		float xyspeed;
		int proneChange;
		float maxSprintTimeMultiplier;
		bool mantleStarted;
		float mantleEndPos[3];
		int mantleDuration;
		int viewChangeTime;
		float viewChange;
		float fTorsoPitch;
		float fWaistPitch;
		unsigned char handler;
	};

	AssertSize(pmove_s, 0x130);
	AssertOffset(pmove_s, cmd, 0x8);
	AssertOffset(pmove_s, oldcmd, 0x30);
	AssertOffset(pmove_s, tracemask, 0x58);
	AssertOffset(pmove_s, numtouch, 0x5C);
	AssertOffset(pmove_s, bounds, 0xE0);
	AssertOffset(pmove_s, handler, 0x128);

	struct mapArena_t
	{
		char uiName[32];
		char mapName[16];
		char description[32];
		char mapimage[32];
		char keys[32][16];
		char values[32][64];
		char pad[144];
	};

	AssertSize(mapArena_t, 2816);
	AssertOffset(mapArena_t, keys, 0x70);
	AssertOffset(mapArena_t, values, 0x270);

	struct newMapArena_t
	{
		char uiName[32];
		char oldMapName[16];
		char description[32];
		char mapimage[32];
		char keys[32][16];
		char values[32][64];
		char other[144];
		char mapName[32];
	};

	AssertSize(newMapArena_t, 0xB20);
	AssertOffset(newMapArena_t, other, 0xA70);
	AssertOffset(newMapArena_t, mapName, 0xB00);

	struct iwd_t
	{
		char iwdFilename[256];
		char iwdBasename[256];
		char iwdGamename[256];
		void* handle;
		int checksum;
		int pure_checksum;
		volatile int hasOpenFile;
		int numfiles;
		char referenced;
		unsigned int hashSize;
		void** hashTable;
		void* buildBuffer;
	};

	AssertSize(iwd_t, 816);
	AssertOffset(iwd_t, handle, 768);
	AssertOffset(iwd_t, hashSize, 796);
	AssertOffset(iwd_t, buildBuffer, 808);

	struct searchpath_s
	{
		searchpath_s* next;
		iwd_t* iwd;
		void* dir;
		int bLocalized;
		int ignore;
		int ignorePureCheck;
		int language;
	};

	AssertSize(searchpath_s, 0x28);
	AssertOffset(searchpath_s, bLocalized, 0x18);
	AssertOffset(searchpath_s, language, 0x24);

	struct PlayerCardData
	{
		unsigned int lastUpdateTime;
		unsigned int titleIndex;
		unsigned int iconIndex;
		unsigned int nameplateIndex;
		int rank;
		int prestige;
		int team;
		char name[32];
		char clanAbbrev[5];
	};

	AssertSize(PlayerCardData, 0x44);
	AssertOffset(PlayerCardData, clanAbbrev, 0x3C);

	enum meansOfDeath
	{
		MOD_SUICIDE = 0xC,
	};

	enum hitLocation_t
	{
		HITLOC_NONE = 0x0,
	};

	struct level_locals_t
	{
		gclient_s* clients;
		gentity_s* gentities;
		int num_entities;
		char pad0[0x24];
		int initializing;
	};

	AssertOffset(level_locals_t, num_entities, 0x10);
	AssertOffset(level_locals_t, initializing, 0x38);

	struct bgs_t;

	struct SessionData;

	enum connstate_t
	{
		CA_DISCONNECTED = 0x0,
		CA_CINEMATIC = 0x1,
		CA_LOGO = 0x2,
		CA_CONNECTING = 0x3,
		CA_CHALLENGING = 0x4,
		CA_CONNECTED = 0x5,
		CA_LOADING = 0x7,
		CA_PRIMED = 0x8,
		CA_ACTIVE = 0x9,
	};

	struct VoicePacket_t
	{
		char talker;
		char data[256];
		int dataSize;
	};

	struct ClientVoicePacket_t
	{
		char data[256];
		int dataSize;
	};

	struct voiceCommunication_t
	{
		ClientVoicePacket_t voicePackets[10];
		int voicePacketCount;
	};

	AssertSize(ClientVoicePacket_t, 260);
	AssertOffset(voiceCommunication_t, voicePacketCount, 0xA28);

	struct clientInfo_t
	{
		int infoValid;
		int nextValid;
		int clientNum;
		char name[16];
		char pad0[0x52C];
	};

	AssertSize(clientInfo_t, 0x548);
	AssertOffset(clientInfo_t, name, 0xC);

	struct PartyMember
	{
		char status;
		char pad0;
		char gamertag[32];
		char pad1[0x8E];
		std::uint64_t player;
		char pad2[0x28];
	};

	AssertSize(PartyMember, 0xE0);
	AssertOffset(PartyMember, gamertag, 0x2);
	AssertOffset(PartyMember, player, 0xB0);

	struct PartyData
	{
		char pad0[0x110];
		PartyMember partyMembers[18];
	};

	AssertOffset(PartyData, partyMembers, 0x110);

	struct GamerSettingExeConfig
	{
		int playlist;
		bool mapPrefs[16];
		char clanPrefix[5];
	};

	struct GamerSettingState
	{
		bool isProfileLoggedIn;
		bool errorOnRead;
		char pad0[0x412];
		GamerSettingExeConfig exeConfig;
		char pad1[0x390];
	};

	AssertSize(GamerSettingState, 0x7C0);
	AssertOffset(GamerSettingState, exeConfig, 0x414);

	constexpr int MAX_GPAD_COUNT = 1;

	constexpr unsigned int GPAD_VALUE_MASK = 0xFFFFFFFu;
	constexpr unsigned int GPAD_DPAD_MASK = XINPUT_GAMEPAD_DPAD_UP | XINPUT_GAMEPAD_DPAD_DOWN | XINPUT_GAMEPAD_DPAD_LEFT | XINPUT_GAMEPAD_DPAD_RIGHT;
	constexpr unsigned int GPAD_DIGITAL_MASK = 1u << 28;
	constexpr unsigned int GPAD_ANALOG_MASK = 1u << 29;
	constexpr unsigned int GPAD_STICK_MASK = 1u << 30;

	enum GamePadButton : unsigned int
	{
		GPAD_NONE = 0,
		GPAD_UP = GPAD_DIGITAL_MASK | XINPUT_GAMEPAD_DPAD_UP,
		GPAD_DOWN = GPAD_DIGITAL_MASK | XINPUT_GAMEPAD_DPAD_DOWN,
		GPAD_LEFT = GPAD_DIGITAL_MASK | XINPUT_GAMEPAD_DPAD_LEFT,
		GPAD_RIGHT = GPAD_DIGITAL_MASK | XINPUT_GAMEPAD_DPAD_RIGHT,
		GPAD_START = GPAD_DIGITAL_MASK | XINPUT_GAMEPAD_START,
		GPAD_BACK = GPAD_DIGITAL_MASK | XINPUT_GAMEPAD_BACK,
		GPAD_L3 = GPAD_DIGITAL_MASK | XINPUT_GAMEPAD_LEFT_THUMB,
		GPAD_R3 = GPAD_DIGITAL_MASK | XINPUT_GAMEPAD_RIGHT_THUMB,
		GPAD_L_SHLDR = GPAD_DIGITAL_MASK | XINPUT_GAMEPAD_LEFT_SHOULDER,
		GPAD_R_SHLDR = GPAD_DIGITAL_MASK | XINPUT_GAMEPAD_RIGHT_SHOULDER,
		GPAD_A = GPAD_DIGITAL_MASK | XINPUT_GAMEPAD_A,
		GPAD_B = GPAD_DIGITAL_MASK | XINPUT_GAMEPAD_B,
		GPAD_X = GPAD_DIGITAL_MASK | XINPUT_GAMEPAD_X,
		GPAD_Y = GPAD_DIGITAL_MASK | XINPUT_GAMEPAD_Y,
		GPAD_L_TRIG = GPAD_ANALOG_MASK | 0,
		GPAD_R_TRIG = GPAD_ANALOG_MASK | 1,
	};

	enum GamePadStick : unsigned int
	{
		GPAD_INVALID = 0,
		GPAD_LX = GPAD_STICK_MASK | 0,
		GPAD_LY = GPAD_STICK_MASK | 1,
		GPAD_RX = GPAD_STICK_MASK | 2,
		GPAD_RY = GPAD_STICK_MASK | 3,
	};

	enum GamePadStickDir
	{
		GPAD_STICK_POS = 0,
		GPAD_STICK_NEG = 1,

		GPAD_STICK_DIR_COUNT
	};

	enum GamePadButtonEvent
	{
		GPAD_BUTTON_RELEASED = 0,
		GPAD_BUTTON_PRESSED = 1,
		GPAD_BUTTON_UPDATE = 2,
	};

	enum GamepadPhysicalAxis
	{
		GPAD_PHYSAXIS_NONE = -1,
		GPAD_PHYSAXIS_RSTICK_X = 0,
		GPAD_PHYSAXIS_RSTICK_Y = 1,
		GPAD_PHYSAXIS_LSTICK_X = 2,
		GPAD_PHYSAXIS_LSTICK_Y = 3,
		GPAD_PHYSAXIS_RTRIGGER = 4,
		GPAD_PHYSAXIS_LTRIGGER = 5,

		GPAD_PHYSAXIS_COUNT
	};

	enum GamepadVirtualAxis
	{
		GPAD_VIRTAXIS_NONE = -1,
		GPAD_VIRTAXIS_SIDE = 0,
		GPAD_VIRTAXIS_FORWARD = 1,
		GPAD_VIRTAXIS_UP = 2,
		GPAD_VIRTAXIS_YAW = 3,
		GPAD_VIRTAXIS_PITCH = 4,
		GPAD_VIRTAXIS_ATTACK = 5,

		GPAD_VIRTAXIS_COUNT
	};

	enum GamepadMapping
	{
		GPAD_MAP_NONE = -1,
		GPAD_MAP_LINEAR = 0,
		GPAD_MAP_SQUARED = 1,

		GPAD_MAP_COUNT
	};

	struct ButtonToCodeMap_t
	{
		GamePadButton padButton;
		int code;
	};

	struct StickToCodeMap_t
	{
		GamePadStick padStick;
		int posCode;
		int negCode;
	};

	struct GamepadVirtualAxisMapping
	{
		GamepadPhysicalAxis physicalAxis;
		GamepadMapping mapType;
	};

	struct GpadAxesGlob
	{
		float axesValues[GPAD_PHYSAXIS_COUNT];
		GamepadVirtualAxisMapping virtualAxes[GPAD_VIRTAXIS_COUNT];
	};

	enum keyNum_t
	{
		K_NONE = 0x0,
		K_FIRSTGAMEPADBUTTON_RANGE_1 = 0x1,
		K_BUTTON_A = 0x1,
		K_BUTTON_B = 0x2,
		K_BUTTON_X = 0x3,
		K_BUTTON_Y = 0x4,
		K_BUTTON_LSHLDR = 0x5,
		K_BUTTON_RSHLDR = 0x6,
		K_LASTGAMEPADBUTTON_RANGE_1 = 0x6,
		K_TAB = 0x9,
		K_ENTER = 0xD,
		K_FIRSTGAMEPADBUTTON_RANGE_2 = 0xE,
		K_BUTTON_START = 0xE,
		K_BUTTON_BACK = 0xF,
		K_BUTTON_LSTICK = 0x10,
		K_BUTTON_RSTICK = 0x11,
		K_BUTTON_LTRIG = 0x12,
		K_BUTTON_RTRIG = 0x13,
		K_DPAD_UP = 0x14,
		K_DPAD_DOWN = 0x15,
		K_DPAD_LEFT = 0x16,
		K_DPAD_RIGHT = 0x17,
		K_LASTGAMEPADBUTTON_RANGE_2 = 0x19,
		K_ESCAPE = 0x1B,
		K_FIRSTGAMEPADBUTTON_RANGE_3 = 0x1C,
		K_APAD_UP = 0x1C,
		K_APAD_DOWN = 0x1D,
		K_APAD_LEFT = 0x1E,
		K_APAD_RIGHT = 0x1F,
		K_LASTGAMEPADBUTTON_RANGE_3 = 0x1F,
		K_UPARROW = 0x9A,
		K_DOWNARROW = 0x9B,
		K_LEFTARROW = 0x9C,
		K_RIGHTARROW = 0x9D,
		K_PGDN = 0xA3,
		K_PGUP = 0xA4,
		K_LAST_KEY = 0xDF,
	};

	struct keyname_t
	{
		const char* name;
		int keynum;
	};

	AssertSize(keyname_t, 0x10);

	constexpr int KEY_NAME_COUNT = 95;
	constexpr int LOCALIZED_KEY_NAME_COUNT = 95;

	constexpr int KEYCATCH_CONSOLE = 0x1;

	constexpr int KEYCATCH_LOCATION_SELECTION = 0x8;
	constexpr int KEYCATCH_UI = 0x10;

	enum LocSelInputState
	{
		LOC_SEL_INPUT_NONE = 0,
		LOC_SEL_INPUT_CONFIRM = 1,
		LOC_SEL_INPUT_CANCEL = 2,
	};

	constexpr int UIMENU_SCOREBOARD = 6;

	struct KeyState
	{
		int down;
		int repeats;
		int binding;
	};

	AssertSize(KeyState, 12);

	struct PlayerKeyState
	{
		char pad0[0x120];
		int anyKeyDown;
		KeyState keys[256];
		int locSelInputState;
	};

	AssertSize(PlayerKeyState, 0xD28);
	AssertOffset(PlayerKeyState, anyKeyDown, 0x120);
	AssertOffset(PlayerKeyState, keys, 0x124);
	AssertOffset(PlayerKeyState, locSelInputState, 0xD24);

	struct AimInput
	{
		float deltaTime;
		float deltaTimeScaled;
		float pitch;
		float pitchAxis;
		float pitchMax;
		float yaw;
		float yawAxis;
		float yawMax;
		float forwardAxis;
		float rightAxis;
		int buttons;
		int localClientNum;
	};

	AssertSize(AimInput, 0x30);
	AssertOffset(AimInput, buttons, 0x28);

	struct AimOutput
	{
		float pitch;
		float yaw;
		float meleeChargeYaw;
		char meleeChargeDist;
	};

	AssertOffset(AimOutput, meleeChargeDist, 0xC);

	struct AimAssistPlayerState
	{
		float velocity[3];
		int eFlags;
		int linkFlags;
		int pm_flags;
		int weapFlags;
		int weaponState;
		float fWeaponPosFrac;
		int weapIndex;
		bool hasAmmo;
		bool isDualWielding;
		bool isThirdPerson;
		bool isExtendedMelee;
	};

	AssertSize(AimAssistPlayerState, 0x2C);

	struct AimTweakables
	{
		float slowdownRegionWidth;
		float slowdownRegionHeight;
		float autoMeleeRegionWidth;
		float autoMeleeRegionHeight;
		float lockOnRegionWidth;
		float lockOnRegionHeight;
	};

	struct AimScreenTarget
	{
		int entIndex;
		float clipMins[2];
		float clipMaxs[2];
		float aimPos[3];
		float velocity[3];
		float distSqr;
		float crosshairDistSqr;
	};

	AssertSize(AimScreenTarget, 0x34);

	enum AutoMeleeState
	{
		AIM_MELEE_STATE_OFF = 0x0,
		AIM_MELEE_STATE_TARGETED = 0x1,
		AIM_MELEE_STATE_UPDATING = 0x2,
	};

	constexpr int AIM_TARGET_INVALID = 2047;

	struct AimAssistGlobals
	{
		AimAssistPlayerState ps;
		char pad0[0x4];
		float screenMtx[4][4];
		float invScreenMtx[4][4];
		bool initialized;
		int prevButtons;
		AimTweakables tweakables;
		float eyeOrigin[3];
		float viewOrigin[3];
		float viewAngles[3];
		float viewAxis[3][3];
		float fovTurnRateScale;
		float fovScaleInv;
		float adsLerp;
		float pitchDelta;
		float yawDelta;
		float screenWidth;
		float screenHeight;
		AimScreenTarget screenTargets[64];
		int screenTargetCount;
		AutoMeleeState autoMeleeState;
		int autoMeleeTargetEnt;
		float autoMeleePitch;
		float autoMeleePitchTarget;
		float autoMeleeYaw;
		float autoMeleeYawTarget;
		int lockOnTargetEnt;
		char pad1[0xC];
	};

	AssertSize(AimAssistGlobals, 0xE60);
	AssertOffset(AimAssistGlobals, initialized, 0xB0);
	AssertOffset(AimAssistGlobals, prevButtons, 0xB4);
	AssertOffset(AimAssistGlobals, tweakables, 0xB8);
	AssertOffset(AimAssistGlobals, eyeOrigin, 0xD0);
	AssertOffset(AimAssistGlobals, viewAxis, 0xF4);
	AssertOffset(AimAssistGlobals, fovTurnRateScale, 0x118);
	AssertOffset(AimAssistGlobals, adsLerp, 0x120);
	AssertOffset(AimAssistGlobals, screenWidth, 0x12C);
	AssertOffset(AimAssistGlobals, screenTargets, 0x134);
	AssertOffset(AimAssistGlobals, screenTargetCount, 0xE34);
	AssertOffset(AimAssistGlobals, autoMeleeState, 0xE38);
	AssertOffset(AimAssistGlobals, autoMeleeTargetEnt, 0xE3C);
	AssertOffset(AimAssistGlobals, lockOnTargetEnt, 0xE50);

	struct GraphFloat
	{
		char name[64];
		float knots[32][2];
		unsigned short knotCount;
		float scale;
	};

	AssertSize(GraphFloat, 0x148);

	constexpr int AIM_ASSIST_GRAPH_COUNT = 4;

	constexpr int PWF_USING_OFFHAND = 1 << 1;
	constexpr int PLF_WEAPONVIEW_ONLY = 1 << 2;
	constexpr int EF_TURRET_ACTIVE_PRONE = 1 << 10;
	constexpr int EF_TURRET_ACTIVE_DUCK = 1 << 11;
	constexpr int EF_VEHICLE_ACTIVE = 1 << 20;
	constexpr int WEAPON_STUNNED_START = 0x1A;
	constexpr int WEAPON_STUNNED_END = 0x1C;
	constexpr int PMF_FROZEN = 1 << 11;
	constexpr int KEYCATCH_MASK_ANY = -1;

	enum weapClass_t
	{
		WEAPCLASS_RIFLE = 0x0,
		WEAPCLASS_SNIPER = 0x1,
		WEAPCLASS_MG = 0x2,
		WEAPCLASS_SMG = 0x3,
		WEAPCLASS_SPREAD = 0x4,
		WEAPCLASS_PISTOL = 0x5,
		WEAPCLASS_ROCKETLAUNCHER = 0x7,
		WEAPCLASS_TURRET = 0x8,
	};

	constexpr int EF_LOOP_RUMBLE = 1 << 14;

	constexpr int EV_MAX_EVENTS = 0xB0;

	struct cspField_t
	{
		const char* szName;
		std::int64_t iOffset;
		int iFieldType;
	};

	AssertSize(cspField_t, 0x18);

	enum RumbleSourceType
	{
		RUMBLESOURCE_INVALID = 0x0,
		RUMBLESOURCE_ENTITY = 0x1,
		RUMBLESOURCE_POS = 0x2,
	};

	struct RumbleGraph
	{
		char graphName[64];
		float knots[16][2];
		unsigned short knotCount;
	};

	struct RumbleInfo
	{
		int rumbleNameIndex;
		float duration;
		float range;
		RumbleGraph* highRumbleGraph;
		RumbleGraph* lowRumbleGraph;
		int fadeWithDistance;
		int broadcast;
	};

	union RumbleSource
	{
		int entityNum;
		float pos[3];
	};

	struct ActiveRumble
	{
		RumbleInfo* rumbleInfo;
		int startTime;
		bool loop;
		RumbleSourceType sourceType;
		unsigned char scale;
		RumbleSource source;
	};

	struct RumbleGlobals
	{
		RumbleGraph graphs[64];
		RumbleInfo infos[32];
		ActiveRumble activeRumbles[32];
		float receiverPos[3];
		int receiverEntNum;
	};
}
