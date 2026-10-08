#include "STDInclude.hpp"

#include "Components/Modules/AssetHandler.hpp"
#include "Components/Modules/Logger.hpp"

#include "IPhysCollmap.hpp"

namespace Assets
{
	void IPhysCollmap::SaveBrushWrapper(Components::ZoneBuilder::Zone* builder, const Game::BrushWrapper* brush)
	{
		AssertSize(Game::X86::BrushWrapper, 68);

		auto* const buffer = builder->GetBuffer();

		auto* const destBrush = buffer->Dest<Game::X86::BrushWrapper>();
		const auto brushRecord = Game::X86::Convert(*brush);
		buffer->Save(&brushRecord);

		AssertSize(Game::X86::cbrush_t, 36);

		if (brush->brush.sides)
		{
			AssertSize(Game::X86::cbrushside_t, 8);

			buffer->Align(Utils::Stream::ALIGN_4);

			auto* const destSides = buffer->Dest<Game::X86::cbrushside_t>();

			for (unsigned short i = 0; i < brush->brush.numsides; ++i)
			{
				const auto sideRecord = Game::X86::Convert(brush->brush.sides[i]);
				buffer->Save(&sideRecord);
			}

			for (unsigned short i = 0; i < brush->brush.numsides; ++i)
			{
				auto* const destSide = &destSides[i];
				const auto* const side = &brush->brush.sides[i];

				if (!side->plane)
				{
					continue;
				}

				if (builder->HasPointer(side->plane))
				{
					destSide->plane = builder->GetPointer(side->plane);
				}
				else
				{
					buffer->Align(Utils::Stream::ALIGN_4);
					builder->StorePointer(side->plane);

					buffer->Save(side->plane, sizeof(Game::X86::cplane_s));
					Utils::Stream::ClearPointer(&destSide->plane);
				}
			}

			Utils::Stream::ClearPointer(&destBrush->brush.sides);
		}

		if (brush->brush.baseAdjacentSide)
		{
			buffer->Save(brush->brush.baseAdjacentSide, brush->totalEdgeCount);
			Utils::Stream::ClearPointer(&destBrush->brush.baseAdjacentSide);
		}

		if (brush->planes)
		{
			AssertSize(Game::X86::cplane_s, 20);
			AssertSize(Game::cplane_s, 20);

			if (builder->HasPointer(brush->planes))
			{
				Components::Logger::Print("Loading cplane pointer before the array has been written. Not sure if this is correct!\n");
				destBrush->planes = builder->GetPointer(brush->planes);
			}
			else
			{
				buffer->Align(Utils::Stream::ALIGN_4);

				for (unsigned short j = 0; j < brush->brush.numsides; ++j)
				{
					builder->StorePointer(&brush->planes[j]);
					buffer->Save(&brush->planes[j]);
				}

				Utils::Stream::ClearPointer(&destBrush->planes);
			}
		}
	}

	void IPhysCollmap::SavePhysGeomInfoArray(Components::ZoneBuilder::Zone* builder, const Game::PhysGeomInfo* geoms, unsigned int count)
	{
		AssertSize(Game::X86::PhysGeomInfo, 68);

		auto* const buffer = builder->GetBuffer();

		auto* const destGeoms = buffer->Dest<Game::X86::PhysGeomInfo>();

		for (unsigned int i = 0; i < count; ++i)
		{
			const auto geomRecord = Game::X86::Convert(geoms[i]);
			buffer->Save(&geomRecord);
		}

		for (unsigned int i = 0; i < count; ++i)
		{
			auto* const destGeom = &destGeoms[i];
			const auto* const geom = &geoms[i];

			if (geom->brushWrapper)
			{
				buffer->Align(Utils::Stream::ALIGN_4);

				this->SaveBrushWrapper(builder, geom->brushWrapper);
				Utils::Stream::ClearPointer(&destGeom->brushWrapper);
			}
		}
	}

	void IPhysCollmap::Save(Game::XAssetHeader header, Components::ZoneBuilder::Zone* builder)
	{
		AssertSize(Game::X86::PhysCollmap, 72);

		auto* const buffer = builder->GetBuffer();
		const auto* const asset = header.physCollmap;
		auto* const dest = buffer->Dest<Game::X86::PhysCollmap>();
		const auto record = Game::X86::Convert(*asset);
		buffer->Save(&record);

		buffer->PushBlock(Game::XFILE_BLOCK_VIRTUAL);

		if (asset->name)
		{
			buffer->SaveString(builder->GetAssetName(this->GetType(), asset->name));
			Utils::Stream::ClearPointer(&dest->name);
		}

		if (asset->geoms)
		{
			buffer->Align(Utils::Stream::ALIGN_4);

			this->SavePhysGeomInfoArray(builder, asset->geoms, asset->count);
			Utils::Stream::ClearPointer(&dest->geoms);
		}

		buffer->PopBlock();
	}
}
