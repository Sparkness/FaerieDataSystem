﻿// Copyright Guy (Drakynfly) Lundvall. All Rights Reserved.

using UnrealBuildTool;

public class FaerieItemDataEditor : ModuleRules
{
    public FaerieItemDataEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        FaerieDataUtils.ApplySharedModuleSetup(this, Target);

        PublicDependencyModuleNames.AddRange(
            new []
            {
                "Core",
                "FaerieItemData",
                "FaerieDataSystemEditor"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new []
            {
                "AssetDefinition",
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore",
                "UnrealEd"
            }
        );
    }
}