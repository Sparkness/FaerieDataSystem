// Copyright Guy (Drakynfly) Lundvall. All Rights Reserved.

#include "Extensions/InventorySpatialGridExtension.h"

#include "FaerieItemContainerBase.h"
#include "FaerieItemStorage.h"
#include "ItemContainerEvent.h"
#include "Net/UnrealNetwork.h"
#include "Tokens/FaerieShapeToken.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InventorySpatialGridExtension)

void FSpatialKeyedEntry::PreReplicatedRemove(const FSpatialContent& InArraySerializer)
{
	InArraySerializer.PreEntryReplicatedRemove(*this);
}

void FSpatialKeyedEntry::PostReplicatedAdd(FSpatialContent& InArraySerializer)
{
	InArraySerializer.PostEntryReplicatedAdd(*this);
}

void FSpatialKeyedEntry::PostReplicatedChange(const FSpatialContent& InArraySerializer)
{
	InArraySerializer.PostEntryReplicatedChange(*this);
}

bool FSpatialContent::EditItem(const FInventoryKey Key, const TFunctionRef<bool(FSpatialItemPlacement&)>& Func)
{
	if (const int32 Index = IndexOf(Key);
		Index != INDEX_NONE)
	{
		if (FSpatialKeyedEntry& Entry = Items[Index];
			Func(Entry.Value))
		{
			MarkItemDirty(Entry);
			PostEntryReplicatedChange(Entry);
			return true;
		}
	}
	return false;
}

void FSpatialContent::PreEntryReplicatedRemove(const FSpatialKeyedEntry& Entry) const
{
	if (ChangeListener.IsValid())
	{
		ChangeListener->PreEntryReplicatedRemove(Entry);
	}
}

void FSpatialContent::PostEntryReplicatedAdd(const FSpatialKeyedEntry& Entry)
{
	if (ChangeListener.IsValid())
	{
		ChangeListener->PostEntryReplicatedAdd(Entry);
	}
}

void FSpatialContent::PostEntryReplicatedChange(const FSpatialKeyedEntry& Entry) const
{
	if (ChangeListener.IsValid())
	{
		ChangeListener->PostEntryReplicatedChange(Entry);
	}
}

void FSpatialContent::Insert(FInventoryKey Key, const FSpatialItemPlacement& Value)
{
	check(Key.IsValid())

	FSpatialKeyedEntry& NewEntry = BSOA::Insert({Key, Value});

	PostEntryReplicatedAdd(NewEntry);
	MarkItemDirty(NewEntry);
}

void FSpatialContent::Remove(const FInventoryKey Key)
{
	if (BSOA::Remove(Key,
			[this](const FSpatialKeyedEntry& Entry)
			{
				// Notify owning server of this removal.
				PreEntryReplicatedRemove(Entry);
			}))
	{
		// Notify clients of this removal.
		MarkArrayDirty();
	}
}

void UInventorySpatialGridExtension::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams SharedParams;
	SharedParams.bIsPushBased = true;

	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, SpatialEntries, SharedParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, GridSize, SharedParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, InitializedContainer, SharedParams);
}

void UInventorySpatialGridExtension::PostInitProperties()
{
	Super::PostInitProperties();
	SpatialEntries.ChangeListener = this;
}

void UInventorySpatialGridExtension::InitializeExtension(const UFaerieItemContainerBase* Container)
{
	checkf(!IsValid(InitializedContainer), TEXT("InventorySpatialGridExtension doesn't support multi-initialization!"))
	InitializedContainer = Container;
	MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, InitializedContainer, this);

	// Add all existing items to the grid on startup.
	// This is dumb, and just adds them in order, it doesn't space pack them. To do that, we would want to sort items by size, and add largest first.
	// This is also skipping possible serialization of grid data.
	// @todo handle serialization loading
	// @todo handle items that are too large to fix / too many items (log error?)
	OccupiedCells.SetNum(GridSize.X * GridSize.Y, false);
	if (const UFaerieItemStorage* ItemStorage = Cast<UFaerieItemStorage>(Container))
	{
		ItemStorage->ForEachKey(
			[this, ItemStorage](const FEntryKey Key)
			{
				const FFaerieItemStackView View = ItemStorage->View(Key);
				const UFaerieShapeToken* Token = View.Item->GetToken<UFaerieShapeToken>();

				const TArray<FInventoryKey> InvKeys = ItemStorage->GetInvKeysForEntry(Key);

				for (auto&& InvKey : InvKeys)
				{
					AddItemToGrid(InvKey, Token);
				}
			});
	}
}

