#include "STDInclude.hpp"

#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>

#include <Utils/JSON.hpp>

#include "IMaterialTechniqueSet.hpp"
#include "../Command.hpp"
#include "../FileSystem.hpp"
#include "../Logger.hpp"

namespace Assets
{
	constexpr int IW4X_TECHSET_VERSION = 1;

	template <typename T>
	static bool TryRead(const rapidjson::Value& object, const char* member, T& out)
	{
		if (!object.IsObject())
		{
			return false;
		}

		const auto found = object.FindMember(member);

		if (found == object.MemberEnd())
		{
			return false;
		}

		const auto& value = found->value;

		if constexpr (std::is_same_v<T, bool>)
		{
			if (!value.IsBool())
			{
				return false;
			}

			out = value.GetBool();
		}
		else if constexpr (std::is_same_v<T, std::string>)
		{
			if (!value.IsString())
			{
				return false;
			}

			out.assign(value.GetString(), value.GetStringLength());
		}
		else if constexpr (std::is_floating_point_v<T>)
		{
			if (!value.IsNumber())
			{
				return false;
			}

			out = static_cast<T>(value.GetDouble());
		}
		else if (value.IsInt64())
		{
			out = static_cast<T>(value.GetInt64());
		}
		else if (value.IsUint64())
		{
			out = static_cast<T>(value.GetUint64());
		}
		else
		{
			return false;
		}

		return true;
	}

	static bool TryReadLiterals(const rapidjson::Value& object, float* literals)
	{
		const auto found = object.FindMember("literals");

		if (found == object.MemberEnd() || !found->value.IsArray() || found->value.Size() < 4)
		{
			return false;
		}

		for (rapidjson::SizeType i = 0; i < 4; ++i)
		{
			if (!found->value[i].IsNumber())
			{
				return false;
			}

			literals[i] = static_cast<float>(found->value[i].GetDouble());
		}

		return true;
	}

	static bool TryReadArgument(const rapidjson::Value& json, Game::MaterialShaderArgument* argument, Utils::Memory::Allocator* allocator)
	{
		std::uint16_t type = 0;

		if (!TryRead(json, "type", type) || !TryRead(json, "dest", argument->dest))
		{
			return false;
		}

		argument->type = static_cast<Game::MaterialShaderArgumentType>(type);

		switch (argument->type)
		{
		case Game::MTL_ARG_LITERAL_VERTEX_CONST:
		case Game::MTL_ARG_LITERAL_PIXEL_CONST:
		{
			auto* const literals = allocator->AllocateArray<float>(4);

			if (!TryReadLiterals(json, literals))
			{
				return false;
			}

			argument->u.literalConst = literals;
			break;
		}

		case Game::MTL_ARG_CODE_VERTEX_CONST:
		case Game::MTL_ARG_CODE_PIXEL_CONST:
		{
			const auto codeConst = json.FindMember("codeConst");

			if (codeConst != json.MemberEnd() && codeConst->value.IsObject())
			{
				const auto& value = codeConst->value;

				if (!TryRead(value, "index", argument->u.codeConst.index)
					|| !TryRead(value, "firstRow", argument->u.codeConst.firstRow)
					|| !TryRead(value, "rowCount", argument->u.codeConst.rowCount))
				{
					return false;
				}
			}

			break;
		}

		case Game::MTL_ARG_MATERIAL_PIXEL_SAMPLER:
		case Game::MTL_ARG_MATERIAL_VERTEX_CONST:
		case Game::MTL_ARG_MATERIAL_PIXEL_CONST:
			return TryRead(json, "nameHash", argument->u.nameHash);

		case Game::MTL_ARG_CODE_PIXEL_SAMPLER:
			return TryRead(json, "codeSampler", argument->u.codeSampler);

		default:
			break;
		}

		return true;
	}

