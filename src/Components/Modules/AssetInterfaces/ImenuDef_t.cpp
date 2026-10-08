#include "STDInclude.hpp"

#include "ImenuDef_t.hpp"
#include "../Logger.hpp"
#include "../Menus.hpp"

namespace Assets
{
	constexpr int ITEM_TYPE_LISTBOX = 6;
	constexpr int ITEM_TYPE_MULTI = 12;
	constexpr int ITEM_TYPE_DVARENUM = 13;
	constexpr int ITEM_TYPE_NEWS_TICKER = 20;
	constexpr int ITEM_TYPE_TEXT_SCROLL = 21;
	constexpr int editFieldItemTypes[] = { 0, 4, 9, 10, 11, 14, 16, 17, 18, 22, 23 };

	static void SaveXString(Utils::Stream* buffer, const char* string, std::uint32_t* dest)
	{
		if (!string)
		{
			return;
		}

		buffer->SaveString(string);
		Utils::Stream::ClearPointer(dest);
	}

	template <typename T>
	static void SavePointerArray(Utils::Stream* buffer, T* const* pointers, int count)
	{
		for (int i = 0; i < count; ++i)
		{
			std::uint32_t slot = 0;

			if (pointers[i])
			{
				Utils::Stream::ClearPointer(&slot);
			}

			buffer->Save(&slot);
		}
	}

	static Game::X86::expressionEntry ConvertEntry(const Game::expressionEntry& entry)
	{
		Game::X86::expressionEntry record{};
		record.type = entry.type;

		if (!entry.type)
		{
			record.data.op = entry.data.op;
			return record;
		}

		const auto& operand = entry.data.operand;
		record.data.operand.dataType = static_cast<std::int32_t>(operand.dataType);

		if (operand.dataType != Game::VAL_STRING && operand.dataType != Game::VAL_FUNCTION)
		{
			record.data.operand.internals.intVal = operand.internals.intVal;
		}

		return record;
	}

	void ImenuDef_t::Load(Game::XAssetHeader* header, const std::string& name, [[maybe_unused]] Components::ZoneBuilder::Zone* builder)
	{
		const auto menus = Components::Menus::LoadMenuByName_Recursive(std::format("ui_mp/{}.menu", name));

		if (menus.empty())
		{
			header->menu = nullptr;
			return;
		}

		if (menus.size() > 1)
		{
			Components::Logger::Print("Menu '{}' on disk has more than one menudef in it. Only saving the first one\n", name);
		}

		header->menu = menus[0];
	}

	void ImenuDef_t::Mark(Game::XAssetHeader header, Components::ZoneBuilder::Zone* builder)
	{
		const auto* const asset = header.menu;

		if (asset->window.background)
		{
			builder->LoadAsset(Game::ASSET_TYPE_MATERIAL, asset->window.background);
		}

		for (int i = 0; i < asset->itemCount; ++i)
		{
			const auto* const item = asset->items[i];

			if (!item)
			{
				continue;
			}

			if (item->window.background)
			{
				builder->LoadAsset(Game::ASSET_TYPE_MATERIAL, item->window.background);
			}

			if (item->focusSound)
			{
				builder->LoadAsset(Game::ASSET_TYPE_SOUND, item->focusSound);
			}

			if (item->type == ITEM_TYPE_LISTBOX && item->typeData.listBox && item->typeData.listBox->selectIcon)
			{
				builder->LoadAsset(Game::ASSET_TYPE_MATERIAL, item->typeData.listBox->selectIcon);
			}
		}
	}

	void ImenuDef_t::Save_windowDef_t(const Game::windowDef_t* asset, Game::X86::windowDef_t* dest, Components::ZoneBuilder::Zone* builder)
	{
		auto* const buffer = builder->GetBuffer();

		SaveXString(buffer, asset->name, &dest->name);
		SaveXString(buffer, asset->group, &dest->group);

		if (asset->background)
		{
			dest->background = builder->SaveSubAsset(Game::ASSET_TYPE_MATERIAL, asset->background);
		}
	}