void UInventorySpatialGridExtension::DeinitializeExtension(const UFaerieItemContainerBase* Container)
{
	// Remove all entries for this container on shutdown.
	Container->ForEachKey(
		[this](const FEntryKey Key)
		{
			RemoveItemsForEntry(Key);
		});

	InitializedContainer = nullptr;
	MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, InitializedContainer, this);
}

EEventExtensionResponse UInventorySpatialGridExtension::AllowsAddition(const UFaerieItemContainerBase* Container,
																	   const FFaerieItemStackView Stack,
																	   EFaerieStorageAddStackBehavior)
{
	// @todo add boolean in config to allow items without a shape
	if (!CanAddItemToGrid(Stack.Item->GetToken<UFaerieShapeToken>()))
	{
		return EEventExtensionResponse::Disallowed;
	}

	return EEventExtensionResponse::Allowed;
}

void UInventorySpatialGridExtension::PostAddition(const UFaerieItemContainerBase* Container,
												const Faerie::Inventory::FEventLog& Event)
{
	// @todo don't add items for existing keys

	FInventoryKey NewKey;
	NewKey.EntryKey = Event.EntryTouched;

	for (const FStackKey& StackKey : Event.StackKeys)
	{
		NewKey.StackKey = StackKey;
		if (const UFaerieShapeToken* ShapeToken = Event.Item->GetToken<UFaerieShapeToken>())
		{
			AddItemToGrid(NewKey, ShapeToken);
		}
		else
		{
			AddItemToGrid(NewKey, nullptr);
		}
	}
}

void UInventorySpatialGridExtension::PostRemoval(const UFaerieItemContainerBase* Container,
												const Faerie::Inventory::FEventLog& Event)
{
	if (const UFaerieItemStorage* ItemStorage = Cast<UFaerieItemStorage>(Container))
	{
		if (!ItemStorage->IsValidKey(Event.EntryTouched))
		{
			RemoveItemsForEntry(Event.EntryTouched);
		}
		else
		{
			FInventoryKey Key;
			Key.EntryKey = Event.EntryTouched;

			for (const FStackKey& StackKey : Event.StackKeys)
			{
				Key.StackKey = StackKey;
				if (!ItemStorage->IsValidKey(Key))
				{
					RemoveItem(Key);
				}
			}
		}
	}
}

void UInventorySpatialGridExtension::PreEntryReplicatedRemove(const FSpatialKeyedEntry& Entry)
{
	// This is to account for removals through proxies that don't directly interface with the grid
	for (const FFaerieGridShape Translated = ApplyPlacement(GetItemShape(Entry.Key.EntryKey), Entry.Value);
		 const FIntPoint& Point : Translated.Points)
	{
		OccupiedCells[Ravel(Point)] = false;
	}

	SpatialEntryChangedDelegateNative.Broadcast(Entry.Key, ESpatialEventType::ItemRemoved);
	SpatialEntryChangedDelegate.Broadcast(Entry.Key, ESpatialEventType::ItemRemoved);
}

void UInventorySpatialGridExtension::PostEntryReplicatedAdd(const FSpatialKeyedEntry& Entry)
{
	SpatialEntryChangedDelegateNative.Broadcast(Entry.Key, ESpatialEventType::ItemAdded);
	SpatialEntryChangedDelegate.Broadcast(Entry.Key, ESpatialEventType::ItemAdded);
}