	static bool TryReadPass(const rapidjson::Value& json, Game::MaterialPass* pass, Components::ZoneBuilder::Zone* builder)
	{
		std::string assetName;

		if (TryRead(json, "vertexDeclaration", assetName))
		{
			pass->vertexDecl = Components::AssetHandler::FindAssetForZone(Game::ASSET_TYPE_VERTEXDECL, assetName, builder).vertexDecl;
		}

		if (TryRead(json, "vertexShader", assetName))
		{
			pass->vertexShader = Components::AssetHandler::FindAssetForZone(Game::ASSET_TYPE_VERTEXSHADER, assetName, builder).vertexShader;
		}

		if (TryRead(json, "pixelShader", assetName))
		{
			pass->pixelShader = Components::AssetHandler::FindAssetForZone(Game::ASSET_TYPE_PIXELSHADER, assetName, builder).pixelShader;
		}

		if (!TryRead(json, "perPrimArgCount", pass->perPrimArgCount)
			|| !TryRead(json, "perObjArgCount", pass->perObjArgCount)
			|| !TryRead(json, "stableArgCount", pass->stableArgCount)
			|| !TryRead(json, "customSamplerFlags", pass->customSamplerFlags))
		{
			return false;
		}

		const auto arguments = json.FindMember("arguments");

		if (arguments == json.MemberEnd() || !arguments->value.IsArray())
		{
			return true;
		}

		const auto argumentCount = pass->perPrimArgCount + pass->perObjArgCount + pass->stableArgCount;

		if (static_cast<int>(arguments->value.Size()) != argumentCount)
		{
			return false;
		}

		auto* const allocator = builder->GetAllocator();
		pass->args = allocator->AllocateArray<Game::MaterialShaderArgument>(arguments->value.Size());

		for (rapidjson::SizeType i = 0; i < arguments->value.Size(); ++i)
		{
			if (!TryReadArgument(arguments->value[i], &pass->args[i], allocator))
			{
				return false;
			}
		}

		return true;
	}

	static Game::MaterialTechnique* ReadTechnique(const std::string& name, Components::ZoneBuilder::Zone* builder)
	{
		Components::FileSystem::File techniqueFile(std::format("techniques/{}.iw4x.json", name));

		if (!techniqueFile.Exists())
		{
			Components::Logger::Fatal("Missing technique '{}'", name);
		}

		rapidjson::Document technique;
		technique.Parse<rapidjson::kParseNanAndInfFlag>(techniqueFile.GetBuffer().data(), techniqueFile.GetBuffer().size());

		if (technique.HasParseError() || !technique.IsObject())
		{
			Components::Logger::Fatal("Reading technique '{}' failed, file is messed up!", name);
		}

		int version = 0;

		if (!TryRead(technique, "version", version) || version != IW4X_TECHSET_VERSION)
		{
			Components::Logger::Fatal("Reading technique '{}' failed, expected version is {}, but it was {}!", name, IW4X_TECHSET_VERSION, version);
		}

		std::string flagsText;
		unsigned long flags = 0;

		if (!TryRead(technique, "flags", flagsText) || !Utils::JSON::TryReadFlags(flagsText, sizeof(std::uint16_t), flags))
		{
			Components::Logger::Fatal("Reading technique '{}' failed, its flags are broken!", name);
		}

		const auto passArray = technique.FindMember("passArray");

		if (passArray == technique.MemberEnd() || !passArray->value.IsArray())
		{
			return nullptr;
		}

		const auto passCount = passArray->value.Size();
		const auto size = sizeof(Game::MaterialTechnique) + sizeof(Game::MaterialPass) * (std::max(passCount, 1u) - 1);

		auto* const allocator = builder->GetAllocator();
		auto* const asset = static_cast<Game::MaterialTechnique*>(allocator->Allocate(size));
		asset->name = allocator->DuplicateString(name);
		asset->flags = static_cast<std::uint16_t>(flags);
		asset->passCount = static_cast<std::uint16_t>(passCount);

		for (rapidjson::SizeType i = 0; i < passCount; ++i)
		{
			if (!TryReadPass(passArray->value[i], &asset->passArray[i], builder))
			{
				Components::Logger::Fatal("Reading technique '{}' failed, pass {} is broken!", name, i);
			}
		}

		return asset;
	}

