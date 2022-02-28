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
            "InputCore",
            "AppFramework",

            "Json",
            "JsonUtilities",

            "Slate",
            "SlateCore",
            "UMG",
            "MoviePlayer",
            "RHI",
            "RenderCore",

            "HTTP",
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

        DynamicallyLoadedModuleNames.Add("OnlineSubsystemSteam");

        AddEngineThirdPartyPrivateStaticDependencies(Target, "Steamworks");
    }
}