void UInventorySpatialGridExtension::PostEntryReplicatedChange(const FSpatialKeyedEntry& Entry)
{
	SpatialEntryChangedDelegateNative.Broadcast(Entry.Key, ESpatialEventType::ItemChanged);
	SpatialEntryChangedDelegate.Broadcast(Entry.Key, ESpatialEventType::ItemChanged);
}

void UInventorySpatialGridExtension::OnRep_GridSize()
{
	GridSizeChangedDelegateNative.Broadcast(GridSize);
	GridSizeChangedDelegate.Broadcast(GridSize);
}

bool UInventorySpatialGridExtension::CanAddItemToGrid(const UFaerieShapeToken* ShapeToken) const
{
	const FSpatialItemPlacement TestPlacement = FindFirstEmptyLocation(ShapeToken->GetShape());
	return TestPlacement.Origin != FIntPoint::NoneValue;
}

bool UInventorySpatialGridExtension::AddItemToGrid(const FInventoryKey& Key, const UFaerieShapeToken* ShapeToken)
{
	if (!Key.IsValid())
	{
		return false;
	}

	if (SpatialEntries.Find(Key) != nullptr)
	{
		return true;
	}

	const FFaerieGridShape Shape = ShapeToken ? ShapeToken->GetShape() : FFaerieGridShape::MakeSquare(1);

	const FSpatialItemPlacement DesiredItemPlacement = FindFirstEmptyLocation(Shape);

	if (DesiredItemPlacement.Origin == FIntPoint::NoneValue)
	{
		return false;
	}

	SpatialEntries.Insert(Key, DesiredItemPlacement);

	for (const FFaerieGridShape Translated = ApplyPlacement(Shape, DesiredItemPlacement);
		 const FIntPoint& Point : Translated.Points)
	{
		OccupiedCells[Ravel(Point)] = true;
	}
	return true;
}

void UInventorySpatialGridExtension::RemoveItem(const FInventoryKey& Key)
{
	SpatialEntries.Remove(Key);
}

void UInventorySpatialGridExtension::RemoveItemsForEntry(const FEntryKey& Key)
{
	auto&& ItemShape = GetItemShape(Key);

	TArray<FInventoryKey> Keys;
	for (auto&& Element : SpatialEntries)
	{
		if (Element.Key.EntryKey == Key)
		{
			Keys.Add(Element.Key);

			for (const FFaerieGridShape Translated = ApplyPlacement(ItemShape, Element.Value);
				 const FIntPoint& Point : Translated.Points)
			{
				OccupiedCells[Ravel(Point)] = false;
			}
		}
	}

	for (auto&& InvKey : Keys)
	{
		RemoveItem(InvKey);
	}
}

FSpatialItemPlacement UInventorySpatialGridExtension::GetEntryPlacementData(const FInventoryKey& Key) const
{
	if (auto&& Placement = SpatialEntries.Find(Key))
	{
		return *Placement;
	}

	return FSpatialItemPlacement();
}

FIntPoint UInventorySpatialGridExtension::GetEntryBounds(const FInventoryKey& Entry) const
{
	return GetItemShape(Entry.EntryKey).GetSize();
}

