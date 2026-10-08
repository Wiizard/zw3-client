#pragma once

namespace Components
{
	class Materials : public Component
	{
	public:
		Materials();
		~Materials();

		static Game::GfxImage* CreateImage(const std::string& name, unsigned int width, unsigned int height, unsigned int depth, unsigned int flags, D3DFORMAT format);
		static void DeleteImage(Game::GfxImage* image);

		static Game::Material* Create(const std::string& name, Game::GfxImage* image);
		static void Delete(Game::Material* material, bool deleteImage = false);

		static void DeleteAll();

		static bool IsValid(Game::Material* material);

		static void ConfigureAnimatedAtlas(Game::Material* material);
		static Game::Material* GetRuntimeMaterial(const std::string& materialName);

		static Game::GfxImage* LoadPreviewImage(const std::string& name);

		static Game::GfxImage* CreateNewsImageFromImageBytes(const std::string& imageName, const std::string& imageData);
		static Game::Material* CreateNewsMaterialFromImageBytes(const std::string& materialName, const std::string& imageData);
		static Game::Material* UpdateNewsMaterialFromImageBytes(const std::string& materialName, const std::string& imageData);
		static std::string ConvertNewsImageBytesToIwi(const std::string& imageData);
		static Game::GfxImage* CreateNewsImageFromIwiBytes(const std::string& imageName, const std::string& iwiData);
		static Game::Material* CreateNewsMaterialFromIwiBytes(const std::string& materialName, const std::string& iwiData);

	private:
		static std::vector<Game::GfxImage*> imageTable;
		static std::vector<Game::Material*> materialTable;

		static bool DecodeImageBytesToBGRA(const std::string& imageData, std::vector<unsigned char>& pixels, unsigned int& width, unsigned int& height);
	};
}
