// Copyright Guy (Drakynfly) Lundvall. All Rights Reserved.

#include "UI/InventoryContentsBase.h"
#include "UI/InventoryFillMeterBase.h"
#include "UI/InventoryUIAction.h"

#include "FaerieItemDataFilter.h"

#include "Extensions/InventoryCapacityExtension.h"

#include "Components/PanelWidget.h"

#define LOCTEXT_NAMESPACE "InventoryContentsBase"

DEFINE_LOG_CATEGORY(LogInventoryContents)

bool UInventoryContentsBase::Initialize()
{
	DefaultFilterProxyObject = NewObject<UInventoryEntryLiteral>(this);
	DefaultSortProxyAObject = NewObject<UInventoryEntryLiteral>(this);
	DefaultSortProxyBObject = NewObject<UInventoryEntryLiteral>(this);
	return Super::Initialize();
}

void UInventoryContentsBase::NativeConstruct()
{
	Super::NativeConstruct();

	// Resort and display items whenever we are reconstructed with an existing inventory.
	if (ItemStorage.IsValid())
	{
		InitWithInventory(ItemStorage.Get());
	}
}

void UInventoryContentsBase::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (NeedsResort)
	{
		TArray<FKeyedInventoryEntry> Entries;
		if (ItemStorage.IsValid())
		{
			ItemStorage->QueryAll(Query, Entries);
		}
		Faerie::Inventory::Utils::BreakKeyedEntriesIntoInventoryKeys(Entries, SortedAndFilteredKeys);
		NeedsReconstructEntries = true;
		NeedsResort = false;
	}

	if (NeedsReconstructEntries)
	{
		DisplaySortedEntries();
		NeedsReconstructEntries = false;
	}
}

void UInventoryContentsBase::Reset()
{
	SortedAndFilteredKeys.Empty();
	ActiveSortRule = nullptr;
	Query.InvertSort = false;
	Actions.Empty();

	ResetFilter(false);
	ResetSort(false);

	if (ItemStorage.IsValid())
	{
		ItemStorage->GetOnKeyAdded().RemoveAll(this);
		ItemStorage->GetOnKeyUpdated().RemoveAll(this);
		ItemStorage->GetOnKeyRemoved().RemoveAll(this);
	}

	OnReset();

	ItemStorage = nullptr;
}

void UInventoryContentsBase::CreateActions()
{
	for (TSubclassOf<UInventoryUIAction> ActionClass : ActionClasses)
	{
		if (UInventoryUIAction* NewAction = NewObject<UInventoryUIAction>(this, ActionClass))
		{
			NewAction->Setup(this);
			Actions.Add(NewAction);
		}
	}
}

// ReSharper disable once CppMemberFunctionMayBeConst
// Cannot be const to bind
bool UInventoryContentsBase::DefaultFilter(const FInventoryEntry& Entry)
{
	if (IsValid(DefaultFilterRule))
	{
		DefaultFilterProxyObject->SetValue(Entry);
		return DefaultFilterRule->Exec(DefaultFilterProxyObject);
	}
	return Entry.IsValid();
}

// ReSharper disable once CppMemberFunctionMayBeConst
// Cannot be const to bind
bool UInventoryContentsBase::DefaultSort(const FInventoryEntry& A, const FInventoryEntry& B)
{
	if (!IsValid(ActiveSortRule)) return false;
	DefaultSortProxyAObject->SetValue(A);
	DefaultSortProxyBObject->SetValue(B);
	return ActiveSortRule->Exec(DefaultSortProxyAObject, DefaultSortProxyBObject);
}

void UInventoryContentsBase::NativeEntryAdded(UFaerieItemStorage* Storage, const FEntryKey Key)
{
	if (bAlwaysAddNewToSortOrder)
	{
		TArray<FInventoryKey> InvKeys = ItemStorage->GetInvKeysForEntry(Key);

		for (auto&& InvKey : InvKeys)
		{
			AddToSortOrder(InvKey, true);
		}

		for (auto&& InvKey : InvKeys)
		{
			OnKeyAdded(InvKey);
		}
	}
}

void UInventoryContentsBase::NativeEntryUpdated(UFaerieItemStorage* Storage, const FEntryKey Key)
{
	if (bAlwaysAddNewToSortOrder)
	{
		TArray<FInventoryKey> InvKeys = ItemStorage->GetInvKeysForEntry(Key);

		for (auto&& InvKey : InvKeys)
		{
			AddToSortOrder(InvKey, false);
		}

		for (auto&& InvKey : InvKeys)
		{
			OnKeyUpdated(InvKey);
		}
	}
}

void UInventoryContentsBase::NativeEntryRemoved(UFaerieItemStorage* Storage, const FEntryKey Key)
{
	TArray<FInventoryKey> DeadKeys;

	SortedAndFilteredKeys.RemoveAll([Key, &DeadKeys](const FInventoryKey InvKey)
		{
			if (InvKey.EntryKey == Key)
			{
				DeadKeys.Add(InvKey);
				return true;
			}
			return false;
		});

	for (auto&& DeadKey : DeadKeys)
	{
		OnKeyRemoved(DeadKey);
	}
}