bool UInventorySpatialGridExtension::FitsInGrid(const FFaerieGridShapeConstView& Shape, const FSpatialItemPlacement& PlacementData, const TConstArrayView<FInventoryKey> ExcludedKeys, FIntPoint* OutCandidate) const
{
	// Build list of excluded indices
	TArray<int32> ExcludedIndices;
	ExcludedIndices.Reserve(ExcludedKeys.Num() * Shape.Points.Num());
	for (const FInventoryKey& Key : ExcludedKeys)
	{
		const FFaerieGridShapeConstView OtherShape = GetItemShape(Key.EntryKey);
		const FSpatialItemPlacement Entry = GetEntryPlacementData(Key);
		for (const FFaerieGridShape Translated = ApplyPlacement(OtherShape, Entry);
			 const auto& Point : Translated.Points)
		{
			ExcludedIndices.Add(Ravel(Point));
		}
	}

	// Check if all points in the shape fit within the grid and don't overlap with occupied cells
	for (const FFaerieGridShape Translated = ApplyPlacement(Shape, PlacementData);
		 const FIntPoint& Point : Translated.Points)
	{
		// Check if point is within grid bounds
		if (Point.X < 0 || Point.X >= GridSize.X ||
			Point.Y < 0 || Point.Y >= GridSize.Y)
		{
			return false;
		}

		// If this index is not in the excluded list, check if it's occupied
		if (const int32 BitGridIndex = Ravel(Point);
			!ExcludedIndices.Contains(BitGridIndex) && OccupiedCells[BitGridIndex])
		{
			if (OutCandidate)
			{
				// Skip past this occupied cell
				OutCandidate->X = Point.X;
				OutCandidate->Y = PlacementData.Origin.Y;
			}
			return false;
		}
	}

	return true;
}

FSpatialItemPlacement UInventorySpatialGridExtension::FindFirstEmptyLocation(const FFaerieGridShape& Shape) const
{
	// Early exit if grid is empty or invalid
	if (GridSize.X <= 0 || GridSize.Y <= 0)
	{
		return FSpatialItemPlacement{FIntPoint::NoneValue};
	}

	// Determine which rotations to check
	TArray<ESpatialItemRotation> RotationRange;
	if (Shape.IsSymmetrical())
	{
		RotationRange.Add(ESpatialItemRotation::None);
	}
	else
	{
		for (const ESpatialItemRotation Rotation : TEnumRange<ESpatialItemRotation>())
		{
			RotationRange.Add(Rotation);
		}
	}

	int32 Min = TNumericLimits<int32>::Max();
	int32 Max = TNumericLimits<int32>::Min();
	for (const FIntPoint& Point : Shape.Points)
	{
		Min = FMath::Min(Min, Point.X);
		Max = FMath::Max(Max, Point.X);
	}

	FSpatialItemPlacement TestPlacement;

	// For each cell in the grid
	FIntPoint TestPoints = FIntPoint::ZeroValue;
	for (TestPoints.Y = 0; TestPoints.Y < GridSize.Y; TestPoints.Y++)
	{
		for (TestPoints.X = 0; TestPoints.X < GridSize.X; TestPoints.X++)
		{
			// Skip if current cell is occupied
			if (OccupiedCells[TestPoints.Y * GridSize.X + TestPoints.X])
			{
				continue;
			}
			// Try each rotation at this potential origin point
			TestPlacement.Origin = FIntPoint(TestPoints.X, TestPoints.Y);
			for (const ESpatialItemRotation Rotation : RotationRange)
			{
				TestPlacement.Rotation = Rotation;
				if (FitsInGrid(Shape, TestPlacement, {}, &TestPoints))
				{
					return TestPlacement;
				}
			}
		}
	}
	// No valid placement found
	return FSpatialItemPlacement{FIntPoint::NoneValue};
}

FFaerieGridShapeConstView UInventorySpatialGridExtension::GetItemShape(const FEntryKey Key) const
{
	if (IsValid(InitializedContainer))
	{
		if (const FFaerieItemStackView View = InitializedContainer->View(Key);
			View.Item.IsValid())
		{
			if (const UFaerieShapeToken* ShapeToken = View.Item->GetToken<UFaerieShapeToken>())
			{
				return ShapeToken->GetShape();
			}
		}
	}

	return FFaerieGridShapeConstView{};
}