	static void WriteJson(const rapidjson::Document& document, const std::string& path)
	{
		rapidjson::StringBuffer output;
		rapidjson::PrettyWriter<rapidjson::StringBuffer, rapidjson::UTF8<>, rapidjson::UTF8<>, rapidjson::CrtAllocator, rapidjson::kWriteNanAndInfFlag> writer(output);
		document.Accept(writer);

		Utils::IO::WriteFile(path, output.GetString());
	}

	static bool TryWriteTechnique(const Game::MaterialTechnique* technique)
	{
		rapidjson::Document output(rapidjson::kObjectType);
		auto& allocator = output.GetAllocator();

		output.AddMember("version", IW4X_TECHSET_VERSION, allocator);
		output.AddMember("name", rapidjson::Value(technique->name, allocator), allocator);

		rapidjson::Value passArray(rapidjson::kArrayType);

		for (int i = 0; i < technique->passCount; ++i)
		{
			const auto* const pass = &technique->passArray[i];
			rapidjson::Value jsonPass(rapidjson::kObjectType);

			if (pass->vertexDecl)
			{
				jsonPass.AddMember("vertexDeclaration", rapidjson::Value(pass->vertexDecl->name, allocator), allocator);
				Components::AssetHandler::DumpAsset({ Game::ASSET_TYPE_VERTEXDECL, pass->vertexDecl });
			}

			if (pass->vertexShader)
			{
				jsonPass.AddMember("vertexShader", rapidjson::Value(pass->vertexShader->name, allocator), allocator);
				Components::AssetHandler::DumpAsset({ Game::ASSET_TYPE_VERTEXSHADER, pass->vertexShader });
			}

			if (pass->pixelShader)
			{
				jsonPass.AddMember("pixelShader", rapidjson::Value(pass->pixelShader->name, allocator), allocator);
				Components::AssetHandler::DumpAsset({ Game::ASSET_TYPE_PIXELSHADER, pass->pixelShader });
			}

			jsonPass.AddMember("perPrimArgCount", static_cast<int>(pass->perPrimArgCount), allocator);
			jsonPass.AddMember("perObjArgCount", static_cast<int>(pass->perObjArgCount), allocator);
			jsonPass.AddMember("stableArgCount", static_cast<int>(pass->stableArgCount), allocator);
			jsonPass.AddMember("customSamplerFlags", static_cast<int>(pass->customSamplerFlags), allocator);

			rapidjson::Value arguments(rapidjson::kArrayType);
			const auto argumentCount = pass->perPrimArgCount + pass->perObjArgCount + pass->stableArgCount;

			for (int j = 0; pass->args && j < argumentCount; ++j)
			{
				const auto* const argument = &pass->args[j];
				rapidjson::Value jsonArgument(rapidjson::kObjectType);

				jsonArgument.AddMember("type", static_cast<unsigned int>(argument->type), allocator);
				jsonArgument.AddMember("dest", static_cast<unsigned int>(argument->dest), allocator);

				switch (argument->type)
				{
				case Game::MTL_ARG_LITERAL_VERTEX_CONST:
				case Game::MTL_ARG_LITERAL_PIXEL_CONST:
					jsonArgument.AddMember("literals", Utils::JSON::MakeArray(argument->u.literalConst, 4, allocator), allocator);
					break;

				case Game::MTL_ARG_CODE_VERTEX_CONST:
				case Game::MTL_ARG_CODE_PIXEL_CONST:
				{
					rapidjson::Value codeConst(rapidjson::kObjectType);
					codeConst.AddMember("index", static_cast<unsigned int>(argument->u.codeConst.index), allocator);
					codeConst.AddMember("firstRow", static_cast<int>(argument->u.codeConst.firstRow), allocator);
					codeConst.AddMember("rowCount", static_cast<int>(argument->u.codeConst.rowCount), allocator);
					jsonArgument.AddMember("codeConst", codeConst, allocator);
					break;
				}

				case Game::MTL_ARG_MATERIAL_PIXEL_SAMPLER:
				case Game::MTL_ARG_MATERIAL_VERTEX_CONST:
				case Game::MTL_ARG_MATERIAL_PIXEL_CONST:
					jsonArgument.AddMember("nameHash", argument->u.nameHash, allocator);
					break;

				case Game::MTL_ARG_CODE_PIXEL_SAMPLER:
					jsonArgument.AddMember("codeSampler", argument->u.codeSampler, allocator);
					break;

				default:
					Components::Logger::Fatal("Unknown arg type {} in {}!", static_cast<unsigned int>(argument->type), technique->name);
				}

				arguments.PushBack(jsonArgument, allocator);
			}

			jsonPass.AddMember("arguments", arguments, allocator);
			passArray.PushBack(jsonPass, allocator);
		}

		output.AddMember("flags", rapidjson::Value(std::format("{:016b}", technique->flags).data(), allocator), allocator);
		output.AddMember("passArray", passArray, allocator);

		WriteJson(output, std::format("{}/techniques/{}.iw4x.json", Components::ZoneBuilder::GetDumpingZonePath(), technique->name));
		return true;
	}

