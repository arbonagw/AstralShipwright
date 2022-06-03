// Astral Shipwright - Gwennaël Arbona

#include "NovaPlayerController.h"
#include "NovaPlayerState.h"
#include "NovaGameViewportClient.h"

#include "Actor/NovaActorTools.h"
#include "Actor/NovaPlayerStart.h"
#include "Actor/NovaTurntablePawn.h"

#include "Game/NovaAsteroid.h"
#include "Game/NovaGameMode.h"
#include "Game/NovaGameState.h"
#include "Game/NovaPlanetarium.h"
#include "Game/Settings/NovaGameUserSettings.h"
#include "Game/Settings/NovaWorldSettings.h"
#include "Game/Station/NovaStationDock.h"

#include "Spacecraft/NovaSpacecraftPawn.h"
#include "Spacecraft/NovaSpacecraftDriveComponent.h"
#include "Spacecraft/NovaSpacecraftThrusterComponent.h"
#include "Spacecraft/NovaSpacecraftMovementComponent.h"

#include "System/NovaAssetManager.h"
#include "System/NovaContractManager.h"
#include "System/NovaGameInstance.h"
#include "System/NovaMenuManager.h"
#include "System/NovaSoundManager.h"
#include "System/NovaSaveManager.h"
#include "System/NovaSessionsManager.h"

#include "UI/Menu/NovaMainMenu.h"
#include "UI/Menu/NovaOverlay.h"
#include "UI/Widget/NovaMenu.h"
#include "Nova.h"

#include "Framework/Application/SlateApplication.h"
#include "Misc/CommandLine.h"

#include "GameFramework/PlayerState.h"
#include "Components/PostProcessComponent.h"
#include "Components/BillboardComponent.h"
#include "Components/SkyLightComponent.h"

#include "Engine/LocalPlayer.h"
#include "Engine/SpotLight.h"
#include "Engine/SkyLight.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"

#include <inttypes.h>

#define LOCTEXT_NAMESPACE "ANovaPlayerController"

/*----------------------------------------------------
    Constructors
----------------------------------------------------*/

ANovaPlayerViewpoint::ANovaPlayerViewpoint() : Super()
{
	RootComponent = CreateDefaultSubobject<UCameraComponent>("Root");

	// Defaults
	CameraAnimationDuration = 5.0f;
	CameraPanAmount         = 5.0f;
	CameraTiltAmount        = 0.0f;
	CameraTravelingAmount   = 250.0f;
}

ANovaPlayerController::ANovaPlayerController()
	: Super()
	, LastNetworkError(ENovaNetworkError::Success)
	, CurrentCameraState(ENovaPlayerCameraState::Default)
	, CurrentTimeInCameraState(0)
	, PhotoModeAction(NAME_None)
	, SharedTransitionActive(false)
{
	// Create the post-processing manager
	PostProcessComponent = CreateDefaultSubobject<UNovaPostProcessComponent>(TEXT("PostProcessComponent"));

	// Default settings
	TSharedPtr<FNovaPostProcessSetting> DefaultSettings = MakeShared<FNovaPostProcessSetting>();
	PostProcessComponent->RegisterPreset(ENovaPostProcessPreset::Neutral, DefaultSettings);

	// Initialize post-processing
	PostProcessComponent->Initialize(

		// Preset control
		FNovaPostProcessControl::CreateLambda(
			[=]()
			{
				ENovaPostProcessPreset TargetPreset = ENovaPostProcessPreset::Neutral;

				return static_cast<int32>(TargetPreset);
			}),

		// Preset tick
		FNovaPostProcessUpdate::CreateLambda(
			[=](UPostProcessComponent* Volume, UMaterialInstanceDynamic* Material, const TSharedPtr<FNovaPostProcessSettingBase>& Current,
				const TSharedPtr<FNovaPostProcessSettingBase>& Target, float Alpha)
			{
				UNovaGameUserSettings*         GameUserSettings = Cast<UNovaGameUserSettings>(GEngine->GetGameUserSettings());
				const FNovaPostProcessSetting* MyCurrent        = static_cast<const FNovaPostProcessSetting*>(Current.Get());
				const FNovaPostProcessSetting* MyTarget         = static_cast<const FNovaPostProcessSetting*>(Target.Get());

				// Config-driven settings
				Volume->Settings.bOverride_BloomMethod                     = true;
				Volume->Settings.bOverride_DynamicGlobalIlluminationMethod = true;
				Volume->Settings.bOverride_ReflectionMethod                = true;
				Volume->Settings.BloomMethod                               = GameUserSettings->EnableCinematicBloom ? BM_FFT : BM_SOG;
				Volume->Settings.DynamicGlobalIlluminationMethod =
					GameUserSettings->EnableLumen ? EDynamicGlobalIlluminationMethod::Lumen : EDynamicGlobalIlluminationMethod::ScreenSpace;
				Volume->Settings.ReflectionMethod =
					GameUserSettings->EnableLumen ? EReflectionMethod::Lumen : EReflectionMethod::ScreenSpace;

				// Custom settings
				ANovaSpacecraftPawn* SpacecraftPawn = GetSpacecraftPawn();
				Material->SetScalarParameterValue("HighlightAlpha", IsValid(SpacecraftPawn) ? SpacecraftPawn->GetHighlightAlpha() : 0);
				Material->SetScalarParameterValue("OutlineAlpha", IsValid(SpacecraftPawn) ? SpacecraftPawn->GetOutlineAlpha() : 0);
				Material->SetVectorParameterValue("HighlightColor", UNovaMenuManager::Get()->GetHighlightColor());
				// Material->SetScalarParameterValue("ChromaIntensity", FMath::Lerp(Current.ChromaIntensity, Target.ChromaIntensity,
		        // Alpha));

				// Built-in settings (overrides)
				Volume->Settings.bOverride_FilmGrainIntensity = true;
				Volume->Settings.bOverride_SceneColorTint     = true;

				// Built in settings (values)
				Volume->Settings.FilmGrainIntensity = FMath::Lerp(MyCurrent->GrainIntensity, MyTarget->GrainIntensity, Alpha);
				Volume->Settings.SceneColorTint     = FMath::Lerp(MyCurrent->SceneColorTint, MyTarget->SceneColorTint, Alpha);
			}));
}

