#include "STDInclude.hpp"

#include <bit>

#include "ModelCache.hpp"
#include "ConfigStrings.hpp"
#include "Flags.hpp"
#include "Logger.hpp"

namespace Components
{
	Game::XModel** ModelCache::gameModelsReallocated = nullptr;
	Game::XModel** ModelCache::cachedModelsReallocated = nullptr;

	bool ModelCache::HasCloneSlots()
	{
		return gameModelsReallocated && cachedModelsReallocated;
	}

	void ModelCache::WidenModelIndexFields()
	{
		constexpr int oldBitLength = std::bit_width(static_cast<unsigned int>(BASE_GMODEL_COUNT - 1));
		constexpr int newBitLength = std::bit_width(static_cast<unsigned int>(G_MODELINDEX_LIMIT - 1));

		static_assert(oldBitLength == 9);
		static_assert(newBitLength == 12);

		static const std::unordered_set<std::string> fieldsToUpdate =
		{
			"modelindex",
			"attachModelIndex[0]",
			"attachModelIndex[1]",
			"attachModelIndex[2]",
			"attachModelIndex[3]",
			"attachModelIndex[4]",
			"attachModelIndex[5]",
		};

		std::vector<Game::NetField*> fields;

		for (int i = 0; i < Game::clientStateFieldsCount; ++i)
		{
			Game::NetField* const field = &Game::clientStateFields[i];

			if (!fieldsToUpdate.contains(field->name))
			{
				continue;
			}

			if (field->bits != oldBitLength)
			{
				Logger::Error("modelcache: {} is {} bits where 9 was expected, nothing widened, so a 1.2.211 server's snapshots will misparse\n",
					field->name, field->bits);
				return;
			}

			fields.push_back(field);
		}

		if (fields.size() != fieldsToUpdate.size())
		{
			Logger::Error("modelcache: found {} of the 7 model fields, nothing widened, so a 1.2.211 server's snapshots will misparse\n", fields.size());
			return;
		}

		for (Game::NetField* const field : fields)
		{
			Utils::Hook::Set<int>(&field->bits, newBitLength);
		}
	}

	constexpr std::uintptr_t imageBase = 0x140000000;
	constexpr std::uintptr_t CG_Init_ClearCgsCall = 0x1400D64C6;
	constexpr std::ptrdiff_t cgsGameModelsOffset = 0x190;

	enum class ModelArraySiteKind
	{
		ArrayAddress,
		ArrayFromCgsField,
		FirstModel,
		ModelsEnd,
		ArrayFromImage,
	};

	struct ModelArraySite
	{
		std::uintptr_t address;
		std::uint8_t length;
		std::uint8_t operandOffset;
		ModelArraySiteKind kind;
		std::uint8_t bytes[8];
	};

	static const ModelArraySite gameModelsSites[] =
	{
		{ 0x1400D4C98, 7, 3, ModelArraySiteKind::ArrayAddress, { 0x48, 0x8D, 0x05, 0x01, 0x2A, 0x4B, 0x00 } },
		{ 0x1400D581F, 7, 3, ModelArraySiteKind::ArrayFromCgsField, { 0x48, 0x8D, 0x05, 0xEA, 0x1C, 0x4B, 0x00 } },
		{ 0x1400D5870, 7, 3, ModelArraySiteKind::ArrayFromCgsField, { 0x48, 0x8D, 0x05, 0x99, 0x1C, 0x4B, 0x00 } },
		{ 0x1400D9C9A, 7, 3, ModelArraySiteKind::FirstModel, { 0x48, 0x8D, 0x1D, 0x07, 0xDA, 0x4A, 0x00 } },
		{ 0x1400D9CA1, 7, 3, ModelArraySiteKind::ModelsEnd, { 0x48, 0x8D, 0x2D, 0xF8, 0xE9, 0x4A, 0x00 } },
		{ 0x1400D535A, 8, 4, ModelArraySiteKind::ArrayFromImage, { 0x4E, 0x8B, 0x8C, 0xC9, 0xA0, 0x76, 0x58, 0x00 } },
		{ 0x1400D548B, 8, 4, ModelArraySiteKind::ArrayFromImage, { 0x4E, 0x8B, 0x8C, 0xC9, 0xA0, 0x76, 0x58, 0x00 } },
		{ 0x1400E61A5, 8, 4, ModelArraySiteKind::ArrayFromImage, { 0x48, 0x89, 0x84, 0xCE, 0xA0, 0x76, 0x58, 0x00 } },
		{ 0x1400EAEE9, 8, 4, ModelArraySiteKind::ArrayFromImage, { 0x49, 0x8B, 0x94, 0xCC, 0xA0, 0x76, 0x58, 0x00 } },
		{ 0x1402980C2, 8, 4, ModelArraySiteKind::ArrayFromImage, { 0x4F, 0x8B, 0x8C, 0xCE, 0xA0, 0x76, 0x58, 0x00 } },
	};

	static const std::uint8_t clearCgsCall[] = { 0xE8, 0xD5, 0x1A, 0x25, 0x00 };

	static Utils::Hook clearCgsHook;

