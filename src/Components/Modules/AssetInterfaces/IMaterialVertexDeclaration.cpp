#include "STDInclude.hpp"

#include "IMaterialVertexDeclaration.hpp"
#include "../FileSystem.hpp"
#include "../Logger.hpp"

namespace Assets
{
	constexpr auto declMagic = "IW4xDECL";
	constexpr char IW4X_TECHSET_VERSION = 1;

	void IMaterialVertexDeclaration::Load(Game::XAssetHeader* header, const std::string& name, Components::ZoneBuilder::Zone* builder)
	{
		if (!header->data)
		{
			this->LoadBinary(header, name, builder);
		}

		if (!header->data)
		{
			this->LoadNative(header, name, builder);
		}
	}

	void IMaterialVertexDeclaration::LoadNative(Game::XAssetHeader* header, const std::string& name, [[maybe_unused]] Components::ZoneBuilder::Zone* builder)
	{
		header->vertexDecl = Components::AssetHandler::FindLoadedAsset(this->GetType(), name.data()).vertexDecl;
	}

	void IMaterialVertexDeclaration::LoadBinary(Game::XAssetHeader* header, const std::string& name, Components::ZoneBuilder::Zone* builder)
	{
		Components::FileSystem::File declFile(std::format("decl/{}.iw4xDECL", name));

		if (!declFile.Exists())
		{
			return;
		}

		const auto& contents = declFile.GetBuffer();
		constexpr auto headerSize = 9 + sizeof(Game::X86::MaterialVertexDeclaration);

		if (contents.size() < headerSize || std::memcmp(contents.data(), declMagic, 8) != 0)
		{
			Components::Logger::Fatal("Reading vertex declaration '{}' failed, header is invalid!", name);
		}

		const auto version = contents[8];

		if (version > IW4X_TECHSET_VERSION)
		{
			Components::Logger::Fatal("Reading vertex declaration '{}' failed, expected version is {}, but it was {:d}!", name, static_cast<int>(IW4X_TECHSET_VERSION), version);
		}

		Game::X86::MaterialVertexDeclaration record{};
		std::memcpy(&record, contents.data() + 9, sizeof(record));

		auto* const allocator = builder->GetAllocator();
		auto* const decl = allocator->Allocate<Game::MaterialVertexDeclaration>();
		*decl = Game::X86::Convert(record);

		if (record.name)
		{
			const auto nameEnd = contents.find('\0', headerSize);

			if (nameEnd == std::string::npos)
			{
				Components::Logger::Fatal("Reading vertex declaration '{}' failed, its name is cut short!", name);
			}

			decl->name = allocator->DuplicateString(contents.substr(headerSize, nameEnd - headerSize));
		}

		header->vertexDecl = decl;
	}

	void IMaterialVertexDeclaration::Save(Game::XAssetHeader header, Components::ZoneBuilder::Zone* builder)
	{
		auto* const buffer = builder->GetBuffer();
		const auto* const asset = header.vertexDecl;
		auto* const dest = buffer->Dest<Game::X86::MaterialVertexDeclaration>();
		const auto record = Game::X86::Convert(*asset);
		buffer->Save(&record);

		buffer->PushBlock(Game::XFILE_BLOCK_VIRTUAL);

		if (asset->name)
		{
			buffer->SaveString(builder->GetAssetName(this->GetType(), asset->name));
			Utils::Stream::ClearPointer(&dest->name);
		}

		buffer->PopBlock();
	}

	void IMaterialVertexDeclaration::Dump(Game::XAssetHeader header)
	{
		const auto* const asset = header.vertexDecl;
		auto record = Game::X86::Convert(*asset);

		std::string output(declMagic);
		output.push_back(IW4X_TECHSET_VERSION);

		if (asset->name)
		{
			Utils::Stream::ClearPointer(&record.name);
		}

		output.append(reinterpret_cast<const char*>(&record), sizeof(record));

		if (asset->name)
		{
			output.append(asset->name);
			output.push_back('\0');
		}

		Utils::IO::WriteFile(std::format("{}/decl/{}.iw4xDECL", Components::ZoneBuilder::GetDumpingZonePath(), asset->name), output);
	}
}