/*----------------------------------------------------
    Loading & saving
----------------------------------------------------*/

struct FNovaPlayerSave
{
	TSharedPtr<struct FNovaSpacecraft> Spacecraft;
	FNovaCredits                       Credits;
};

TSharedPtr<FNovaPlayerSave> ANovaPlayerController::Save() const
{
	TSharedPtr<FNovaPlayerSave> SaveData = MakeShared<FNovaPlayerSave>();

	// Save the spacecraft
	ANovaSpacecraftPawn* SpacecraftPawn = GetSpacecraftPawn();
	NCHECK(SpacecraftPawn);
	const FNovaSpacecraft* Spacecraft = GetSpacecraft();
	if (Spacecraft)
	{
		SaveData->Spacecraft = Spacecraft->GetSharedCopy();
	}

	// Save credits
	SaveData->Credits = Credits;

	return SaveData;
}

void ANovaPlayerController::Load(TSharedPtr<FNovaPlayerSave> SaveData)
{
	NCHECK(GetLocalRole() == ROLE_Authority);
	NLOG("ANovaPlayerController::Load");

	// Store the save data so that the spacecraft pawn can fetch it later when it spawns
	UpdateSpacecraft(*SaveData->Spacecraft.Get());

	Credits = SaveData->Credits;
}

void ANovaPlayerController::SerializeJson(
	TSharedPtr<FNovaPlayerSave>& SaveData, TSharedPtr<FJsonObject>& JsonData, ENovaSerialize Direction)
{
	// Writing to save
	if (Direction == ENovaSerialize::DataToJson)
	{
		JsonData = MakeShared<FJsonObject>();

		// Spacecraft
		TSharedPtr<FJsonObject> SpacecraftJsonData;
		if (SaveData)
		{
			FNovaSpacecraft::SerializeJson(SaveData->Spacecraft, SpacecraftJsonData, ENovaSerialize::DataToJson);
		}
		JsonData->SetObjectField("Spacecraft", SpacecraftJsonData);

		// Credits
		JsonData->SetNumberField("Credits", SaveData->Credits.GetValue());
	}

	// Reading from save
	else
	{
		SaveData = MakeShared<FNovaPlayerSave>();

		// Spacecraft
		TSharedPtr<FJsonObject> SpacecraftJsonData =
			JsonData->HasTypedField<EJson::Object>("Spacecraft") ? JsonData->GetObjectField("Spacecraft") : MakeShared<FJsonObject>();
		FNovaSpacecraft::SerializeJson(SaveData->Spacecraft, SpacecraftJsonData, ENovaSerialize::JsonToData);

		// Credits
		int64 Credits = 0;
		if (JsonData->TryGetNumberField("Credits", Credits))
		{
			SaveData->Credits = Credits;
		}
		else
		{
#if WITH_EDITOR
			SaveData->Credits = 20000;
#else
			SaveData->Credits = 2000;
#endif    // WITH_EDITOR
		}
	}
}

/*----------------------------------------------------
    Inherited
----------------------------------------------------*/

