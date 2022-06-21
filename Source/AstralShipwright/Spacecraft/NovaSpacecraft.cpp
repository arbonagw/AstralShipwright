// Astral Shipwright - Gwennaël Arbona

#include "NovaSpacecraft.h"
#include "NovaSpacecraftPawn.h"

#include "Game/NovaGameState.h"
#include "Game/NovaOrbitalSimulationTypes.h"
#include "Nova.h"

#include "Neutron/System/NeutronAssetManager.h"

#include "Dom/JsonObject.h"
#include "EngineUtils.h"

#define LOCTEXT_NAMESPACE "NovaSpacecraft"

/*----------------------------------------------------
    Spacecraft compartment
----------------------------------------------------*/

FNovaCompartmentModule::FNovaCompartmentModule()
	: Description(nullptr)
	, ForwardBulkheadType(ENovaBulkheadType::None)
	, AftBulkheadType(ENovaBulkheadType::None)
	, SkirtPipingType(ENovaSkirtPipingType::None)
	, NeedsConnectionWiring(false)
	, NeedsCollectorPiping(false)
	, NeedsDome(false)
	, NeedsSkirt(false)
{}

FNovaCompartment::FNovaCompartment() : Description(nullptr), HullType(nullptr), Modules{FNovaCompartmentModule()}, Equipment{nullptr}
{}

FNovaCompartment::FNovaCompartment(const class UNovaCompartmentDescription* K) : FNovaCompartment()
{
	Description = K;
}

bool FNovaCompartment::operator==(const FNovaCompartment& Other) const
{
	if (Description != Other.Description || HullType != Other.HullType)
	{
		return false;
	}
	else
	{
		for (int32 ModuleIndex = 0; ModuleIndex < ENovaConstants::MaxModuleCount; ModuleIndex++)
		{
			if (Modules[ModuleIndex] != Other.Modules[ModuleIndex])
			{
				return false;
			}
		}
		for (int32 EquipmentIndex = 0; EquipmentIndex < ENovaConstants::MaxEquipmentCount; EquipmentIndex++)
		{
			if (Equipment[EquipmentIndex] != Other.Equipment[EquipmentIndex])
			{
				return false;
			}
		}

		return true;
	}
}

const FNovaCompartmentModule* FNovaCompartment::GetModuleDataBySocket(FName SocketName, int32& FoundModuleIndex) const
{
	if (Description)
	{
		for (int32 ModuleIndex = 0; ModuleIndex < ENovaConstants::MaxModuleCount; ModuleIndex++)
		{
			if (Description->GetModuleSlot(ModuleIndex).SocketName == SocketName)
			{
				FoundModuleIndex = ModuleIndex;
				return &Modules[ModuleIndex];
			}
		}
	}
	return nullptr;
}

const UNovaModuleDescription* FNovaCompartment::GetModuleBySocket(FName SocketName) const
{
	if (Description)
	{
		for (int32 ModuleIndex = 0; ModuleIndex < ENovaConstants::MaxModuleCount; ModuleIndex++)
		{
			if (Description->GetModuleSlot(ModuleIndex).SocketName == SocketName)
			{
				return Modules[ModuleIndex].Description;
			}
		}
	}
	return nullptr;
}

const UNovaEquipmentDescription* FNovaCompartment::GetEquipmentySocket(FName SocketName) const
{
	if (Description)
	{
		for (int32 EquipmentIndex = 0; EquipmentIndex < ENovaConstants::MaxEquipmentCount; EquipmentIndex++)
		{
			if (Description->GetEquipmentSlot(EquipmentIndex).SocketName == SocketName)
			{
				return Equipment[EquipmentIndex];
			}
		}
	}
	return nullptr;
}

bool FNovaCompartment::HasForwardOrAftEquipment() const
{
	if (Description)
	{
		for (int32 EquipmentIndex = 0; EquipmentIndex < ENovaConstants::MaxEquipmentCount; EquipmentIndex++)
		{
			const FNovaEquipmentSlot& EquipmentSlot = Description->GetEquipmentSlot(EquipmentIndex);
			if (Equipment[EquipmentIndex] && (EquipmentSlot.SupportedTypes.Contains(ENovaEquipmentType::Forward) ||
												 EquipmentSlot.SupportedTypes.Contains(ENovaEquipmentType::Aft)))
			{
				return true;
			}
		}
	}

	return false;
}

ENovaResourceType FNovaCompartment::GetCargoType(int32 ModuleIndex) const
{
	const UNovaCargoModuleDescription* CargoModule = Cast<UNovaCargoModuleDescription>(Modules[ModuleIndex].Description);

	if (::IsValid(CargoModule))
	{
		return CargoModule->CargoType;
	}

	return ENovaResourceType::None;
}

float FNovaCompartment::GetCargoCapacity(int32 ModuleIndex) const
{
	const UNovaCargoModuleDescription* CargoModule = Cast<UNovaCargoModuleDescription>(Modules[ModuleIndex].Description);

	if (::IsValid(CargoModule))
	{
		return CargoModule->CargoMass;
	}

	return 0.0f;
}

float FNovaCompartment::GetCargoCapacity(int32 ModuleIndex, ENovaResourceType Type) const
{
	const UNovaCargoModuleDescription* CargoModule = Cast<UNovaCargoModuleDescription>(Modules[ModuleIndex].Description);

	if (::IsValid(CargoModule) && CargoModule->CargoType == Type)
	{
		return CargoModule->CargoMass;
	}

	return 0.0f;
}

