// Astral Shipwright - Gwennaël Arbona

#include "NovaMainMenuOperations.h"

#include "NovaMainMenu.h"

#include "Game/NovaGameState.h"
#include "Player/NovaPlayerController.h"
#include "Spacecraft/NovaSpacecraftPawn.h"
#include "Spacecraft/System/NovaSpacecraftPropellantSystem.h"
#include "Spacecraft/System/NovaSpacecraftProcessingSystem.h"

#include "UI/Widgets/NovaTradingPanel.h"
#include "UI/Widgets/NovaTradableAssetItem.h"

#include "Nova.h"

#include "Neutron/System/NeutronAssetManager.h"
#include "Neutron/System/NeutronGameInstance.h"
#include "Neutron/System/NeutronMenuManager.h"
#include "Neutron/UI/Widgets/NeutronFadingWidget.h"
#include "Neutron/UI/Widgets/NeutronModalPanel.h"

#include "Widgets/Layout/SBackgroundBlur.h"
#include "Widgets/Layout/SScaleBox.h"

#define LOCTEXT_NAMESPACE "SNovaMainMenuOperations"

/*----------------------------------------------------
    Constructor
----------------------------------------------------*/

SNovaMainMenuOperations::SNovaMainMenuOperations()
	: PC(nullptr)
	, GameState(nullptr)
	, Spacecraft(nullptr)
	, SpacecraftPawn(nullptr)
	, CurrentCompartmentIndex(INDEX_NONE)
	, CurrentModuleIndexx(INDEX_NONE)
{}

