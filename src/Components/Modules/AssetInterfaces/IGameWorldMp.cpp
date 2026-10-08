#include "STDInclude.hpp"

#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>

#include <Utils/JSON.hpp>

#include "IGameWorldMp.hpp"

#include "Components/Modules/FileSystem.hpp"
#include "Components/Modules/Logger.hpp"

#define IW4X_GAMEWORLD_VERSION 1

namespace Assets
{
	static std::string GetFileName(const std::string& name)
	{
		std::string baseName = name;
		Utils::String::Replace(baseName, "maps/mp/", "");
		Utils::String::Replace(baseName, ".d3dbsp", "");

		return std::format("gameworld/{}.iw4x.json", baseName);
	}

	static bool TryReadGlassData(const rapidjson::Value& json, Game::G_GlassData* glassData, Utils::Memory::Allocator* allocator)
	{
		glassData->damageToDestroy = json["damageToDestroy"].Get<std::uint16_t>();
		glassData->damageToWeaken = json["damageToWeaken"].Get<std::uint16_t>();

		if (json.HasMember("glassNames") && json["glassNames"].IsArray())
		{
			const auto& glassNames = json["glassNames"];
			glassData->glassNameCount = glassNames.Size();
			glassData->glassNames = allocator->AllocateArray<Game::G_GlassName>(glassData->glassNameCount);

			for (unsigned int i = 0; i < glassData->glassNameCount; ++i)
			{
				const auto& jsonGlassName = glassNames[i];
				auto* const glassName = &glassData->glassNames[i];

				if (!jsonGlassName.IsObject() || !jsonGlassName.HasMember("nameStr") || !jsonGlassName["nameStr"].IsString())
				{
					return false;
				}

				glassName->nameStr = allocator->DuplicateString(jsonGlassName["nameStr"].GetString());
				glassName->name = jsonGlassName["name"].Get<std::uint16_t>();

				if (jsonGlassName.HasMember("piecesIndices") && jsonGlassName["piecesIndices"].IsArray())
				{
					const auto& jsonPiecesIndices = jsonGlassName["piecesIndices"];
					glassName->pieceCount = static_cast<std::uint16_t>(jsonPiecesIndices.Size());
					glassName->pieceIndices = allocator->AllocateArray<std::uint16_t>(glassName->pieceCount);
					Utils::JSON::CopyArray(glassName->pieceIndices, jsonPiecesIndices, glassName->pieceCount);
				}
			}
		}

		if (json.HasMember("glassPieces") && json["glassPieces"].IsArray())
		{
			const auto& glassPieces = json["glassPieces"];
			glassData->pieceCount = glassPieces.Size();
			glassData->glassPieces = allocator->AllocateArray<Game::G_GlassPiece>(glassData->pieceCount);

			for (unsigned int i = 0; i < glassData->pieceCount; ++i)
			{
				const auto& jsonPiece = glassPieces[i];
				auto* const piece = &glassData->glassPieces[i];

				if (!jsonPiece.IsObject() || !jsonPiece.HasMember("impactPos") || !jsonPiece["impactPos"].IsArray() || jsonPiece["impactPos"].Size() < 2)
				{
					return false;
				}

				piece->collapseTime = jsonPiece["collapseTime"].Get<std::uint16_t>();
				piece->damageTaken = jsonPiece["damageTaken"].Get<std::uint16_t>();
				piece->lastStateChangeTime = jsonPiece["lastStateChangeTime"].Get<std::int32_t>();
				piece->impactDir = jsonPiece["impactDir"].Get<char>();
				piece->impactPos[0] = jsonPiece["impactPos"][0].Get<char>();
				piece->impactPos[1] = jsonPiece["impactPos"][1].Get<char>();
			}
		}

		return true;
	}

