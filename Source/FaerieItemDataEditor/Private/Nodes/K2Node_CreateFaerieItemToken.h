// Copyright Guy (Drakynfly) Lundvall. All Rights Reserved.

#pragma once

#include "UObject/ObjectMacros.h"
#include "K2Node_ConstructObjectFromClass.h"
#include "K2Node_CreateFaerieItemToken.generated.h"

class UEdGraph;

UCLASS()
class FAERIEITEMDATAEDITOR_API UK2Node_CreateFaerieItemToken : public UK2Node_ConstructObjectFromClass
{
	GENERATED_BODY()

	UK2Node_CreateFaerieItemToken(const FObjectInitializer& ObjectInitializer);

	//~ Begin UEdGraphNode Interface.
	virtual void AllocateDefaultPins() override;
	virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	//~ End UEdGraphNode Interface.

	//~ Begin UK2Node Interface
	virtual FText GetMenuCategory() const override;
	virtual FName GetCornerIcon() const override;
	//~ End UK2Node Interface.

protected:
	/** Gets the default node title when no class is selected */
	virtual FText GetBaseNodeTitle() const override;
	/** Gets the node title when a class has been selected. */
	virtual FText GetNodeTitleFormat() const override;
	/** Gets base class to use for the 'class' pin.  UObject by default. */
	virtual UClass* GetClassPinBaseClass() const override;
};