void ANovaPlayerController::BeginPlay()
{
	NLOG("ANovaPlayerController::BeginPlay");

	Super::BeginPlay();

	// Process client-side player initialization
	if (IsLocalPlayerController())
	{
		// Load save data, process local game startup
		if (!IsOnMainMenu())
		{
			ClientLoadPlayer();
		}

		// Setup systems
		UNovaMenuManager::Get()->BeginPlay<SNovaMainMenu, SNovaOverlay>(this);
		UNovaSoundManager::Get()->BeginPlay(this,    //
			FNovaMusicCallback::CreateWeakLambda(this,
				[this]()
				{
					if (IsOnMainMenu())
					{
						return "Menu";
					}
					else
					{
						return "Ambient";
					}
				}));

		// Setup thruster sounds
		UNovaSoundManager::Get()->AddEnvironmentSound("Thrusters",    //
			FNovaSoundInstanceCallback::CreateWeakLambda(this,
				[=]()
				{
					ANovaSpacecraftPawn* SpacecraftPawn = GetSpacecraftPawn();
					if (IsValid(SpacecraftPawn))
					{
						TArray<const UNovaSpacecraftThrusterComponent*> Thrusters;
						SpacecraftPawn->GetComponents(Thrusters);
						for (const UNovaSpacecraftThrusterComponent* Thruster : Thrusters)
						{
							if (Thruster->IsThrusterActive())
							{
								return true;
							}
						}
					}

					return false;
				}));

		// Setup drive sounds
		UNovaSoundManager::Get()->AddEnvironmentSound("Drive",    //
			FNovaSoundInstanceCallback::CreateWeakLambda(this,
				[=]()
				{
					ANovaSpacecraftPawn* SpacecraftPawn = GetSpacecraftPawn();
					if (IsValid(SpacecraftPawn))
					{
						TArray<const UNovaSpacecraftDriveComponent*> Drives;
						SpacecraftPawn->GetComponents(Drives);
						for (const UNovaSpacecraftDriveComponent* Drive : Drives)
						{
							if (Drive->IsDriveActive())
							{
								return true;
							}
						}
					}

					return false;
				}));
	}

	// Initialize persistent objects
	UNovaSessionsManager* SessionsManager = UNovaSessionsManager::Get();
	SessionsManager->SetAcceptedInvitationCallback(FOnFriendInviteAccepted::CreateUObject(this, &ANovaPlayerController::AcceptInvitation));

#if WITH_EDITOR

	// Start a host session if requested through command line
	if (FParse::Param(FCommandLine::Get(), TEXT("host")))
	{
		FCommandLine::Set(TEXT(""));

		SessionsManager->StartSession(ENovaConstants::DefaultLevel, ENovaConstants::MaxPlayerCount, true);
	}

#endif    // WITH_EDITOR
}

void ANovaPlayerController::PawnLeavingGame()
{
	NLOG("ANovaPlayerController::PawnLeavingGame");

	ANovaSpacecraftPawn* SpacecraftPawn = GetSpacecraftPawn();
	ANovaGameState*      GameState      = GetWorld()->GetGameState<ANovaGameState>();

	if (IsValid(SpacecraftPawn) && IsValid(GameState))
	{
		FGuid PlayerSpacecraftIdentifier = SpacecraftPawn->GetSpacecraftIdentifier();
		GameState->RemoveSpacecraft(PlayerSpacecraftIdentifier);
	}

	SetPawn(nullptr);
}

void ANovaPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	if (IsLocalPlayerController())
	{
		UNovaGameUserSettings* GameUserSettings = Cast<UNovaGameUserSettings>(GEngine->GetGameUserSettings());

		// Process FOV
		NCHECK(PlayerCameraManager);
		float FOV = PlayerCameraManager->GetFOVAngle();
		if (FOV != GameUserSettings->FOV)
		{
			NLOG("ANovaPlayerController::PlayerTick : new FOV %d", static_cast<int>(GameUserSettings->FOV));
			PlayerCameraManager->SetFOV(GameUserSettings->FOV);
		}

		// Show network error
		UNovaSessionsManager* SessionsManager = UNovaSessionsManager::Get();
		NCHECK(SessionsManager);
		if (SessionsManager->GetNetworkError() != LastNetworkError)
		{
			LastNetworkError = SessionsManager->GetNetworkError();
			if (LastNetworkError != ENovaNetworkError::Success)
			{
				Notify(LOCTEXT("NetworkError", "Network error"), SessionsManager->GetNetworkErrorString(), ENovaNotificationType::Error);
			}
		}

		// Update contracts
		UNovaContractManager::Get()->OnEvent(FNovaContractEvent(ENovaContratEventType::Tick));

		// Disable overlays in photo mode
		ANovaSpacecraftPawn* SpacecraftPawn = GetSpacecraftPawn();
		if (IsValid(SpacecraftPawn) && IsInPhotoMode())
		{
			SpacecraftPawn->SetOutlinedCompartment(INDEX_NONE);
			SpacecraftPawn->SetHighlightedCompartment(INDEX_NONE);
		}

		CurrentTimeInCameraState += DeltaTime;
	}
}