	static bool TryEncodeOperand(const ModelArraySite& site, Game::XModel** models, std::int32_t& operand)
	{
		const auto nextInstruction = static_cast<std::int64_t>(Utils::Hook::Rebase(site.address) + site.length);
		const auto modelsAddress = reinterpret_cast<std::int64_t>(models);
		std::int64_t encoded = 0;

		switch (site.kind)
		{
		case ModelArraySiteKind::ArrayAddress:
			encoded = modelsAddress - nextInstruction;
			break;
		case ModelArraySiteKind::ArrayFromCgsField:
			encoded = modelsAddress - cgsGameModelsOffset - nextInstruction;
			break;
		case ModelArraySiteKind::FirstModel:
			encoded = reinterpret_cast<std::int64_t>(&models[1]) - nextInstruction;
			break;
		case ModelArraySiteKind::ModelsEnd:
			encoded = reinterpret_cast<std::int64_t>(&models[ModelCache::BASE_GMODEL_COUNT]) - nextInstruction;
			break;
		case ModelArraySiteKind::ArrayFromImage:
			encoded = modelsAddress - static_cast<std::int64_t>(Utils::Hook::Rebase(imageBase));
			break;
		}

		if (encoded < INT32_MIN || encoded > INT32_MAX)
		{
			return false;
		}

		operand = static_cast<std::int32_t>(encoded);
		return true;
	}

	void* ModelCache::CG_Init_Memset_Hook(void* dest, int value, std::size_t size)
	{
		void* const result = reinterpret_cast<void*(*)(void*, int, std::size_t)>(clearCgsHook.GetOriginal())(dest, value, size);

		std::memset(gameModelsReallocated, 0, G_MODELINDEX_LIMIT * sizeof(Game::XModel*));

		return result;
	}

	void ModelCache::RelocateGameModels()
	{
		for (const auto& site : gameModelsSites)
		{
			if (!Utils::Hook::MatchesBytes(site.address, site.bytes, site.length))
			{
				Logger::Error("modelcache: 0x{:X} does not read as expected, gameModels not moved, so a model past 511 reads out of bounds\n", site.address);
				return;
			}
		}

		if (!Utils::Hook::MatchesBytes(CG_Init_ClearCgsCall, clearCgsCall, sizeof(clearCgsCall)))
		{
			Logger::Error("modelcache: CG_Init's memset of cgs does not read as expected, gameModels not moved, so a model past 511 reads out of bounds\n");
			return;
		}

		auto** const gameModels = static_cast<Game::XModel**>(Utils::Hook::AllocateDataNear(CG_Init_ClearCgsCall, G_MODELINDEX_LIMIT * sizeof(Game::XModel*)));

		if (!gameModels)
		{
			Logger::Error("modelcache: no memory free within reach of the image, gameModels not moved, so a model past 511 reads out of bounds\n");
			return;
		}

		std::int32_t operands[std::size(gameModelsSites)]{};

		for (std::size_t i = 0; i < std::size(gameModelsSites); ++i)
		{
			if (!TryEncodeOperand(gameModelsSites[i], gameModels, operands[i]))
			{
				VirtualFree(gameModels, 0, MEM_RELEASE);
				Logger::Error("modelcache: the new gameModels is out of reach of 0x{:X}, not moved, so a model past 511 reads out of bounds\n", gameModelsSites[i].address);
				return;
			}
		}

		gameModelsReallocated = gameModels;

		if (!clearCgsHook.Initialize(CG_Init_ClearCgsCall, reinterpret_cast<void*>(CG_Init_Memset_Hook), HOOK_CALL)->Install()->IsInstalled())
		{
			gameModelsReallocated = nullptr;
			VirtualFree(gameModels, 0, MEM_RELEASE);
			Logger::Error("modelcache: could not hook CG_Init's memset of cgs, gameModels not moved, so a model past 511 reads out of bounds\n");
			return;
		}

		clearCgsHook.Quick();

		for (std::size_t i = 0; i < std::size(gameModelsSites); ++i)
		{
			const ModelArraySite& site = gameModelsSites[i];
			Utils::Hook::Set<std::int32_t>(reinterpret_cast<void*>(Utils::Hook::Rebase(site.address) + site.operandOffset), operands[i]);
		}
	}

	constexpr std::uintptr_t G_ClearCachedModels_MemsetSize = 0x1401AA2F9;
	constexpr std::uintptr_t G_ModelIndex_LimitCompares[] = { 0x1401AC06C, 0x1401AC082 };
	constexpr std::uintptr_t cached_models = 0x1419F2FC0;

