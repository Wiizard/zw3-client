#pragma once

#include "Components/Modules/AssetHandler.hpp"

namespace Assets
{
	class IclipMap_t : public Components::AssetHandler::IAsset
	{
	public:
		Game::XAssetType GetType() override
		{
			return Game::ASSET_TYPE_CLIPMAP_MP;
		}

		void Save(Game::XAssetHeader header, Components::ZoneBuilder::Zone* builder) override;
		void Mark(Game::XAssetHeader header, Components::ZoneBuilder::Zone* builder) override;
		void Load(Game::XAssetHeader* header, const std::string& name, Components::ZoneBuilder::Zone* builder) override;
		bool HasDump() override
		{
			return true;
		}

		void Dump(Game::XAssetHeader header) override;

	private:
		class SModelQuadtree
		{
		public:
			SModelQuadtree() = default;

			SModelQuadtree(Game::cStaticModel_s* modelList, int numModels)
			{
				for (int i = 0; i < numModels; ++i)
				{
					this->Insert(&modelList[i]);
				}
			}

			void Insert(Game::cStaticModel_s* item)
			{
				if (this->numValues < 4)
				{
					this->values[this->numValues++] = item;
					return;
				}

				if (this->numValues == 4)
				{
					for (auto& child : this->children)
					{
						child = std::make_unique<SModelQuadtree>();
					}

					for (int i = 0; i < this->numValues; ++i)
					{
						if (item->origin[0] > this->x && this->values[i]->origin[1] > this->y)
						{
							this->children[0]->Insert(this->values[i]);
						}

						if (item->origin[0] < this->x && this->values[i]->origin[1] > this->y)
						{
							this->children[1]->Insert(this->values[i]);
						}

						if (item->origin[0] < this->x && this->values[i]->origin[1] < this->y)
						{
							this->children[2]->Insert(this->values[i]);
						}

						if (item->origin[0] > this->x && this->values[i]->origin[1] < this->y)
						{
							this->children[3]->Insert(this->values[i]);
						}

						this->values[i] = nullptr;
					}

					for (auto& child : this->children)
					{
						child->halfX = this->halfX / 2;
						child->halfY = this->halfY / 2;
						child->halfZ = this->halfZ;
					}

					++this->numValues;
				}

				if (item->origin[0] > this->x && item->origin[1] > this->y)
				{
					this->children[0]->Insert(item);
				}

				if (item->origin[0] < this->x && item->origin[1] > this->y)
				{
					this->children[1]->Insert(item);
				}

				if (item->origin[0] < this->x && item->origin[1] < this->y)
				{
					this->children[2]->Insert(item);
				}

				if (item->origin[0] > this->x && item->origin[1] < this->y)
				{
					this->children[3]->Insert(item);
				}
			}

		private:
			std::unique_ptr<SModelQuadtree> children[4];
			Game::cStaticModel_s* values[4]{};
			int numValues = 0;
			float x = 0.0f;
			float y = 0.0f;
			float z = 0.0f;
			float halfX = 0.0f;
			float halfY = 0.0f;
			float halfZ = 0.0f;
		};
	};
}