void SNovaMainMenuOperations::Construct(const FArguments& InArgs)
{
	// Data
	const FNeutronMainTheme& Theme = FNeutronStyleSet::GetMainTheme();
	MenuManager                    = InArgs._MenuManager;

	// Parent constructor
	SNeutronNavigationPanel::Construct(SNeutronNavigationPanel::FArguments().Menu(InArgs._Menu));

	// Local data
	TSharedPtr<SScrollBox> MainLayoutBox;

	// clang-format off
	ChildSlot
	.HAlign(HAlign_Center)
	[
		SAssignNew(MainLayoutBox, SScrollBox)
		.Style(&Theme.ScrollBoxStyle)
		.ScrollBarVisibility(EVisibility::Collapsed)
		.AnimateWheelScrolling(true)
		
		// Bulk trading title
		+ SScrollBox::Slot()
		.Padding(Theme.VerticalContentPadding)
		[
			SNew(STextBlock)
			.TextStyle(&Theme.HeadingFont)
			.Text(LOCTEXT("BatchTrade", "Batch trading"))
		]

		// Bulk trading box
		+ SScrollBox::Slot()
		[
			SNew(SHorizontalBox)

			// Batch buying
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNeutronNew(SNeutronButton)
				.Text(LOCTEXT("BulkBuy", "Bulk buy"))
				.HelpText(LOCTEXT("BulkBuyHelp", "Bulk buy resources across all compartments"))
				.Enabled(this, &SNovaMainMenuOperations::IsBulkTradeEnabled)
				.OnClicked(this, &SNovaMainMenuOperations::OnBatchBuy)
				.Size("HighButtonSize")
			]

			// Batch selling
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNeutronNew(SNeutronButton)
				.Text(LOCTEXT("BulkSell", "Bulk sell"))
				.HelpText(LOCTEXT("BulkSellHelp", "Bulk sell resources across all compartments"))
				.Enabled(this, &SNovaMainMenuOperations::IsBulkTradeEnabled)
				.OnClicked(this, &SNovaMainMenuOperations::OnBatchSell)
				.Size("HighButtonSize")
			]

			// Propellant data
			+ SHorizontalBox::Slot()
			[
				SNew(SNeutronButtonLayout)
				.Size("HighButtonSize")
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(Theme.VerticalContentPadding)
					[
						SNew(STextBlock)
						.TextStyle(&Theme.HeadingFont)
						.Text(LOCTEXT("Propellant", "Propellant"))
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(Theme.VerticalContentPadding)
					.HAlign(HAlign_Left)
					[
						SNew(SNeutronRichText)
						.TextStyle(&Theme.MainFont)
						.Text(FNeutronTextGetter::CreateSP(this, &SNovaMainMenuOperations::GetPropellantText))
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SProgressBar)
						.Style(&Theme.ProgressBarStyle)
						.Percent(this, &SNovaMainMenuOperations::GetPropellantRatio)
					]

					+ SVerticalBox::Slot()
				]
			]
			
			// Propellant button
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNeutronNew(SNeutronButton)
				.HelpText(LOCTEXT("TradePropellantHelp", "Trade propellant with this station"))
				.Action(FNeutronPlayerInput::MenuPrimary)
				.ActionFocusable(false)
				.Size("HighButtonSize")
				.Enabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateLambda([=]()
				{
					return SpacecraftPawn && SpacecraftPawn->IsDocked() && !SpacecraftPawn->HasModifications();
				})))
				.OnClicked(this, &SNovaMainMenuOperations::OnRefillPropellant)
				.Content()
				[
					SNew(SOverlay)
						
					+ SOverlay::Slot()
					[
						SNew(SScaleBox)
						.Stretch(EStretch::ScaleToFill)
						[
							SNew(SImage)
							.Image(&UNovaResource::GetPropellant()->AssetRender)
						]
					]

					+ SOverlay::Slot()
					.VAlign(VAlign_Top)
					[
						SNew(SBorder)
						.BorderImage(&Theme.MainMenuDarkBackground)
						.Padding(Theme.ContentPadding)
						[
							SNew(STextBlock)
							.TextStyle(&Theme.MainFont)
							.Text(LOCTEXT("TradePropellant", "Trade propellant"))
						]
					]
				]
			]
		]
		
		// Module group controls
		+ SScrollBox::Slot()
		.Padding(Theme.VerticalContentPadding)
		[
			SNew(STextBlock)
			.TextStyle(&Theme.HeadingFont)
			.Text(LOCTEXT("ModuleGroupsTitle", "Module groups"))
		]

		// module groups
		+ SScrollBox::Slot()
		[
			SAssignNew(ModuleGroupsBox, SVerticalBox)
		]

		// Compartments title
		+ SScrollBox::Slot()
		.Padding(Theme.VerticalContentPadding)
		[
			SNew(STextBlock)
			.TextStyle(&Theme.HeadingFont)
			.Text(LOCTEXT("CompartmentsTitle", "Compartments"))
		]
	];

	// Module line generator
	auto BuildModuleLine = [&](int32 ModuleIndex)
	{
		TSharedPtr<SHorizontalBox> ModuleLineBox;
		MainLayoutBox->AddSlot()
		[
			SAssignNew(ModuleLineBox, SHorizontalBox)
		];

		for (int32 CompartmentIndex = 0; CompartmentIndex < ENovaConstants::MaxCompartmentCount; CompartmentIndex++)
		{
			ModuleLineBox->AddSlot()
			.AutoWidth()
			[
				SNeutronNew(SNeutronButton)
				.Size("InventoryButtonSize")
				.HelpText(this, &SNovaMainMenuOperations::GetModuleHelpText, CompartmentIndex, ModuleIndex)
				.Enabled(this, &SNovaMainMenuOperations::IsModuleEnabled, CompartmentIndex, ModuleIndex)
				.OnClicked(this, &SNovaMainMenuOperations::OnInteractWithModule, CompartmentIndex, ModuleIndex)
				.Content()
				[
					SNew(SOverlay)

					// Resource picture
					+ SOverlay::Slot()
					[
						SNew(SScaleBox)
						.Stretch(EStretch::ScaleToFill)
						[
							SNew(SNeutronImage)
							.Image(FNeutronImageGetter::CreateSP(this, &SNovaMainMenuOperations::GetModuleImage, CompartmentIndex, ModuleIndex))
						]
					]

					// Compartment index
					+ SOverlay::Slot()
					[
						SNew(SVerticalBox)

						// Name & details
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(SBorder)
							.BorderImage(&Theme.MainMenuDarkBackground)
							.Padding(Theme.ContentPadding)
							[
								SNew(SVerticalBox)

								+ SVerticalBox::Slot()
								.AutoHeight()
								[
									SNew(SNeutronText)
									.Text(FNeutronTextGetter::CreateSP(this, &SNovaMainMenuOperations::GetModuleText, CompartmentIndex, ModuleIndex))
									.TextStyle(&Theme.MainFont)
									.AutoWrapText(true)
								]

								+ SVerticalBox::Slot()
								.AutoHeight()
								[
									SNew(SNeutronRichText)
									.Text(FNeutronTextGetter::CreateSP(this, &SNovaMainMenuOperations::GetModuleDetails, CompartmentIndex, ModuleIndex))
									.TextStyle(&Theme.MainFont)
									.AutoWrapText(true)
								]
							]
						]
					
						+ SVerticalBox::Slot()

						// Module group
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(SBorder)
							.ColorAndOpacity(TAttribute<FLinearColor>::Create(TAttribute<FLinearColor>::FGetter::CreateLambda([=]()
							{
								if (IsValidModule(CompartmentIndex, ModuleIndex))
								{
									const FNovaModuleGroup* ModuleGroup = SpacecraftPawn->FindModuleGroup(CompartmentIndex, ModuleIndex);
									if (ModuleGroup)
									{
										return ModuleGroup->Color;
									}
								}

								return FLinearColor::White;
							})))
							.BorderImage(&Theme.MainMenuGenericBackground)
							.Padding(Theme.ContentPadding)
							.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateLambda([=]()
							{
								if (IsValidModule(CompartmentIndex, ModuleIndex))
								{
									return EVisibility::Visible;
								}

								return EVisibility::Collapsed;
							})))
							[
								SNew(SNeutronRichText)
								.Text(FNeutronTextGetter::CreateLambda([=]() -> FText
								{
									if (IsValidModule(CompartmentIndex, ModuleIndex))
									{
										const FNovaModuleGroup* ModuleGroup = SpacecraftPawn->FindModuleGroup(CompartmentIndex, ModuleIndex);
										if (ModuleGroup)
										{
											return FText::FormatNamed(INVTEXT("<img src=\"{icon}\"/> {index}"),
												TEXT("icon"), FNovaSpacecraft::GetModuleGroupIcon(ModuleGroup->Type),
												TEXT("index"), FText::AsNumber(ModuleGroup->Index + 1));
										}
									}
							
									return FText();
								}))
								.TextStyle(&Theme.MainFont)
								.WrapTextAt(1.9f * FNeutronStyleSet::GetButtonSize("CompartmentButtonSize").Width)
								.AutoWrapText(false)
							]
						]
					]
				]
			];
		}
	};
	// clang-format on

	// Build the trading panels
	GenericModalPanel = Menu->CreateModalPanel();
	TradingModalPanel = Menu->CreateModalPanel<SNovaTradingPanel>();

	// Build the resource list panel
	// clang-format off
	SAssignNew(ResourceListView, SNeutronListView<const UNovaResource*>)
	.Panel(GenericModalPanel.Get())
	.ItemsSource(&ResourceList)
	.OnGenerateItem(this, &SNovaMainMenuOperations::GenerateResourceItem)
	.OnGenerateTooltip(this, &SNovaMainMenuOperations::GenerateResourceTooltip)
	.OnSelectionDoubleClicked(this, &SNovaMainMenuOperations::OnBuyResource)
	.Horizontal(true)
	.ButtonSize("LargeListButtonSize");
	// clang-format on

	// Build the procedural cargo lines
	for (int32 ModuleIndex = 0; ModuleIndex < ENovaConstants::MaxModuleCount; ModuleIndex++)
	{
		BuildModuleLine(ModuleIndex);
	}

	AveragedPropellantRatio.SetPeriod(1.0f);
}