	static const ModelArraySite cachedModelsSites[] =
	{
		{ 0x1401AA2F2, 7, 3, ModelArraySiteKind::ArrayAddress, { 0x48, 0x8D, 0x0D, 0xC7, 0x8C, 0x84, 0x01 } },
		{ 0x1401AACC2, 8, 4, ModelArraySiteKind::ArrayFromImage, { 0x49, 0x8B, 0x84, 0xD1, 0xC0, 0x2F, 0x9F, 0x01 } },
		{ 0x1401AAD20, 8, 4, ModelArraySiteKind::ArrayFromImage, { 0x49, 0x8B, 0x84, 0xC1, 0xC0, 0x2F, 0x9F, 0x01 } },
		{ 0x1401AB1F4, 7, 3, ModelArraySiteKind::ArrayAddress, { 0x48, 0x8D, 0x05, 0xC5, 0x7D, 0x84, 0x01 } },
		{ 0x1401ABCAD, 7, 3, ModelArraySiteKind::ArrayAddress, { 0x48, 0x8D, 0x1D, 0x0C, 0x73, 0x84, 0x01 } },
		{ 0x1401ABCF3, 7, 3, ModelArraySiteKind::ArrayAddress, { 0x48, 0x8D, 0x0D, 0xC6, 0x72, 0x84, 0x01 } },
		{ 0x1401AC0A6, 7, 3, ModelArraySiteKind::ArrayAddress, { 0x48, 0x8D, 0x15, 0x13, 0x6F, 0x84, 0x01 } },
		{ 0x1401AC194, 7, 3, ModelArraySiteKind::FirstModel, { 0x48, 0x8D, 0x1D, 0x2D, 0x6E, 0x84, 0x01 } },
		{ 0x1401AC19B, 7, 3, ModelArraySiteKind::ModelsEnd, { 0x48, 0x8D, 0x35, 0x1E, 0x7E, 0x84, 0x01 } },
		{ 0x1401AC9F3, 7, 3, ModelArraySiteKind::ArrayAddress, { 0x48, 0x8D, 0x05, 0xC6, 0x65, 0x84, 0x01 } },
	};

	static const std::uint8_t memsetSize[] = { 0x41, 0xB8, 0x00, 0x10, 0x00, 0x00 };
	static const std::uint8_t limitCompare[] = { 0x81, 0xFB, 0x00, 0x02, 0x00, 0x00 };

	void ModelCache::RelocateCachedModels()
	{
		if (!ConfigStrings::HasServerModelStrings())
		{
			Logger::Error("modelcache: the server model strings are not in place, so the server keeps 512 models\n");
			return;
		}

		bool isExpected = Utils::Hook::MatchesBytes(G_ClearCachedModels_MemsetSize, memsetSize, sizeof(memsetSize));

		for (const std::uintptr_t compare : G_ModelIndex_LimitCompares)
		{
			isExpected = isExpected && Utils::Hook::MatchesBytes(compare, limitCompare, sizeof(limitCompare));
		}

		for (const ModelArraySite& site : cachedModelsSites)
		{
			isExpected = isExpected && Utils::Hook::MatchesBytes(site.address, site.bytes, site.length);
		}

		if (!isExpected)
		{
			Logger::Error("modelcache: a cached_models site does not read as expected, so the server keeps 512 models\n");
			return;
		}

		auto** const cachedModels = static_cast<Game::XModel**>(Utils::Hook::AllocateDataNear(G_ClearCachedModels_MemsetSize, G_MODELINDEX_LIMIT * sizeof(Game::XModel*)));

		if (!cachedModels)
		{
			Logger::Error("modelcache: no memory free within reach of the image, so the server keeps 512 models\n");
			return;
		}

		std::int32_t operands[std::size(cachedModelsSites)]{};

		for (std::size_t i = 0; i < std::size(cachedModelsSites); ++i)
		{
			if (!TryEncodeOperand(cachedModelsSites[i], cachedModels, operands[i]))
			{
				VirtualFree(cachedModels, 0, MEM_RELEASE);
				Logger::Error("modelcache: the new cached_models is out of reach of 0x{:X}, so the server keeps 512 models\n", cachedModelsSites[i].address);
				return;
			}
		}

		std::memcpy(cachedModels, reinterpret_cast<const void*>(Utils::Hook::Rebase(cached_models)), BASE_GMODEL_COUNT * sizeof(Game::XModel*));

		for (std::size_t i = 0; i < std::size(cachedModelsSites); ++i)
		{
			const ModelArraySite& site = cachedModelsSites[i];
			Utils::Hook::Set<std::int32_t>(reinterpret_cast<void*>(Utils::Hook::Rebase(site.address) + site.operandOffset), operands[i]);
		}

		Utils::Hook::Set<std::int32_t>(reinterpret_cast<void*>(Utils::Hook::Rebase(G_ClearCachedModels_MemsetSize) + 2), static_cast<std::int32_t>(G_MODELINDEX_LIMIT * sizeof(Game::XModel*)));

		for (const std::uintptr_t compare : G_ModelIndex_LimitCompares)
		{
			Utils::Hook::Set<std::int32_t>(reinterpret_cast<void*>(Utils::Hook::Rebase(compare) + 2), SERVER_MODEL_LIMIT);
		}

		cachedModelsReallocated = cachedModels;
	}

	ModelCache::ModelCache()
	{
		if (Flags::HasFlag("steamdemo") || Flags::HasFlag("retaildemo") || Flags::HasFlag("iw4x_legacydemo"))
		{
			return;
		}

		WidenModelIndexFields();
		RelocateGameModels();
		RelocateCachedModels();
	}
}