	void ImenuDef_t::Save_ExpressionSupportingData(const Game::ExpressionSupportingData* asset, Components::ZoneBuilder::Zone* builder)
	{
		auto* const buffer = builder->GetBuffer();

		buffer->Align(Utils::Stream::ALIGN_4);

		auto* const dest = buffer->Dest<Game::X86::ExpressionSupportingData>();
		const auto record = Game::X86::Convert(*asset);
		buffer->Save(&record);

		if (asset->uifunctions.functions)
		{
			buffer->Align(Utils::Stream::ALIGN_4);
			SavePointerArray(buffer, asset->uifunctions.functions, asset->uifunctions.totalFunctions);

			for (int i = 0; i < asset->uifunctions.totalFunctions; ++i)
			{
				if (asset->uifunctions.functions[i])
				{
					buffer->Align(Utils::Stream::ALIGN_4);
					this->Save_Statement_s(asset->uifunctions.functions[i], builder);
				}
			}

			Utils::Stream::ClearPointer(&dest->uifunctions.functions);
		}

		if (asset->staticDvarList.staticDvars)
		{
			buffer->Align(Utils::Stream::ALIGN_4);
			SavePointerArray(buffer, asset->staticDvarList.staticDvars, asset->staticDvarList.numStaticDvars);

			for (int i = 0; i < asset->staticDvarList.numStaticDvars; ++i)
			{
				const auto* const staticDvar = asset->staticDvarList.staticDvars[i];

				if (!staticDvar)
				{
					continue;
				}

				buffer->Align(Utils::Stream::ALIGN_4);

				auto* const destStaticDvar = buffer->Dest<Game::X86::StaticDvar>();
				const auto staticDvarRecord = Game::X86::Convert(*staticDvar);
				buffer->Save(&staticDvarRecord);

				SaveXString(buffer, staticDvar->dvarName, &destStaticDvar->dvarName);
			}

			Utils::Stream::ClearPointer(&dest->staticDvarList.staticDvars);
		}

		if (asset->uiStrings.strings)
		{
			buffer->Align(Utils::Stream::ALIGN_4);
			SavePointerArray(buffer, asset->uiStrings.strings, asset->uiStrings.totalStrings);

			for (int i = 0; i < asset->uiStrings.totalStrings; ++i)
			{
				if (asset->uiStrings.strings[i])
				{
					buffer->SaveString(asset->uiStrings.strings[i]);
				}
			}

			Utils::Stream::ClearPointer(&dest->uiStrings.strings);
		}
	}

	void ImenuDef_t::Save_Statement_s(const Game::Statement_s* asset, Components::ZoneBuilder::Zone* builder)
	{
		auto* const buffer = builder->GetBuffer();
		auto* const dest = buffer->Dest<Game::X86::Statement_s>();

		auto record = Game::X86::Convert(*asset);

		if (asset->lastResult.dataType == Game::VAL_STRING || asset->lastResult.dataType == Game::VAL_FUNCTION)
		{
			record.lastResult.internals.intVal = 0;
		}

		buffer->Save(&record);

		if (asset->entries)
		{
			buffer->Align(Utils::Stream::ALIGN_4);

			auto* const destEntries = buffer->Dest<Game::X86::expressionEntry>();

			for (int i = 0; i < asset->numEntries; ++i)
			{
				const auto entryRecord = ConvertEntry(asset->entries[i]);
				buffer->Save(&entryRecord);
			}

			for (int i = 0; i < asset->numEntries; ++i)
			{
				const auto* const entry = &asset->entries[i];

				if (!entry->type)
				{
					continue;
				}

				auto& destInternals = destEntries[i].data.operand.internals;

				if (entry->data.operand.dataType == Game::VAL_STRING)
				{
					SaveXString(buffer, entry->data.operand.internals.stringVal.string, &destInternals.stringVal.string);
				}
				else if (entry->data.operand.dataType == Game::VAL_FUNCTION && entry->data.operand.internals.function)
				{
					buffer->Align(Utils::Stream::ALIGN_4);
					this->Save_Statement_s(entry->data.operand.internals.function, builder);
					Utils::Stream::ClearPointer(&destInternals.function);
				}
			}

			Utils::Stream::ClearPointer(&dest->entries);
		}

		if (asset->supportingData)
		{
			this->Save_ExpressionSupportingData(asset->supportingData, builder);
			Utils::Stream::ClearPointer(&dest->supportingData);
		}
	}

	void ImenuDef_t::Save_StatementPtr(const Game::Statement_s* asset, std::uint32_t* dest, Components::ZoneBuilder::Zone* builder)
	{
		if (!asset)
		{
			return;
		}

		builder->GetBuffer()->Align(Utils::Stream::ALIGN_4);
		this->Save_Statement_s(asset, builder);
		Utils::Stream::ClearPointer(dest);
	}