void ANovaPlayerController::GetPlayerViewPoint(FVector& Location, FRotator& Rotation) const
{
	Super::GetPlayerViewPoint(Location, Rotation);

	// During cutscenes, use the closest camera viewpoint and focus the player ship
	if (IsReady() && (CurrentCameraState == ENovaPlayerCameraState::CinematicSpacecraft ||
						 CurrentCameraState == ENovaPlayerCameraState::CinematicEnvironment ||
						 CurrentCameraState == ENovaPlayerCameraState::CinematicBrake))
	{
		// Get points of interest nearby
		TArray<AActor*> Viewpoints;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), ANovaPlayerViewpoint::StaticClass(), Viewpoints);
		TArray<AActor*> Asteroids;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), ANovaAsteroid::StaticClass(), Asteroids);
		TArray<AActor*> StationDocks;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), ANovaStationDock::StaticClass(), StationDocks);
		TArray<AActor*> Planetariums;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), ANovaPlanetarium::StaticClass(), Planetariums);

		// Get player start
		ANovaSpacecraftPawn*    SpacecraftPawn = GetSpacecraftPawn();
		const ANovaPlayerStart* PlayerStart    = SpacecraftPawn->GetSpacecraftMovement()->GetPlayerStart();

		// Get the first viewpoint actor and extract its transform
		const ANovaPlayerViewpoint* PlayerViewpoint = nullptr;
		if (Viewpoints.Num())
		{
			UNovaActorTools::SortActorsByClosestDistance(Viewpoints, GetPawn()->GetActorLocation());
			PlayerViewpoint = Cast<ANovaPlayerViewpoint>(Viewpoints[0]);
		}
		float AnimationDuration =
			PlayerViewpoint ? PlayerViewpoint->CameraAnimationDuration : GetDefault<ANovaPlayerViewpoint>()->CameraAnimationDuration;

		FVector PlayerLocation = GetPawn()->GetActorLocation();
		if (PlayerViewpoint)
		{
			Location = PlayerViewpoint->GetActorLocation();

			// Spacecraft focus
			if (CurrentCameraState == ENovaPlayerCameraState::CinematicSpacecraft)
			{
				Rotation = (PlayerLocation - Location).Rotation();
			}

			// Environment panning shot
			else if (CurrentCameraState == ENovaPlayerCameraState::CinematicEnvironment)
			{
				float AnimationAlpha = FMath::Clamp(CurrentTimeInCameraState / AnimationDuration, 0.0f, 1.0f);
				AnimationAlpha       = FMath::InterpEaseOut(-0.5f, 0.5f, AnimationAlpha, ENovaUIConstants::EaseStandard);

				Rotation = PlayerViewpoint->GetActorRotation();
				Rotation +=
					FRotator(0, AnimationAlpha * PlayerViewpoint->CameraPanAmount, AnimationAlpha * PlayerViewpoint->CameraTiltAmount);
				Location += Rotation.Vector() * AnimationAlpha * PlayerViewpoint->CameraTravelingAmount;
			}
		}

		// Braking spacecraft entry
		if (CurrentCameraState == ENovaPlayerCameraState::CinematicBrake &&
			((StationDocks.Num() > 0 && IsValid(PlayerStart)) || Asteroids.Num() > 0))
		{
			NCHECK(Planetariums.Num() > 0);

			// Define scene parameters
			double        ViewDistance = StationDocks.Num() > 0 ? 20000.0 : 50000.0;
			const FVector TargetLocation =
				StationDocks.Num() > 0 ? PlayerStart->GetWaitingPointLocation() : Asteroids[0]->GetActorLocation();
			const FVector BackdropLocation =
				StationDocks.Num() > 0 ? PlayerStart->GetActorLocation() : Cast<ANovaPlanetarium>(Planetariums[0])->GetPlanetLocation();

			// Define the appropriate viewpoint characteristics
			const FVector TargetSeparation   = SpacecraftPawn->GetActorLocation() - TargetLocation;
			const FVector TargetViewpoint    = (SpacecraftPawn->GetActorLocation() + TargetLocation) / 2.0;
			const FVector ViewpointDirection = (BackdropLocation - TargetViewpoint).GetSafeNormal();
			ViewDistance                     = FMath::Max(ViewDistance, 2 * TargetSeparation.Size());

			Location = TargetViewpoint - ViewpointDirection * ViewDistance;
			Rotation = ViewpointDirection.Rotation();
		}
	}
}

/*----------------------------------------------------
    Gameplay
----------------------------------------------------*/

void ANovaPlayerController::ProcessTransaction(FNovaCredits CreditsDelta)
{
	// Authority
	if (GetLocalRole() == ROLE_Authority)
	{
		NCHECK(CanAffordTransaction(CreditsDelta));

		Credits += CreditsDelta;
		Credits = FMath::Max(Credits, FNovaCredits(0));

		NLOG("ANovaPlayerController::ProcessTransaction : %" PRId64 " in account (%+" PRId64 ")", Credits.GetValue(),
			CreditsDelta.GetValue());
	}

	// Remote client
	else if (GetLocalRole() == ROLE_AutonomousProxy)
	{
		ServerProcessTransaction(CreditsDelta);
	}
}

void ANovaPlayerController::ServerProcessTransaction_Implementation(FNovaCredits CreditsDelta)
{
	NCHECK(GetLocalRole() == ROLE_Authority);

	ProcessTransaction(CreditsDelta);
}

bool ANovaPlayerController::CanAffordTransaction(FNovaCredits CreditsDelta) const
{
	return Credits + CreditsDelta >= 0;
}

