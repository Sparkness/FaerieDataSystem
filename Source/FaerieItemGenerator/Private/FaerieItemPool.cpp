﻿// Copyright Guy (Drakynfly) Lundvall. All Rights Reserved.

#include "FaerieItemPool.h"
#include "FaerieAssetInfo.h"
#include "ItemInstancingContext_Crafting.h"

#include "Squirrel.h"
#include "Algo/AnyOf.h"
#include "UObject/ObjectSaveContext.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(FaerieItemPool)

const FTableDrop* FFaerieWeightedDropPool::GetDrop(const double RanWeight) const
{
	if (DropList.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("Exiting generation: Empty Table"));
		return nullptr;
	}

	// Skip performing binary search if there is only one possible result.
	if (DropList.Num() == 1)
	{
		return &DropList[0].Drop;
	}

	const int32 BinarySearchResult = Algo::LowerBoundBy(DropList, RanWeight, &FWeightedDrop::AdjustedWeight);

	if (!DropList.IsValidIndex(BinarySearchResult))
	{
		UE_LOG(LogTemp, Error, TEXT("Binary search returned out-of-bounds index!"));
		return nullptr;
	}

	return &DropList[BinarySearchResult].Drop;
}

#if WITH_EDITOR
void FFaerieWeightedDropPool::CalculatePercentages()
{
	/**
	 * Sum all weights into a total weight value, while also adjusting the weight of each drop to include to weight
	 * of all drops before it.
	 */

	int32 WeightSum = 0;
	for (FWeightedDrop& Entry : DropList)
	{
		WeightSum += Entry.Weight;
		Entry.AdjustedWeight = WeightSum;
	}

	for (FWeightedDrop& Entry : DropList)
	{
		Entry.AdjustedWeight /= WeightSum;
		Entry.PercentageChanceToDrop = 100.f * (static_cast<float>(Entry.Weight) / static_cast<float>(WeightSum));
	}
}

void FFaerieWeightedDropPool::SortTable()
{
	Algo::SortBy(DropList, &FWeightedDrop::Weight);
}
#endif

UFaerieItemPool::UFaerieItemPool()
{
	TableInfo.ObjectName = FText::FromString("<Unnamed Table>");
}

void UFaerieItemPool::PreSave(FObjectPreSaveContext SaveContext)
{
	Super::PreSave(SaveContext);

#if WITH_EDITOR
	DropPool.SortTable();

	HasMutableDrops = Algo::AnyOf(DropPool.DropList,
		[](const FWeightedDrop& Drop)
		{
			auto&& Interface = Cast<IFaerieItemSource>(Drop.Drop.Asset.Object.LoadSynchronous());
			return Interface && Interface->CanBeMutable();
		});
#endif
}

void UFaerieItemPool::PostLoad()
{
	Super::PostLoad();
#if WITH_EDITOR
	DropPool.CalculatePercentages();
#endif
}

#if WITH_EDITOR

#define LOCTEXT_NAMESPACE "FaerieItemPoolValidation"

EDataValidationResult UFaerieItemPool::IsDataValid(FDataValidationContext& Context) const
{
	TArray<FFaerieItemSourceObject> AssetList;

	for (const FWeightedDrop& Entry : DropPool.DropList)
	{
		if (!Entry.Drop.IsValid())
		{
			Context.AddWarning(LOCTEXT("DropTableInvalidAsset_Ref", "Invalid Asset Reference"));
		}
		else
		{
			if (AssetList.Contains(Entry.Drop.Asset))
			{
				Context.AddWarning(LOCTEXT("DropTableInvalidAsset_Dup", "Asset already exists in table. Please only have one weight per asset."));
			}
			else
			{
				AssetList.Add(Entry.Drop.Asset);
			}
		}
		if (Entry.Weight <= 0)
		{
			Context.AddWarning(LOCTEXT("DropTableInvalidWeight", "Weight must be larger than 0!"));
		}
	}
	if (Context.GetNumErrors()) return EDataValidationResult::Invalid;
	return Super::IsDataValid(Context);
}

#undef LOCTEXT_NAMESPACE

void UFaerieItemPool::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	DropPool.CalculatePercentages();
}

void UFaerieItemPool::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);
	DropPool.CalculatePercentages();
}
#endif

FFaerieAssetInfo UFaerieItemPool::GetSourceInfo() const
{
	return TableInfo;
}

UFaerieItem* UFaerieItemPool::CreateItemInstance(UObject* Outer) const
{
	return nullptr;
}

UFaerieItem* UFaerieItemPool::CreateItemInstance(const UItemInstancingContext* Context) const
{
	const UItemInstancingContext_Crafting* CraftingContent = Cast<UItemInstancingContext_Crafting>(Context);

	if (!IsValid(CraftingContent))
	{
		return nullptr;
	}

	FTableDrop Drop;

	if (IsValid(CraftingContent->Squirrel))
	{
		Drop = GenerateDrop(CraftingContent->Squirrel->NextReal());
	}
	else
	{
		Drop = GenerateDrop(FMath::FRand());
	}

	if (Drop.IsValid())
	{
		return Drop.Resolve(CraftingContent);
	}

	return nullptr;
}

const FTableDrop* UFaerieItemPool::GetDrop(const double RanWeight) const
{
	return DropPool.GetDrop(RanWeight);
}

const FTableDrop* UFaerieItemPool::GetDrop_Seeded(USquirrel* Squirrel) const
{
	return DropPool.GetDrop(Squirrel->NextReal());
}

FTableDrop UFaerieItemPool::GenerateDrop(const double RanWeight) const
{
	if (auto&& DropPtr = GetDrop(RanWeight))
	{
		return *DropPtr;
	}
	return FTableDrop();
}

FTableDrop UFaerieItemPool::GenerateDrop_Seeded(USquirrel* Squirrel) const
{
	if (auto&& DropPtr = GetDrop_Seeded(Squirrel))
	{
		return *DropPtr;
	}
	return FTableDrop();
}