float FNovaCompartment::GetCargoMass(int32 ModuleIndex) const
{
	if (::IsValid(Modules[ModuleIndex].Description))
	{
		return Modules[ModuleIndex].Cargo.Amount;
	}

	return 0.0f;
}

float FNovaCompartment::GetCargoMass(int32 ModuleIndex, const UNovaResource* Resource) const
{
	if (::IsValid(Modules[ModuleIndex].Description) && Modules[ModuleIndex].Cargo.Resource == Resource)
	{
		return Modules[ModuleIndex].Cargo.Amount;
	}

	return 0.0f;
}

float FNovaCompartment::GetAvailableCargoMass(int32 ModuleIndex, const UNovaResource* Resource) const
{
	if (::IsValid(Modules[ModuleIndex].Description))
	{
		const FNovaSpacecraftCargo& Cargo = GetCargo(ModuleIndex);
		if (!::IsValid(Cargo.Resource) || Cargo.Resource == Resource)
		{
			return FMath::Max(GetCargoCapacity(ModuleIndex, Resource->Type) - Cargo.Amount, 0.0f);
		}
	}

	return 0.0f;
}

bool FNovaCompartment::CanModifyCargo(int32 ModuleIndex, const class UNovaResource* Resource, float MassDelta) const
{
	NCHECK(::IsValid(Resource));
	const FNovaSpacecraftCargo& Cargo = GetCargo(ModuleIndex);

	if (MassDelta == 0)
	{
		return false;
	}
	else if (MassDelta <= 0 && !::IsValid(Cargo.Resource))
	{
		return false;
	}
	else if (MassDelta > 0 && ::IsValid(Cargo.Resource) && Cargo.Resource != Resource)
	{
		return false;
	}
	else
	{
		return true;
	}
}

void FNovaCompartment::ModifyCargo(int32 ModuleIndex, const class UNovaResource* Resource, float& MassDelta)
{
	NCHECK(::IsValid(Resource));
	FNovaSpacecraftCargo& Cargo = GetCargo(ModuleIndex);

	// Run sanity checks
	if (::IsValid(Cargo.Resource))
	{
		NCHECK(Cargo.Resource == Resource);
	}

	// Actually update the amount and null the resource if necessary
	const float PreviousAmount = Cargo.Amount;
	Cargo.Amount               = FMath::Clamp(Cargo.Amount + MassDelta, 0.0f, GetCargoCapacity(ModuleIndex, Resource->Type));
	if (Cargo.Amount == 0)
	{
		Cargo.Resource = nullptr;
	}
	else
	{
		Cargo.Resource = Resource;
	}

	MassDelta -= (Cargo.Amount - PreviousAmount);
}

void FNovaSpacecraftCustomization::Create()
{
	StructuralPaint = UNeutronAssetManager::Get()->GetDefaultAsset<UNovaPaintDescription>();
	HullPaint       = UNeutronAssetManager::Get()->GetDefaultAsset<UNovaPaintDescription>();
	DetailPaint     = UNeutronAssetManager::Get()->GetDefaultAsset<UNovaPaintDescription>();
	Emblem          = UNeutronAssetManager::Get()->GetDefaultAsset<UNovaEmblemDescription>();
	EnableHullPaint = false;
}

/*----------------------------------------------------
    Spacecraft compartment metrics
----------------------------------------------------*/

FNovaSpacecraftCompartmentMetrics::FNovaSpacecraftCompartmentMetrics(const FNovaSpacecraft& Spacecraft, int32 CompartmentIndex)
	: ModuleCount(0)
	, EquipmentCount(0)
	, DryMass(0.0f)
	, PropellantMassCapacity(0.0f)
	, CargoMassCapacity(0.0f)
	, Thrust(0.0f)
	, TotalEngineISPTimesThrust(0.0f)
{
	if (CompartmentIndex >= 0 && CompartmentIndex < Spacecraft.Compartments.Num())
	{
		const FNovaCompartment& Compartment = Spacecraft.Compartments[CompartmentIndex];

		if (Compartment.IsValid())
		{
			DryMass = Compartment.Description->Mass;

			// Iterate over modules
			for (int32 ModuleIndex = 0; ModuleIndex < ENovaConstants::MaxModuleCount; ModuleIndex++)
			{
				const FNovaCompartmentModule& Module = Compartment.Modules[ModuleIndex];
				if (IsValid(Module.Description))
				{
					ModuleCount++;
					DryMass += Module.Description->Mass;

					// Handle propellant modules
					const UNovaPropellantModuleDescription* PropellantModule = Cast<UNovaPropellantModuleDescription>(Module.Description);
					if (PropellantModule)
					{
						float PropellantMass = PropellantModule->PropellantMass;

						if (Spacecraft.IsSameModuleInNextCompartment(CompartmentIndex, ModuleIndex))
						{
							PropellantMass *= ENovaConstants::SkirtCapacityMultiplier;
						}

						PropellantMassCapacity += PropellantMass;
					}

					// Handle cargo modules
					const UNovaCargoModuleDescription* CargoModule = Cast<UNovaCargoModuleDescription>(Module.Description);
					if (CargoModule)
					{
						float CargoMass = CargoModule->CargoMass;

						if (Spacecraft.IsSameModuleInNextCompartment(CompartmentIndex, ModuleIndex))
						{
							CargoMass *= ENovaConstants::SkirtCapacityMultiplier;
						}

						CargoMassCapacity += CargoMass;
					}
				}
			}

			// Iterate over equipment
			for (const UNovaEquipmentDescription* Equipment : Compartment.Equipment)
			{
				if (IsValid(Equipment))
				{
					EquipmentCount++;
					DryMass += Equipment->Mass;

					// Handle engine equipment
					const UNovaEngineDescription* Engine = Cast<UNovaEngineDescription>(Equipment);
					if (Engine)
					{
						Thrust += Engine->Thrust;
						TotalEngineISPTimesThrust += Engine->SpecificImpulse * Engine->Thrust;
					}

					// Handle external propellant tank
					const UNovaPropellantEquipmentDescription* PropellantTank = Cast<UNovaPropellantEquipmentDescription>(Equipment);
					if (PropellantTank)
					{
						PropellantMassCapacity += PropellantTank->PropellantMass;
					}
				}
			}
		}
	}
}