void UInventorySpatialGridExtension::SetGridSize(const FIntPoint NewGridSize)
{
	if (GridSize != NewGridSize)
	{
		const FIntPoint OldSize = GridSize;
		TBitArray<> OldOccupied = OccupiedCells;

		// Resize to new dimensions
		GridSize = NewGridSize;
		OccupiedCells.Init(false, GridSize.X * GridSize.Y);

		// Copy over existing data that's still in bounds
		for (int32 y = 0; y < FMath::Min(OldSize.Y, GridSize.Y); y++)
		{
			for (int32 x = 0; x < FMath::Min(OldSize.X, GridSize.X); x++)
			{
				const int32 OldIndex = x + y * OldSize.X;
				const int32 NewIndex = x + y * GridSize.X;
				OccupiedCells[NewIndex] = OldOccupied[OldIndex];
			}
		}

		MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, GridSize, this);

		// OnReps must be called manually on the server in c++
		OnRep_GridSize();
	}
}

bool UInventorySpatialGridExtension::MoveItem(const FInventoryKey& Key, const FIntPoint& TargetPoint)
{
	return SpatialEntries.EditItem(Key,
		[&](FSpatialItemPlacement& Placement)
		{
			const FFaerieGridShapeConstView ItemShape = GetItemShape(Key.EntryKey);

			// Get the rotated shape based on current entry rotation so we can correctly get items that would overlap
			const FFaerieGridShape Translated = ApplyPlacement(ItemShape, Placement);

			if (FSpatialKeyedEntry* OverlappingItem = FindOverlappingItem(Translated, Key))
			{
				if (TrySwapItems(
					Key, Placement,
					OverlappingItem->Key, OverlappingItem->Value))
				{
					// @todo this is gross
					SpatialEntries.MarkItemDirty(*OverlappingItem);
					return true;
				}
				return false;
			}

			return MoveSingleItem(Key, Placement, TargetPoint);
		});
}

int32 UInventorySpatialGridExtension::Ravel(const FIntPoint& Point) const
{
	return Point.Y * GridSize.X + Point.X;
}

FIntPoint UInventorySpatialGridExtension::Unravel(const int32 Index) const
{
	const int32 X = Index % GridSize.X;
	const int32 Y = Index / GridSize.X;
	return FIntPoint{ X, Y };
}

FFaerieGridShape UInventorySpatialGridExtension::ApplyPlacement(const FFaerieGridShapeConstView& Shape, const FSpatialItemPlacement& Placement)
{
	return Shape.Copy().Rotate(Placement.Rotation).Translate(Placement.Origin);
}

FSpatialKeyedEntry* UInventorySpatialGridExtension::FindOverlappingItem(const FFaerieGridShape& TranslatedShape,
																		const FInventoryKey& ExcludeKey)
{
	return SpatialEntries.Items.FindByPredicate(
		[this, &TranslatedShape, ExcludeKey](const FSpatialKeyedEntry& Other)
		{
			if (ExcludeKey == Other.Key)
			{
				return false;
			}

			const FFaerieGridShapeConstView OtherItemShape = GetItemShape(Other.Key.EntryKey);

			// Create a rotated and translated version of the other item's shape
			const FFaerieGridShape OtherTranslatedShape = ApplyPlacement(OtherItemShape, Other.Value);

			return TranslatedShape.Contains(OtherTranslatedShape);
		});
}

