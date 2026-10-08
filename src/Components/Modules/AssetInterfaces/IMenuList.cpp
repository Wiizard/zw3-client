#include "STDInclude.hpp"

#include "IMenuList.hpp"
#include "../Menus.hpp"

namespace Assets
{
	void IMenuList::Load(Game::XAssetHeader* header, const std::string& name, Components::ZoneBuilder::Zone* builder)
	{
		auto* const allocator = builder->GetAllocator();

		const auto menus = Components::Menus::LoadMenuByName_Recursive(name);

		if (menus.empty())
		{
			return;
		}

		auto* const newList = allocator->Allocate<Game::MenuList>();
		newList->menus = allocator->AllocateArray<Game::menuDef_t*>(menus.size());
		newList->name = allocator->DuplicateString(name);
		newList->menuCount = static_cast<int>(menus.size());

		for (std::size_t i = 0; i < menus.size(); ++i)
		{
			newList->menus[i] = menus[i];
		}

		header->menuList = newList;
	}

	void IMenuList::Mark(Game::XAssetHeader header, Components::ZoneBuilder::Zone* builder)
	{
		const auto* const asset = header.menuList;

		for (int i = 0; i < asset->menuCount; ++i)
		{
			if (asset->menus[i])
			{
				builder->LoadAsset(Game::ASSET_TYPE_MENU, asset->menus[i]);
			}
		}
	}

	void IMenuList::Save(Game::XAssetHeader header, Components::ZoneBuilder::Zone* builder)
	{
		auto* const buffer = builder->GetBuffer();
		const auto* const asset = header.menuList;
		auto* const dest = buffer->Dest<Game::X86::MenuList>();
		const auto record = Game::X86::Convert(*asset);
		buffer->Save(&record);

		buffer->PushBlock(Game::XFILE_BLOCK_VIRTUAL);

		if (asset->name)
		{
			buffer->SaveString(builder->GetAssetName(this->GetType(), asset->name));
			Utils::Stream::ClearPointer(&dest->name);
		}

		if (asset->menus)
		{
			buffer->Align(Utils::Stream::ALIGN_4);

			auto* const destMenus = buffer->Dest<std::uint32_t>();

			for (int i = 0; i < asset->menuCount; ++i)
			{
				buffer->SaveObject<std::uint32_t>(0);
			}

			for (int i = 0; i < asset->menuCount; ++i)
			{
				if (asset->menus[i])
				{
					destMenus[i] = builder->SaveSubAsset(Game::ASSET_TYPE_MENU, asset->menus[i]);
				}
			}

			Utils::Stream::ClearPointer(&dest->menus);
		}

		buffer->PopBlock();
	}
}
