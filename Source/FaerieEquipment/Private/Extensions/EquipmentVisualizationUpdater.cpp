﻿// Copyright Guy (Drakynfly) Lundvall. All Rights Reserved.

#include "Extensions/EquipmentVisualizationUpdater.h"
#include "Extensions/RelevantActorsExtension.h"
#include "Extensions/VisualSlotExtension.h"

#include "EquipmentVisualizer.h"
#include "FaerieItemContainerBase.h"
#include "FaerieMeshSubsystem.h"
#include "ItemContainerEvent.h"

#include "Actors/ItemRepresentationActor.h"
#include "Components/FaerieItemMeshComponent.h"
#include "Tokens/FaerieMeshToken.h"
#include "Tokens/FaerieVisualEquipment.h"

#include "GameFramework/Character.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(EquipmentVisualizationUpdater)

void UEquipmentVisualizationUpdater::InitializeExtension(const UFaerieItemContainerBase* Container)
{
	auto&& Visualizer = GetVisualizer(Container);
	if (!IsValid(Visualizer))
	{
		return;
	}

	Container->ForEachKey(
		[this, Container, Visualizer](const FEntryKey Key)
		{
			CreateNewVisualImpl(Container, Visualizer, Container->Proxy(Key));
			SpawnKeys.Add(Container, Key);
		});
}

void UEquipmentVisualizationUpdater::DeinitializeExtension(const UFaerieItemContainerBase* Container)
{
	TArray<FEntryKey> Keys;
	SpawnKeys.MultiFind(Container, Keys);
	SpawnKeys.Remove(Container);

	if (!Keys.IsEmpty())
	{
		auto&& Visualizer = GetVisualizer(Container);
		if (!IsValid(Visualizer))
		{
			return;
		}

		for (auto&& Key : Keys)
		{
			RemoveOldVisualImpl(Visualizer, Container->Proxy(Key));
		}
	}
}

void UEquipmentVisualizationUpdater::PostAddition(const UFaerieItemContainerBase* Container,
												  const Faerie::Inventory::FEventLog& Event)
{
	// A previously empty slot now has been filled with an item.
	CreateNewVisual(Container, Event.EntryTouched);
}

void UEquipmentVisualizationUpdater::PreRemoval(const UFaerieItemContainerBase* Container, const FEntryKey Key,
	const int32 Removal)
{
	// If the whole stack is being removed, remove the visual for it
	if (Container->GetStack(Key) == Removal || Removal == Faerie::ItemData::UnlimitedStack)
	{
		RemoveOldVisual(Container, Key);
	}
}

void UEquipmentVisualizationUpdater::PostEntryChanged(const UFaerieItemContainerBase* Container,
	const Faerie::Inventory::FEventLog& Event)
{
	// The item in a slot has changed. Recreate the visual.
	// @todo maybe don't always do this?!?! determine if we need to. use the event tag type

	auto&& Visualizer = GetVisualizer(Container);
	if (!IsValid(Visualizer))
	{
		return;
	}
	const FFaerieItemProxy Proxy = Container->Proxy(Event.EntryTouched);
	RemoveOldVisualImpl(Visualizer, Proxy);
	CreateNewVisualImpl(Container, Visualizer, Proxy);
}

UEquipmentVisualizer* UEquipmentVisualizationUpdater::GetVisualizer(const UFaerieItemContainerBase* Container)
{
	if (!IsValid(Container))
	{
		return nullptr;
	}

	if (auto&& Relevants = GetExtension<URelevantActorsExtension>(Container))
	{
		if (auto&& Pawn = Relevants->FindActor<APawn>())
		{
			return Pawn->GetComponentByClass<UEquipmentVisualizer>();
		}
	}

	return nullptr;
}

void UEquipmentVisualizationUpdater::CreateNewVisual(const UFaerieItemContainerBase* Container, const FEntryKey Key)
{
	auto&& Visualizer = GetVisualizer(Container);
	if (!IsValid(Visualizer))
	{
		return;
	}

	CreateNewVisualImpl(Container, Visualizer, Container->Proxy(Key));
	SpawnKeys.Add(Container, Key);
}

