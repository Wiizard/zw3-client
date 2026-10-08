#include "STDInclude.hpp"

#include "Utils/JSON.hpp"

#include <rapidjson/prettywriter.h>

#include "Components/Modules/AssetHandler.hpp"
#include "Components/Modules/FileSystem.hpp"
#include "Components/Modules/Logger.hpp"

#include "IPhysPreset.hpp"

namespace Assets
{
	constexpr int physPresetVersion = 1;

	static std::unordered_set<std::string> dumpedPaths;

	static bool IsNumber(const rapidjson::Value& json, const char* key)
	{
		const auto member = json.FindMember(key);
		return member != json.MemberEnd() && member->value.IsNumber();
	}

	static bool IsInt(const rapidjson::Value& json, const char* key)
	{
		const auto member = json.FindMember(key);
		return member != json.MemberEnd() && member->value.IsInt();
	}

	static bool IsString(const rapidjson::Value& json, const char* key)
	{
		const auto member = json.FindMember(key);
		return member != json.MemberEnd() && member->value.IsString();
	}

	static bool IsBool(const rapidjson::Value& json, const char* key)
	{
		const auto member = json.FindMember(key);
		return member != json.MemberEnd() && member->value.IsBool();
	}

	static rapidjson::Value StringOrNull(const char* text)
	{
		if (!text)
		{
			return rapidjson::Value(rapidjson::kNullType);
		}

		return rapidjson::Value(rapidjson::StringRef(text));
	}

	void IPhysPreset::Save(Game::XAssetHeader header, Components::ZoneBuilder::Zone* builder)
	{
		AssertSize(Game::X86::PhysPreset, 44);

		auto* const buffer = builder->GetBuffer();
		const auto* const asset = header.physPreset;
		auto* const dest = buffer->Dest<Game::X86::PhysPreset>();
		const auto record = Game::X86::Convert(*asset);
		buffer->Save(&record);

		buffer->PushBlock(Game::XFILE_BLOCK_VIRTUAL);

		if (asset->name)
		{
			buffer->SaveString(builder->GetAssetName(this->GetType(), asset->name));
			Utils::Stream::ClearPointer(&dest->name);
		}

		if (asset->sndAliasPrefix)
		{
			buffer->SaveString(asset->sndAliasPrefix);
			Utils::Stream::ClearPointer(&dest->sndAliasPrefix);
		}

		buffer->PopBlock();
	}

	void IPhysPreset::Dump(Game::XAssetHeader header)
	{
		const auto* const asset = header.physPreset;
		const auto path = std::format("{}/physpreset/{}.iw4x.json", Components::ZoneBuilder::GetDumpingZonePath(), asset->name);

		if (!dumpedPaths.insert(path).second)
		{
			return;
		}

		rapidjson::Document output(rapidjson::kObjectType);
		auto& allocator = output.GetAllocator();

		output.AddMember("version", physPresetVersion, allocator);
		output.AddMember("name", StringOrNull(asset->name), allocator);
		output.AddMember("type", asset->type, allocator);
		output.AddMember("mass", asset->mass, allocator);
		output.AddMember("bounce", asset->bounce, allocator);
		output.AddMember("friction", asset->friction, allocator);
		output.AddMember("bulletForceScale", asset->bulletForceScale, allocator);
		output.AddMember("explosiveForceScale", asset->explosiveForceScale, allocator);
		output.AddMember("sndAliasPrefix", StringOrNull(asset->sndAliasPrefix), allocator);
		output.AddMember("piecesSpreadFraction", asset->piecesSpreadFraction, allocator);
		output.AddMember("piecesUpwardVelocity", asset->piecesUpwardVelocity, allocator);
		output.AddMember("tempDefaultToCylinder", asset->tempDefaultToCylinder, allocator);
		output.AddMember("perSurfaceSndAlias", asset->perSurfaceSndAlias, allocator);

		rapidjson::StringBuffer text;
		rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(text);
		output.Accept(writer);

		if (!Utils::IO::WriteFile(path, text.GetString()))
		{
			Components::Logger::Error("physpreset: could not write {}\n", path);
		}
	}

	void IPhysPreset::Load(Game::XAssetHeader* header, const std::string& name, Components::ZoneBuilder::Zone* builder)
	{
		this->LoadFromDisk(header, name, builder);
	}

	void IPhysPreset::LoadFromDisk(Game::XAssetHeader* header, const std::string& name, Components::ZoneBuilder::Zone* builder)
	{
		Components::FileSystem::File file(std::format("physpreset/{}.iw4x.json", name));

		if (!file.Exists())
		{
			return;
		}

		rapidjson::Document json;
		json.Parse(file.GetBuffer().data());

		if (json.HasParseError() || !json.IsObject())
		{
			Components::Logger::Error("physpreset: {} is not valid json\n", file.GetName());
			return;
		}

		const bool isComplete = IsString(json, "name")
			&& IsInt(json, "type")
			&& IsNumber(json, "mass")
			&& IsNumber(json, "bounce")
			&& IsNumber(json, "friction")
			&& IsNumber(json, "bulletForceScale")
			&& IsNumber(json, "explosiveForceScale")
			&& IsString(json, "sndAliasPrefix")
			&& IsNumber(json, "piecesSpreadFraction")
			&& IsNumber(json, "piecesUpwardVelocity")
			&& IsBool(json, "tempDefaultToCylinder")
			&& IsBool(json, "perSurfaceSndAlias");

		if (!isComplete)
		{
			Components::Logger::Error("physpreset: {} is missing a field or holds the wrong type\n", file.GetName());
			return;
		}

		auto* const allocator = builder->GetAllocator();
		auto* const asset = allocator->Allocate<Game::PhysPreset>();

		asset->name = allocator->DuplicateString(json["name"].GetString());
		asset->type = json["type"].GetInt();
		asset->bounce = json["bounce"].GetFloat();
		asset->mass = json["mass"].GetFloat();
		asset->friction = json["friction"].GetFloat();
		asset->bulletForceScale = json["bulletForceScale"].GetFloat();
		asset->explosiveForceScale = json["explosiveForceScale"].GetFloat();
		asset->sndAliasPrefix = allocator->DuplicateString(json["sndAliasPrefix"].GetString());
		asset->piecesSpreadFraction = json["piecesSpreadFraction"].GetFloat();
		asset->piecesUpwardVelocity = json["piecesUpwardVelocity"].GetFloat();
		asset->tempDefaultToCylinder = json["tempDefaultToCylinder"].GetBool();
		asset->perSurfaceSndAlias = json["perSurfaceSndAlias"].GetBool();

		header->physPreset = asset;
	}
}
