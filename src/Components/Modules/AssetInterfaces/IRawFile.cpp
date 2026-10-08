#include "STDInclude.hpp"

#include <Utils/Compression.hpp>

#include "IRawFile.hpp"
#include "../FileSystem.hpp"
#include "../Logger.hpp"

namespace Assets
{
	using AssetFinder = std::function<void*(Game::XAssetType type, const std::string& name)>;

	static std::string ReadScript(const Game::RawFile* asset)
	{
		std::string script;

		if (asset->compressedLen > 0)
		{
			script = Utils::Compression::ZLib::Decompress(std::string(asset->buffer, static_cast<std::size_t>(asset->compressedLen)));
		}
		else
		{
			script.assign(asset->buffer, static_cast<std::size_t>(asset->len));
		}

		if (script.size() > static_cast<std::size_t>(asset->len))
		{
			script.resize(static_cast<std::size_t>(asset->len));
		}

		return script;
	}

	static std::vector<Game::XAsset> GetAnimtreeAnims(const std::string& script, const AssetFinder& find)
	{
		std::vector<Game::XAsset> anims;
		std::istringstream stream(script);

		for (std::string line; std::getline(stream, line);)
		{
			line.erase(0, line.find_first_not_of(" \n\r\t"));

			while (!line.empty() && (line.back() == '\n' || line.back() == '\r' || line.back() == ' '))
			{
				line.pop_back();
			}

			if (line.empty() || line[0] == '/' || line[1] == '/')
			{
				continue;
			}

			auto* const parts = find(Game::ASSET_TYPE_XANIMPARTS, line);

			if (parts)
			{
				anims.push_back({ Game::ASSET_TYPE_XANIMPARTS, parts });
			}
		}

		return anims;
	}

	static std::vector<Game::XAsset> GetMapAnimtrees(const std::string& script, const AssetFinder& find)
	{
		static const std::regex animtreeCatcher("#using_animtree\\(\"(.*)\"\\);");

		std::vector<Game::XAsset> trees;
		std::smatch match;
		auto searchStart = script.cbegin();

		while (std::regex_search(searchStart, script.cend(), match, animtreeCatcher))
		{
			const auto animtreeName = std::format("animtrees/{}.atr", match[1].str());
			searchStart = match.suffix().first;

			auto* const animtree = find(Game::ASSET_TYPE_RAWFILE, animtreeName);

			if (!animtree)
			{
				Components::Logger::Fatal("Could not find animtree {}\n", animtreeName);
			}

			trees.push_back({ Game::ASSET_TYPE_RAWFILE, animtree });
		}

		return trees;
	}

	static std::vector<Game::XAsset> GetMapAnimatedModelAnims(const std::string& script, const AssetFinder& find)
	{
		static const std::regex animCatchers[] =
		{
			std::regex("%(.*);"),
			std::regex("\"\\] *= \"*(.*)\";"),
		};

		std::vector<Game::XAsset> anims;

		for (const auto& animCatcher : animCatchers)
		{
			std::smatch match;
			auto searchStart = script.cbegin();

			while (std::regex_search(searchStart, script.cend(), match, animCatcher))
			{
				auto* const anim = find(Game::ASSET_TYPE_XANIMPARTS, match[1].str());
				searchStart = match.suffix().first;

				if (anim)
				{
					anims.push_back({ Game::ASSET_TYPE_XANIMPARTS, anim });
				}
			}
		}

		return anims;
	}

	static void* GetAmbientPlay(const std::string& script, const AssetFinder& find)
	{
		static const std::regex ambientPlay("ambientplay\\( \"((.*))\" \\);");

		std::smatch match;

		if (!std::regex_search(script.cbegin(), script.cend(), match, ambientPlay))
		{
			return nullptr;
		}

		return find(Game::ASSET_TYPE_SOUND, match[1].str());
	}

	static std::vector<Game::XAsset> GetMatches(const std::string& script, const std::regex& catcher, Game::XAssetType type, const AssetFinder& find)
	{
		std::vector<Game::XAsset> assets;
		std::smatch match;
		auto searchStart = script.cbegin();

		while (std::regex_search(searchStart, script.cend(), match, catcher))
		{
			searchStart = match.suffix().first;

			for (std::size_t i = 1; i < match.size(); ++i)
			{
				auto* const asset = find(type, match[i].str());

				if (asset)
				{
					assets.push_back({ static_cast<unsigned int>(type), asset });
				}
			}
		}

		return assets;
	}

	static std::vector<Game::XAsset> GetAssetsInPrecache(const std::string& script, const AssetFinder& find)
	{
		static const std::regex animScriptCatcher(" *(.*)::main\\(\\);");

		std::vector<Game::XAsset> assets;
		std::smatch match;
		auto searchStart = script.cbegin();

		while (std::regex_search(searchStart, script.cend(), match, animScriptCatcher))
		{
			auto scriptName = std::format("{}.gsc", match[1].str());
			std::replace(scriptName.begin(), scriptName.end(), '\\', '/');
			searchStart = match.suffix().first;

			auto* const animScript = static_cast<Game::RawFile*>(find(Game::ASSET_TYPE_RAWFILE, scriptName));

			if (!animScript)
			{
				continue;
			}

			assets.push_back({ Game::ASSET_TYPE_RAWFILE, animScript });

			const auto contents = ReadScript(animScript);

			for (const auto& tree : GetMapAnimtrees(contents, find))
			{
				assets.push_back(tree);
			}

			for (const auto& anim : GetMapAnimatedModelAnims(contents, find))
			{
				assets.push_back(anim);
			}
		}

		return assets;
	}