	static void WriteTechniqueSet(const Game::MaterialTechniqueSet* techset)
	{
		rapidjson::Document output(rapidjson::kObjectType);
		auto& allocator = output.GetAllocator();

		output.AddMember("version", IW4X_TECHSET_VERSION, allocator);

		if (techset->name)
		{
			output.AddMember("name", rapidjson::Value(techset->name, allocator), allocator);
		}

		const auto* const remapped = techset->remappedTechniqueSet;

		if (remapped && remapped != techset && remapped->name)
		{
			output.AddMember("remappedTechniqueSet", rapidjson::Value(remapped->name, allocator), allocator);
			WriteTechniqueSet(remapped);
		}

		output.AddMember("hasBeenUploaded", techset->hasBeenUploaded, allocator);
		output.AddMember("worldVertFormat", static_cast<int>(techset->worldVertFormat), allocator);

		rapidjson::Value techniqueMap(rapidjson::kObjectType);

		for (std::size_t i = 0; i < std::size(techset->techniques); ++i)
		{
			rapidjson::Value value(rapidjson::kNullType);
			const auto* const technique = techset->techniques[i];

			if (technique)
			{
				if (TryWriteTechnique(technique))
				{
					value = rapidjson::Value(technique->name, allocator);
				}
				else
				{
					Components::Logger::Fatal("Could not export technique {}", technique->name);
				}
			}

			techniqueMap.AddMember(rapidjson::Value(std::to_string(i).data(), allocator), value, allocator);
		}

		output.AddMember("techniques", techniqueMap, allocator);

		WriteJson(output, std::format("{}/techsets/{}.iw4x.json", Components::ZoneBuilder::GetDumpingZonePath(), techset->name));
	}

	void IMaterialTechniqueSet::Load(Game::XAssetHeader* header, const std::string& name, Components::ZoneBuilder::Zone* builder)
	{
		if (!header->data)
		{
			this->LoadFromDisk(header, name, builder);
		}

		if (!header->data)
		{
			this->LoadNative(header, name, builder);
		}
	}

	void IMaterialTechniqueSet::LoadNative(Game::XAssetHeader* header, const std::string& name, [[maybe_unused]] Components::ZoneBuilder::Zone* builder)
	{
		header->techniqueSet = Components::AssetHandler::FindLoadedAsset(this->GetType(), name.data()).techniqueSet;
	}

