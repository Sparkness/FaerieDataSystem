﻿// Copyright Guy (Drakynfly) Lundvall. All Rights Reserved.

#pragma once

#include "NetSupportedObject.h"
#include "FaerieContainerExtensionInterface.h"
#include "FaerieItemContainerStructs.h"
#include "FaerieItemOwnerInterface.h"
#include "FaerieItemContainerBase.generated.h"

class UItemContainerExtensionBase;

/**
 * Base class for objects that store FaerieItems
 */
UCLASS(Abstract, Blueprintable)
class FAERIEINVENTORY_API UFaerieItemContainerBase : public UNetSupportedObject, public IFaerieItemOwnerInterface, public IFaerieContainerExtensionInterface
{
	GENERATED_BODY()

public:
	UFaerieItemContainerBase();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	//~ UNetSupportedObject
	virtual void InitializeNetObject(AActor* Actor) override;
	virtual void DeinitializeNetObject(AActor* Actor) override;
	//~ UNetSupportedObject

	//~ IFaerieItemOwnerInterface
	virtual FFaerieItemStack Release(FFaerieItemStackView Stack) override;
	virtual bool Possess(FFaerieItemStack Stack) override;
	//~ IFaerieItemOwnerInterface

	//~ IFaerieContainerExtensionInterface
	virtual UItemContainerExtensionGroup* GetExtensionGroup() const override final;
	virtual bool AddExtension(UItemContainerExtensionBase* Extension) override;
	//~ IFaerieContainerExtensionInterface


	/**------------------------------*/
	/*		 SAVE DATA API			 */
	/**------------------------------*/
public:
	virtual FFaerieContainerSaveData MakeSaveData() const PURE_VIRTUAL(UFaerieItemContainerBase::MakeSaveData, return {}; )
	virtual void LoadSaveData(const FFaerieContainerSaveData& SaveData) PURE_VIRTUAL(UFaerieItemContainerBase::SaveData, )

protected:
	void RavelExtensionData(TMap<FGuid, FInstancedStruct>& Data) const;
	void UnravelExtensionData(const TMap<FGuid, FInstancedStruct>& Data);

	void TryApplyUnclaimedSaveData(UItemContainerExtensionBase* Extension);


	/**------------------------------*/
	/*		 ITEM ENTRY API			 */
	/**------------------------------*/
public:
	UFUNCTION(BlueprintCallable, Category = "Faerie|ItemContainer")
	virtual bool Contains(FEntryKey Key) const PURE_VIRTUAL(UFaerieItemContainerBase::Contains, return false; )

	// Get a view of an entry
	UFUNCTION(BlueprintCallable, Category = "Faerie|ItemContainer")
	virtual FFaerieItemStackView View(FEntryKey Key) const PURE_VIRTUAL(UFaerieItemContainerBase::View, return FFaerieItemStackView(); )

	// Creates or retrieves a proxy for an entry
	UFUNCTION(BlueprintCallable, Category = "Faerie|ItemContainer")
	virtual FFaerieItemProxy Proxy(FEntryKey Key) const PURE_VIRTUAL(UFaerieItemContainerBase::Proxy, return nullptr; )

	// A more efficient overload of Release if we already know the Key.
	virtual FFaerieItemStack Release(FEntryKey Key, int32 Copies) PURE_VIRTUAL(UFaerieItemContainerBase::Release, return FFaerieItemStack(); )

	// Iterate over and perform a task for each key.
	virtual void ForEachKey(const TFunctionRef<void(FEntryKey)>& Func) const PURE_VIRTUAL(UFaerieItemContainerBase::ForEachKey, ; )

	// Get the stack for a key.
	UFUNCTION(BlueprintCallable, Category = "Faerie|ItemContainer")
	virtual int32 GetStack(FEntryKey Key) const PURE_VIRTUAL(UFaerieItemContainerBase::GetStack, return 0; )

protected:
	virtual void OnItemMutated(const UFaerieItem* Item, const UFaerieItemToken* Token, FGameplayTag EditTag);

	// This function must be called by child classes when binding items to new keys.
	void ReleaseOwnership(const UFaerieItem* Item);

	// This function must be called by child classes when releasing a key.
	void TakeOwnership(const UFaerieItem* Item);


	/**------------------------------*/
	/*			 VARIABLES			 */
	/**------------------------------*/

protected:
	// Subobject responsible for adding to or customizing container behavior.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "ItemContainer")
	TObjectPtr<UItemContainerExtensionGroup> Extensions;

	// Save data for extensions that did not exist on us during unraveling.
	UPROPERTY(Transient)
	TMap<FGuid, FInstancedStruct> UnclaimedExtensionData;

	Faerie::TKeyGen<FEntryKey> KeyGen;
};