	static std::vector<Game::XAsset> GetChildAssets(const std::string& scriptName, const std::string& script, const AssetFinder& find)
	{
		static const std::regex fxCatcher("\\] = loadfx\\( *\"(.+)\" *\\);");
		static const std::regex soundCatcher("(?:\\.v\\[\"soundalias\"\\] *= *\"(.+)\")");

		std::vector<Game::XAsset> children;

		if (Utils::String::EndsWith(scriptName, ".atr"))
		{
			return GetAnimtreeAnims(script, find);
		}

		if (!Utils::String::EndsWith(scriptName, ".gsc"))
		{
			return children;
		}

		if (Utils::String::EndsWith(scriptName, "_fx.gsc"))
		{
			if (Utils::String::StartsWith(scriptName, "maps/createfx/"))
			{
				children = GetMatches(script, soundCatcher, Game::ASSET_TYPE_SOUND, find);
			}
			else
			{
				children = GetMatches(script, fxCatcher, Game::ASSET_TYPE_FX, find);
			}
		}
		else if (Utils::String::EndsWith(scriptName, "_precache.gsc"))
		{
			children = GetAssetsInPrecache(script, find);
		}

		for (const auto& tree : GetMapAnimtrees(script, find))
		{
			children.push_back(tree);
		}

		auto* const ambient = GetAmbientPlay(script, find);

		if (ambient)
		{
			children.push_back({ Game::ASSET_TYPE_SOUND, ambient });
		}

		return children;
	}

	static void* FindLoadedAsset(Game::XAssetType type, const std::string& name)
	{
		return Components::AssetHandler::FindLoadedAsset(type, name.data()).data;
	}

	void IRawFile::Load(Game::XAssetHeader* header, const std::string& name, Components::ZoneBuilder::Zone* builder)
	{
		Components::FileSystem::File rawFile(name);

		if (!rawFile.Exists())
		{
			return;
		}

		const auto& contents = rawFile.GetBuffer();
		auto* const allocator = builder->GetAllocator();

		auto* const asset = allocator->Allocate<Game::RawFile>();
		asset->name = allocator->DuplicateString(name);
		asset->len = static_cast<int>(contents.size());

		GetChildAssets(name, contents, [builder](Game::XAssetType type, const std::string& childName)
		{
			return Components::AssetHandler::FindAssetForZone(type, childName, builder).data;
		});

		const auto compressed = Utils::Compression::ZLib::Compress(contents);

		if (!compressed.empty() && compressed.size() < contents.size())
		{
			auto* const data = allocator->AllocateArray<char>(compressed.size());
			std::memcpy(data, compressed.data(), compressed.size());
			asset->buffer = data;
			asset->compressedLen = static_cast<int>(compressed.size());
		}
		else
		{
			auto* const data = allocator->AllocateArray<char>(contents.size() + 1);
			std::memcpy(data, contents.data(), contents.size());
			asset->buffer = data;
			asset->compressedLen = 0;
		}

		header->rawfile = asset;
	}

	void IRawFile::Save(Game::XAssetHeader header, Components::ZoneBuilder::Zone* builder)
	{
		auto* const buffer = builder->GetBuffer();
		const auto* const asset = header.rawfile;
		auto* const dest = buffer->Dest<Game::X86::RawFile>();
		const auto record = Game::X86::Convert(*asset);
		buffer->Save(&record);

		buffer->PushBlock(Game::XFILE_BLOCK_VIRTUAL);

		if (asset->name)
		{
			buffer->SaveString(builder->GetAssetName(this->GetType(), asset->name));
			Utils::Stream::ClearPointer(&dest->name);
		}

		if (asset->buffer)
		{
			if (asset->compressedLen)
			{
				buffer->Save(asset->buffer, static_cast<std::size_t>(asset->compressedLen));
			}
			else
			{
				buffer->Save(asset->buffer, static_cast<std::size_t>(asset->len) + 1);
			}

			Utils::Stream::ClearPointer(&dest->buffer);
		}

		buffer->PopBlock();
	}

	void IRawFile::Dump(Game::XAssetHeader header)
	{
		const auto* const asset = header.rawfile;

		if (!asset->buffer)
		{
			Components::Logger::Fatal("rawfile {} has no buffer, could not dump it\n", asset->name);
		}

		if (asset->len <= 0)
		{
			return;
		}

		const auto script = ReadScript(asset);

		for (const auto& child : GetChildAssets(asset->name, script, FindLoadedAsset))
		{
			Components::AssetHandler::DumpAsset(child);
		}

		Utils::IO::WriteFile(std::format("{}/{}", Components::ZoneBuilder::GetDumpingZonePath(), asset->name), script);
	}
}
