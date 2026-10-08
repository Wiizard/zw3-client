#include "STDInclude.hpp"

#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>

#include <Utils/JSON.hpp>

#include "../AssetHandler.hpp"
#include "../FileSystem.hpp"
#include "../Logger.hpp"
#include "ITracerDef.hpp"

namespace Assets
{
	constexpr int tracerDefVersion = 1;

	static std::string GetTracerPath(const std::string& name)
	{
		return std::format("tracers/{}.iw4x.json", name);
	}

	static bool IsTracerJson(const rapidjson::Document& document)
	{
		if (!document.IsObject())
		{
			return false;
		}

		if (!document.HasMember("drawInterval") || !document["drawInterval"].IsUint())
		{
			return false;
		}

		for (const char* number : { "speed", "beamLength", "beamWidth", "screwRadius", "screwDist" })
		{
			if (!document.HasMember(number) || !document[number].IsNumber())
			{
				return false;
			}
		}

		if (!document.HasMember("name") || !document["name"].IsString() || !document.HasMember("material"))
		{
			return false;
		}

		if (!document.HasMember("colors") || !document["colors"].IsArray() || document["colors"].Size() != 5)
		{
			return false;
		}

		for (const auto& color : document["colors"].GetArray())
		{
			if (!color.IsArray() || color.Size() != 4)
			{
				return false;
			}

			for (const auto& channel : color.GetArray())
			{
				if (!channel.IsNumber())
				{
					return false;
				}
			}
		}

		return true;
	}

	static Game::TracerDef* TryReadTracer(const std::string& name, Components::ZoneBuilder::Zone* builder)
	{
		Components::FileSystem::File file(GetTracerPath(name));

		if (!file.Exists())
		{
			return nullptr;
		}

		rapidjson::Document document;
		document.Parse(file.GetBuffer().data(), file.GetBuffer().size());

		if (document.HasParseError())
		{
			Components::Logger::Error("Invalid JSON for tracerdef {}!\n", name);
			return nullptr;
		}

		if (!IsTracerJson(document))
		{
			Components::Logger::Error("Malformed JSON for tracerdef {}!\n", name);
			return nullptr;
		}

		auto* const allocator = builder->GetAllocator();
		auto* const tracer = allocator->Allocate<Game::TracerDef>();

		tracer->name = allocator->DuplicateString(document["name"].GetString());
		tracer->drawInterval = document["drawInterval"].GetUint();
		tracer->speed = document["speed"].Get<float>();
		tracer->beamLength = document["beamLength"].Get<float>();
		tracer->beamWidth = document["beamWidth"].Get<float>();
		tracer->screwRadius = document["screwRadius"].Get<float>();
		tracer->screwDist = document["screwDist"].Get<float>();

		if (document["material"].IsString())
		{
			tracer->material = Components::AssetHandler::FindAssetForZone(Game::ASSET_TYPE_MATERIAL, document["material"].GetString(), builder).material;
		}

		for (rapidjson::SizeType i = 0; i < 5; ++i)
		{
			Utils::JSON::CopyArray(tracer->colors[i], document["colors"][i], 4);
		}

		return tracer;
	}

	void ITracerDef::Load(Game::XAssetHeader* header, const std::string& name, Components::ZoneBuilder::Zone* builder)
	{
		header->tracerDef = TryReadTracer(name, builder);
	}

	void ITracerDef::Mark(Game::XAssetHeader header, Components::ZoneBuilder::Zone* builder)
	{
		const auto* const asset = header.tracerDef;

		if (asset->material)
		{
			builder->LoadAsset(Game::ASSET_TYPE_MATERIAL, asset->material);
		}
	}

	void ITracerDef::Save(Game::XAssetHeader header, Components::ZoneBuilder::Zone* builder)
	{
		auto* const buffer = builder->GetBuffer();
		const auto* const asset = header.tracerDef;
		auto* const dest = buffer->Dest<Game::X86::TracerDef>();
		const auto converted = Game::X86::Convert(*asset);
		buffer->Save(&converted);

		buffer->PushBlock(Game::XFILE_BLOCK_VIRTUAL);

		if (asset->name)
		{
			buffer->SaveString(builder->GetAssetName(this->GetType(), asset->name));
			Utils::Stream::ClearPointer(&dest->name);
		}

		if (asset->material)
		{
			dest->material = builder->SaveSubAsset(Game::ASSET_TYPE_MATERIAL, asset->material);
		}

		buffer->PopBlock();
	}

	void ITracerDef::Dump(Game::XAssetHeader header)
	{
		const auto* const asset = header.tracerDef;

		if (!asset->name)
		{
			return;
		}

		rapidjson::Document output(rapidjson::kObjectType);
		auto& allocator = output.GetAllocator();

		output.AddMember("version", tracerDefVersion, allocator);
		output.AddMember("name", rapidjson::Value(asset->name, allocator), allocator);

		if (asset->material)
		{
			Components::AssetHandler::DumpAsset({ Game::ASSET_TYPE_MATERIAL, asset->material });
			output.AddMember("material", rapidjson::Value(asset->material->info.name, allocator), allocator);
		}
		else
		{
			output.AddMember("material", rapidjson::Value(rapidjson::kNullType), allocator);
		}

		output.AddMember("drawInterval", asset->drawInterval, allocator);
		output.AddMember("speed", asset->speed, allocator);
		output.AddMember("beamLength", asset->beamLength, allocator);
		output.AddMember("beamWidth", asset->beamWidth, allocator);
		output.AddMember("screwRadius", asset->screwRadius, allocator);
		output.AddMember("screwDist", asset->screwDist, allocator);

		rapidjson::Value colors(rapidjson::kArrayType);

		for (const auto& color : asset->colors)
		{
			colors.PushBack(Utils::JSON::MakeArray(color, 4, allocator), allocator);
		}

		output.AddMember("colors", colors, allocator);

		rapidjson::StringBuffer text;
		rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(text);
		output.Accept(writer);

		Utils::IO::WriteFile(std::format("{}/{}", Components::ZoneBuilder::GetDumpingZonePath(), GetTracerPath(asset->name)), text.GetString());
	}
}