	void IMaterialTechniqueSet::LoadFromDisk(Game::XAssetHeader* header, const std::string& name, Components::ZoneBuilder::Zone* builder)
	{
		Components::FileSystem::File techsetFile(std::format("techsets/{}.iw4x.json", name));

		if (!techsetFile.Exists())
		{
			return;
		}

		rapidjson::Document techset;
		techset.Parse<rapidjson::kParseNanAndInfFlag>(techsetFile.GetBuffer().data(), techsetFile.GetBuffer().size());

		if (techset.HasParseError() || !techset.IsObject())
		{
			Components::Logger::Fatal("Reading techset '{}' failed, file is messed up!", name);
		}

		int version = 0;

		if (!TryRead(techset, "version", version) || version != IW4X_TECHSET_VERSION)
		{
			Components::Logger::Fatal("Reading techset '{}' failed, expected version is {}, but it was {}!", name, IW4X_TECHSET_VERSION, version);
		}

		auto* const allocator = builder->GetAllocator();
		auto* const asset = allocator->Allocate<Game::MaterialTechniqueSet>();
		std::string text;

		if (TryRead(techset, "name", text))
		{
			asset->name = allocator->DuplicateString(text);
		}

		if (!TryRead(techset, "hasBeenUploaded", asset->hasBeenUploaded) || !TryRead(techset, "worldVertFormat", asset->worldVertFormat))
		{
			Components::Logger::Fatal("Reading techset '{}' failed, file is messed up!", name);
		}

		if (TryRead(techset, "remappedTechniqueSet", text) && (!asset->name || text != asset->name))
		{
			asset->remappedTechniqueSet = Components::AssetHandler::FindAssetForZone(Game::ASSET_TYPE_TECHNIQUE_SET, text, builder).techniqueSet;
		}

		const auto techniques = techset.FindMember("techniques");

		if (techniques != techset.MemberEnd() && techniques->value.IsObject())
		{
			for (std::size_t i = 0; i < std::size(asset->techniques); ++i)
			{
				if (TryRead(techniques->value, std::to_string(i).data(), text))
				{
					asset->techniques[i] = ReadTechnique(text, builder);
				}
			}
		}

		header->techniqueSet = asset;

		auto* remapped = asset;

		while (remapped->remappedTechniqueSet && remapped->remappedTechniqueSet != remapped)
		{
			remapped = remapped->remappedTechniqueSet;
			builder->LoadAsset(Game::ASSET_TYPE_TECHNIQUE_SET, remapped, false);

			for (const auto* const technique : remapped->techniques)
			{
				if (!technique)
				{
					continue;
				}

				for (std::uint16_t j = 0; j < technique->passCount; ++j)
				{
					const auto* const pass = &technique->passArray[j];

					if (pass->vertexDecl)
					{
						builder->LoadAsset(Game::ASSET_TYPE_VERTEXDECL, pass->vertexDecl, true);
					}

					if (pass->pixelShader)
					{
						builder->LoadAsset(Game::ASSET_TYPE_PIXELSHADER, pass->pixelShader, true);
					}

					if (pass->vertexShader)
					{
						builder->LoadAsset(Game::ASSET_TYPE_VERTEXSHADER, pass->vertexShader, true);
					}
				}
			}
		}
	}

	void IMaterialTechniqueSet::Mark(Game::XAssetHeader header, Components::ZoneBuilder::Zone* builder)
	{
		const auto* const asset = header.techniqueSet;

		for (const auto* const technique : asset->techniques)
		{
			if (!technique)
			{
				continue;
			}

			for (std::uint16_t j = 0; j < technique->passCount; ++j)
			{
				const auto* const pass = &technique->passArray[j];

				if (pass->vertexDecl)
				{
					builder->LoadAsset(Game::ASSET_TYPE_VERTEXDECL, pass->vertexDecl);
				}

				if (pass->vertexShader)
				{
					builder->LoadAsset(Game::ASSET_TYPE_VERTEXSHADER, pass->vertexShader);
				}

				if (pass->pixelShader)
				{
					builder->LoadAsset(Game::ASSET_TYPE_PIXELSHADER, pass->pixelShader);
				}
			}
		}
	}