void UInventoryContentsBase::SetLinkedStorage(UFaerieItemStorage* Storage)
{
	if (IsConstructed())
	{
		InitWithInventory(Storage);
	}
	else
	{
		ItemStorage = Storage;
	}
}

void UInventoryContentsBase::InitWithInventory(UFaerieItemStorage* Storage)
{
	// Reset state fully.
	Reset();

	if (IsValid(Storage))
	{
		ItemStorage = Storage;

		CreateActions();

		ItemStorage->GetOnKeyAdded().AddUObject(this, &ThisClass::NativeEntryAdded);
		ItemStorage->GetOnKeyUpdated().AddUObject(this, &ThisClass::NativeEntryUpdated);
		ItemStorage->GetOnKeyRemoved().AddUObject(this, &ThisClass::NativeEntryRemoved);

		OnInitWithInventory();

		// Load in entries that should be initially displayed
		NeedsResort = true;
	}
}

void UInventoryContentsBase::SetInventoryClient(UFaerieInventoryClient* Client)
{
	InventoryClient = Client;
}

void UInventoryContentsBase::AddToSortOrder(const FInventoryKey Key, const bool WarnIfAlreadyExists)
{
	struct FInsertKeyPredicate
	{
		FInsertKeyPredicate(UFaerieItemDataComparator* Rule, const TWeakObjectPtr<UFaerieItemStorage> Storage, const bool Reverse)
		  : Rule(Rule),
			Storage(Storage),
			Reverse(Reverse)
		{}

		UFaerieItemDataComparator* Rule;
		TWeakObjectPtr<UFaerieItemStorage> Storage;
		bool Reverse;

		bool operator()(const FInventoryKey A, const FInventoryKey B) const
		{
			UInventoryEntryProxy* EntryA;
			UInventoryEntryProxy* EntryB;

			Storage->GetProxyForEntry(A, EntryA);
			Storage->GetProxyForEntry(B, EntryB);

			bool Result = false;

			if (EntryA && EntryB && IsValid(Rule))
			{
				Result = Rule->Exec(EntryA, EntryB);
			}

			return Reverse ? !Result : Result;
		}
	};

	if (SortedAndFilteredKeys.IsEmpty())
	{
		SortedAndFilteredKeys.Add(Key);
	}
	else
	{
		// Binary search to find position to insert the new key.
		const int32 Index = Algo::UpperBound(SortedAndFilteredKeys, Key, FInsertKeyPredicate(ActiveSortRule, ItemStorage, Query.InvertSort));

		// Error and return if the key we were sorted above is ourself.
		if (SortedAndFilteredKeys.IsValidIndex(Index) && SortedAndFilteredKeys[Index] == Key ||
			(SortedAndFilteredKeys.IsValidIndex(Index-1) && SortedAndFilteredKeys[Index-1] == Key))
		{
			if (WarnIfAlreadyExists)
			{
				UE_LOG(LogInventoryContents, Warning, TEXT("Cannot add sort key that already exists in the array"));
			}
			return;
		}

		if (!ensureAlwaysMsgf(!SortedAndFilteredKeys.Contains(Key), TEXT("Cannot add key that already exists. How did code get here?")))
		{
			return;
		}

		SortedAndFilteredKeys.Insert(Key, Index);
	}

	NeedsReconstructEntries = true;
}

void UInventoryContentsBase::SetFilterByDelegate(const FBlueprintStorageFilter& Filter, const bool bResort)
{
	Query.Filter.Unbind();
	if (Filter.IsBound())
	{
		Query.Filter.BindLambda([Filter](const FInventoryEntry& Entry)
			{
				return Filter.Execute(Entry);
			});
	}

	if (bResort)
	{
		NeedsResort = true;
	}
}

void UInventoryContentsBase::ResetFilter(const bool bResort)
{
	UE_LOG(LogInventoryContents, Log, TEXT("Resetting to the default filter rule"));
	Query.Filter.BindUObject(this, &ThisClass::DefaultFilter);
	if (bResort)
	{
		NeedsResort = true;
	}
}

void UInventoryContentsBase::SetSortRule(UInventorySortRule* Rule, const bool bResort)
{
	if (IsValid(Rule))
	{
		ActiveSortRule = Rule;
		if (bResort)
		{
			NeedsResort = true;
		}
	}
}

void UInventoryContentsBase::SetSortReverse(const bool Reverse, const bool bResort)
{
	if (Reverse != Query.InvertSort)
	{
		Query.InvertSort = Reverse;
		if (bResort)
		{
			NeedsResort = true;
		}
	}
}

void UInventoryContentsBase::ResetSort(const bool bResort)
{
	Query.Sort.BindUObject(this, &ThisClass::DefaultSort);
	if (bResort)
	{
		NeedsResort = true;
	}
}

#undef LOCTEXT_NAMESPACE