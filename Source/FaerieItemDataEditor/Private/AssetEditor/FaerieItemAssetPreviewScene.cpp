// Fill out your copyright notice in the Description page of Project Settings.

#include "AssetEditor/FaerieItemAssetPreviewScene.h"
#include "AssetEditor/FaerieItemAssetEditor.h"

#include "FaerieItemAsset.h"
#include "FaerieItemMeshLoader.h"
#include "Components/FaerieItemMeshComponent.h"

#include "GameFramework/WorldSettings.h"
#include "Tokens/FaerieMeshToken.h"

FFaerieItemAssetPreviewScene::FFaerieItemAssetPreviewScene(ConstructionValues CVS, const TSharedRef<FFaerieItemAssetEditor>& EditorToolkit)
	: FAdvancedPreviewScene(CVS)
	, EditorPtr(EditorToolkit)

{
	// Disable killing actors outside of the world
	AWorldSettings* WorldSettings = GetWorld()->GetWorldSettings(true);
	WorldSettings->bEnableWorldBoundsChecks = false;

	// Hide default floor
	SetFloorVisibility(false, false);

	{
		UStaticMesh* PreviewMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/EngineMeshes/Cube.Cube"), nullptr, LOAD_None, nullptr);

		DefaultCube = NewObject<UStaticMeshComponent>(GetTransientPackage());
		DefaultCube->SetStaticMesh(PreviewMesh);
		DefaultCube->bSelectable = true;
		
		FFaerieItemAssetPreviewScene::AddComponent(DefaultCube, FTransform::Identity);
	}

	{
		Actor = GetWorld()->SpawnActor<AActor>(FVector::ZeroVector, FRotator::ZeroRotator);
	}

	{
		ItemMeshComponent = NewObject<UFaerieItemMeshComponent>(Actor);
		//ItemMeshComponent->bSelectable = true;

		FFaerieItemAssetPreviewScene::AddComponent(ItemMeshComponent, FTransform::Identity);
	}

	MeshLoader = NewObject<UFaerieItemMeshLoader>(Actor, MakeUniqueObjectName(Actor, UFaerieItemMeshLoader::StaticClass()));
}

FFaerieItemAssetPreviewScene::~FFaerieItemAssetPreviewScene()
{
}

void FFaerieItemAssetPreviewScene::AddReferencedObjects(FReferenceCollector& Collector)
{
	FAdvancedPreviewScene::AddReferencedObjects(Collector);
	Collector.AddReferencedObject(MeshLoader);
}

void FFaerieItemAssetPreviewScene::Tick(const float InDeltaTime)
{
	FAdvancedPreviewScene::Tick(InDeltaTime);

	if (const TSharedPtr<FFaerieItemAssetEditor> Toolkit = EditorPtr.Pin())
	{
		if (GEditor->bIsSimulatingInEditor ||
			GEditor->PlayWorld != nullptr)
		{
			return;
		}

		GetWorld()->Tick(LEVELTICK_All, InDeltaTime);
	}
}

FBoxSphereBounds FFaerieItemAssetPreviewScene::GetBounds() const
{
	return Actor->GetComponentsBoundingBox(true);
}

void FFaerieItemAssetPreviewScene::RefreshMesh()
{
	if (!ensure(IsValid(ItemMeshComponent))) return;
	if (!ensure(IsValid(MeshLoader))) return;

	ItemMeshComponent->ClearItemMesh();

	const UFaerieItemAsset* ItemAsset = EditorPtr.Pin()->GetItemAsset();
	if (!IsValid(ItemAsset)) return;

	const UFaerieMeshTokenBase* MeshToken = nullptr;
	for (auto&& Element : ItemAsset->GetEditorTokensView())
	{
		MeshToken = Cast<UFaerieMeshTokenBase>(Element);
		if (MeshToken)
		{
			break;
		}
	}

	if (IsValid(MeshToken))
	{
		// @todo expose MeshPurpose to AssetEditor
		const FGameplayTag MeshPurpose = Faerie::ItemMesh::Tags::MeshPurpose_Default;

		if (FFaerieItemMesh ItemMesh;
			MeshLoader->LoadMeshFromTokenSynchronous(MeshToken, MeshPurpose, ItemMesh))
		{
			ItemMeshComponent->SetItemMesh(ItemMesh);
			DefaultCube->SetVisibility(false);
			return;
		}
	}

	// If the code above doesn't evaluate to a mesh, show the debug cube.
	DefaultCube->SetVisibility(true);
}