void ANovaPlayerController::Dock()
{
	NLOG("ANovaPlayerController::Dock");

	FSimpleDelegate EndCutscene = FSimpleDelegate::CreateLambda(
		[=]()
		{
			UNovaMenuManager::Get()->OpenMenu(FNovaAsyncAction::CreateLambda(
				[=]()
				{
					SetCameraState(ENovaPlayerCameraState::Default);
					GetSpacecraftPawn()->ResetView();
					GetGameInstance<UNovaGameInstance>()->SaveGame(this);
				}));
		});

	FNovaAsyncAction StartCutscene = FNovaAsyncAction::CreateLambda(
		[=]()
		{
			SetCameraState(ENovaPlayerCameraState::CinematicSpacecraft);
			GetSpacecraftPawn()->Dock(EndCutscene);
		});

	UNovaMenuManager::Get()->CloseMenu(StartCutscene);
}

void ANovaPlayerController::Undock()
{
	NLOG("ANovaPlayerController::Undock");

	FSimpleDelegate EndCutscene = FSimpleDelegate::CreateLambda(
		[=]()
		{
			UNovaMenuManager::Get()->OpenMenu(FNovaAsyncAction::CreateLambda(
				[=]()
				{
					SetCameraState(ENovaPlayerCameraState::Default);
					GetSpacecraftPawn()->ResetView();
				}));
		});

	FNovaAsyncAction StartCutscene = FNovaAsyncAction::CreateLambda(
		[=]()
		{
			GetGameInstance<UNovaGameInstance>()->SaveGame(this);
			SetCameraState(ENovaPlayerCameraState::CinematicSpacecraft);
			GetSpacecraftPawn()->Undock(EndCutscene);
		});

	UNovaMenuManager::Get()->CloseMenu(StartCutscene);
}

void ANovaPlayerController::SharedTransition(
	ENovaPlayerCameraState NewCameraState, FNovaAsyncAction StartAction, FNovaAsyncCondition Condition, FNovaAsyncAction FinishAction)
{
	NCHECK(GetLocalRole() == ROLE_Authority);
	NLOG("ANovaPlayerController::ServerSharedTransition");

	for (ANovaPlayerController* OtherPlayer : TActorRange<ANovaPlayerController>(GetWorld()))
	{
		OtherPlayer->ClientStartSharedTransition(NewCameraState);
	}

	SharedTransitionStartAction  = StartAction;
	SharedTransitionFinishAction = FinishAction;
	SharedTransitionCondition    = Condition;
}

void ANovaPlayerController::ClientStartSharedTransition_Implementation(ENovaPlayerCameraState NewCameraState)
{
	NLOG("ANovaPlayerController::ClientStartSharedTransition_Implementation");

	// Shared transitions work like this :
	// - Server signals all players to fade to black through this very method
	// - Once faded, Action is called and all players call ServerSharedTransitionReady() to signal they're dark
	// - Server fires SharedTransitionStartAction when all clients have called ServerSharedTransitionReady()
	// - Server fires SharedTransitionFinishAction once SharedTransitionCondition returns true on the server
	// - Server then calls ClientStopSharedTransition() on all players so that they know to resume
	// - All players then fade back to the game

	SharedTransitionActive = true;

	// Action : mark as in shared transition locally and remotely
	FNovaAsyncAction Action = FNovaAsyncAction::CreateLambda(
		[=]()
		{
			SetCameraState(NewCameraState);
			ServerSharedTransitionReady();
			NLOG("ANovaPlayerController::ClientStartSharedTransition_Implementation : done, waiting for server");
		});

	// Condition : on server, when all clients have reported as ready
	// On client, when the server has signaled to stop
	FNovaAsyncCondition Condition = FNovaAsyncCondition::CreateLambda(
		[=]()
		{
			if (GetLocalRole() == ROLE_Authority)
			{
				// Check if all players are in transition
				bool AllPlayersInTransition = true;
				for (ANovaPlayerController* OtherPlayer : TActorRange<ANovaPlayerController>(GetWorld()))
				{
					if (!OtherPlayer->SharedTransitionActive)
					{
						AllPlayersInTransition = false;
						break;
					}
				}

				// Once all players are in the transition, fire the start event, wait for the condition, fire the end event, and stop
				if (AllPlayersInTransition)
				{
					SharedTransitionStartAction.ExecuteIfBound();
					SharedTransitionStartAction.Unbind();

					if (!SharedTransitionCondition.IsBound() || SharedTransitionCondition.Execute())
					{
						SharedTransitionFinishAction.ExecuteIfBound();
						SharedTransitionFinishAction.Unbind();
						SharedTransitionCondition.Unbind();

						for (ANovaPlayerController* OtherPlayer : TActorRange<ANovaPlayerController>(GetWorld()))
						{
							OtherPlayer->ClientStopSharedTransition();
						}

						return true;
					}
				}

				return false;
			}
			else
			{
				return !SharedTransitionActive;
			}
		});

	// Run the process
	switch (NewCameraState)
	{
		// UI enabled states
		case ENovaPlayerCameraState::Default:
			UNovaMenuManager::Get()->OpenMenu(Action, Condition);
			break;

		// UI disabled states
		case ENovaPlayerCameraState::CinematicSpacecraft:
		case ENovaPlayerCameraState::CinematicEnvironment:
		case ENovaPlayerCameraState::CinematicBrake:
		case ENovaPlayerCameraState::FastForward:
			UNovaMenuManager::Get()->CloseMenu(Action, Condition);
			break;
	}
}

