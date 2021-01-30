﻿// Nova project - Gwennaël Arbona

#include "NovaMainMenuFlight.h"

#include "NovaMainMenu.h"

#include "Nova/Game/NovaGameMode.h"

#include "Nova/Player/NovaMenuManager.h"
#include "Nova/Player/NovaPlayerController.h"

#include "Nova/Spacecraft/NovaSpacecraftPawn.h"
#include "Nova/Spacecraft/NovaSpacecraftMovementComponent.h"

#include "Nova/UI/Component/NovaLargeButton.h"

#include "Nova/Nova.h"


#define LOCTEXT_NAMESPACE "SNovaMainMenuFlight"


/*----------------------------------------------------
	Constructor
----------------------------------------------------*/

void SNovaMainMenuFlight::Construct(const FArguments& InArgs)
{
	// Data
	const FNovaMainTheme& Theme = FNovaStyleSet::GetMainTheme();
	MenuManager = InArgs._MenuManager;

	// Parent constructor
	SNovaNavigationPanel::Construct(SNovaNavigationPanel::FArguments()
		.Menu(InArgs._Menu)
	);

	// Structure
	ChildSlot
	[
		SNew(SHorizontalBox)

		// Main box
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Top)
		.AutoWidth()
		[
			SNew(SVerticalBox)
			
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SNovaButton)
				.Text(LOCTEXT("TestJoin", "Join random session"))
				.HelpText(LOCTEXT("HelpTestJoin", "Join random session"))
				.OnClicked(FSimpleDelegate::CreateLambda([=]()
				{
					MenuManager->GetPC()->TestJoin();
				}))
			]
			
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SNovaButton)
				.Text(LOCTEXT("LeaveStation", "Leave station"))
				.HelpText(LOCTEXT("HelpLeaveStation", "Leave station"))
				.OnClicked(FSimpleDelegate::CreateLambda([=]()
				{
					MenuManager->GetWorld()->GetAuthGameMode<ANovaGameMode>()->LeaveStation();
				}))
				.Enabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateLambda([=]()
				{
					return MenuManager->GetPC() && MenuManager->GetPC()->GetLocalRole() == ROLE_Authority;
				})))
			]
			
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNovaAssignNew(UndockButton, SNovaButton)
				.Text(LOCTEXT("Undock", "Undock"))
				.HelpText(LOCTEXT("UndockHelp", "Undock from the station"))
				.OnClicked(this, &SNovaMainMenuFlight::OnUndock)
				.Enabled(this, &SNovaMainMenuFlight::IsUndockEnabled)
			]
			
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNovaAssignNew(DockButton, SNovaButton)
				.Text(LOCTEXT("Dock", "Dock"))
				.HelpText(LOCTEXT("DockHelp", "Dock at the station"))
				.OnClicked(this, &SNovaMainMenuFlight::OnDock)
				.Enabled(this, &SNovaMainMenuFlight::IsDockEnabled)
			]
		]

		+ SHorizontalBox::Slot()
	];
}


/*----------------------------------------------------
	Interaction
----------------------------------------------------*/

void SNovaMainMenuFlight::Show()
{
	SNovaTabPanel::Show();

	GetSpacecraftPawn()->SetHighlightCompartment(INDEX_NONE);
}

void SNovaMainMenuFlight::Hide()
{
	SNovaTabPanel::Hide();
}

void SNovaMainMenuFlight::HorizontalAnalogInput(float Value)
{
	if (GetSpacecraftPawn())
	{
		GetSpacecraftPawn()->PanInput(Value);
	}
}

void SNovaMainMenuFlight::VerticalAnalogInput(float Value)
{
	if (GetSpacecraftPawn())
	{
		GetSpacecraftPawn()->TiltInput(Value);
	}
}

TSharedPtr<SNovaButton> SNovaMainMenuFlight::GetDefaultFocusButton() const
{
	if (IsUndockEnabled())
	{
		return UndockButton;
	}
	else
	{
		return DockButton;
	}
}


/*----------------------------------------------------
	Internals
----------------------------------------------------*/

ANovaSpacecraftPawn* SNovaMainMenuFlight::GetSpacecraftPawn() const
{
	return MenuManager->GetPC() ? MenuManager->GetPC()->GetSpacecraftPawn() : nullptr;
}

UNovaSpacecraftMovementComponent* SNovaMainMenuFlight::GetSpacecraftMovement() const
{
	ANovaSpacecraftPawn* SpacecraftPawn = GetSpacecraftPawn();
	if (SpacecraftPawn)
	{
		return Cast<UNovaSpacecraftMovementComponent>(
			SpacecraftPawn->GetComponentByClass(UNovaSpacecraftMovementComponent::StaticClass()));
	}
	else
	{
		return nullptr;
	}
}


/*----------------------------------------------------
	Content callbacks
----------------------------------------------------*/

bool SNovaMainMenuFlight::IsUndockEnabled() const
{
	return GetSpacecraftMovement() && GetSpacecraftMovement()->GetState() == ENovaMovementState::Docked;
}

bool SNovaMainMenuFlight::IsDockEnabled() const
{
	return GetSpacecraftMovement() && GetSpacecraftMovement()->GetState() == ENovaMovementState::Idle;
}


/*----------------------------------------------------
	Callbacks
----------------------------------------------------*/

void SNovaMainMenuFlight::OnUndock()
{
	NCHECK(IsUndockEnabled());

	MenuManager->GetPC()->Undock();
}

void SNovaMainMenuFlight::OnDock()
{
	NCHECK(IsDockEnabled());

	MenuManager->GetPC()->Dock();
}


#undef LOCTEXT_NAMESPACE