/*----------------------------------------------------
    Interaction
----------------------------------------------------*/

void SNovaMainMenuOperations::Tick(const FGeometry& AllottedGeometry, const double CurrentTime, const float DeltaTime)
{
	SNeutronTabPanel::Tick(AllottedGeometry, CurrentTime, DeltaTime);

	if (Spacecraft && GameState)
	{
		AveragedPropellantRatio.Set(PropellantSystem->GetCurrentPropellantMass() / PropellantSystem->GetPropellantCapacity(), DeltaTime);
	}
}

void SNovaMainMenuOperations::Show()
{
	SNeutronTabPanel::Show();

	NCHECK(Spacecraft);
	const FNeutronMainTheme& Theme = FNeutronStyleSet::GetMainTheme();

	for (const FNovaModuleGroup& Group : Spacecraft->GetModuleGroups())
	{
		// TODO get resources, if output...

		// clang-format off
		ModuleGroupsBox->AddSlot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNeutronNew(SNeutronButton)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SRichTextBlock)
				.TextStyle(&Theme.MainFont)
				.Text(FText::FormatNamed(INVTEXT("<img src=\"{icon}\"/> {index}"),
												TEXT("icon"), FNovaSpacecraft::GetModuleGroupIcon(Group.Type),
												TEXT("index"), FText::AsNumber(Group.Index + 1)))
				.DecoratorStyleSet(&FNeutronStyleSet::GetStyle())
				+ SRichTextBlock::ImageDecorator()
			]
		];
		// clang-format on
	}
}

