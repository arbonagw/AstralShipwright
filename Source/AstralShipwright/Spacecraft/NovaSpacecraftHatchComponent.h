// Astral Shipwright - Gwennaël Arbona

#pragma once

#include "CoreMinimal.h"
#include "NovaSpacecraftTypes.h"

#include "Neutron/Actor/NeutronActorTools.h"
#include "Neutron/Actor/NeutronSkeletalMeshComponent.h"

#include "NovaSpacecraftHatchComponent.generated.h"

/** Hatch equipment addition */
UCLASS(ClassGroup = (Nova), meta = (BlueprintSpawnableComponent))
class UNovaSpacecraftHatchComponent
	: public UStaticMeshComponent
	, public INovaAdditionalComponentInterface
{
	GENERATED_BODY()

public:

	UNovaSpacecraftHatchComponent();

	/*----------------------------------------------------
	    Inherited
	----------------------------------------------------*/

	virtual void SetAdditionalAsset(TSoftObjectPtr<class UObject> AdditionalAsset) override;

	virtual void BeginPlay() override;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:

	/*----------------------------------------------------
	    Data
	----------------------------------------------------*/

	UPROPERTY()
	class UNeutronSkeletalMeshComponent* HatchMesh;

	bool CurrentDockingState;
};