	void ImenuDef_t::Save_MenuEventHandlerSet(const Game::MenuEventHandlerSet* asset, Components::ZoneBuilder::Zone* builder)
	{
		auto* const buffer = builder->GetBuffer();
		auto* const destSet = buffer->Dest<Game::X86::MenuEventHandlerSet>();
		const auto record = Game::X86::Convert(*asset);
		buffer->Save(&record);

		if (!asset->eventHandlers)
		{
			return;
		}

		buffer->Align(Utils::Stream::ALIGN_4);
		SavePointerArray(buffer, asset->eventHandlers, asset->eventHandlerCount);

		for (int i = 0; i < asset->eventHandlerCount; ++i)
		{
			const auto* const handler = asset->eventHandlers[i];

			if (!handler)
			{
				continue;
			}

			buffer->Align(Utils::Stream::ALIGN_4);

			auto* const dest = buffer->Dest<Game::X86::MenuEventHandler>();
			const auto handlerRecord = Game::X86::Convert(*handler);
			buffer->Save(&handlerRecord);

			switch (handler->eventType)
			{
			case Game::EVENT_UNCONDITIONAL:
				SaveXString(buffer, handler->eventData.unconditionalScript, &dest->eventData.unconditionalScript);
				break;

			case Game::EVENT_IF:
			{
				const auto* const conditionalScript = handler->eventData.conditionalScript;

				if (!conditionalScript)
				{
					break;
				}

				buffer->Align(Utils::Stream::ALIGN_4);

				auto* const destConditionalScript = buffer->Dest<Game::X86::ConditionalScript>();
				const auto conditionalRecord = Game::X86::Convert(*conditionalScript);
				buffer->Save(&conditionalRecord);

				this->Save_StatementPtr(conditionalScript->eventExpression, &destConditionalScript->eventExpression, builder);
				this->Save_MenuEventHandlerSetPtr(conditionalScript->eventHandlerSet, &destConditionalScript->eventHandlerSet, builder);

				Utils::Stream::ClearPointer(&dest->eventData.conditionalScript);
				break;
			}

			case Game::EVENT_ELSE:
				this->Save_MenuEventHandlerSetPtr(handler->eventData.elseScript, &dest->eventData.elseScript, builder);
				break;

			case Game::EVENT_SET_LOCAL_VAR_BOOL:
			case Game::EVENT_SET_LOCAL_VAR_INT:
			case Game::EVENT_SET_LOCAL_VAR_FLOAT:
			case Game::EVENT_SET_LOCAL_VAR_STRING:
			{
				const auto* const localVarData = handler->eventData.setLocalVarData;

				if (!localVarData)
				{
					break;
				}

				buffer->Align(Utils::Stream::ALIGN_4);

				auto* const destLocalVarData = buffer->Dest<Game::X86::SetLocalVarData>();
				const auto localVarRecord = Game::X86::Convert(*localVarData);
				buffer->Save(&localVarRecord);

				SaveXString(buffer, localVarData->localVarName, &destLocalVarData->localVarName);
				this->Save_StatementPtr(localVarData->expression, &destLocalVarData->expression, builder);

				Utils::Stream::ClearPointer(&dest->eventData.setLocalVarData);
				break;
			}

			default:
				break;
			}
		}

		Utils::Stream::ClearPointer(&destSet->eventHandlers);
	}

	void ImenuDef_t::Save_MenuEventHandlerSetPtr(const Game::MenuEventHandlerSet* asset, std::uint32_t* dest, Components::ZoneBuilder::Zone* builder)
	{
		if (!asset)
		{
			return;
		}

		builder->GetBuffer()->Align(Utils::Stream::ALIGN_4);
		this->Save_MenuEventHandlerSet(asset, builder);
		Utils::Stream::ClearPointer(dest);
	}

	void ImenuDef_t::Save_ItemKeyHandler(const Game::ItemKeyHandler* asset, Components::ZoneBuilder::Zone* builder)
	{
		auto* const buffer = builder->GetBuffer();

		while (asset)
		{
			auto* const dest = buffer->Dest<Game::X86::ItemKeyHandler>();
			const auto record = Game::X86::Convert(*asset);
			buffer->Save(&record);

			this->Save_MenuEventHandlerSetPtr(asset->action, &dest->action, builder);

			if (asset->next)
			{
				buffer->Align(Utils::Stream::ALIGN_4);
				Utils::Stream::ClearPointer(&dest->next);
			}

			asset = asset->next;
		}
	}