TArray<FText> FNovaSpacecraftCompartmentMetrics::GetDescription() const
{
	TArray<FText> Result = INeutronDescriptibleInterface::GetDescription();

	Result.Add(FText::FormatNamed(
		LOCTEXT("CompartmentMassFormat", "<img src=\"/Text/Mass\"/> {mass} T"), TEXT("mass"), FText::AsNumber(FMath::RoundToInt(DryMass))));

	if (ModuleCount)
	{
		Result.Add(FText::FormatNamed(
			LOCTEXT("CompartmentModulesFormat", "<img src=\"/Text/Module\"/> {modules} {modules}|plural(one=module,other=modules)"),
			TEXT("modules"), FText::AsNumber(ModuleCount)));
	}

	if (EquipmentCount)
	{
		Result.Add(FText::FormatNamed(LOCTEXT("CompartmentEquipmentFormat", "<img src=\"/Text/Equipment\"/> {equipment} equipment"),
			TEXT("equipment"), FText::AsNumber(EquipmentCount)));
	}

	if (PropellantMassCapacity)
	{
		Result.Add(FText::FormatNamed(LOCTEXT("CompartmentPropellantFormat", "<img src=\"/Text/Propellant\"/> {propellant} T propellant"),
			TEXT("propellant"), FText::AsNumber(FMath::RoundToInt(PropellantMassCapacity))));
	}

	if (CargoMassCapacity)
	{
		Result.Add(FText::FormatNamed(LOCTEXT("CompartmentCargoFormat", "<img src=\"/Text/Cargo\"/> {cargo} T cargo"), TEXT("cargo"),
			FText::AsNumber(FMath::RoundToInt(CargoMassCapacity))));
	}

	return Result;
}

/*----------------------------------------------------
    Spacecraft constructor & operators
----------------------------------------------------*/

bool FNovaSpacecraft::operator==(const FNovaSpacecraft& Other) const
{
	if (Identifier != Other.Identifier)
	{
		return false;
	}
	else if (Name != Other.Name)
	{
		return false;
	}
	else if (Customization != Other.Customization)
	{
		return false;
	}
	else if (Compartments.Num() != Other.Compartments.Num())
	{
		return false;
	}
	else
	{
		for (int32 CompartmentIndex = 0; CompartmentIndex < Compartments.Num(); CompartmentIndex++)
		{
			if (Compartments[CompartmentIndex] != Other.Compartments[CompartmentIndex])
			{
				return false;
			}
		}

		return true;
	}
}

/*----------------------------------------------------
    Spacecraft interface
----------------------------------------------------*/

bool FNovaSpacecraft::IsValid(FText* Details) const
{
	TArray<FText> Issues;

	// Check for simple issues
	if (Name.Len() == 0)
	{
		Issues.Add(LOCTEXT("NoName", "This spacecraft is unnamed"));
	}
	if (PropulsionMetrics.EngineThrust <= 0)
	{
		Issues.Add(LOCTEXT("InsufficientThrust", "This spacecraft has no engine"));
	}
	if (PropulsionMetrics.PropellantMassCapacity <= 0)
	{
		Issues.Add(LOCTEXT("InsufficientPropellant", "This spacecraft has no propellant tank"));
	}
	if (PropulsionMetrics.MaximumDeltaV < 100)
	{
		Issues.Add(LOCTEXT("InsufficientDeltaV", "This spacecraft does not have enough delta-v"));
	}

	// Prepare for detailed analysis
	bool HasAnyThruster = false;
	bool HasAnyHabitat  = false;

	for (int32 CompartmentIndex = 0; CompartmentIndex < Compartments.Num(); CompartmentIndex++)
	{
		const FNovaCompartment& Compartment = Compartments[CompartmentIndex];
		if (::IsValid(Compartment.Description))
		{
			// Check equipment for required items, invalid pairings
			for (int32 EquipmentIndex = 0; EquipmentIndex < ENovaConstants::MaxEquipmentCount; EquipmentIndex++)
			{
				const UNovaEquipmentDescription* Equipment = Compartment.Equipment[EquipmentIndex];
				if (Equipment && Equipment->RequiresPairing)
				{
					for (int32 GroupedIndex : Compartment.Description->GetGroupedEquipmentSlotsIndices(EquipmentIndex))
					{
						if (Compartment.Equipment[GroupedIndex] != Equipment)
						{
							Issues.Add(FText::FormatNamed(LOCTEXT("InvalidPairing",
															  "Equipment in slot {slot} of compartment {compartment} is not "
															  "correctly paired with symmetrical equipment"),
								TEXT("slot"), Compartment.Description->GetEquipmentSlot(EquipmentIndex).DisplayName, TEXT("compartment"),
								FText::AsNumber(CompartmentIndex + 1)));
						}
					}
				}

				if (Equipment)
				{
					if (Equipment->IsA<UNovaThrusterDescription>())
					{
						HasAnyThruster = true;
					}
					else if (Equipment->IsA<UNovaHatchDescription>() && Cast<UNovaHatchDescription>(Equipment)->IsHabitat)
					{
						HasAnyHabitat = true;
					}
				}
			}
		}
	}

	// Check for hatches
	TArray<int32> ModuleGroupsWithoutHatch;
	int32         CurrentIndex = 1;
	for (const FNovaModuleGroup& Group : ModuleGroups)
	{
		if (!Group.HasHatch)
		{
			ModuleGroupsWithoutHatch.Add(CurrentIndex);
		}
		CurrentIndex++;
	}
	if (ModuleGroupsWithoutHatch.Num())
	{
		FString IndicesText;
		for (int32 Index : ModuleGroupsWithoutHatch)
		{
			if (IndicesText.Len())
			{
				IndicesText += TEXT(", ");
			}
			IndicesText += FText::AsNumber(Index).ToString();
		}

		Issues.Add(FText::FormatNamed(LOCTEXT("NoHatchFormat",
										  "{count} module {count}|plural(one=group,other=groups) ({indices}) "
										  "{count}|plural(one=doesn't,other=don't) have a hatch attached"),
			TEXT("count"), FText::AsNumber(ModuleGroupsWithoutHatch.Num()), TEXT("indices"), FText::FromString(IndicesText)));
	}

	// Check for required equipment
	if (!HasAnyThruster)
	{
		Issues.Add(LOCTEXT("NoThrusters", "This spacecraft has no maneuvering thrusters"));
	}
	if (!HasAnyHabitat)
	{
		Issues.Add(LOCTEXT("NoHabitat", "This spacecraft doesn't have any habitable space"));
	}

	// Report issues
	if (Issues.Num() == 0)
	{
		if (Details)
		{
			*Details = LOCTEXT("Changes", "This spacecraft has a valid design");
		}
		return true;
	}
	else
	{
		if (Details)
		{
			FString IssueText;
			for (const FText& Issue : Issues)
			{
				if (IssueText.Len())
				{
					IssueText += "\n";
				}
				IssueText += Issue.ToString();
			}
			*Details = FText::FromString(IssueText);
		}

		return false;
	}
}

