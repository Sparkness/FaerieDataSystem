// Copyright Guy (Drakynfly) Lundvall. All Rights Reserved.

#pragma once

#include "FaerieItemDataProxy.h"
#include "Toolkits/IToolkitHost.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "UObject/GCObject.h"

enum class EWidgetPreviewWidgetChangeType : uint8;
class UFaerieWidgetPreview;
class FFaerieItemAssetPreviewScene;
class SFaerieItemAssetViewport;
class UFaerieItemAsset;

namespace Faerie::UMGWidgetPreview
{
	struct FWidgetPreviewToolkitStateBase;
}

using FOnStateChanged = TMulticastDelegate<void(Faerie::UMGWidgetPreview::FWidgetPreviewToolkitStateBase* InOldState, Faerie::UMGWidgetPreview::FWidgetPreviewToolkitStateBase* InNewState)>;

/**
 * 
 */
class FFaerieItemAssetEditor : public FAssetEditorToolkit, public FGCObject, public FNotifyHook
{
public:
	FFaerieItemAssetEditor() = default;
	virtual ~FFaerieItemAssetEditor() override;

protected:
	//~ Begin IToolkit interface
	virtual FName GetToolkitFName() const override { return "FaerieItemAssetEditor"; }
	virtual FText GetBaseToolkitName() const override { return INVTEXT("Faerie Item Asset Editor"); }
	virtual FString GetWorldCentricTabPrefix() const override { return "FaerieItemAsset"; }
	virtual FLinearColor GetWorldCentricTabColorScale() const override { return {}; }
	//~ End IToolkit interface

	//~ FAssetEditorToolkit
	virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual void PostInitAssetEditor() override;
	virtual void OnClose() override;
	//~ FAssetEditorToolkit

	//~ FGCObject interface
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override;
	virtual FString GetReferencerName() const override;
	//~ FGCObject interface

	//~ FNotifyHook
	virtual void NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, FProperty* PropertyThatChanged) override;
	virtual void NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, class FEditPropertyChain* PropertyThatChanged) override;
	//~ FNotifyHook


	/**		 SETUP		 */
public:
	void InitAssetEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UFaerieItemAsset* InItemAsset);

protected:
	void BindCommands();
	void ExtendToolbars();
	TSharedRef<FFaerieItemAssetPreviewScene> CreatePreviewScene();
	UFaerieWidgetPreview* CreateWidgetPreview();

	TSharedRef<SDockTab> SpawnTab_Details(const FSpawnTabArgs& Args) const;
	TSharedRef<SDockTab> SpawnTab_Viewport(const FSpawnTabArgs& Args) const;
	TSharedRef<SDockTab> SpawnTab_WidgetPreview(const FSpawnTabArgs& Args) const;

	bool ShouldUpdate() const;

	void OnBlueprintPrecompile(UBlueprint* InBlueprint);

	void OnWidgetChanged(const EWidgetPreviewWidgetChangeType InChangeType);

	void OnFocusChanging(
		const FFocusEvent& InFocusEvent,
		const FWeakWidgetPath& InOldWidgetPath, const TSharedPtr<SWidget>& InOldWidget,
		const FWidgetPath& InNewWidgetPath, const TSharedPtr<SWidget>& InNewWidget);

	/** Resolve and set the current state based on various conditions. */
	void ResolveState();

	/** Resets to the default state. */
	void ResetPreview();


	/**		 GETTERS		 */
public:
	FOnStateChanged::RegistrationType& OnStateChanged() { return OnStateChangedDelegate; }
	Faerie::UMGWidgetPreview::FWidgetPreviewToolkitStateBase* GetState() const { return CurrentState; }

	UFaerieItemAsset* GetItemAsset() const { return ItemAsset; }
	UFaerieWidgetPreview* GetPreview() const { return WidgetPreview; }

	UWorld* GetPreviewWorld() const;


	/**		 ACTIONS		 */
protected:
	void FocusViewport() const;

	/** If the given state is different to the current state, this will handle transitions and events. */
	void SetState(Faerie::UMGWidgetPreview::FWidgetPreviewToolkitStateBase* InNewState);

private:
	TObjectPtr<UFaerieItemAsset> ItemAsset = nullptr;

	TObjectPtr<UFaerieWidgetPreview> WidgetPreview = nullptr;
	TSharedPtr<FFaerieItemAssetPreviewScene> PreviewScene;

	TSharedPtr<SFaerieItemAssetViewport> MeshViewportWidget;
	TSharedPtr<SWidget> WidgetPreviewWidget;

	FOnStateChanged OnStateChangedDelegate;

	Faerie::UMGWidgetPreview::FWidgetPreviewToolkitStateBase* CurrentState = nullptr;

	bool bIsFocused = false;

	FDelegateHandle OnBlueprintPrecompileHandle;
	FDelegateHandle OnWidgetChangedHandle;
	FDelegateHandle OnFocusChangingHandle;
};
