﻿// Copyright Guy (Drakynfly) Lundvall. All Rights Reserved.

using UnrealBuildTool;

public class FaerieEquipmentEditor : ModuleRules
{
    public FaerieEquipmentEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        FaerieDataUtils.ApplySharedModuleSetup(this, Target);

        PublicDependencyModuleNames.AddRange(
            new []
            {
                "Core",
                "FaerieEquipment",
                "FaerieDataSystemEditor"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new []
            {
                "CoreUObject",
                "DeveloperSettings",
                "Engine",
                "GameplayTags",
                "GameplayTagsEditor",
                "Slate",
                "SlateCore"
            }
        );

        // Plugin dependencies
        PublicDependencyModuleNames.AddRange(
            new []
            {
                "FaerieInventory",
                "FaerieItemGenerator",
                "FaerieItemData",
                "FaerieItemMesh"
            });
    }
}