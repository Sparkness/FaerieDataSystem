﻿// Copyright Guy (Drakynfly) Lundvall. All Rights Reserved.

#include "FaerieEquipmentManager.h"
#include "FaerieEquipmentSlot.h"
#include "FaerieItemStorage.h"
#include "FaerieUtils.h"
#include "ItemContainerExtensionBase.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Tokens/FaerieItemStorageToken.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(FaerieEquipmentManager)

DEFINE_LOG_CATEGORY(LogEquipmentManager)

UFaerieEquipmentManager::UFaerieEquipmentManager()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	bReplicateUsingRegisteredSubObjectList = true;
	ExtensionGroup = CreateDefaultSubobject<UItemContainerExtensionGroup>("ExtensionGroup");
	ExtensionGroup->SetIdentifier();
}

void UFaerieEquipmentManager::PostInitProperties()
{
	Super::PostInitProperties();

	for (auto&& DefaultSlot : InstanceDefaultSlots)
	{
		if (IsValid(DefaultSlot.ExtensionGroup))
		{
			DefaultSlot.ExtensionGroup->SetIdentifier();
		}
	}
}

void UFaerieEquipmentManager::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, Slots, Params);
}

void UFaerieEquipmentManager::InitializeComponent()
{
	Super::InitializeComponent();

	AddDefaultSlots();
}

void UFaerieEquipmentManager::OnComponentCreated()
{
	Super::OnComponentCreated();

	if (!IsTemplate())
	{
		AddDefaultSlots();
	}
}

void UFaerieEquipmentManager::ReadyForReplication()
{
	Super::ReadyForReplication();

	AddSubobjectsForReplication();
}

UItemContainerExtensionGroup* UFaerieEquipmentManager::GetExtensionGroup() const
{
	return ExtensionGroup;
}

void UFaerieEquipmentManager::AddDefaultSlots()
{
	if (!Slots.IsEmpty())
	{
		// Default slots already added
		return;
	}

	// Wipe load flags from Extensions. Hack to make replication work :/
	ExtensionGroup->ReplicationFixup();

	for (auto&& Element : InstanceDefaultSlots)
	{
		auto&& DefaultSlot = AddSlot(Element.SlotConfig);
		if (!IsValid(DefaultSlot))
		{
			continue;
		}

		if (IsValid(Element.ExtensionGroup))
		{
			// The default ExtensionGroups are "Assets" in that they are default instances baked into the component, and
			// need to be fixed before they can replicate.
			Element.ExtensionGroup->ReplicationFixup();
			DefaultSlot->AddExtension(Element.ExtensionGroup.Get());
		}
	}
}

void UFaerieEquipmentManager::AddSubobjectsForReplication()
{
	AActor* Owner = GetOwner();
	check(Owner);

	if (!Owner->HasAuthority()) return;

	if (!Owner->IsUsingRegisteredSubObjectList())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("Owner of Equipment Manager '%s' does not replicate SubObjectList. Component will not be replicated correctly!"), *Owner->GetName())
	}
	else
	{
		GetOwner()->AddReplicatedSubObject(ExtensionGroup);
		ExtensionGroup->InitializeNetObject(Owner);

		for (auto&& Slot : Slots)
		{
			if (IsValid(Slot))
			{
				GetOwner()->AddReplicatedSubObject(Slot);
				Slot->InitializeNetObject(Owner);
			}
		}

		// Make slots replicate once
		MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, Slots, this)
	}
}

void UFaerieEquipmentManager::OnSlotItemChanged(UFaerieEquipmentSlot* Slot, const bool TokenEdit)
{
	const EFaerieEquipmentSlotChangeType Type = TokenEdit ? EFaerieEquipmentSlotChangeType::TokenEdit : EFaerieEquipmentSlotChangeType::ItemChange;
	OnEquipmentChangedEventNative.Broadcast(Slot, Type);
	OnEquipmentChangedEvent.Broadcast(Slot, Type);
}

