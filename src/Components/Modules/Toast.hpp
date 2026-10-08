#pragma once

namespace Components
{
	class Toast : public Component
	{
	public:
		Toast();

		static void Show(const std::string& image, const std::string& title, const std::string& description, int length, const std::function<void()>& callback = {});
		static void Show(Game::Material* material, const std::string& title, const std::string& description, int length, const std::function<void()>& callback = {});

	private:
		class UIToast
		{
		public:
			Game::Material* image;
			std::string title;
			std::string desc;
			int length;
			int start;
			std::function<void()> callback;
		};

		static void Handler();
		static void Draw(UIToast* toast);

		static std::queue<UIToast> queue;
		static std::mutex mutex;
	};
}
