#pragma once

namespace Assets
{
	class IXModel : public Components::AssetHandler::IAsset
	{
	public:
		Game::XAssetType GetType() override
		{
			return Game::ASSET_TYPE_XMODEL;
		}

		void Save(Game::XAssetHeader header, Components::ZoneBuilder::Zone* builder) override;
		void Mark(Game::XAssetHeader header, Components::ZoneBuilder::Zone* builder) override;
		bool HasDump() override
		{
			return true;
		}

		void Dump(Game::XAssetHeader header) override;
		void Load(Game::XAssetHeader* header, const std::string& name, Components::ZoneBuilder::Zone* builder) override;

		static void ConvertPlayerModelFromSingleplayerToMultiplayer(Game::XModel* model, Utils::Memory::Allocator& allocator);

	private:
		static std::uint8_t GetIndexOfBone(const Game::XModel* model, const std::string& name);
		static std::uint8_t GetParentIndexOfBone(const Game::XModel* model, std::uint8_t index);
		static void SetParentIndexOfBone(Game::XModel* model, std::uint8_t boneIndex, std::uint8_t parentIndex);
		static std::string GetParentOfBone(const Game::XModel* model, std::uint8_t index);
		static std::uint8_t GetHighestAffectingBoneIndex(const Game::XModelLodInfo* lod);
		static void RebuildPartBits(Game::XModel* model);
		static std::uint8_t InsertBone(Game::XModel* model, const std::string& boneName, const std::string& parentName, Utils::Memory::Allocator& allocator);
		static void TransferWeights(Game::XModel* model, std::uint8_t origin, std::uint8_t destination);

		static void SetBoneTrans(Game::XModel* model, std::uint8_t boneIndex, bool baseMat, float x, float y, float z);
		static void SetBoneQuaternion(Game::XModel* model, std::uint8_t boneIndex, bool baseMat, float x, float y, float z, float w);
	};
}