FFaerieContainerSaveData UFaerieEquipmentManager::MakeSaveData() const
{
	FFaerieEquipmentSaveData SlotSaveData;
	SlotSaveData.PerSlotData.Reserve(Slots.Num());
	for (auto&& Slot : Slots)
	{
		SlotSaveData.PerSlotData.Add(Slot->MakeSaveData());
	}

	FFaerieContainerSaveData SaveData;
	SaveData.ItemData = FInstancedStruct::Make(SlotSaveData);
	return SaveData;
}

void UFaerieEquipmentManager::LoadSaveData(const FFaerieContainerSaveData& SaveData)
{
	Slots.Reset();

	const FFaerieEquipmentSaveData& EquipmentSaveData = SaveData.ItemData.Get<FFaerieEquipmentSaveData>();
	for (const FFaerieContainerSaveData& SlotSaveData : EquipmentSaveData.PerSlotData)
	{
		if (UFaerieEquipmentSlot* NewSlot = NewObject<UFaerieEquipmentSlot>(this))
		{
			NewSlot->LoadSaveData(SlotSaveData);
			Slots.Add(NewSlot);
			NewSlot->OnItemChangedNative.AddUObject(this, &ThisClass::OnSlotItemChanged, false);
			NewSlot->OnItemDataChangedNative.AddUObject(this, &ThisClass::OnSlotItemChanged, true);
			NewSlot->AddExtension(ExtensionGroup);
		}
	}

	MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, Slots, this);

	if (IsReadyForReplication())
	{
		AddSubobjectsForReplication();
	}
}

UFaerieEquipmentSlot* UFaerieEquipmentManager::AddSlot(const FFaerieEquipmentSlotConfig& Config)
{
	if (!Config.SlotID.IsValid()) return nullptr;
	if (Config.SlotDescription == nullptr) return nullptr;

	if (UFaerieEquipmentSlot* NewSlot = NewObject<UFaerieEquipmentSlot>(this);
		ensure(IsValid(NewSlot)))
	{
		NewSlot->Config = Config;
		MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, Slots, this)
		Slots.Add(NewSlot);
		GetOwner()->AddReplicatedSubObject(NewSlot);
		NewSlot->InitializeNetObject(GetOwner());

		NewSlot->OnItemChangedNative.AddUObject(this, &ThisClass::OnSlotItemChanged, false);
		NewSlot->OnItemDataChangedNative.AddUObject(this, &ThisClass::OnSlotItemChanged, true);

		NewSlot->AddExtension(ExtensionGroup);

		OnEquipmentSlotAddedNative.Broadcast(NewSlot);
		OnEquipmentSlotAdded.Broadcast(NewSlot);

		return NewSlot;
	}

	return nullptr;
}

bool UFaerieEquipmentManager::RemoveSlot(UFaerieEquipmentSlot* Slot)
{
	if (IsValid(Slot))
	{
		return false;
	}

	if (Slots.Remove(Slot))
	{
		OnPreEquipmentSlotRemovedNative.Broadcast(Slot);
		OnPreEquipmentSlotRemoved.Broadcast(Slot);

		Slot->RemoveExtension(ExtensionGroup);

		MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, Slots, this)
		Slot->DeinitializeNetObject(GetOwner());
		GetOwner()->RemoveReplicatedSubObject(Slot);

		Slot->OnItemChangedNative.RemoveAll(this);
		Slot->OnItemDataChangedNative.RemoveAll(this);

		return true;
	}

	return false;
}

UFaerieEquipmentSlot* UFaerieEquipmentManager::FindSlot(const FFaerieSlotTag SlotID, const bool Recursive) const
{
	for (auto&& Slot : Slots)
	{
		if (!IsValid(Slot)) continue;
		if (Slot->GetSlotID() == SlotID)
		{
			return Slot;
		}
	}

	if (Recursive)
	{
		for (auto&& Slot : Slots)
		{
			if (!IsValid(Slot)) continue;
			if (auto&& ChildSlot = Slot->FindSlot(SlotID, true))
			{
				return ChildSlot;
			}
		}
	}

	return nullptr;
}

bool UFaerieEquipmentManager::AddExtension(UItemContainerExtensionBase* Extension)
{
	if (ExtensionGroup->AddExtension(Extension))
	{
		GetOwner()->AddReplicatedSubObject(Extension);
		Extension->InitializeNetObject(GetOwner());
		return true;
	}
	return false;
}