	static Game::GameWorldMp* TryReadGameWorldMp(const std::string& name, Components::ZoneBuilder::Zone* builder)
	{
		Components::FileSystem::File file(GetFileName(name));

		if (!file.Exists())
		{
			return nullptr;
		}

		rapidjson::Document json;
		json.Parse(file.GetBuffer().data());

		if (json.HasParseError() || !json.IsObject())
		{
			Components::Logger::Error("Invalid GameWorldMp json for {}\n", name);
			return nullptr;
		}

		std::int32_t version = 0;

		if (json.HasMember("version") && json["version"].IsNumber())
		{
			version = json["version"].Get<std::int32_t>();
		}

		if (version != IW4X_GAMEWORLD_VERSION)
		{
			Components::Logger::Error("Invalid GameWorld json version for {}, expected {} and got {}\n", name, IW4X_GAMEWORLD_VERSION, version);
			return nullptr;
		}

		if (!json.HasMember("name") || !json["name"].IsString())
		{
			Components::Logger::Error("Missing gameworld name! on {}\n", name);
			return nullptr;
		}

		auto* const allocator = builder->GetAllocator();

		auto* const asset = allocator->Allocate<Game::GameWorldMp>();
		asset->name = allocator->DuplicateString(json["name"].GetString());

		auto* const glassData = allocator->Allocate<Game::G_GlassData>();

		if (json.HasMember("glassData") && json["glassData"].IsObject() && !TryReadGlassData(json["glassData"], glassData, allocator))
		{
			Components::Logger::Error("Malformed GameWorldMp json for {}\n", name);
			return nullptr;
		}

		asset->g_glassData = glassData;
		return asset;
	}

	void IGameWorldMp::Load(Game::XAssetHeader* header, const std::string& name, Components::ZoneBuilder::Zone* builder)
	{
		header->gameWorldMp = TryReadGameWorldMp(name, builder);
	}

	void IGameWorldMp::Save(Game::XAssetHeader header, Components::ZoneBuilder::Zone* builder)
	{
		AssertSize(Game::X86::GameWorldMp, 8);

		auto* const buffer = builder->GetBuffer();
		const auto* const asset = header.gameWorldMp;
		auto* const dest = buffer->Dest<Game::X86::GameWorldMp>();

		const auto world = Game::X86::Convert(*asset);
		buffer->Save(&world);

		buffer->PushBlock(Game::XFILE_BLOCK_VIRTUAL);

		if (asset->name)
		{
			buffer->SaveString(builder->GetAssetName(this->GetType(), asset->name));
			Utils::Stream::ClearPointer(&dest->name);
		}

		if (asset->g_glassData)
		{
			buffer->Align(Utils::Stream::ALIGN_4);

			AssertSize(Game::X86::G_GlassData, 128);

			const auto* const glassData = asset->g_glassData;
			auto* const destGlass = buffer->Dest<Game::X86::G_GlassData>();

			const auto storedGlass = Game::X86::Convert(*glassData);
			buffer->Save(&storedGlass);

			if (glassData->glassPieces)
			{
				AssertSize(Game::X86::G_GlassPiece, 12);
				static_assert(sizeof(Game::G_GlassPiece) == sizeof(Game::X86::G_GlassPiece));

				buffer->Align(Utils::Stream::ALIGN_4);
				buffer->SaveArray(glassData->glassPieces, glassData->pieceCount);
				Utils::Stream::ClearPointer(&destGlass->glassPieces);
			}

			if (glassData->glassNames)
			{
				AssertSize(Game::X86::G_GlassName, 12);
				buffer->Align(Utils::Stream::ALIGN_4);

				auto* const destGlassNames = buffer->Dest<Game::X86::G_GlassName>();

				for (unsigned int i = 0; i < glassData->glassNameCount; ++i)
				{
					const auto glassName = Game::X86::Convert(glassData->glassNames[i]);
					buffer->Save(&glassName);
				}

				for (unsigned int i = 0; i < glassData->glassNameCount; ++i)
				{
					auto* const destGlassName = &destGlassNames[i];
					const auto* const glassName = &glassData->glassNames[i];

					if (glassName->nameStr)
					{
						buffer->SaveString(glassName->nameStr);
						Utils::Stream::ClearPointer(&destGlassName->nameStr);
					}

					if (glassName->pieceIndices)
					{
						buffer->Align(Utils::Stream::ALIGN_2);
						buffer->SaveArray(glassName->pieceIndices, glassName->pieceCount);
						Utils::Stream::ClearPointer(&destGlassName->pieceIndices);
					}
				}

				Utils::Stream::ClearPointer(&destGlass->glassNames);
			}

			Utils::Stream::ClearPointer(&dest->g_glassData);
		}

		buffer->PopBlock();
	}

