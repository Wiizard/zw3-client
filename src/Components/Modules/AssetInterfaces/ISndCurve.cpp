#include "STDInclude.hpp"

#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>

#include <Utils/JSON.hpp>

#include "../AssetHandler.hpp"
#include "../FileSystem.hpp"
#include "../Logger.hpp"
#include "ISndCurve.hpp"

namespace Assets
{
	constexpr int sndCurveVersion = 1;
	constexpr std::size_t knotCapacity = 16;

	static std::string GetCurvePath(const std::string& name)
	{
		return std::format("sndcurve/{}.iw4x.json", name);
	}

	static Game::SndCurve* TryReadCurve(const std::string& name, Utils::Memory::Allocator* allocator)
	{
		Components::FileSystem::File file(GetCurvePath(name));

		if (!file.Exists())
		{
			return nullptr;
		}

		rapidjson::Document document;
		document.Parse(file.GetBuffer().data(), file.GetBuffer().size());

		if (document.HasParseError() || !document.IsObject())
		{
			Components::Logger::Error("Invalid JSON for sndcurve {}!\n", name);
			return nullptr;
		}

		const bool isWellFormed = document.HasMember("filename") && document["filename"].IsString()
			&& document.HasMember("knotCount") && document["knotCount"].IsUint()
			&& document.HasMember("knots") && document["knots"].IsArray();

		if (!isWellFormed)
		{
			Components::Logger::Error("Malformed JSON for sndcurve {}!\n", name);
			return nullptr;
		}

		const auto& knots = document["knots"];
		const auto knotCount = document["knotCount"].GetUint();

		if (knotCount > knotCapacity || knots.Size() > knotCapacity)
		{
			Components::Logger::Error("Malformed JSON for sndcurve {}! it has more than {} knots\n", name, knotCapacity);
			return nullptr;
		}

		auto* const curve = allocator->Allocate<Game::SndCurve>();
		curve->filename = allocator->DuplicateString(document["filename"].GetString());
		curve->knotCount = static_cast<std::uint16_t>(knotCount);

		for (rapidjson::SizeType knot = 0; knot < knots.Size(); ++knot)
		{
			const auto& sides = knots[knot];

			if (!sides.IsArray() || sides.Size() < 2 || !sides[0].IsNumber() || !sides[1].IsNumber())
			{
				Components::Logger::Error("Malformed JSON for sndcurve {}! knot {} is not two numbers\n", name, knot);
				return nullptr;
			}

			curve->knots[knot][0] = sides[0].Get<float>();
			curve->knots[knot][1] = sides[1].Get<float>();
		}

		return curve;
	}

	void ISndCurve::Save(Game::XAssetHeader header, Components::ZoneBuilder::Zone* builder)
	{
		auto* const buffer = builder->GetBuffer();
		const auto* const asset = header.sndCurve;
		auto* const dest = buffer->Dest<Game::X86::SndCurve>();
		const auto converted = Game::X86::Convert(*asset);
		buffer->Save(&converted);

		buffer->PushBlock(Game::XFILE_BLOCK_VIRTUAL);

		if (asset->filename)
		{
			buffer->SaveString(builder->GetAssetName(this->GetType(), asset->filename));
			Utils::Stream::ClearPointer(&dest->filename);
		}

		buffer->PopBlock();
	}

	void ISndCurve::Dump(Game::XAssetHeader header)
	{
		const auto* const asset = header.sndCurve;

		if (!asset->filename)
		{
			return;
		}

		rapidjson::Document output(rapidjson::kObjectType);
		auto& allocator = output.GetAllocator();

		output.AddMember("version", sndCurveVersion, allocator);
		output.AddMember("filename", rapidjson::Value(asset->filename, allocator), allocator);
		output.AddMember("knotCount", asset->knotCount, allocator);

		rapidjson::Value knots(rapidjson::kArrayType);

		for (std::size_t knot = 0; knot < knotCapacity; ++knot)
		{
			knots.PushBack(Utils::JSON::MakeArray(asset->knots[knot], 2, allocator), allocator);
		}

		output.AddMember("knots", knots, allocator);

		rapidjson::StringBuffer text;
		rapidjson::PrettyWriter<rapidjson::StringBuffer, rapidjson::UTF8<>, rapidjson::UTF8<>, rapidjson::CrtAllocator, rapidjson::kWriteNanAndInfFlag> writer(text);
		writer.SetFormatOptions(rapidjson::PrettyFormatOptions::kFormatSingleLineArray);
		output.Accept(writer);

		Utils::IO::WriteFile(std::format("{}/{}", Components::ZoneBuilder::GetDumpingZonePath(), GetCurvePath(asset->filename)), text.GetString());
	}

	void ISndCurve::Load(Game::XAssetHeader* header, const std::string& name, Components::ZoneBuilder::Zone* builder)
	{
		header->sndCurve = TryReadCurve(name, builder->GetAllocator());

		if (header->sndCurve)
		{
			return;
		}

		header->sndCurve = Components::AssetHandler::FindLoadedAsset(this->GetType(), name.data()).sndCurve;
	}
}