bool UFaerieEquipmentManager::RemoveExtension(UItemContainerExtensionBase* Extension)
{
	if (!ensure(IsValid(Extension)))
	{
		return false;
	}

	Extension->DeinitializeNetObject(GetOwner());
	GetOwner()->RemoveReplicatedSubObject(Extension);
	return ExtensionGroup->RemoveExtension(Extension);
}

bool UFaerieEquipmentManager::CanClientRunActions(const UFaerieInventoryClient* Client) const
{
	// @todo implement permissions
	return true;
}

UItemContainerExtensionBase* UFaerieEquipmentManager::AddExtensionToSlot(const FFaerieSlotTag SlotID,
																		 const TSubclassOf<UItemContainerExtensionBase> ExtensionClass)
{
	if (!ensure(
			IsValid(ExtensionClass) &&
			ExtensionClass != UItemContainerExtensionBase::StaticClass()))
	{
		return nullptr;
	}

	auto&& Slot = FindSlot(SlotID, true);
	if (!IsValid(Slot))
	{
		return nullptr;
	}

	UItemContainerExtensionBase* NewExtension = NewObject<UItemContainerExtensionBase>(Slot, ExtensionClass);
	NewExtension->SetIdentifier();

	GetOwner()->AddReplicatedSubObject(NewExtension);
	NewExtension->InitializeNetObject(GetOwner());
	Slot->AddExtension(NewExtension);

	return NewExtension;
}

bool UFaerieEquipmentManager::RemoveExtensionFromSlot(const FFaerieSlotTag SlotID, const TSubclassOf<UItemContainerExtensionBase> ExtensionClass)
{
	if (!ensure(
			IsValid(ExtensionClass) &&
			ExtensionClass != UItemContainerExtensionBase::StaticClass()))
	{
		return false;
	}

	auto&& Slot = FindSlot(SlotID, true);
	if (!IsValid(Slot))
	{
		return false;
	}

	UItemContainerExtensionBase* Extension = Slot->GetExtension(ExtensionClass, false);
	if (!IsValid(Extension))
	{
		return false;
	}

	Extension->DeinitializeNetObject(GetOwner());
	GetOwner()->RemoveReplicatedSubObject(Extension);
	Slot->RemoveExtension(Extension);

	return true;
}

TArray<FFaerieStoragePath> UFaerieEquipmentManager::GetAllContainerPaths() const
{
	// @todo Track this function's performance, and cache the result somewhere if it's expensive and called a lot.

	TArray<FFaerieStoragePath> OutPaths;
	OutPaths.Reserve(Slots.Num());

	TFunction<void(UFaerieItemContainerBase*, const FFaerieStoragePath&)> BuildPaths;
	BuildPaths = [&OutPaths, &BuildPaths](UFaerieItemContainerBase* Container, const FFaerieStoragePath& BasePath)
	{
		FFaerieStoragePath& NewPath = OutPaths.Add_GetRef(FFaerieStoragePath(BasePath));
		NewPath.Containers.Add(Container);

		Container->ForEachKey(
			[Container, &BuildPaths, &NewPath](const FEntryKey Key)
			{
				auto View = Container->View(Key);
				if (!View.Item->CanMutate()) return;

				TSet<UFaerieItemContainerBase*> Nested = UFaerieItemContainerToken::GetAllContainersInItem(View.Item.Get());
				for (auto SubContainer : Nested)
				{
					BuildPaths(SubContainer, NewPath);
				}
			});
	};

	FFaerieStoragePath EmptyRoot;
	for (auto&& Slot : Slots)
	{
		BuildPaths(Slot, EmptyRoot);
	}

	return OutPaths;
}

void UFaerieEquipmentManager::PrintSlotDebugInfo() const
{
#if !UE_BUILD_SHIPPING
	for (auto&& Slot : Slots)
	{
		if (Slot->GetExtensionGroup())
		{
			UE_LOG(LogTemp, Log, TEXT("*** Printing Debug Data for: '%s'"), *Slot->Config.SlotID.ToString())
			Slot->GetExtensionGroup()->PrintDebugData();
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("Slot '%s' has no extension group."), *Slot->Config.SlotID.ToString())
		}
	}
#endif
}
