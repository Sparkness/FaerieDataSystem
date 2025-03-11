﻿// Copyright Guy (Drakynfly) Lundvall. All Rights Reserved.

#include "FaerieCardGenerator.h"
#include "CardTokens/FaerieItemCardToken.h"

#include "FaerieItemDataProxy.h"
#include "FaerieItem.h"
#include "FaerieItemCardModule.h"
#include "Engine/AssetManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(FaerieCardGenerator)

TSoftClassPtr<UFaerieCardBase> UFaerieCardGenerator::GetCardClassFromProxy(const FFaerieItemProxy Proxy, const FGameplayTag& Type) const
{
	auto&& Item = Proxy.GetItemObject();

	if (!IsValid(Item))
	{
		UE_LOG(LogFaerieItemCard, Warning, TEXT("Unable to determine card class: Invalid Item!"))
		return nullptr;
	}

	if (auto&& CardClassProvider = Item->GetToken<UFaerieItemCardToken>())
	{
		if (auto&& Class = CardClassProvider->GetCardClass(Type);
			Class.IsValid())
		{
			return Class;
		}

		UE_LOG(LogFaerieItemCard, Warning, TEXT("CustomCard token contained invalid class (%s). Reverting to default!"), *Type.GetTagName().ToString())
	}

	if (auto&& Class = DefaultClasses.Find(Type))
	{
		return *Class;
	}

	UE_LOG(LogFaerieItemCard, Warning, TEXT("Unable to determine card class: No default for '%s'!"), *Type.GetTagName().ToString())

	return nullptr;
}

UFaerieCardBase* UFaerieCardGenerator::Generate(const Faerie::Card::FSyncGeneration& Params)
{
	if (!Params.Proxy.IsValid() ||
		!IsValid(Params.Player) ||
		IsValid(Params.CardType))
	{
		UE_LOG(LogFaerieItemCard, Warning, TEXT("Invalid Params for generation!"))
		return nullptr;
	}

	if (const TSoftClassPtr<UFaerieCardBase> CardClass = GetCardClassFromProxy(Params.Proxy, Params.Tag);
		IsValid(CardClass.LoadSynchronous()))
	{
		UFaerieCardBase* CardWidget = CreateWidget<UFaerieCardBase>(Params.Player, CardClass.Get());

		if (IsValid(CardWidget))
		{
			CardWidget->SetItemData(Params.Proxy, false);
		}

		return CardWidget;
	}
	return nullptr;
}

void UFaerieCardGenerator::GenerateAsync(const Faerie::Card::FAsyncGeneration& Params)
{
	if (!Params.Proxy.IsValid() ||
		!IsValid(Params.Player) ||
		!IsValid(Params.CardType))
	{
		UE_LOG(LogFaerieItemCard, Warning, TEXT("Invalid Params for generation!"))
		Params.Callback.ExecuteIfBound(false, nullptr);
		return;
	}

	if (const TSoftClassPtr<UFaerieCardBase> CardClass = GetCardClassFromProxy(Params.Proxy, Params.Tag);
		!CardClass.IsNull())
	{
		UAssetManager::GetStreamableManager().RequestAsyncLoad(CardClass.ToSoftObjectPath(),
			FStreamableDelegate::CreateUObject(this, &ThisClass::OnCardClassLoaded, FAsyncCallback{Params.Player, Params.Proxy, CardClass, Params.Callback}));
	}
	else
	{
		Params.Callback.ExecuteIfBound(false, nullptr);
	}
}

void UFaerieCardGenerator::OnCardClassLoaded(FAsyncCallback Params)
{
	if (const TSubclassOf<UFaerieCardBase> LoadedClass = Params.CardClass.Get();
		IsValid(LoadedClass) && IsValid(Params.Player))
	{
		UFaerieCardBase* CardWidget = CreateWidget<UFaerieCardBase>(Params.Player, LoadedClass);

		if (IsValid(CardWidget))
		{
			CardWidget->SetItemData(Params.Proxy, false);
		}

		Params.Callback.ExecuteIfBound(IsValid(CardWidget), CardWidget);
	}
	else
	{
		UE_LOG(LogFaerieItemCard, Warning, TEXT("Generation failed: Async load failed!"))

		Params.Callback.ExecuteIfBound(false, nullptr);
	}
}