void ANovaPlayerController::ClientStopSharedTransition_Implementation()
{
	NLOG("ANovaPlayerController::ClientStopSharedTransition_Implementation");

	SharedTransitionActive = false;
}

void ANovaPlayerController::ServerSharedTransitionReady_Implementation()
{
	NCHECK(GetLocalRole() == ROLE_Authority);
	NLOG("ANovaPlayerController::ServerSharedTransitionReady_Implementation");

	SharedTransitionActive = true;
}

void ANovaPlayerController::SetCameraState(ENovaPlayerCameraState State)
{
	CurrentCameraState       = State;
	CurrentTimeInCameraState = 0;

	// Handle the fast-forward camera
	if (State == ENovaPlayerCameraState::FastForward)
	{
		UNovaMenuManager::Get()->GetOverlay<SNovaOverlay>()->StartFastForward();
	}
	else
	{
		UNovaMenuManager::Get()->GetOverlay<SNovaOverlay>()->StopFastForward();
	}
}

/*----------------------------------------------------
    Server-side save
----------------------------------------------------*/

void ANovaPlayerController::ClientLoadPlayer()
{
	NLOG("ANovaPlayerController::ClientLoadPlayer");

	// Check for save data
	UNovaGameInstance* GameInstance = GetGameInstance<UNovaGameInstance>();
	NCHECK(GameInstance);

#if WITH_EDITOR

	// Ensure valid save data exists even if the game was loaded directly on a map (PIE client)
	if (GetLocalRole() != ROLE_Authority && !GameInstance->HasSave())
	{
		GameInstance->LoadGame("PIE");
	}

#endif    // WITH_EDITOR

	NCHECK(GameInstance->HasSave());

	// Serialize the save data and spawn the player actors on the server
	TSharedPtr<FJsonObject>     JsonData;
	TSharedPtr<FNovaPlayerSave> PlayerSaveData = GameInstance->GetPlayerSave();
	SerializeJson(PlayerSaveData, JsonData, ENovaSerialize::DataToJson);
	ServerLoadPlayer(UNovaSaveManager::JsonToString(JsonData));
}

void ANovaPlayerController::ServerLoadPlayer_Implementation(const FString& SerializedSaveData)
{
	NCHECK(GetLocalRole() == ROLE_Authority);
	NLOG("ANovaPlayerController::ServerLoadPlayer_Implementation");

	// Deserialize save data
	TSharedPtr<FNovaPlayerSave> SaveData;
	TSharedPtr<FJsonObject>     JsonData = UNovaSaveManager::StringToJson(SerializedSaveData);
	SerializeJson(SaveData, JsonData, ENovaSerialize::JsonToData);

	// Load
	Load(SaveData);
}

void ANovaPlayerController::UpdateSpacecraft(const FNovaSpacecraft& Spacecraft)
{
	NLOG("ANovaPlayerController::UpdateSpacecraft ('%s')", *GetRoleString(this));

	// Update the player spacecraft
	if (GetLocalRole() == ROLE_Authority)
	{
		ANovaGameState* GameState = GetWorld()->GetGameState<ANovaGameState>();
		NCHECK(IsValid(GameState));

		GameState->UpdatePlayerSpacecraft(Spacecraft, true);

		ANovaSpacecraftPawn* SpacecraftPawn = GetSpacecraftPawn();
		NCHECK(SpacecraftPawn);

		SpacecraftPawn->SetSpacecraftIdentifier(Spacecraft.Identifier);
	}

	// Tell the server
	if (GetLocalRole() == ROLE_AutonomousProxy)
	{
		ServerUpdateSpacecraft(Spacecraft);
	}
}

void ANovaPlayerController::ServerUpdateSpacecraft_Implementation(const FNovaSpacecraft& Spacecraft)
{
	NCHECK(GetLocalRole() == ROLE_Authority);
	NLOG("ANovaPlayerController::ServerUpdateSpacecraft_Implementation");

	UpdateSpacecraft(Spacecraft);
}

void ANovaPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ANovaPlayerController, Credits);
}

/*----------------------------------------------------
    Game flow
----------------------------------------------------*/

void ANovaPlayerController::StartGame(FString SaveName, bool Online)
{
	NLOG("ANovaPlayerController::StartGame : loading from '%s', online = %d", *SaveName, Online);

	if (UNovaMenuManager::Get()->IsIdle())
	{
		UNovaSoundManager::Get()->Mute();

		UNovaMenuManager::Get()->RunAction(ENovaLoadingScreen::Launch,    //
			FNovaAsyncAction::CreateLambda(
				[=]()
				{
					GetGameInstance<UNovaGameInstance>()->StartGame(SaveName, Online);
				}));
	}
}