	void IGameWorldMp::Dump(Game::XAssetHeader header)
	{
		const auto* const asset = header.gameWorldMp;

		if (!asset->name)
		{
			return;
		}

		rapidjson::Document output(rapidjson::kObjectType);
		auto& allocator = output.GetAllocator();

		output.AddMember("version", IW4X_GAMEWORLD_VERSION, allocator);
		output.AddMember("name", rapidjson::StringRef(asset->name), allocator);

		if (asset->g_glassData)
		{
			const auto* const glassData = asset->g_glassData;
			rapidjson::Value jsonGlassData(rapidjson::kObjectType);

			rapidjson::Value pieces(rapidjson::kArrayType);

			for (unsigned int i = 0; i < glassData->pieceCount; ++i)
			{
				const auto& glassPiece = glassData->glassPieces[i];
				rapidjson::Value piece(rapidjson::kObjectType);

				piece.AddMember("damageTaken", glassPiece.damageTaken, allocator);
				piece.AddMember("collapseTime", glassPiece.collapseTime, allocator);
				piece.AddMember("lastStateChangeTime", glassPiece.lastStateChangeTime, allocator);
				piece.AddMember("impactDir", glassPiece.impactDir, allocator);

				rapidjson::Value impactPos(rapidjson::kArrayType);
				impactPos.PushBack(glassPiece.impactPos[0], allocator);
				impactPos.PushBack(glassPiece.impactPos[1], allocator);

				piece.AddMember("impactPos", impactPos, allocator);
				pieces.PushBack(piece, allocator);
			}

			jsonGlassData.AddMember("glassPieces", pieces, allocator);
			jsonGlassData.AddMember("damageToWeaken", glassData->damageToWeaken, allocator);
			jsonGlassData.AddMember("damageToDestroy", glassData->damageToDestroy, allocator);

			rapidjson::Value glassNames(rapidjson::kArrayType);

			for (unsigned int i = 0; i < glassData->glassNameCount; ++i)
			{
				const auto& glassName = glassData->glassNames[i];
				rapidjson::Value jsonGlassName(rapidjson::kObjectType);

				if (glassName.nameStr)
				{
					jsonGlassName.AddMember("nameStr", rapidjson::StringRef(glassName.nameStr), allocator);
				}
				else
				{
					jsonGlassName.AddMember("nameStr", rapidjson::Value(rapidjson::kNullType), allocator);
				}

				jsonGlassName.AddMember("name", glassName.name, allocator);
				jsonGlassName.AddMember("piecesIndices", Utils::JSON::MakeArray(glassName.pieceIndices, glassName.pieceCount, allocator), allocator);

				glassNames.PushBack(jsonGlassName, allocator);
			}

			jsonGlassData.AddMember("glassNames", glassNames, allocator);
			output.AddMember("glassData", jsonGlassData, allocator);
		}

		rapidjson::StringBuffer stringBuffer;
		rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(stringBuffer);
		output.Accept(writer);

		const auto path = std::format("{}/{}", Components::ZoneBuilder::GetDumpingZonePath(), GetFileName(asset->name));

		if (!Utils::IO::WriteFile(path, stringBuffer.GetString()))
		{
			Components::Logger::Error("Dumping gameworld '{}' failed, could not write {}\n", asset->name, path);
		}
	}
}
