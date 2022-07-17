// Astral Shipwright - Gwennaël Arbona

#pragma once

#include "EngineMinimal.h"
#include "NovaGameTypes.h"
#include "NovaOrbitalSimulationTypes.h"
#include "NovaArea.generated.h"

/*----------------------------------------------------
    Description types
----------------------------------------------------*/

/** Resource trade metadata */
USTRUCT()
struct FNovaResourceTrade
{
	GENERATED_BODY()

	FNovaResourceTrade() : Resource(nullptr), PriceModifier(ENovaPriceModifier::Average), ForSale(false)
	{}

public:

	// Resource this data applies to
	UPROPERTY(Category = Properties, EditDefaultsOnly)
	const class UNovaResource* Resource;

	// Resource price modifier
	UPROPERTY(Category = Properties, EditDefaultsOnly)
	ENovaPriceModifier PriceModifier;

	// Resource is being sold here as opposed to bought
	UPROPERTY(Category = Properties, EditDefaultsOnly)
	bool ForSale;
};

/** World area description */
UCLASS(ClassGroup = (Nova), BlueprintType)
class UNovaArea : public UNeutronAssetDescription
{
	GENERATED_BODY()

public:

	UNovaArea()
		: IsInSpace(false)
		, AIQuota(3)

		, Sign(nullptr)

		, PaintColor(FLinearColor::White)
		, LightColor(FLinearColor::White)
		, DecalColor(FLinearColor::Black)
		, DirtyIntensity(0.5f)
		, Temperature(0.0f)
	{}

	/*----------------------------------------------------
	    Resources
	----------------------------------------------------*/

	/** Is a particular resource sold in this area */
	bool IsResourceSold(const class UNovaResource* Asset) const;

	/** Get resources bought in this area */
	TArray<const class UNovaResource*> GetResourcesBought() const;

	/** Get resources sold in this area */
	TArray<const class UNovaResource*> GetResourcesSold() const;

public:

	// Body orbited
	UPROPERTY(Category = Properties, EditDefaultsOnly)
	const class UNovaCelestialBody* Body;

	// Altitude in kilometers
	UPROPERTY(Category = Properties, EditDefaultsOnly)
	float Altitude;

	// Initial phase on the orbit in degrees
	UPROPERTY(Category = Properties, EditDefaultsOnly)
	float Phase;

	// Check if this area is in space - not used for movement reference, not dockable
	UPROPERTY(Category = Properties, EditDefaultsOnly)
	bool IsInSpace;

	// Amount of AI spacecraft to allow there
	UPROPERTY(Category = Properties, EditDefaultsOnly)
	int32 AIQuota;

	// Sub-level to load
	UPROPERTY(Category = Properties, EditDefaultsOnly)
	FName LevelName;

	// Area description
	UPROPERTY(Category = Properties, EditDefaultsOnly)
	FText Description;

	// Resources sold in this area
	UPROPERTY(Category = Properties, EditDefaultsOnly)
	TArray<FNovaResourceTrade> ResourceTradeMetadata;

	// Signage to use for this station
	UPROPERTY(Category = Style, EditDefaultsOnly, BlueprintReadOnly)
	const class UStaticMesh* Sign;

	// Paint color to apply on all meshes
	UPROPERTY(Category = Style, EditDefaultsOnly, BlueprintReadOnly)
	FLinearColor PaintColor;

	// Color to apply on spotlights and signs
	UPROPERTY(Category = Style, EditDefaultsOnly, BlueprintReadOnly)
	FLinearColor LightColor;

	// Color to apply on decals
	UPROPERTY(Category = Style, EditDefaultsOnly, BlueprintReadOnly)
	FLinearColor DecalColor;

	// Ring index
	UPROPERTY(Category = Style, EditDefaultsOnly, BlueprintReadOnly)
	float DirtyIntensity;

	// Temperature
	UPROPERTY(Category = Style, EditDefaultsOnly, BlueprintReadOnly)
	float Temperature;
};