	void IMaterialTechniqueSet::Save(Game::XAssetHeader header, Components::ZoneBuilder::Zone* builder)
	{
		auto* const buffer = builder->GetBuffer();
		const auto* const asset = header.techniqueSet;
		auto* const dest = buffer->Dest<Game::X86::MaterialTechniqueSet>();
		const auto record = Game::X86::Convert(*asset);
		buffer->Save(&record);

		buffer->PushBlock(Game::XFILE_BLOCK_VIRTUAL);

		if (asset->name)
		{
			buffer->SaveString(builder->GetAssetName(this->GetType(), asset->name));
			Utils::Stream::ClearPointer(&dest->name);
		}

		for (std::size_t i = 0; i < std::size(asset->techniques); ++i)
		{
			const auto* const technique = asset->techniques[i];

			if (!technique)
			{
				continue;
			}

			if (builder->HasPointer(technique))
			{
				dest->techniques[i] = builder->GetPointer(technique);
				continue;
			}

			buffer->Align(Utils::Stream::ALIGN_4);
			builder->StorePointer(technique);

			auto* const destTechnique = buffer->Dest<Game::X86::MaterialTechnique>();
			const auto techniqueRecord = Game::X86::Convert(*technique);
			buffer->Save(&techniqueRecord, offsetof(Game::X86::MaterialTechnique, passArray));

			auto* const destPasses = buffer->Dest<Game::X86::MaterialPass>();

			for (std::uint16_t j = 0; j < technique->passCount; ++j)
			{
				const auto passRecord = Game::X86::Convert(technique->passArray[j]);
				buffer->Save(&passRecord);
			}

			for (std::uint16_t j = 0; j < technique->passCount; ++j)
			{
				auto* const destPass = &destPasses[j];
				const auto* const pass = &technique->passArray[j];

				if (pass->vertexDecl)
				{
					destPass->vertexDecl = builder->SaveSubAsset(Game::ASSET_TYPE_VERTEXDECL, pass->vertexDecl);
				}

				if (pass->vertexShader)
				{
					destPass->vertexShader = builder->SaveSubAsset(Game::ASSET_TYPE_VERTEXSHADER, pass->vertexShader);
				}

				if (pass->pixelShader)
				{
					destPass->pixelShader = builder->SaveSubAsset(Game::ASSET_TYPE_PIXELSHADER, pass->pixelShader);
				}

				if (!pass->args)
				{
					continue;
				}

				buffer->Align(Utils::Stream::ALIGN_4);

				const auto argumentCount = pass->perPrimArgCount + pass->perObjArgCount + pass->stableArgCount;
				auto* const destArgs = buffer->Dest<Game::X86::MaterialShaderArgument>();

				for (int k = 0; k < argumentCount; ++k)
				{
					const auto argumentRecord = Game::X86::Convert(pass->args[k]);
					buffer->Save(&argumentRecord);
				}

				for (int k = 0; k < argumentCount; ++k)
				{
					const auto* const argument = &pass->args[k];
					auto* const destArg = &destArgs[k];

					if (argument->type != Game::MTL_ARG_LITERAL_VERTEX_CONST && argument->type != Game::MTL_ARG_LITERAL_PIXEL_CONST)
					{
						continue;
					}

					if (!argument->u.literalConst)
					{
						destArg->u.literalConst = 0;
						continue;
					}

					if (builder->HasPointer(argument->u.literalConst))
					{
						destArg->u.literalConst = builder->GetPointer(argument->u.literalConst);
						continue;
					}

					buffer->Align(Utils::Stream::ALIGN_4);
					builder->StorePointer(argument->u.literalConst);

					buffer->SaveArray(argument->u.literalConst, 4);
					Utils::Stream::ClearPointer(&destArg->u.literalConst);
				}

				Utils::Stream::ClearPointer(&destPass->args);
			}

			if (technique->name)
			{
				buffer->SaveString(technique->name);
				Utils::Stream::ClearPointer(&destTechnique->name);
			}

			Utils::Stream::ClearPointer(&dest->techniques[i]);
		}

		buffer->PopBlock();
	}

	void IMaterialTechniqueSet::Dump(Game::XAssetHeader header)
	{
		WriteTechniqueSet(header.techniqueSet);
	}

	IMaterialTechniqueSet::IMaterialTechniqueSet()
	{
		Components::Command::Add("dumptechset", [](const Components::Command::Params* params)
		{
			if (params->Size() < 2)
			{
				return;
			}

			const std::string techset = params->Get(1);

			if (!Game::DB_FindXAssetEntry(Game::ASSET_TYPE_TECHNIQUE_SET, techset.data()))
			{
				Components::Logger::Print("Could not find techset {}!\n", techset);
				return;
			}

			const Game::XAsset asset{ Game::ASSET_TYPE_TECHNIQUE_SET, Game::DB_FindXAssetHeader(Game::ASSET_TYPE_TECHNIQUE_SET, techset.data()) };
			Components::AssetHandler::DumpAsset(asset);
		});
	}
}