void ANovaPlayerController::SetGameOnline(bool Online)
{
	NLOG("ANovaPlayerController::SetGameOnline : online = %d", Online);

	if (UNovaMenuManager::Get()->IsIdle())
	{
		UNovaSoundManager::Get()->Mute();

		UNovaMenuManager::Get()->RunAction(ENovaLoadingScreen::Launch,    //
			FNovaAsyncAction::CreateLambda(
				[=]()
				{
					GetGameInstance<UNovaGameInstance>()->SetGameOnline(GetWorld()->GetName(), Online);
				}));
	}
}

void ANovaPlayerController::GoToMainMenu(bool SaveGame)
{
	if (UNovaMenuManager::Get()->IsIdle())
	{
		NLOG("ANovaPlayerController::GoToMainMenu %d", SaveGame);

		UNovaSoundManager::Get()->Mute();

		UNovaMenuManager::Get()->RunAction(ENovaLoadingScreen::Black,    //
			FNovaAsyncAction::CreateLambda(
				[=]()
				{
					if (SaveGame)
					{
						NLOG("ANovaPlayerController::GoToMainMenu : saving game");
						GetGameInstance<UNovaGameInstance>()->SaveGame(this, true);
					}

					GetGameInstance<UNovaGameInstance>()->GoToMainMenu();
				}));
	}
}

void ANovaPlayerController::ExitGame()
{
	if (UNovaMenuManager::Get()->IsIdle())
	{
		NLOG("ANovaPlayerController::ExitGame");

		UNovaSoundManager::Get()->Mute();

		UNovaMenuManager::Get()->RunAction(ENovaLoadingScreen::Black,    //
			FNovaAsyncAction::CreateLambda(
				[=]()
				{
					FGenericPlatformMisc::RequestExit(false);
				}));
	}
}

void ANovaPlayerController::InviteFriend(TSharedRef<FOnlineFriend> Friend)
{
	NLOG("ANovaPlayerController::InviteFriend");

	Notify(LOCTEXT("InviteFriend", "Invited friend"), FText::FromString(Friend->GetDisplayName()), ENovaNotificationType::Info);

	UNovaSessionsManager::Get()->InviteFriend(Friend->GetUserId());
}

void ANovaPlayerController::JoinFriend(TSharedRef<FOnlineFriend> Friend)
{
	NLOG("ANovaPlayerController::JoinFriend");

	UNovaMenuManager::Get()->RunAction(ENovaLoadingScreen::Launch,    //
		FNovaAsyncAction::CreateLambda(
			[=]()
			{
				Notify(
					LOCTEXT("JoiningFriend", "Joining friend"), FText::FromString(Friend->GetDisplayName()), ENovaNotificationType::Info);

				UNovaSessionsManager::Get()->JoinFriend(Friend->GetUserId());
			}));
}

void ANovaPlayerController::AcceptInvitation(const FOnlineSessionSearchResult& InviteResult)
{
	NLOG("ANovaPlayerController::AcceptInvitation");

	UNovaMenuManager::Get()->RunAction(ENovaLoadingScreen::Launch,    //
		FNovaAsyncAction::CreateLambda(
			[=]()
			{
				UNovaSessionsManager::Get()->JoinSearchResult(InviteResult);
			}));
}

bool ANovaPlayerController::IsReady() const
{
	if (IsOnMainMenu())
	{
		return true;
	}
	else
	{
		// Check spacecraft pawn
		ANovaSpacecraftPawn* SpacecraftPawn = GetSpacecraftPawn();
		bool IsSpacecraftReady = IsValid(SpacecraftPawn) && GetSpacecraft() && SpacecraftPawn->GetSpacecraftMovement()->IsInitialized();

		// Check game state
		ANovaGameState* GameState        = GetWorld()->GetGameState<ANovaGameState>();
		bool            IsGameStateReady = IsValid(GameState) && GameState->IsReady();

		return IsSpacecraftReady && IsGameStateReady;
	}
}

/*----------------------------------------------------
    Menus
----------------------------------------------------*/

bool ANovaPlayerController::IsOnMainMenu() const
{
	return Cast<ANovaWorldSettings>(GetWorld()->GetWorldSettings())->IsMainMenuMap();
}

bool ANovaPlayerController::IsMenuOnly() const
{
	return Cast<ANovaWorldSettings>(GetWorld()->GetWorldSettings())->IsMenuMap();
}

void ANovaPlayerController::Notify(const FText& Text, const FText& Subtext, ENovaNotificationType Type)
{
	UNovaMenuManager::Get()->GetOverlay<SNovaOverlay>()->Notify(Text, Subtext, Type);
}

void ANovaPlayerController::EnterPhotoMode(FName ActionName)
{
	NLOG("ANovaPlayerController::EnterPhotoMode");

	UNovaMenuManager::Get()->CloseMenu(FNovaAsyncAction::CreateLambda(
		[=]()
		{
			PhotoModeAction = ActionName;
			SetCameraState(ENovaPlayerCameraState::PhotoMode);
		}));
}

