#include "STDInclude.hpp"

#include "Toast.hpp"
#include "Dedicated.hpp"
#include "Materials.hpp"
#include "Renderer.hpp"
#include "Scheduler.hpp"

namespace Components
{
	constexpr std::uintptr_t sharedUiInfo_whiteMaterial = 0x1465D0A80;

	std::queue<Toast::UIToast> Toast::queue;
	std::mutex Toast::mutex;

	void Toast::Show(const std::string& image, const std::string& title, const std::string& description, int length, const std::function<void()>& callback)
	{
		auto* const material = static_cast<Game::Material*>(Game::DB_FindXAssetHeader(Game::ASSET_TYPE_MATERIAL, image.data()));
		Show(material, title, description, length, callback);
	}

	void Toast::Show(Game::Material* material, const std::string& title, const std::string& description, int length, const std::function<void()>& callback)
	{
		std::lock_guard _(mutex);
		queue.push({ material, Utils::String::ToUpper(title), description, length, 0, callback });
	}

	void Toast::Draw(UIToast* toast)
	{
		if (!toast)
		{
			return;
		}

		const int width = Renderer::Width();
		int height = Renderer::Height();
		const int slideTime = 100;

		const int duration = toast->length;
		const int startTime = toast->start;

		const int border = 1;
		const int cornerSize = 15;
		int bHeight = 74;

		const int imgDim = 60;

		const float fontSize = 0.9f;
		const float descSize = 0.9f;

		auto* const font = static_cast<Game::Font_s*>(Game::DB_FindXAssetHeader(Game::ASSET_TYPE_FONT, "fonts/objectiveFont"));
		auto* const descfont = static_cast<Game::Font_s*>(Game::DB_FindXAssetHeader(Game::ASSET_TYPE_FONT, "fonts/normalFont"));

		if (!font || !descfont)
		{
			return;
		}

		auto* const whiteMaterial = Utils::Hook::Get<Game::Material*>(sharedUiInfo_whiteMaterial);

		const Game::vec4_t wColor = { 1.0f, 1.0f, 1.0f, 1.0f };
		const Game::vec4_t bgColor = { 0.0f, 0.0f, 0.0f, 0.8f };
		const Game::vec4_t borderColor = { 1.0f, 1.0f, 1.0f, 0.2f };

		height /= 5;
		height *= 4;

		const int now = Game::Sys_Milliseconds();

		if (now < startTime || (startTime + duration) < now)
		{
			return;
		}

		if (now - startTime < slideTime)
		{
			int diffH = Renderer::Height() / 5;
			const int diff = now - startTime;
			const double scale = 1.0 - ((1.0 * diff) / (1.0 * slideTime));
			diffH = static_cast<int>(diffH * scale);
			height += diffH;
		}
		else if (now - startTime > (duration - slideTime))
		{
			int diffH = Renderer::Height() / 5;
			const int diff = (startTime + duration) - now;
			const double scale = 1.0 - ((1.0 * diff) / (1.0 * slideTime));
			diffH = static_cast<int>(diffH * scale);
			height += diffH;
		}

		height += bHeight / 2 - cornerSize;

		const int iOffset = (bHeight - imgDim) / 2;
		const int iOffsetLeft = iOffset * 2;
		const float titleSize = Game::R_TextWidth(toast->title.data(), std::numeric_limits<int>::max(), font) * fontSize;
		const float descrSize = Game::R_TextWidth(toast->desc.data(), std::numeric_limits<int>::max(), descfont) * descSize;
		float bWidth = iOffsetLeft * 3 + imgDim + std::max(titleSize, descrSize);

		bWidth = (static_cast<int>(bWidth) + (static_cast<int>(bWidth) % 2)) * 1.0f;
		bHeight += (bHeight % 2);

		const float left = static_cast<float>(width / 2 - bWidth / 2);
		const float top = static_cast<float>(height - bHeight / 2);

		Game::CL_DrawStretchPicPhysical(left, top, bWidth * 1.0f, bHeight * 1.0f, 0, 0, 1.0f, 1.0f, bgColor, whiteMaterial);

		Game::CL_DrawStretchPicPhysical(left - border, top - border, border * 1.0f, bHeight + (border * 2.0f), 0, 0, 1.0f, 1.0f, borderColor, whiteMaterial);
		Game::CL_DrawStretchPicPhysical(left + bWidth, top - border, border * 1.0f, bHeight + (border * 2.0f), 0, 0, 1.0f, 1.0f, borderColor, whiteMaterial);
		Game::CL_DrawStretchPicPhysical(left, top - border, bWidth * 1.0f, border * 1.0f, 0, 0, 1.0f, 1.0f, borderColor, whiteMaterial);
		Game::CL_DrawStretchPicPhysical(left, static_cast<float>(height + bHeight / 2), bWidth * 1.0f, border * 1.0f, 0, 0, 1.0f, 1.0f, borderColor, whiteMaterial);

		Game::Material* image = toast->image;

		if (!Materials::IsValid(image))
		{
			image = static_cast<Game::Material*>(Game::DB_FindXAssetDefaultHeaderInternal(Game::ASSET_TYPE_MATERIAL));
		}

		Game::CL_DrawStretchPicPhysical(left + iOffsetLeft, top + iOffset, imgDim * 1.0f, imgDim * 1.0f, 0, 0, 1.0f, 1.0f, wColor, image);

		const float leftText = width / 2 - bWidth / 2 - cornerSize + iOffsetLeft * 2 + imgDim;
		const float rightText = width / 2 + bWidth / 2 - cornerSize - iOffsetLeft;
		Game::R_AddCmdDrawText(toast->title.data(), std::numeric_limits<int>::max(), font, static_cast<float>(leftText + (rightText - leftText) / 2 - titleSize / 2 + cornerSize), static_cast<float>(height - bHeight / 2 + cornerSize * 2 + 7), fontSize, fontSize, 0, wColor, Game::ITEM_TEXTSTYLE_SHADOWED);
		Game::R_AddCmdDrawText(toast->desc.data(), std::numeric_limits<int>::max(), descfont, leftText + (rightText - leftText) / 2 - descrSize / 2 + cornerSize, static_cast<float>(height - bHeight / 2 + cornerSize * 2 + 33), descSize, descSize, 0, wColor, Game::ITEM_TEXTSTYLE_SHADOWED);
	}

	void Toast::Handler()
	{
		std::lock_guard _(mutex);

		if (queue.empty())
		{
			return;
		}

		UIToast* toast = &queue.front();

		if (!toast->start)
		{
			toast->start = Game::Sys_Milliseconds();
		}

		if ((toast->start + toast->length) < Game::Sys_Milliseconds())
		{
			if (toast->callback)
			{
				toast->callback();
			}

			queue.pop();
		}
		else
		{
			Draw(toast);
		}
	}

	Toast::Toast()
	{
		if (Dedicated::IsEnabled())
		{
			return;
		}

		Scheduler::Loop(Handler, Scheduler::Pipeline::RENDERER);
	}
}
