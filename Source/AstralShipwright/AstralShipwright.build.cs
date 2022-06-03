// Astral Shipwright - Gwennaël Arbona

using UnrealBuildTool;

public class AstralShipwright : ModuleRules
{
    public AstralShipwright(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicIncludePaths.Add("AstralShipwright");
        PrivatePCHHeaderFile = "Nova.h";

        PrivateDependencyModuleNames.AddRange(new string[] {
            "Core",
            "CoreUObject",
            "Engine",
            "EngineSettings",
            "InputCore",
            "AppFramework",

            "Slate",
            "SlateCore",
            "UMG",
            "MoviePlayer",
            "RHI",
            "RenderCore",
            "Niagara",

            "Json",
            "JsonUtilities",

            "OnlineSubsystem",
            "OnlineSubsystemUtils",
            "NetCore"
        });

        if (Target.Type == TargetType.Editor)
        {
            PrivateDependencyModuleNames.AddRange(new string[] {
                "UnrealEd"
            });
        }

        if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows))
        {
            PrivateDependencyModuleNames.AddRange(new string[] {
                "DLSS"
            });
        }

        DynamicallyLoadedModuleNames.Add("OnlineSubsystemSteam");

        AddEngineThirdPartyPrivateStaticDependencies(Target, "Steamworks");
    }
}
