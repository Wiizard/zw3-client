#pragma once

namespace Components
{
	class Materials : public Component
	{
	public:
		Materials();
		~Materials();

		static int FormatImagePath(char* buffer, size_t size, int, int, const char* image);

		static Game::Material* Create(const std::string& name, Game::GfxImage* image);
		static void ConfigureAnimatedAtlas(Game::Material* material);
		static void Delete(Game::Material* material, bool deleteImage = false);

		static Game::GfxImage* CreateImage(const std::string& name, unsigned int width, unsigned int height, unsigned int depth, unsigned int flags, _D3DFORMAT format);
		static void DeleteImage(Game::GfxImage* image);

		static bool IsValid(Game::Material* material);

		static Game::GfxImage* CreateNewsImageFromImageBytes(const std::string& imageName, const std::string& imageData);
		static Game::Material* CreateNewsMaterialFromImageBytes(const std::string& materialName, const std::string& imageData);
		static std::string ConvertNewsImageBytesToIwi(const std::string& imageData);
		static Game::GfxImage* CreateNewsImageFromIwiBytes(const std::string& imageName, const std::string& iwiData);
		static Game::Material* CreateNewsMaterialFromIwiBytes(const std::string& materialName, const std::string& iwiData);
		static Game::Material* GetRuntimeMaterial(const std::string& materialName);
		static Game::Material* UpdateNewsMaterialFromImageBytes(const std::string& materialName, const std::string& imageData);

	private:
		static std::vector<Game::GfxImage*> ImageTable;
		static std::vector<Game::Material*> MaterialTable;

		static Utils::Hook ImageVersionCheckHook;
		static void ImageVersionCheck();

		static bool DecodeImageBytesToBGRA(const std::string& imageData, std::vector<unsigned char>& pixels, unsigned int& width, unsigned int& height);

		static int WriteDeathMessageIcon(char* string, int offset, Game::Material* material);
		static void DeathMessageStub();

#ifdef DEBUG
		static void DumpImageCfg(int, const char*, const char* material);
		static void DumpImageCfgPath(int, const char*, const char* material);
#endif

		static int MaterialComparePrint(Game::Material* m1, Game::Material* m2);

		static void DeleteAll();
	};
}