void SNovaMainMenuOperations::Hide()
{
	SNeutronTabPanel::Hide();

	ModuleGroupsBox->ClearChildren();
}

void SNovaMainMenuOperations::UpdateGameObjects()
{
	PC             = MenuManager.IsValid() ? MenuManager->GetPC<ANovaPlayerController>() : nullptr;
	GameState      = IsValid(PC) ? MenuManager->GetWorld()->GetGameState<ANovaGameState>() : nullptr;
	Spacecraft     = IsValid(PC) ? PC->GetSpacecraft() : nullptr;
	SpacecraftPawn = IsValid(PC) ? PC->GetSpacecraftPawn() : nullptr;
	PropellantSystem =
		IsValid(GameState) && Spacecraft ? GameState->GetSpacecraftSystem<UNovaSpacecraftPropellantSystem>(Spacecraft) : nullptr;
	ProcessingSystem =
		IsValid(GameState) && Spacecraft ? GameState->GetSpacecraftSystem<UNovaSpacecraftProcessingSystem>(Spacecraft) : nullptr;
}

/*----------------------------------------------------
    Resource list
----------------------------------------------------*/

TSharedRef<SWidget> SNovaMainMenuOperations::GenerateResourceItem(const UNovaResource* Resource)
{
	return SNew(SNovaTradableAssetItem).Asset(Resource).GameState(GameState).Dark(true);
}

FText SNovaMainMenuOperations::GenerateResourceTooltip(const UNovaResource* Resource)
{
	return FText::FormatNamed(LOCTEXT("ResourceHelp", "Buy {resource}"), TEXT("resource"), Resource->Name);
}

/*----------------------------------------------------
    Content callbacks
----------------------------------------------------*/

bool SNovaMainMenuOperations::IsBulkTradeEnabled() const
{
	return SpacecraftPawn->IsDocked() && !SpacecraftPawn->HasModifications();
}

TOptional<float> SNovaMainMenuOperations::GetPropellantRatio() const
{
	return AveragedPropellantRatio.Get();
}

FText SNovaMainMenuOperations::GetPropellantText() const
{
	if (SpacecraftPawn && Spacecraft)
	{
		FNumberFormattingOptions Options;
		Options.MaximumFractionalDigits = 0;

		float RemainingDeltaV = Spacecraft->GetPropulsionMetrics().GetRemainingDeltaV(
			Spacecraft->GetCurrentCargoMass(), PropellantSystem->GetCurrentPropellantMass());

		return FText::FormatNamed(
			LOCTEXT("PropellantFormat",
				"<img src=\"/Text/Propellant\"/> {remaining} T out of {total} T <img src=\"/Text/Thrust\"/> {deltav} m/s delta-v"),
			TEXT("remaining"), FText::AsNumber(PropellantSystem->GetCurrentPropellantMass(), &Options), TEXT("total"),
			FText::AsNumber(PropellantSystem->GetPropellantCapacity(), &Options), TEXT("deltav"),
			FText::AsNumber(RemainingDeltaV, &Options));
	}

	return FText();
}

bool SNovaMainMenuOperations::IsValidModule(int32 CompartmentIndex, int32 ModuleIndex) const
{
	if (Spacecraft && IsValid(SpacecraftPawn) && CompartmentIndex >= 0 && CompartmentIndex < Spacecraft->Compartments.Num() &&
		IsValid(GetModule(CompartmentIndex, ModuleIndex).Description))
	{
		const UNovaModuleDescription* Desc = GetModule(CompartmentIndex, ModuleIndex).Description;

		// Cargo
		if (Desc->IsA<UNovaCargoModuleDescription>())
		{
			return Spacecraft->Compartments[CompartmentIndex].GetCargoCapacity(ModuleIndex) > 0;
		}

		// Propellant
		else if (Desc->IsA<UNovaPropellantModuleDescription>())
		{
			return true;
		}

		// Processing
		else if (Desc->IsA<UNovaProcessingModuleDescription>())
		{
			return true;
		}
	}

	return false;
}