void ANovaPlayerController::ExitPhotoMode()
{
	NLOG("ANovaPlayerController::ExitPhotoMode");

	UNovaMenuManager::Get()->OpenMenu(FNovaAsyncAction::CreateLambda(
		[=]()
		{
			PhotoModeAction = NAME_None;
			SetCameraState(ENovaPlayerCameraState::Default);
		}));
}

/*----------------------------------------------------
    Input
----------------------------------------------------*/

void ANovaPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// Core bindings
	FInputActionBinding& AnyKeyBinding =
		InputComponent->BindAction("AnyKey", EInputEvent::IE_Pressed, this, &ANovaPlayerController::AnyKey);
	AnyKeyBinding.bConsumeInput = false;
	InputComponent->BindAction(FNovaPlayerInput::MenuToggle, EInputEvent::IE_Released, this, &ANovaPlayerController::ToggleMenuOrQuit);
	if (GetWorld()->WorldType == EWorldType::PIE)
	{
		InputComponent->BindKey(EKeys::Enter, EInputEvent::IE_Released, this, &ANovaPlayerController::ToggleMenuOrQuit);
	}
}

void ANovaPlayerController::ToggleMenuOrQuit()
{
	if (!Cast<ANovaWorldSettings>(GetWorld()->GetWorldSettings())->IsMenuMap())
	{
		if (IsOnMainMenu())
		{
			ExitGame();
		}
		else
		{
			UNovaMenuManager* MenuManager = UNovaMenuManager::Get();

			if (!MenuManager->IsMenuOpen())
			{
				MenuManager->OpenMenu();
			}
			else
			{
				MenuManager->CloseMenu();
			}
		}
	}
}

void ANovaPlayerController::AnyKey(FKey Key)
{
	UNovaMenuManager::Get()->SetUsingGamepad(Key.IsGamepadKey());

	// Exit photo mode
	if (IsInPhotoMode())
	{
		if (UNovaMenuManager::Get()->GetMenu()->IsActionKey(PhotoModeAction, Key) ||
			UNovaMenuManager::Get()->GetMenu()->IsActionKey(FNovaPlayerInput::MenuCancel, Key))
		{
			ExitPhotoMode();
		}
	}
}

/*----------------------------------------------------
    Getters
----------------------------------------------------*/

const FNovaSpacecraft* ANovaPlayerController::GetSpacecraft() const
{

	ANovaSpacecraftPawn* SpacecraftPawn = GetSpacecraftPawn();
	ANovaGameState*      GameState      = GetWorld()->GetGameState<ANovaGameState>();

	if (IsValid(SpacecraftPawn) && IsValid(GameState))
	{
		FGuid PlayerSpacecraftIdentifier = SpacecraftPawn->GetSpacecraftIdentifier();
		return GameState->GetSpacecraft(PlayerSpacecraftIdentifier);
	}
	else
	{
		return nullptr;
	}
}

/*----------------------------------------------------
    Test code
----------------------------------------------------*/

#if WITH_EDITOR

void ANovaPlayerController::OnJoinRandomFriend(TArray<TSharedRef<FOnlineFriend>> FriendList)
{
	for (auto Friend : FriendList)
	{
		if (Friend.Get().GetDisplayName() == "Deimos Games" || Friend.Get().GetDisplayName() == "Stranger")
		{
			JoinFriend(Friend);
		}
	}
}

void ANovaPlayerController::OnJoinRandomSession(TArray<FOnlineSessionSearchResult> SearchResults)
{
	for (auto Result : SearchResults)
	{
		if (Result.Session.OwningUserId != GetLocalPlayer()->GetPreferredUniqueNetId())
		{
			UNovaMenuManager* MenuManager = UNovaMenuManager::Get();

			MenuManager->RunAction(ENovaLoadingScreen::Launch,    //
				FNovaAsyncAction::CreateLambda(
					[=]()
					{
						MenuManager->GetOverlay<SNovaOverlay>()->Notify(
							FText::FormatNamed(LOCTEXT("JoinFriend", "Joining {session}"), TEXT("session"),
								FText::FromString(*Result.Session.GetSessionIdStr())),
							FText(), ENovaNotificationType::Info);

						UNovaSessionsManager::Get()->JoinSearchResult(Result);
					}));
		}
	}
}

void ANovaPlayerController::TestJoin()
{
	UNovaSessionsManager* SessionsManager = UNovaSessionsManager::Get();

	if (SessionsManager->GetOnlineSubsystemName() != TEXT("Null"))
	{
		SessionsManager->SearchFriends(FOnFriendSearchComplete::CreateUObject(this, &ANovaPlayerController::OnJoinRandomFriend));
	}
	else
	{
		SessionsManager->SearchSessions(true, FOnSessionSearchComplete::CreateUObject(this, &ANovaPlayerController::OnJoinRandomSession));
	}
}

#endif

class ANovaSpacecraftPawn* ANovaPlayerController::GetSpacecraftPawn() const
{
	return GetPawn<class ANovaSpacecraftPawn>();
}

#undef LOCTEXT_NAMESPACE
