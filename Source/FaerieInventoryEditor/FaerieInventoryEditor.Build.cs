﻿using UnrealBuildTool;

public class FaerieInventoryEditor : ModuleRules
{
    public FaerieInventoryEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new []
            {
                "Core",
                "FaerieInventory",
                "FaerieDataSystemEditor"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new []
            {
                "CoreUObject",
                "Engine",
                "GameplayTags",
                "GameplayTagsEditor",
                "Slate",
                "SlateCore",
                "PropertyEditor",
            }
        );
    }
}