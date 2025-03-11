﻿// Copyright Guy (Drakynfly) Lundvall. All Rights Reserved.

#pragma once

#include "FaerieItemDataProxy.h"
#include "Blueprint/UserWidget.h"
#include "FaerieCardBase.generated.h"

using FOnCardRefreshed = TMulticastDelegate<void()>;

/**
 *
 */
UCLASS(Abstract)
class FAERIEITEMCARD_API UFaerieCardBase : public UUserWidget
{
	GENERATED_BODY()

public:
	//~ UObject
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
	//~ UObject

	//~ UUserWidget
	virtual void NativeConstruct() override;
	//~ UUserWidget

	void SetItemData(FFaerieItemProxy InItemProxy, bool bRefresh);

	FFaerieItemProxy GetItemData() const { return ItemProxy; }

	FOnCardRefreshed::RegistrationType& GetOnCardRefreshed() { return OnCardRefreshed; }

	UFUNCTION(BlueprintCallable, Category = "Faerie|ItemCard")
	FFaerieItemStackView GetStackView() const;

	UFUNCTION(BlueprintCallable, Category = "Faerie|ItemCard")
	virtual void Refresh();

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "Faerie|ItemCard", meta = (DisplayName = "Refresh"))
	void BP_Refresh();

protected:
	UPROPERTY(BlueprintReadOnly, Category = "CardWidget")
	FFaerieItemProxy ItemProxy;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "CardWidget")
	bool RefreshOnConstruct = true;

	FOnCardRefreshed OnCardRefreshed;
};