FNovaSpacecraftUpgradeCost FNovaSpacecraft::GetUpgradeCost(const ANovaGameState* GameState, const FNovaSpacecraft* Other) const
{
	FNovaSpacecraftUpgradeCost                        Cost;
	TMap<const UNovaTradableAssetDescription*, int32> PartsDelta;
	FNovaCredits                                      TotalCostAsNew = 0;

	// Increase or decrease the amount of one particular part
	auto UpdatePartsData = [&PartsDelta, &TotalCostAsNew, GameState](const UNovaTradableAssetDescription* Asset, bool Add)
	{
		if (::IsValid(Asset))
		{
			int32* Value = PartsDelta.Find(Asset);
			if (Value)
			{
				*Value += Add ? 1 : -1;
			}
			else
			{
				PartsDelta.Add(Asset, Add ? 1 : -1);
			}

			if (Add)
			{
				TotalCostAsNew += GameState->GetCurrentPrice(Asset, GameState->GetCurrentArea(), false);
			}
		}
	};

	// Update the amounts of each parts in a spacecraft
	auto ProcessSpacecraft = [UpdatePartsData](const FNovaSpacecraft* Spacecraft, bool Add)
	{
		for (int32 CompartmentIndex = 0; CompartmentIndex < Spacecraft->Compartments.Num(); CompartmentIndex++)
		{
			const FNovaCompartment& Compartment = Spacecraft->Compartments[CompartmentIndex];
			UpdatePartsData(Compartment.Description, Add);
			UpdatePartsData(Compartment.HullType, Add);

			for (int32 ModuleIndex = 0; ModuleIndex < ENovaConstants::MaxModuleCount; ModuleIndex++)
			{
				UpdatePartsData(Compartment.Modules[ModuleIndex].Description, Add);
			}

			for (int32 EquipmentIndex = 0; EquipmentIndex < ENovaConstants::MaxEquipmentCount; EquipmentIndex++)
			{
				UpdatePartsData(Compartment.Equipment[EquipmentIndex], Add);
			}
		}
	};

	// Run the process on both spacecraft
	ProcessSpacecraft(this, true);
	ProcessSpacecraft(Other, false);

	// Process paint
	if (Customization != Other->Customization)
	{
		Cost.PaintCost += 10 * Compartments.Num();
	}

	// Compute the total change costs
	for (const TPair<const UNovaTradableAssetDescription*, int32>& Entry : PartsDelta)
	{
		if (Entry.Value > 0)
		{
			Cost.UpgradeCost += Entry.Value * GameState->GetCurrentPrice(Entry.Key, GameState->GetCurrentArea(), false);
		}
		else if (Entry.Value < 0)
		{
			Cost.ResaleGain += -Entry.Value * GameState->GetCurrentPrice(Entry.Key, GameState->GetCurrentArea(), true);
		}
	}
	Cost.TotalCost       = TotalCostAsNew + Cost.PaintCost;
	Cost.TotalChangeCost = Cost.UpgradeCost + Cost.PaintCost - Cost.ResaleGain;

	return Cost;
}

FText FNovaSpacecraft::GetClassification() const
{
	if (Compartments.Num() == 0)
	{
		return LOCTEXT("Blank", "N/A");
	}
	else if (PropulsionMetrics.CargoMassCapacity == 0)
	{
		return LOCTEXT("Tug", "Tug");
	}
	else if (PropulsionMetrics.CargoMassCapacity < 500)
	{
		return LOCTEXT("LightFreighter", "Light freighter");
	}
	else
	{
		return LOCTEXT("HeavyFreighter", "Heavy freighter");
	}
}