	void ImenuDef_t::Save_itemDefData_t(const Game::itemDefData_t* asset, int type, Game::X86::itemDef_s* dest, Components::ZoneBuilder::Zone* builder)
	{
		auto* const buffer = builder->GetBuffer();

		if (type == ITEM_TYPE_LISTBOX)
		{
			buffer->Align(Utils::Stream::ALIGN_4);

			auto* const destListBox = buffer->Dest<Game::X86::listBoxDef_s>();
			const auto record = Game::X86::Convert(*asset->listBox);
			buffer->Save(&record);

			this->Save_MenuEventHandlerSetPtr(asset->listBox->onDoubleClick, &destListBox->onDoubleClick, builder);

			if (asset->listBox->selectIcon)
			{
				destListBox->selectIcon = builder->SaveSubAsset(Game::ASSET_TYPE_MATERIAL, asset->listBox->selectIcon);
			}
		}
		else if (std::ranges::find(editFieldItemTypes, type) == std::end(editFieldItemTypes))
		{
			switch (type)
			{
			case ITEM_TYPE_DVARENUM:
				buffer->SaveString(asset->enumDvarName);
				break;

			case ITEM_TYPE_NEWS_TICKER:
			{
				buffer->Align(Utils::Stream::ALIGN_4);

				const auto record = Game::X86::Convert(*asset->ticker);
				buffer->Save(&record);
				break;
			}

			case ITEM_TYPE_TEXT_SCROLL:
			{
				buffer->Align(Utils::Stream::ALIGN_4);

				const auto record = Game::X86::Convert(*asset->scroll);
				buffer->Save(&record);
				break;
			}

			case ITEM_TYPE_MULTI:
			{
				buffer->Align(Utils::Stream::ALIGN_4);

				auto* const destMulti = buffer->Dest<Game::X86::multiDef_s>();
				const auto record = Game::X86::Convert(*asset->multi);
				buffer->Save(&record);

				for (int i = 0; i < 32; ++i)
				{
					SaveXString(buffer, asset->multi->dvarList[i], &destMulti->dvarList[i]);
				}

				for (int i = 0; i < 32; ++i)
				{
					SaveXString(buffer, asset->multi->dvarStr[i], &destMulti->dvarStr[i]);
				}

				break;
			}

			default:
				break;
			}
		}
		else
		{
			buffer->Align(Utils::Stream::ALIGN_4);

			const auto record = Game::X86::Convert(*asset->editField);
			buffer->Save(&record);
		}

		Utils::Stream::ClearPointer(&dest->typeData.data);
	}