const FNovaCompartmentModule& SNovaMainMenuOperations::GetModule(int32 CompartmentIndex, int32 ModuleIndex) const
{
	return Spacecraft->Compartments[CompartmentIndex].Modules[ModuleIndex];
}

bool SNovaMainMenuOperations::IsModuleEnabled(int32 CompartmentIndex, int32 ModuleIndex) const
{
	if (IsValidModule(CompartmentIndex, ModuleIndex))
	{
		const UNovaModuleDescription* Desc = GetModule(CompartmentIndex, ModuleIndex).Description;

		// Cargo
		if (Desc->IsA<UNovaCargoModuleDescription>())
		{
			const FNovaSpacecraftCargo& Cargo = ProcessingSystem->GetCargo(CompartmentIndex, ModuleIndex);
			return (SpacecraftPawn->IsDocked() || IsValid(Cargo.Resource)) && !SpacecraftPawn->HasModifications();
		}

		// Propellant
		else if (Desc->IsA<UNovaPropellantModuleDescription>())
		{
			return SpacecraftPawn->IsDocked() && !SpacecraftPawn->HasModifications();
		}

		// Processing
		else if (Desc->IsA<UNovaProcessingModuleDescription>())
		{
			return true;
		}
	}

	return false;
}

FText SNovaMainMenuOperations::GetModuleHelpText(int32 CompartmentIndex, int32 ModuleIndex) const
{
	if (IsValidModule(CompartmentIndex, ModuleIndex))
	{
		const UNovaModuleDescription* Desc = GetModule(CompartmentIndex, ModuleIndex).Description;

		// Cargo
		if (Desc->IsA<UNovaCargoModuleDescription>())
		{
			if (SpacecraftPawn && SpacecraftPawn->IsDocked())
			{
				return LOCTEXT("TradeSlotHelp", "Trade with this cargo slot");
			}
			else
			{
				return LOCTEXT("InspectSlotHelp", "Inspect this cargo slot");
			}
		}

		// Propellant
		else if (Desc->IsA<UNovaPropellantModuleDescription>())
		{
			return LOCTEXT("PropellantHelp", "Trade propellant");
		}

		// Processing
		else if (Desc->IsA<UNovaProcessingModuleDescription>())
		{
			return LOCTEXT("ProcessingHelp", "Control resource processing");
		}
	}

	return FText();
}

const FSlateBrush* SNovaMainMenuOperations::GetModuleImage(int32 CompartmentIndex, int32 ModuleIndex) const
{
	if (IsValidModule(CompartmentIndex, ModuleIndex))
	{
		const UNovaModuleDescription* Desc = GetModule(CompartmentIndex, ModuleIndex).Description;

		// Cargo
		if (Desc->IsA<UNovaCargoModuleDescription>())
		{
			const FNovaSpacecraftCargo& Cargo = ProcessingSystem->GetCargo(CompartmentIndex, ModuleIndex);
			if (IsValid(Cargo.Resource))
			{
				return &Cargo.Resource->AssetRender;
			}
		}

		// Propellant
		else if (Desc->IsA<UNovaPropellantModuleDescription>())
		{
			return &UNovaResource::GetPropellant()->AssetRender;
		}

		// Processing
		else if (Desc->IsA<UNovaProcessingModuleDescription>())
		{
			return &Desc->AssetRender;
		}
	}

	return &UNovaResource::GetEmpty()->AssetRender;
}

FText SNovaMainMenuOperations::GetModuleText(int32 CompartmentIndex, int32 ModuleIndex) const
{
	if (IsValidModule(CompartmentIndex, ModuleIndex))
	{
		const UNovaModuleDescription* Desc = GetModule(CompartmentIndex, ModuleIndex).Description;

		// Cargo
		if (Desc->IsA<UNovaCargoModuleDescription>())
		{
			const FNovaSpacecraftCargo& Cargo = ProcessingSystem->GetCargo(CompartmentIndex, ModuleIndex);
			if (IsValid(Cargo.Resource))
			{
				return Cargo.Resource->Name;
			}
			else
			{
				return LOCTEXT("EmptyCargo", "Empty");
			}
		}

		// Propellant
		else if (Desc->IsA<UNovaPropellantModuleDescription>())
		{
			return LOCTEXT("PropellantModule", "Propellant");
		}

		// Processing
		else if (Desc->IsA<UNovaProcessingModuleDescription>())
		{
			return Desc->Name;
		}
	}

	return FText();
}