/*----------------------------------------------------
    Propulsion metrics
----------------------------------------------------*/

void FNovaSpacecraft::UpdateProceduralElements()
{
	for (int32 CompartmentIndex = 0; CompartmentIndex < Compartments.Num(); CompartmentIndex++)
	{
		FNovaCompartment& Compartment = Compartments[CompartmentIndex];

		if (Compartment.IsValid())
		{
			// Process modules
			for (int32 ModuleIndex = 0; ModuleIndex < ENovaConstants::MaxModuleCount; ModuleIndex++)
			{
				FNovaCompartmentModule& Module     = Compartment.Modules[ModuleIndex];
				const FNovaModuleSlot&  ModuleSlot = Compartment.Description->GetModuleSlot(ModuleIndex);

				// Reset state
				Module.ForwardBulkheadType   = ENovaBulkheadType::Standard;
				Module.AftBulkheadType       = ENovaBulkheadType::Standard;
				Module.SkirtPipingType       = ENovaSkirtPipingType::None;
				Module.NeedsConnectionWiring = false;
				Module.NeedsCollectorPiping  = false;
				Module.NeedsDome             = false;
				Module.NeedsSkirt            = false;

				if (Module.Description)
				{
					// Defaults
					Module.NeedsConnectionWiring = true;
					Module.NeedsCollectorPiping  = !ModuleSlot.ForceSkirtPiping;

					// Define bulkheads
					if (IsFirstCompartment(CompartmentIndex))
					{
						Module.ForwardBulkheadType = ENovaBulkheadType::Outer;
					}
					else if (IsSameModuleInPreviousCompartment(CompartmentIndex, ModuleIndex))
					{
						Module.ForwardBulkheadType   = ENovaBulkheadType::Skirt;
						Module.NeedsConnectionWiring = false;
					}
					if (IsLastCompartment(CompartmentIndex))
					{
						Module.AftBulkheadType = ENovaBulkheadType::Outer;
					}
					else if (IsSameModuleInNextCompartment(CompartmentIndex, ModuleIndex))
					{
						Module.AftBulkheadType = ENovaBulkheadType::Skirt;
					}

					// Check for supported equipments
					bool HasSupportedEquipment = false;
					for (int32 EquipmentIndex = 0; EquipmentIndex < ENovaConstants::MaxEquipmentCount; EquipmentIndex++)
					{
						if (ModuleSlot.SupportedEquipments.Contains(Compartment.Description->GetEquipmentSlot(EquipmentIndex).SocketName) &&
							Compartment.Equipment[EquipmentIndex])
						{
							HasSupportedEquipment = true;
							break;
						}
					};

					// Define skirt piping: no module or supported equipment vs regular piping
					if (!IsAnyModuleInNextCompartment(CompartmentIndex, ModuleIndex) && !HasSupportedEquipment &&
						!ModuleSlot.ForceSkirtPiping)
					{
						Module.SkirtPipingType =
							Module.Description->NeedsPiping ? ENovaSkirtPipingType::ShortConnection : ENovaSkirtPipingType::None;
					}
					else
					{
						Module.NeedsSkirt = !ModuleSlot.ForceSkirtPiping;

						if (Module.Description->NeedsPiping && !IsSameModuleInNextCompartment(CompartmentIndex, ModuleIndex))
						{
							Module.SkirtPipingType = ENovaSkirtPipingType::Connection;
						}
						else
						{
							Module.SkirtPipingType = ENovaSkirtPipingType::Simple;
						}
					}

					// Add skirt domes to non-central modules if the previous compartment is different
					if ((IsFirstCompartment(CompartmentIndex) ||
							Compartments[CompartmentIndex - 1].Description != Compartment.Description) &&
						!ModuleSlot.ForceSkirtPiping)
					{
						Module.NeedsDome = true;
					}
				}
			}
		}
	}
}

