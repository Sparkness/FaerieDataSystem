﻿// Copyright Guy (Drakynfly) Lundvall. All Rights Reserved.

#include "InventoryReplicatedDataExtensionBase.h"
#include "FaerieItemContainerBase.h"
#include "StructUtils/StructView.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InventoryReplicatedDataExtensionBase)

void FRepDataPerEntryBase::PreReplicatedRemove(const FRepDataFastArray& InArraySerializer)
{
	InArraySerializer.PreDataReplicatedRemove(*this);
}

void FRepDataPerEntryBase::PostReplicatedAdd(const FRepDataFastArray& InArraySerializer)
{
	InArraySerializer.PostDataReplicatedAdd(*this);
}

void FRepDataPerEntryBase::PostReplicatedChange(const FRepDataFastArray& InArraySerializer)
{
	InArraySerializer.PostDataReplicatedChange(*this);
}

void FRepDataFastArray::RemoveDataForEntry(const FEntryKey Key)
{
	if (Remove(Key,
		[this](const FRepDataPerEntryBase& Entry)
		{
			// Notify server of this removal.
			PreDataReplicatedRemove(Entry);
		}))
	{
		// Notify clients of this removal.
		MarkArrayDirty();
	}
}

void FRepDataFastArray::SetDataForEntry(const FEntryKey Key, const FInstancedStruct& Data)
{
	if (const int32 Index = IndexOf(Key);
		Index != INDEX_NONE)
	{
		FRepDataPerEntryBase& EntryData = Entries[Index];
		EntryData.Value = Data;
		MarkItemDirty(EntryData);

		// Notify server of this change.
		PostDataReplicatedChange(EntryData);
	}
	else
	{
		FRepDataPerEntryBase& NewEntry = Insert(FRepDataPerEntryBase(Key, Data));
		MarkItemDirty(NewEntry);

		// Notify server of this change.
		PostDataReplicatedAdd(NewEntry);
	}
}

void FRepDataFastArray::PreDataReplicatedRemove(const FRepDataPerEntryBase& Data) const
{
	if (OwningWrapper.IsValid())
	{
		OwningWrapper->PreContentRemoved(Data);
	}
}

void FRepDataFastArray::PostDataReplicatedAdd(const FRepDataPerEntryBase& Data) const
{
	if (OwningWrapper.IsValid())
	{
		OwningWrapper->PostContentAdded(Data);
	}
}

void FRepDataFastArray::PostDataReplicatedChange(const FRepDataPerEntryBase& Data) const
{
	if (OwningWrapper.IsValid())
	{
		OwningWrapper->PostContentChanged(Data);
	}
}

void URepDataArrayWrapper::PostInitProperties()
{
	Super::PostInitProperties();

	// Bind replication functions out into this class.
	DataArray.OwningWrapper = this;
}

void URepDataArrayWrapper::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, DataArray);
}

void URepDataArrayWrapper::PreContentRemoved(const FRepDataPerEntryBase& Data)
{
	if (Container.IsValid())
	{
		GetOuterUInventoryReplicatedDataExtensionBase()->PreEntryDataRemoved(Container.Get(), Data);
	}
}

void URepDataArrayWrapper::PostContentAdded(const FRepDataPerEntryBase& Data)
{
	if (Container.IsValid())
	{
		GetOuterUInventoryReplicatedDataExtensionBase()->PreEntryDataAdded(Container.Get(), Data);
	}
}

void URepDataArrayWrapper::PostContentChanged(const FRepDataPerEntryBase& Data)
{
	if (Container.IsValid())
	{
		GetOuterUInventoryReplicatedDataExtensionBase()->PreEntryDataChanged(Container.Get(), Data);
	}
}

void UInventoryReplicatedDataExtensionBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams SharedParams;
	SharedParams.bIsPushBased = true;

	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, PerContainerData, SharedParams);
}

FInstancedStruct UInventoryReplicatedDataExtensionBase::MakeSaveData(const UFaerieItemContainerBase* Container) const
{
	if (SaveRepDataArray())
	{
		return FInstancedStruct(FindFastArrayForContainer(Container));
	}

	return FInstancedStruct();
}

void UInventoryReplicatedDataExtensionBase::LoadSaveData(const UFaerieItemContainerBase* Container,
	const FInstancedStruct& SaveData)
{
	if (const TStructView<FRepDataFastArray> ContainerData = FindFastArrayForContainer(Container);
		ContainerData.IsValid())
	{
		FRepDataFastArray& Ref = ContainerData.Get<FRepDataFastArray>();
		Ref.Entries = SaveData.Get<FRepDataFastArray>().Entries;
		Ref.MarkArrayDirty();
	}
}