FText SNovaMainMenuOperations::GetModuleDetails(int32 CompartmentIndex, int32 ModuleIndex) const
{
	if (IsValidModule(CompartmentIndex, ModuleIndex))
	{
		const UNovaModuleDescription* Desc = GetModule(CompartmentIndex, ModuleIndex).Description;

		// Cargo
		if (Desc->IsA<UNovaCargoModuleDescription>())
		{
			const FNovaCompartment&     Compartment = Spacecraft->Compartments[CompartmentIndex];
			const FNovaSpacecraftCargo& Cargo       = ProcessingSystem->GetCargo(CompartmentIndex, ModuleIndex);

			int32 Amount   = Cargo.Amount;
			int32 Capacity = Compartment.GetCargoCapacity(ModuleIndex);

			return FText::FormatNamed(LOCTEXT("CargoAmountFormat", "<img src=\"/Text/Cargo\"/> {amount} T / {capacity} T"), TEXT("amount"),
				FText::AsNumber(Amount), TEXT("capacity"), FText::AsNumber(Capacity));
		}

		// Propellant
		else if (Desc->IsA<UNovaPropellantModuleDescription>())
		{
			FNumberFormattingOptions Options;
			Options.MaximumFractionalDigits = 0;

			float RemainingDeltaV = Spacecraft->GetPropulsionMetrics().GetRemainingDeltaV(
				Spacecraft->GetCurrentCargoMass(), PropellantSystem->GetCurrentPropellantMass());

			return FText::FormatNamed(LOCTEXT("PropellantPercentFormat", "<img src=\"/Text/Propellant\"/> {percent}%"), TEXT("percent"),
				FText::AsNumber(100 * PropellantSystem->GetCurrentPropellantMass() / PropellantSystem->GetPropellantCapacity(), &Options));
		}

		// Processing
		else if (Desc->IsA<UNovaProcessingModuleDescription>())
		{
			// TODO: what goes there?

			/*switch (ProcessingSystem->GetModuleStatus(CompartmentIndex, ModuleIndex))
			{
			    case ENovaSpacecraftProcessingSystemStatus::Stopped:
			        return LOCTEXT("ProcessingStopped", "Stopped");
			        break;
			    case ENovaSpacecraftProcessingSystemStatus::Processing:
			        return LOCTEXT("ProcessingProcessing", "Processing");
			        break;
			    case ENovaSpacecraftProcessingSystemStatus::Blocked:
			        return LOCTEXT("ProcessingBlocked", "Blocked");
			        break;
			    case ENovaSpacecraftProcessingSystemStatus::Docked:
			        return LOCTEXT("ProcessingDocked", "Docked");
			        break;
			}*/
		}
	}

	return FText();
}

/*----------------------------------------------------
    Callbacks
----------------------------------------------------*/

void SNovaMainMenuOperations::OnRefillPropellant()
{
	TradingModalPanel->Trade(PC, GameState->GetCurrentArea(), UNovaResource::GetPropellant(), INDEX_NONE, INDEX_NONE);
}

void SNovaMainMenuOperations::OnBatchBuy()
{
	CurrentCompartmentIndex = INDEX_NONE;
	CurrentModuleIndexx     = INDEX_NONE;

	auto TradeResource = [this]()
	{
		TradingModalPanel->Trade(PC, GameState->GetCurrentArea(), ResourceListView->GetSelectedItem(), INDEX_NONE, INDEX_NONE);
	};

	// Fill the resource list
	ResourceList.Empty();
	for (const UNovaResource* Resource : Spacecraft->GetOwnedResources())
	{
		if (GameState->IsResourceSold(Resource))
		{
			ResourceList.Add(Resource);
		}
	}
	ResourceListView->Refresh(0);

	// Proceed with the modal panel
	GenericModalPanel->Show(
		FText::FormatNamed(LOCTEXT("SelectBulkBuyFormat", "Bulk buy from {station}"), TEXT("station"), GameState->GetCurrentArea()->Name),
		ResourceList.Num() == 0 ? LOCTEXT("NoBulkBuyResource", "No resources are available for sale at this station") : FText(),
		FSimpleDelegate::CreateLambda(TradeResource), FSimpleDelegate(), FSimpleDelegate(), ResourceListView);
}