void FNovaSpacecraft::UpdatePropulsionMetrics()
{
	PropulsionMetrics               = FNovaSpacecraftPropulsionMetrics();
	float TotalEngineISPTimesThrust = 0;

	// Iterate over compartments
	for (int32 CompartmentIndex = 0; CompartmentIndex < Compartments.Num(); CompartmentIndex++)
	{
		FNovaSpacecraftCompartmentMetrics Metrics(*this, CompartmentIndex);

		PropulsionMetrics.DryMass += Metrics.DryMass;
		PropulsionMetrics.PropellantMassCapacity += Metrics.PropellantMassCapacity;
		PropulsionMetrics.CargoMassCapacity += Metrics.CargoMassCapacity;
		PropulsionMetrics.EngineThrust += Metrics.Thrust;
		TotalEngineISPTimesThrust += Metrics.TotalEngineISPTimesThrust;

		for (const UNovaEquipmentDescription* Equipment : Compartments[CompartmentIndex].Equipment)
		{
			const UNovaThrusterDescription* Thruster = Cast<UNovaThrusterDescription>(Equipment);
			if (Thruster)
			{
				PropulsionMetrics.ThrusterThrust += Thruster->Thrust;
			}
		}
	}

	// Compute metrics
	PropulsionMetrics.MaximumMass =
		PropulsionMetrics.DryMass + PropulsionMetrics.PropellantMassCapacity + PropulsionMetrics.CargoMassCapacity;
	if (PropulsionMetrics.EngineThrust > 0)
	{
		PropulsionMetrics.SpecificImpulse = TotalEngineISPTimesThrust / PropulsionMetrics.EngineThrust;
		PropulsionMetrics.ExhaustVelocity = ENovaConstants::StandardGravity * PropulsionMetrics.SpecificImpulse;
		PropulsionMetrics.PropellantRate  = PropulsionMetrics.EngineThrust / PropulsionMetrics.ExhaustVelocity;
		PropulsionMetrics.MaximumDeltaV =
			PropulsionMetrics.ExhaustVelocity * log((PropulsionMetrics.MaximumMass) / PropulsionMetrics.DryMass);
		PropulsionMetrics.MaximumBurnTime = PropulsionMetrics.PropellantMassCapacity / PropulsionMetrics.PropellantRate;
	}

	PropellantMassAtLaunch = FMath::Min(PropellantMassAtLaunch, PropulsionMetrics.PropellantMassCapacity);

#if 0
	NLOG("--------------------------------------------------------------------------------");
	NLOG("Mass specifications : dry %.1fT, propellant %.1fT, cargo %.1fT, total %.1fT", PropulsionMetrics.DryMass, PropulsionMetrics.PropellantMass,
		PropulsionMetrics.CargoMass, PropulsionMetrics.TotalMass);
	NLOG("Engine specifications : thrust %.1fkN, ISP %.1fm/s, EV %.1fm/s", PropulsionMetrics.Thrust, PropulsionMetrics.SpecificImpulse,
		PropulsionMetrics.ExhaustVelocity);
	NLOG("Delta-v specifications : total %.1fm/s, burn time %.1fs", PropulsionMetrics.TotalDeltaV, PropulsionMetrics.TotalBurnTime);
	NLOG("--------------------------------------------------------------------------------");
#endif
}

void FNovaSpacecraft::UpdateModuleGroups()
{
	ModuleGroups.Empty();

	// Build basic groups first as simple lines
	for (int32 CompartmentIndex = 0; CompartmentIndex < Compartments.Num(); CompartmentIndex++)
	{
		const FNovaCompartment& Compartment = Compartments[CompartmentIndex];
		if (::IsValid(Compartment.Description))
		{
			// Check modules for groups
			for (int32 ModuleIndex = 0; ModuleIndex < ENovaConstants::MaxModuleCount; ModuleIndex++)
			{
				const FNovaModuleSlot&        ModuleSlot = Compartment.Description->GetModuleSlot(ModuleIndex);
				const UNovaModuleDescription* Module     = Compartment.Modules[ModuleIndex].Description;

				if (Module)
				{
					ENovaModuleGroupType Type                  = GetModuleType(Module);
					FNovaModuleGroup*    CurrentModuleGroup    = nullptr;
					int32                FoundCompartmentIndex = INDEX_NONE;
					int32                FoundModuleIndex      = INDEX_NONE;

					// This module is part of an existing group, append it to the last compartment
					if (IsSameKindModuleInPreviousCompartment(CompartmentIndex, ModuleIndex, Type, FoundCompartmentIndex, FoundModuleIndex))
					{
						for (FNovaModuleGroup& OtherGroup : ModuleGroups)
						{
							for (const FNovaModuleGroupCompartment& OtherGroupCompartment : OtherGroup.Compartments)
							{
								if (OtherGroupCompartment.CompartmentIndex == FoundCompartmentIndex &&
									OtherGroupCompartment.ModuleIndices.Contains(FoundModuleIndex))
								{
									// NLOG("FOUND PRE for %d %d (%d %d), slot is %s", CompartmentIndex, ModuleIndex, FoundCompartmentIndex,
									//	FoundModuleIndex, *ModuleSlot.SocketName.ToString());

									CurrentModuleGroup = &OtherGroup;
									OtherGroup.Compartments.Add(FNovaModuleGroupCompartment(CompartmentIndex, ModuleIndex));
									break;
								}
							}
						}
						NCHECK(CurrentModuleGroup);
					}

					// Work sideways to find linked equipments
					else
					{
						for (FNovaModuleGroup& OtherGroup : ModuleGroups)
						{
							FNovaModuleGroupCompartment& OtherGroupCompartment = OtherGroup.Compartments.Last();
							if (OtherGroup.Type == Type && OtherGroupCompartment.CompartmentIndex == CompartmentIndex)
							{
								for (const FName EquipmentSlot : ModuleSlot.LinkedEquipments)
								{
									if (OtherGroupCompartment.LinkedEquipments.Contains(EquipmentSlot))
									{
										// NLOG("FOUND LINK %d %d because %s (%d), slot is %s", CompartmentIndex, ModuleIndex,
										//	*EquipmentSlot.ToString(), OtherGroupCompartment.CompartmentIndex,
										//	*ModuleSlot.SocketName.ToString());
										// for (int32 DebugOtherModuleIndex : OtherGroupCompartment.ModuleIndices)
										//{
										//	NLOG("> Other module there was %d", DebugOtherModuleIndex);
										// }

										OtherGroupCompartment.ModuleIndices.Add(ModuleIndex);
										CurrentModuleGroup = &OtherGroup;
										break;
									}
								}
							}
						}
					}

					// This module is the start of a new group
					if (CurrentModuleGroup == nullptr)
					{
						// NLOG("ADD %d %d, slot is %s", CompartmentIndex, ModuleIndex, *ModuleSlot.SocketName.ToString());

						FNovaModuleGroup Group;
						Group.Compartments.Add(FNovaModuleGroupCompartment(CompartmentIndex, ModuleIndex));
						Group.Type = Type;
						ModuleGroups.Add(Group);
						CurrentModuleGroup = &ModuleGroups.Last();
					}
					NCHECK(CurrentModuleGroup);

					// Process linked equipment
					FNovaModuleGroupCompartment& GroupCompartment = CurrentModuleGroup->Compartments.Last();
					GroupCompartment.LinkedEquipments.Append(ModuleSlot.LinkedEquipments);
					for (const FName EquipmentSlot : ModuleSlot.LinkedEquipments)
					{
						const UNovaEquipmentDescription* Equipment = Compartment.GetEquipmentySocket(EquipmentSlot);
						if (Equipment && Equipment->IsA<UNovaHatchDescription>())
						{
							CurrentModuleGroup->HasHatch = true;
							break;
						}
					}
				}
			}
		}
	}
}