void UInventoryReplicatedDataExtensionBase::InitializeExtension(const UFaerieItemContainerBase* Container)
{
#if WITH_EDITOR
	checkf(FUObjectThreadContext::Get().IsInConstructor == false, TEXT("Do not call InitializeExtension from a constructor! Use InitializeNetObject if available."));

	// Explanation: For UFaerieInventoryComponents that are added to Blueprint Classes, sometimes in PIE, the
	// component's GEN_VARIABLE version will somehow slip through and try to be added as a container. They are invalid.
	if (Container->GetFullName().Contains("GEN_VARIABLE"))
	{
		//ensure(0);
		return;
	}
#endif

	checkSlow(!FindFastArrayForContainer(Container).IsValid());

	if (ensure(IsValid(Container)))
	{
		URepDataArrayWrapper* NewWrapper = NewObject<URepDataArrayWrapper>(this);
		NewWrapper->Container = Container;

		if (AActor* Actor = GetTypedOuter<AActor>();
			IsValid(Actor) && Actor->IsUsingRegisteredSubObjectList())
		{
			Actor->AddReplicatedSubObject(NewWrapper);
		}

		MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, PerContainerData, this);
		PerContainerData.Add(NewWrapper);
	}
}

void UInventoryReplicatedDataExtensionBase::DeinitializeExtension(const UFaerieItemContainerBase* Container)
{
	if (!!PerContainerData.RemoveAll(
		[this, Container](const TObjectPtr<URepDataArrayWrapper>& Wrapper)
		{
			if (Wrapper->Container == Container)
			{
				if (AActor* Actor = GetTypedOuter<AActor>();
				IsValid(Actor) && Actor->IsUsingRegisteredSubObjectList())
				{
					Actor->RemoveReplicatedSubObject(Wrapper);
				}
				return true;
			}
			return false;
		}))
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, PerContainerData, this);
	}
}

void UInventoryReplicatedDataExtensionBase::PreRemoval(const UFaerieItemContainerBase* Container, const FEntryKey Key,
													   const int32 Removal)
{
	Super::PreRemoval(Container, Key, Removal);

	if (const TStructView<FRepDataFastArray> ContainerData = FindFastArrayForContainer(Container);
		ContainerData.IsValid())
	{
		FRepDataFastArray& Ref = ContainerData.Get<FRepDataFastArray>();

		// If the whole stack is being removed, auto-delete any data we have for the entry
		if (Container->GetStack(Key) == Removal || Removal == Faerie::ItemData::UnlimitedStack)
		{
			Ref.RemoveDataForEntry(Key);
		}
	}
}

FConstStructView UInventoryReplicatedDataExtensionBase::GetDataForEntry(const UFaerieItemContainerBase* Container,
	const FEntryKey Key) const
{
	if (const TConstStructView<FRepDataFastArray> ContainerData = FindFastArrayForContainer(Container);
		ensure(ContainerData.IsValid()))
	{
		const FRepDataFastArray& Ref = ContainerData.Get();

		if (Ref.Contains(Key))
		{
			return Ref[Key];
		}
	}
	return {};
}

bool UInventoryReplicatedDataExtensionBase::EditDataForEntry(const UFaerieItemContainerBase* Container,
	const FEntryKey Key, const TFunctionRef<void(FStructView)>& Edit)
{
	const TStructView<FRepDataFastArray> ContainerData = FindFastArrayForContainer(Container);
	if (!ContainerData.IsValid())
	{
		return false;
	}

	FRepDataFastArray& Ref = ContainerData.Get<FRepDataFastArray>();

	if (Ref.Contains(Key))
	{
		// Edit existing data entry
		Edit(Ref[Key]);
	}
	else
	{
		// Make new data entry
		FInstancedStruct Data(GetDataScriptStruct());
		Edit(Data);
		Ref.SetDataForEntry(Key, Data);
	}

	return true;
}

TStructView<FRepDataFastArray> UInventoryReplicatedDataExtensionBase::FindFastArrayForContainer(const UFaerieItemContainerBase* Container)
{
	if (auto&& Found = PerContainerData.FindByPredicate(
			[Container](const TObjectPtr<URepDataArrayWrapper>& Userdata)
			{
				return Userdata && Userdata->Container == Container;
			}))
	{
		return (*Found)->DataArray;
	}
#if WITH_EDITOR
	UE_LOG(LogTemp, Warning, TEXT("Failed to find FastArray for container '%s'. Is this container initialized to this extension?"), *Container->GetFullName())
	PrintPerContainerDataDebug();
#endif
	return TStructView<FRepDataFastArray>();
}

TConstStructView<FRepDataFastArray> UInventoryReplicatedDataExtensionBase::FindFastArrayForContainer(const UFaerieItemContainerBase* Container) const
{
	if (auto&& Found = PerContainerData.FindByPredicate(
			[Container](const TObjectPtr<URepDataArrayWrapper>& Userdata)
			{
				return Userdata && Userdata->Container == Container;
			}))
	{
		return (*Found)->DataArray;
	}
#if WITH_EDITOR
	UE_LOG(LogTemp, Warning, TEXT("Failed to find FastArray for container '%s'. Is this container initialized to this extension?"), *Container->GetFullName())
	PrintPerContainerDataDebug();
#endif
	return TConstStructView<FRepDataFastArray>();
}

#if WITH_EDITOR
void UInventoryReplicatedDataExtensionBase::PrintPerContainerDataDebug() const
{
	UE_LOG(LogTemp, Log, TEXT("Printing Containers with FastArrays"))
	for (auto&& Element : PerContainerData)
	{
		UE_LOG(LogTemp, Log, TEXT("    %s"), *Element->Container->GetFullName())
	}
}
#endif