void SNovaMainMenuOperations::OnBatchSell()
{
	CurrentCompartmentIndex = INDEX_NONE;
	CurrentModuleIndexx     = INDEX_NONE;

	auto TradeResource = [this]()
	{
		TradingModalPanel->Trade(PC, GameState->GetCurrentArea(), ResourceListView->GetSelectedItem(), INDEX_NONE, INDEX_NONE);
	};

	// Fill the resource list
	ResourceList.Empty();
	for (const UNovaResource* Resource : Spacecraft->GetOwnedResources())
	{
		if (!GameState->IsResourceSold(Resource))
		{
			ResourceList.Add(Resource);
		}
	}
	ResourceListView->Refresh(0);

	// Proceed with the modal panel
	GenericModalPanel->Show(
		FText::FormatNamed(LOCTEXT("SelectBulkSellFormat", "Bulk sell to {station}"), TEXT("station"), GameState->GetCurrentArea()->Name),
		ResourceList.Num() == 0 ? LOCTEXT("NoBulkSellResource", "No resources in cargo can be sold at this station") : FText(),
		FSimpleDelegate::CreateLambda(TradeResource), FSimpleDelegate(), FSimpleDelegate(), ResourceListView);
}

void SNovaMainMenuOperations::OnInteractWithModule(int32 CompartmentIndex, int32 ModuleIndex)
{
	NCHECK(GameState);
	NCHECK(GameState->GetCurrentArea());

	const UNovaModuleDescription* Desc = GetModule(CompartmentIndex, ModuleIndex).Description;

	// Cargo
	if (Desc->IsA<UNovaCargoModuleDescription>())
	{
		const FNovaSpacecraftCargo& Cargo = ProcessingSystem->GetCargo(CompartmentIndex, ModuleIndex);

		// Docked mode : trade with resource
		if (SpacecraftPawn->IsDocked())
		{
			// Valid resource in hold - allow trading it directly
			if (IsValid(Cargo.Resource))
			{
				TradingModalPanel->Trade(PC, GameState->GetCurrentArea(), Cargo.Resource, CompartmentIndex, ModuleIndex);
			}

			// Cargo hold is empty, pick a resource to buy first
			else
			{
				CurrentCompartmentIndex = CompartmentIndex;
				CurrentModuleIndexx     = ModuleIndex;

				auto BuyResource = [this, CompartmentIndex, ModuleIndex]()
				{
					TradingModalPanel->Trade(
						PC, GameState->GetCurrentArea(), ResourceListView->GetSelectedItem(), CompartmentIndex, ModuleIndex);
				};

				//	Fill the resource list
				ResourceList = GameState->GetResourcesSold();
				ResourceListView->Refresh(0);

				// Proceed with the modal panel
				GenericModalPanel->Show(FText::FormatNamed(LOCTEXT("SelectResourceFormat", "For sale at {station}"), TEXT("station"),
											GameState->GetCurrentArea()->Name),
					ResourceList.Num() == 0 ? LOCTEXT("NoResource", "No resources are available for sale at this station") : FText(),
					FSimpleDelegate::CreateLambda(BuyResource), FSimpleDelegate(), FSimpleDelegate(), ResourceListView);
			}
		}

		// Show the resource
		else
		{
			NCHECK(IsValid(Cargo.Resource));
			TradingModalPanel->Inspect(PC, GameState->GetCurrentArea(), Cargo.Resource, CompartmentIndex, ModuleIndex);
		}
	}

	// Propellant
	else if (Desc->IsA<UNovaPropellantModuleDescription>())
	{
		OnRefillPropellant();
	}

	// Processing
	else if (Desc->IsA<UNovaProcessingModuleDescription>())
	{
		// TODO: inspection, resource flow window?
	}
}

void SNovaMainMenuOperations::OnBuyResource()
{
	GenericModalPanel->Hide();

	TradingModalPanel->Trade(
		PC, GameState->GetCurrentArea(), ResourceListView->GetSelectedItem(), CurrentCompartmentIndex, CurrentModuleIndexx);
}

#undef LOCTEXT_NAMESPACE