/*----------------------------------------------------
    Cargo
----------------------------------------------------*/

float FNovaSpacecraft::GetCargoCapacity(ENovaResourceType Type, int32 CompartmentIndex, int32 ModuleIndex) const
{
	float CargoMass = 0;

	for (int32 CI = 0; CI < Compartments.Num(); CI++)
	{
		if (CI == CompartmentIndex || CompartmentIndex == INDEX_NONE)
		{
			for (int32 MI = 0; MI < ENovaConstants::MaxModuleCount; MI++)
			{
				if (MI == ModuleIndex || ModuleIndex == INDEX_NONE)
				{
					CargoMass += Compartments[CI].GetCargoCapacity(MI, Type);
				}
			}
		}
	}

	return CargoMass;
}

float FNovaSpacecraft::GetCargoMass(const class UNovaResource* Resource, int32 CompartmentIndex, int32 ModuleIndex) const
{
	float CargoMass = 0;

	for (int32 CI = 0; CI < Compartments.Num(); CI++)
	{
		if (CI == CompartmentIndex || CompartmentIndex == INDEX_NONE)
		{
			for (int32 MI = 0; MI < ENovaConstants::MaxModuleCount; MI++)
			{
				if (MI == ModuleIndex || ModuleIndex == INDEX_NONE)
				{
					CargoMass += Compartments[CI].GetCargoMass(MI, Resource);
				}
			}
		}
	}

	return CargoMass;
}

float FNovaSpacecraft::GetAvailableCargoMass(const UNovaResource* Resource, int32 CompartmentIndex, int32 ModuleIndex) const
{
	float CargoMass = 0;

	for (int32 CI = 0; CI < Compartments.Num(); CI++)
	{
		if (CI == CompartmentIndex || CompartmentIndex == INDEX_NONE)
		{
			for (int32 MI = 0; MI < ENovaConstants::MaxModuleCount; MI++)
			{
				if (MI == ModuleIndex || ModuleIndex == INDEX_NONE)
				{
					CargoMass += Compartments[CI].GetAvailableCargoMass(MI, Resource);
				}
			}
		}
	}

	return CargoMass;
}

bool FNovaSpacecraft::ModifyCargo(const class UNovaResource* Resource, float MassDelta, int32 CompartmentIndex, int32 ModuleIndex)
{
	for (int32 CI = 0; CI < Compartments.Num(); CI++)
	{
		if (CI == CompartmentIndex || CompartmentIndex == INDEX_NONE)
		{
			for (int32 MI = 0; MI < ENovaConstants::MaxModuleCount; MI++)
			{
				if (MI == ModuleIndex || ModuleIndex == INDEX_NONE)
				{
					if (Compartments[CI].CanModifyCargo(MI, Resource, MassDelta))
					{
						Compartments[CI].ModifyCargo(MI, Resource, MassDelta);
						if (MassDelta == 0)
						{
							break;
						}
					}
				}
			}
		}
	}

	return MassDelta == 0;
}

/*----------------------------------------------------
    UI helpers
----------------------------------------------------*/

TArray<const UNovaCompartmentDescription*> FNovaSpacecraft::GetCompatibleCompartments(int32 CompartmentIndex) const
{
	TArray<const UNovaCompartmentDescription*> CompartmentDescriptions;

	for (const UNovaCompartmentDescription* Description : UNeutronAssetManager::Get()->GetSortedAssets<UNovaCompartmentDescription>())
	{
		if (Description->IsForwardCompartment && (!IsFirstCompartment(CompartmentIndex) || Compartments.Num() == 0))
		{
			continue;
		}
		else
		{
			CompartmentDescriptions.Add(Description);
		}
	}

	return CompartmentDescriptions;
}

TArray<const UNovaModuleDescription*> FNovaSpacecraft::GetCompatibleModules(int32 CompartmentIndex, int32 SlotIndex) const
{
	TArray<const UNovaModuleDescription*> ModuleDescriptions;
	TArray<const UNovaModuleDescription*> AllModuleDescriptions = UNeutronAssetManager::Get()->GetSortedAssets<UNovaModuleDescription>();
	const FNovaCompartment&               Compartment           = Compartments[CompartmentIndex];

	ModuleDescriptions.Add(nullptr);
	if (Compartment.IsValid() && SlotIndex < Compartment.Description->ModuleSlots.Num())
	{
		for (const UNovaModuleDescription* ModuleDescription : AllModuleDescriptions)
		{
			ModuleDescriptions.AddUnique(ModuleDescription);
		}
	}

	return ModuleDescriptions;
}

