#include "STDInclude.hpp"

#include "IMaterialPixelShader.hpp"
#include "../FileSystem.hpp"

namespace Assets
{
	constexpr unsigned short GFX_RENDERER_SHADER_SM3 = 0;

	void IMaterialPixelShader::Load(Game::XAssetHeader* header, const std::string& name, Components::ZoneBuilder::Zone* builder)
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

	void IMaterialPixelShader::LoadNative(Game::XAssetHeader* header, const std::string& name, [[maybe_unused]] Components::ZoneBuilder::Zone* builder)
	{
		header->pixelShader = Components::AssetHandler::FindLoadedAsset(this->GetType(), name.data()).pixelShader;
	}

	void IMaterialPixelShader::LoadBinary(Game::XAssetHeader* header, const std::string& name, Components::ZoneBuilder::Zone* builder)
	{
		Components::FileSystem::File shaderFile(std::format("ps/{}.cso", name));

		if (!shaderFile.Exists())
		{
			return;
		}

		const auto& program = shaderFile.GetBuffer();
		auto* const allocator = builder->GetAllocator();

		auto* const shader = allocator->Allocate<Game::MaterialPixelShader>();
		shader->name = allocator->DuplicateString(name);
		shader->prog.loadDef.loadForRenderer = GFX_RENDERER_SHADER_SM3;
		shader->prog.loadDef.programSize = static_cast<unsigned short>(program.size() / sizeof(std::uint32_t));
		shader->prog.loadDef.program = allocator->AllocateArray<unsigned int>(shader->prog.loadDef.programSize);
		std::memcpy(shader->prog.loadDef.program, program.data(), shader->prog.loadDef.programSize * sizeof(std::uint32_t));

		header->pixelShader = shader;
	}

	void IMaterialPixelShader::Save(Game::XAssetHeader header, Components::ZoneBuilder::Zone* builder)
	{
		auto* const buffer = builder->GetBuffer();
		const auto* const asset = header.pixelShader;
		auto* const dest = buffer->Dest<Game::X86::MaterialPixelShader>();
		const auto record = Game::X86::Convert(*asset);
		buffer->Save(&record);

		buffer->PushBlock(Game::XFILE_BLOCK_VIRTUAL);

		if (asset->name)
		{
			buffer->SaveString(builder->GetAssetName(this->GetType(), asset->name));
			Utils::Stream::ClearPointer(&dest->name);
		}

		if (asset->prog.loadDef.program)
		{
			buffer->Align(Utils::Stream::ALIGN_4);
			buffer->SaveArray(asset->prog.loadDef.program, asset->prog.loadDef.programSize);
			Utils::Stream::ClearPointer(&dest->prog.loadDef.program);
		}

		buffer->PopBlock();
	}

	void IMaterialPixelShader::Dump(Game::XAssetHeader header)
	{
		const auto* const asset = header.pixelShader;

		if (!asset->prog.loadDef.program)
		{
			return;
		}

		const std::string program(reinterpret_cast<const char*>(asset->prog.loadDef.program), asset->prog.loadDef.programSize * sizeof(std::uint32_t));
		Utils::IO::WriteFile(std::format("{}/ps/{}.cso", Components::ZoneBuilder::GetDumpingZonePath(), asset->name), program);
	}
}