	void ImenuDef_t::Save_itemDef_s(const Game::itemDef_s* asset, Components::ZoneBuilder::Zone* builder)
	{
		auto* const buffer = builder->GetBuffer();
		auto* const dest = buffer->Dest<Game::X86::itemDef_s>();

		auto record = Game::X86::Convert(*asset);
		record.typeData.data = 0;
		buffer->Save(&record);

		this->Save_windowDef_t(&asset->window, &dest->window, builder);

		SaveXString(buffer, asset->text, &dest->text);

		this->Save_MenuEventHandlerSetPtr(asset->mouseEnterText, &dest->mouseEnterText, builder);
		this->Save_MenuEventHandlerSetPtr(asset->mouseExitText, &dest->mouseExitText, builder);
		this->Save_MenuEventHandlerSetPtr(asset->mouseEnter, &dest->mouseEnter, builder);
		this->Save_MenuEventHandlerSetPtr(asset->mouseExit, &dest->mouseExit, builder);
		this->Save_MenuEventHandlerSetPtr(asset->action, &dest->action, builder);
		this->Save_MenuEventHandlerSetPtr(asset->accept, &dest->accept, builder);
		this->Save_MenuEventHandlerSetPtr(asset->onFocus, &dest->onFocus, builder);
		this->Save_MenuEventHandlerSetPtr(asset->leaveFocus, &dest->leaveFocus, builder);

		SaveXString(buffer, asset->dvar, &dest->dvar);
		SaveXString(buffer, asset->dvarTest, &dest->dvarTest);

		if (asset->onKey)
		{
			buffer->Align(Utils::Stream::ALIGN_4);
			this->Save_ItemKeyHandler(asset->onKey, builder);
			Utils::Stream::ClearPointer(&dest->onKey);
		}

		SaveXString(buffer, asset->enableDvar, &dest->enableDvar);
		SaveXString(buffer, asset->localVar, &dest->localVar);

		if (asset->focusSound)
		{
			dest->focusSound = builder->SaveSubAsset(Game::ASSET_TYPE_SOUND, asset->focusSound);
		}

		if (asset->typeData.data)
		{
			this->Save_itemDefData_t(&asset->typeData, asset->type, dest, builder);
		}

		if (asset->floatExpressions)
		{
			buffer->Align(Utils::Stream::ALIGN_4);

			auto* const destExpressions = buffer->Dest<Game::X86::ItemFloatExpression>();

			for (int i = 0; i < asset->floatExpressionCount; ++i)
			{
				const auto expressionRecord = Game::X86::Convert(asset->floatExpressions[i]);
				buffer->Save(&expressionRecord);
			}

			for (int i = 0; i < asset->floatExpressionCount; ++i)
			{
				this->Save_StatementPtr(asset->floatExpressions[i].expression, &destExpressions[i].expression, builder);
			}

			Utils::Stream::ClearPointer(&dest->floatExpressions);
		}

		this->Save_StatementPtr(asset->visibleExp, &dest->visibleExp, builder);
		this->Save_StatementPtr(asset->disabledExp, &dest->disabledExp, builder);
		this->Save_StatementPtr(asset->textExp, &dest->textExp, builder);
		this->Save_StatementPtr(asset->materialExp, &dest->materialExp, builder);
	}

	void ImenuDef_t::Save(Game::XAssetHeader header, Components::ZoneBuilder::Zone* builder)
	{
		auto* const buffer = builder->GetBuffer();
		const auto* const asset = header.menu;
		auto* const dest = buffer->Dest<Game::X86::menuDef_t>();
		const auto record = Game::X86::Convert(*asset);
		buffer->Save(&record);

		buffer->PushBlock(Game::XFILE_BLOCK_VIRTUAL);

		if (asset->expressionData)
		{
			this->Save_ExpressionSupportingData(asset->expressionData, builder);
			Utils::Stream::ClearPointer(&dest->expressionData);
		}

		this->Save_windowDef_t(&asset->window, &dest->window, builder);

		SaveXString(buffer, asset->font, &dest->font);

		this->Save_MenuEventHandlerSetPtr(asset->onOpen, &dest->onOpen, builder);
		this->Save_MenuEventHandlerSetPtr(asset->onClose, &dest->onClose, builder);
		this->Save_MenuEventHandlerSetPtr(asset->onCloseRequest, &dest->onCloseRequest, builder);
		this->Save_MenuEventHandlerSetPtr(asset->onESC, &dest->onESC, builder);

		if (asset->onKey)
		{
			buffer->Align(Utils::Stream::ALIGN_4);
			this->Save_ItemKeyHandler(asset->onKey, builder);
			Utils::Stream::ClearPointer(&dest->onKey);
		}

		this->Save_StatementPtr(asset->visibleExp, &dest->visibleExp, builder);

		SaveXString(buffer, asset->allowedBinding, &dest->allowedBinding);
		SaveXString(buffer, asset->soundName, &dest->soundName);

		this->Save_StatementPtr(asset->rectXExp, &dest->rectXExp, builder);
		this->Save_StatementPtr(asset->rectYExp, &dest->rectYExp, builder);
		this->Save_StatementPtr(asset->rectWExp, &dest->rectWExp, builder);
		this->Save_StatementPtr(asset->rectHExp, &dest->rectHExp, builder);
		this->Save_StatementPtr(asset->openSoundExp, &dest->openSoundExp, builder);
		this->Save_StatementPtr(asset->closeSoundExp, &dest->closeSoundExp, builder);

		if (asset->items)
		{
			buffer->Align(Utils::Stream::ALIGN_4);
			SavePointerArray(buffer, asset->items, asset->itemCount);

			for (int i = 0; i < asset->itemCount; ++i)
			{
				if (asset->items[i])
				{
					buffer->Align(Utils::Stream::ALIGN_4);
					this->Save_itemDef_s(asset->items[i], builder);
				}
			}

			Utils::Stream::ClearPointer(&dest->items);
		}

		buffer->PopBlock();
	}
}