TArray<const UNovaEquipmentDescription*> FNovaSpacecraft::GetCompatibleEquipment(int32 CompartmentIndex, int32 SlotIndex) const
{
	TArray<const UNovaEquipmentDescription*> EquipmentDescriptions;
	TArray<const UNovaEquipmentDescription*> AllEquipmentDescriptions =
		UNeutronAssetManager::Get()->GetSortedAssets<UNovaEquipmentDescription>();
	const FNovaCompartment& Compartment = Compartments[CompartmentIndex];

	EquipmentDescriptions.Add(nullptr);
	if (Compartment.IsValid() && SlotIndex < Compartment.Description->EquipmentSlots.Num())
	{
		for (const UNovaEquipmentDescription* EquipmentDescription : AllEquipmentDescriptions)
		{
			const TArray<ENovaEquipmentType>& SupportedTypes = Compartment.Description->EquipmentSlots[SlotIndex].SupportedTypes;
			if (SupportedTypes.Num() == 0 || SupportedTypes.Contains(EquipmentDescription->EquipmentType))
			{
				if (EquipmentDescription->EquipmentType == ENovaEquipmentType::Forward && !IsFirstCompartment(CompartmentIndex))
				{
					continue;
				}
				else if (EquipmentDescription->EquipmentType == ENovaEquipmentType::Aft && !IsLastCompartment(CompartmentIndex))
				{
					continue;
				}
				else
				{
					EquipmentDescriptions.AddUnique(EquipmentDescription);
				}
			}
		}
	}

	return EquipmentDescriptions;
}

bool FNovaSpacecraft::IsFirstCompartment(int32 CompartmentIndex) const
{
	for (int32 Index = 0; Index < CompartmentIndex; Index++)
	{
		if (Compartments[Index].IsValid())
		{
			return false;
		}
	}
	return true;
}

bool FNovaSpacecraft::IsLastCompartment(int32 CompartmentIndex) const
{
	for (int32 Index = CompartmentIndex + 1; Index < Compartments.Num(); Index++)
	{
		if (Compartments[Index].IsValid())
		{
			return false;
		}
	}
	return true;
}

const UNovaModuleDescription* FNovaSpacecraft::GetModuleInPreviousCompartment(
	int32 CompartmentIndex, int32 ModuleIndex, bool RequireSameType) const
{
	const FNovaCompartment&       Compartment             = Compartments[CompartmentIndex];
	const FNovaCompartmentModule& Module                  = Compartment.Modules[ModuleIndex];
	FName                         CurrentModuleSocketName = Compartment.Description->GetModuleSlot(ModuleIndex).SocketName;

	CompartmentIndex--;
	if (CompartmentIndex >= 0)
	{
		const FNovaCompartment& PreviousCompartment = Compartments[CompartmentIndex];
		if (PreviousCompartment.IsValid())
		{
			const UNovaModuleDescription* PreviousModule = PreviousCompartment.GetModuleBySocket(CurrentModuleSocketName);
			if ((RequireSameType && PreviousModule == Module.Description) || (!RequireSameType && PreviousModule))
			{
				return PreviousModule;
			}
		}
	}

	return nullptr;
};

const UNovaModuleDescription* FNovaSpacecraft::GetModuleInNextCompartment(
	int32 CompartmentIndex, int32 ModuleIndex, bool RequireSameType) const
{
	const FNovaCompartment&       Compartment             = Compartments[CompartmentIndex];
	const FNovaCompartmentModule& Module                  = Compartment.Modules[ModuleIndex];
	FName                         CurrentModuleSocketName = Compartment.Description->GetModuleSlot(ModuleIndex).SocketName;

	CompartmentIndex++;
	if (CompartmentIndex < Compartments.Num())
	{
		const FNovaCompartment& NextCompartment = Compartments[CompartmentIndex];
		if (NextCompartment.IsValid())
		{
			const UNovaModuleDescription* NextModule = NextCompartment.GetModuleBySocket(CurrentModuleSocketName);
			if ((RequireSameType && NextModule == Module.Description) || (!RequireSameType && NextModule))
			{
				return NextModule;
			}
		}
	}

	return nullptr;
};

bool FNovaSpacecraft::IsSameKindModuleInPreviousCompartment(
	int32 CompartmentIndex, int32 ModuleIndex, ENovaModuleGroupType Type, int32& FoundCompartmentIndex, int32& FoundModuleIndex) const
{
	const FNovaCompartment&       Compartment             = Compartments[CompartmentIndex];
	const FNovaCompartmentModule& Module                  = Compartment.Modules[ModuleIndex];
	FName                         CurrentModuleSocketName = Compartment.Description->GetModuleSlot(ModuleIndex).SocketName;

	CompartmentIndex--;
	if (CompartmentIndex >= 0)
	{
		const FNovaCompartment& PreviousCompartment = Compartments[CompartmentIndex];
		if (PreviousCompartment.IsValid())
		{
			int32                         OtherModuleIndex;
			const FNovaCompartmentModule* ModuleData = PreviousCompartment.GetModuleDataBySocket(CurrentModuleSocketName, OtherModuleIndex);
			if (ModuleData && ModuleData->Description && GetModuleType(ModuleData->Description) == Type)
			{
				FoundCompartmentIndex = CompartmentIndex;
				FoundModuleIndex      = OtherModuleIndex;
				return true;
			}
		}
	}

	return false;
};

ENovaModuleGroupType FNovaSpacecraft::GetModuleType(const UNovaModuleDescription* Module) const
{
	if (Module->IsA<UNovaCargoModuleDescription>() /* || Module->IsA<UnovaCrewModuleDescription>()*/)
	{
		return ENovaModuleGroupType::Hatch;
	}
	else
	{
		return ENovaModuleGroupType::Propellant;
	}
}

#undef LOCTEXT_NAMESPACE