void UEquipmentVisualizationUpdater::RemoveOldVisual(const UFaerieItemContainerBase* Container, const FEntryKey Key)
{
	auto&& Visualizer = GetVisualizer(Container);
	if (!IsValid(Visualizer))
	{
		return;
	}

	RemoveOldVisualImpl(Visualizer, Container->Proxy(Key));
	SpawnKeys.RemoveSingle(Container, Key);
}

void UEquipmentVisualizationUpdater::CreateNewVisualImpl(const UFaerieItemContainerBase* Container,
	UEquipmentVisualizer* Visualizer, const FFaerieItemProxy Proxy)
{
	if (!Proxy.IsValid())
	{
		return;
	}

	// Step 1: Figure out what we are attaching to. For now, we rely on a VisualSlotExtension on the container to tell us!

	auto&& SlotExtension = GetExtension<UVisualSlotExtension>(Container);
	if (!IsValid(SlotExtension))
	{
		UE_LOG(LogTemp, Warning, TEXT("No slot extension for container!"))
		return;
	}

	FEquipmentVisualAttachment Attachment;
	Attachment.Parent = Visualizer->GetOwner()->FindComponentByTag<USceneComponent>(SlotExtension->GetComponentTag());
	if (!Attachment.Parent.IsValid() && Visualizer->GetOwner()->IsA<ACharacter>())
	{
		// Default to using the character mesh for attachment if no other is found.
		Attachment.Parent = Cast<ACharacter>(Visualizer->GetOwner())->GetMesh();
	}
	Attachment.Socket = SlotExtension->GetSocket();

	// Step 2: What are we creating as a visual.

	// Path 1: A Visual Actor
	{
		auto&& VisualToken = Proxy->GetItemObject()->GetToken<UFaerieVisualEquipment>();
		if (IsValid(VisualToken))
		{
			if (VisualToken->GetActorClass().IsNull())
			{
				return;
			}

			// @todo implement async path here
			const TSubclassOf<AItemRepresentationActor> VisualClass = VisualToken->GetActorClass().LoadSynchronous();
			if (!IsValid(VisualClass))
			{
				UE_LOG(LogTemp, Warning, TEXT("VisualClass failed to load!"))
				return;
			}

			AItemRepresentationActor* NewVisual = Visualizer->SpawnVisualActorNative<AItemRepresentationActor>(
				{ Proxy }, VisualClass, Attachment);
			if (!IsValid(NewVisual))
			{
				return;
			}

			NewVisual->SetSourceProxy(Proxy);
			return;
		}
	}

	// Path 2: A Visual Component
	{
		auto&& MeshToken = Proxy->GetItemObject()->GetToken<UFaerieMeshTokenBase>();
		{
			UFaerieMeshSubsystem* MeshSubsystem = Visualizer->GetWorld()->GetSubsystem<UFaerieMeshSubsystem>();
			if (!MeshSubsystem)
			{
				return;
			}

			FFaerieItemMesh Mesh;
			if (!MeshSubsystem->LoadMeshFromTokenSynchronous(MeshToken, Visualizer->GetPreferredTag(), Mesh))
			{
				UE_LOG(LogTemp, Warning, TEXT("LoadMeshFromTokenSynchronous failed!"))
				return;
			}

			UFaerieItemMeshComponent* NewVisual = Visualizer->SpawnVisualComponentNative<UFaerieItemMeshComponent>(
				{ Proxy }, UFaerieItemMeshComponent::StaticClass(), Attachment);
			if (!IsValid(NewVisual))
			{
				return;
			}

			//NewVisual->SetPreferredMeshType()
			NewVisual->SetItemMesh(Mesh);
			return;
		}
	}
}

void UEquipmentVisualizationUpdater::RemoveOldVisualImpl(UEquipmentVisualizer* Visualizer, const FFaerieItemProxy Proxy)
{
	check(Visualizer);
	Visualizer->DestroyVisualByKey({Proxy});
}