bool UInventorySpatialGridExtension::TrySwapItems(const FInventoryKey KeyA, FSpatialItemPlacement& PlacementA, const FInventoryKey KeyB, FSpatialItemPlacement& PlacementB)
{
	// Store original positions
	const FIntPoint OriginA = PlacementA.Origin;
	const FIntPoint OriginB = PlacementB.Origin;

	// Get rotated shapes for both items
	FSpatialItemPlacement PlacementCopyA = PlacementA;
	FSpatialItemPlacement PlacementCopyB = PlacementB;

	const FFaerieGridShapeConstView ShapeA = GetItemShape(KeyA.EntryKey);
	const FFaerieGridShapeConstView ShapeB = GetItemShape(KeyB.EntryKey);

	// Check if both items would fit in their new positions
	PlacementCopyA.Origin = OriginB;
	PlacementCopyB.Origin = OriginA;

	// This is a first check mainly to see if the item would fit inside the grids bounds
	if (!FitsInGrid(ShapeA, PlacementCopyA, MakeArrayView(&KeyB, 1)) ||
		!FitsInGrid(ShapeB, PlacementCopyB, MakeArrayView(&KeyA, 1)))
	{
		return false;
	}

	UpdateItemPosition(KeyA, PlacementA, OriginB);
	UpdateItemPosition(KeyB, PlacementB, OriginA);

	// Check if both items can exist in their new positions without overlapping
	bool bValidSwap = true;
	for (const FIntPoint& Point : ShapeA.Points)
	{
		for (const FIntPoint& OtherPoint : ShapeB.Points)
		{
			if (Point + PlacementA.Origin == OtherPoint + PlacementB.Origin)
			{
				bValidSwap = false;
				break;
			}
		}
		if (!bValidSwap)
		{
			break;
		}
	}

	// Revert to original positions if validation fails
	if (!bValidSwap)
	{
		UpdateItemPosition(KeyA, PlacementA, OriginA);
		UpdateItemPosition(KeyB, PlacementB, OriginB);
		return false;
	}

	return true;
}

bool UInventorySpatialGridExtension::MoveSingleItem(const FInventoryKey Key, FSpatialItemPlacement& Placement, const FIntPoint& NewPosition)
{
	const FFaerieGridShapeConstView ItemShape = GetItemShape(Key.EntryKey);
	FSpatialItemPlacement PlacementCopy = Placement;
	PlacementCopy.Origin = NewPosition;
	if (!FitsInGrid(ItemShape, PlacementCopy, MakeArrayView(&Key, 1)))
	{
		return false;
	}

	UpdateItemPosition(Key, Placement, NewPosition);
	return true;
}

void UInventorySpatialGridExtension::UpdateItemPosition(const FInventoryKey Key, FSpatialItemPlacement& Placement, const FIntPoint& NewPosition)
{
	const FFaerieGridShapeConstView ItemShape = GetItemShape(Key.EntryKey);

	// We could have the same index in both the add and removal so we need to clear first
	const FFaerieGridShape Rotated = ItemShape.Copy().Rotate(Placement.Rotation);

	// Clear old positions first
	for (auto& Point : Rotated.Points)
	{
		const FIntPoint OldPoint = Placement.Origin + Point;
		OccupiedCells[Ravel(OldPoint)] = false;
	}

	// Then set new positions
	for (auto& Point : Rotated.Points)
	{
		const FIntPoint Translated = NewPosition + Point;
		OccupiedCells[Ravel(Translated)] = true;
	}

	Placement.Origin = NewPosition;
}

bool UInventorySpatialGridExtension::RotateItem(const FInventoryKey& Key)
{
	return SpatialEntries.EditItem(Key,
		[this, Key](FSpatialItemPlacement& Placement)
		{
			const FFaerieGridShapeConstView ItemShape = GetItemShape(Key.EntryKey);

			// No Point in Trying to Rotate
			if (ItemShape.IsSymmetrical()) return false;

			const ESpatialItemRotation NextRotation = GetNextRotation(Placement.Rotation);

			FSpatialItemPlacement TempPlacementData = Placement;
			TempPlacementData.Rotation = NextRotation;
			if (!FitsInGrid(ItemShape, TempPlacementData, MakeArrayView(&Key, 1)))
			{
				return false;
			}

			// Store old points before transformations so we can clear them from the bit grid
			const FFaerieGridShape OldShape = ApplyPlacement(ItemShape, Placement);
			Placement.Rotation = NextRotation;

			// Clear old occupied cells
			for (const auto& OldPoint : OldShape.Points)
			{
				OccupiedCells[Ravel(OldPoint)] = false;
			}

			// Set new occupied cells taking into account rotation
			const FFaerieGridShape NewShape = ApplyPlacement(ItemShape, Placement);
			for (const auto& Point : NewShape.Points)
			{
				OccupiedCells[Ravel(Point)] = true;
			}

			return true;
		});
}