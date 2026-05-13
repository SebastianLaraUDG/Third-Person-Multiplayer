// Sebastian Lara. All rights reserved.


// ReSharper disable CppTooWideScope
#include "BlasterPlayerController.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Blaster/BlasterComponents/CombatComponent.h"
#include "Blaster/Character/BlasterCharacter.h"
#include "Blaster/GameMode/BlasterGameMode.h"
#include "Blaster/GameState/BlasterGameState.h"
#include "Blaster/HUD/Announcement.h"
#include "Blaster/HUD/BlasterHUD.h"
#include "Blaster/HUD/CharacterOverlay.h"
#include "Blaster/HUD/PauseMenu.h"
#include "Blaster/HUD/SniperScope.h"
#include "Blaster/PlayerState/BlasterPlayerState.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "GameFramework/GameMode.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "MultiplayerSessions/Public/MultiplayerSessionsSubsystem.h"


void ABlasterPlayerController::BeginPlay()
{
	Super::BeginPlay();
	
	EnsureBlasterHUD();

	ServerCheckMatchState();

	// Start timer to check server-client time sync.
	// Update time HUD and check time sync. 
	GetWorldTimerManager().SetTimer(CountdownTimer, this, &ThisClass::SetHUDTime, TimeSyncFrequency, true);
	// TODO: stop timer when game ends.
}

void ABlasterPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	auto EnhancedInputComp = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EnhancedInputComp)
	{
		UE_LOG(LogTemp, Warning, TEXT("Enhanced Input Component is NULL in %s"), *GetNameSafe(this));
		return;
	}

	if (PauseInputAction)
	{
		EnhancedInputComp->BindAction(PauseInputAction, ETriggerEvent::Started, this, &ThisClass::TogglePauseMenu);
	}
}

void ABlasterPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	
	// Update HUD on possess. This is because when the character respawned HUD (health bar) was not
	// being initialized correctly.
	if (const auto BlasterCharacter = Cast<ABlasterCharacter>(InPawn))
	{
		BlasterCharacter->UpdateHUD();
	}
}

void ABlasterPlayerController::ReceivedPlayer()
{
	Super::ReceivedPlayer();
	if (IsLocalController())
	{
		ServerRequestServerTime(GetWorld()->GetTimeSeconds());
		/*
		 // Add sniper in next tick due to its dependency with the player controller (it cannot be created yet in begin play).
		const auto AddSniperScopeCallback = FTimerDelegate::CreateLambda([this]
			{
				// Add sniper scope and hide it.
				BlasterHUD = BlasterHUD ? BlasterHUD.Get() : Cast<ABlasterHUD>(GetHUD());
				BlasterHUD->AddSniperScope();
			if (BlasterHUD->SniperScope)
				BlasterHUD->SniperScope->SetVisibility(ESlateVisibility::Hidden);
			}
		);
		//GetWorldTimerManager().SetTimerForNextTick(AddSniperScopeCallback);
		FTimerHandle TimerHandle;
		GetWorldTimerManager().SetTimer(TimerHandle, AddSniperScopeCallback, 5.f, false);
		*/
	}
}

void ABlasterPlayerController::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, MatchState)
}

void ABlasterPlayerController::ClientRestart_Implementation(class APawn* NewPawn)
{
	Super::ClientRestart_Implementation(NewPawn);
	
	// Binding in BeginPlay fails in remote clients since GetLocalPlayer is null at the time of call Therefore, we have to bind here.
	// NOTE for myself (I am noob), ClientRestart_Implementation is a crucial function within the APlayerController class used to handle the client-side, post-possession logic when a player pawn is spawned, possessed, or respawned in a networked game.
	// It is called on the client when the server informs it to possess a new pawn.
	if (UIMappingContext)
	{
		if (auto EnhancedInputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			EnhancedInputSubsystem->AddMappingContext(UIMappingContext, 0);
		}
	}
}

void ABlasterPlayerController::SetHUDHealth(float Health, float MaxHealth)
{
	EnsureBlasterHUD();

	const bool bHUDValid = HUDAndOverlayAreValid() &&
		BlasterHUD->CharacterOverlay->HealthBar &&
		BlasterHUD->CharacterOverlay->HealthText;

	if (bHUDValid)
	{
		const float HealthPercent = Health / MaxHealth;
		// Progress bar Percent.
		BlasterHUD->CharacterOverlay->HealthBar->SetPercent(HealthPercent);
		// Text.
		const FString HealthText = FString::Printf(TEXT("%d/%d"), FMath::CeilToInt32(Health),
		                                           FMath::CeilToInt32(MaxHealth));
		BlasterHUD->CharacterOverlay->HealthText->SetText(FText::FromString(HealthText));
	}
}

void ABlasterPlayerController::SetHUDScore(const float Score)
{
	EnsureBlasterHUD();

	if (HUDAndOverlayAreValid() && BlasterHUD->CharacterOverlay->ScoreAmount)
	{
		const FString ScoreText = FString::Printf(TEXT("%d"), FMath::FloorToInt(Score));
		BlasterHUD->CharacterOverlay->ScoreAmount->SetText(FText::FromString(ScoreText));
	}
}

void ABlasterPlayerController::SetHUDDefeats(const int32 Defeats)
{
	EnsureBlasterHUD();

	if (HUDAndOverlayAreValid() && BlasterHUD->CharacterOverlay->ScoreAmount)
	{
		const FString DefeatsText = FString::Printf(TEXT("%d"), Defeats);
		BlasterHUD->CharacterOverlay->DefeatsAmount->SetText(FText::FromString(DefeatsText));
	}
}

void ABlasterPlayerController::SetHUDWeaponAmmo(const int32 Ammo)
{
	EnsureBlasterHUD();

	if (HUDAndOverlayAreValid() && BlasterHUD->CharacterOverlay->WeaponAmmoAmount)
	{
		const FString AmmoText = FString::Printf(TEXT("%d"), Ammo);
		BlasterHUD->CharacterOverlay->WeaponAmmoAmount->SetText(FText::FromString(AmmoText));
	}
}

void ABlasterPlayerController::SetHUDWeaponCarriedAmmo(const int32 CarriedAmmo)
{
	EnsureBlasterHUD();

	if (HUDAndOverlayAreValid() && BlasterHUD->CharacterOverlay->CarriedAmmoAmount)
	{
		const FString CarriedAmmoText = FString::Printf(TEXT("%d"), CarriedAmmo);
		BlasterHUD->CharacterOverlay->CarriedAmmoAmount->SetText(FText::FromString(CarriedAmmoText));
	}
}

void ABlasterPlayerController::SetHUDEquippedWeaponName(const EWeaponType WeaponType)
{
	EnsureBlasterHUD();

	if (HUDAndOverlayAreValid() && BlasterHUD->CharacterOverlay->EquippedWeaponName)
	{
		FText EquippedWeaponName;
		switch (WeaponType)
		{
		case EWeaponType::EWT_AssaultRifle: EquippedWeaponName = FText::FromString(TEXT("Assault Rifle"));
			break;
		case EWeaponType::EWT_RocketLauncher: EquippedWeaponName = FText::FromString(TEXT("Rocket Launcher"));
			break;
		case EWeaponType::EWT_Pistol: EquippedWeaponName = FText::FromString(TEXT("Pistol"));
			break;
		case EWeaponType::EWT_Shotgun: EquippedWeaponName = FText::FromString(TEXT("Shotgun"));
			break;
		case EWeaponType::EWT_SniperRifle: EquippedWeaponName = FText::FromString(TEXT("Sniper Rifle"));
			break;
		case EWeaponType::EWT_SubmachineGun: EquippedWeaponName = FText::FromString(TEXT("SMG"));
			break;
		case EWeaponType::EWT_GrenadeLauncher: EquippedWeaponName = FText::FromString(TEXT("Grenade Launcher"));
			break;
		// Empty text.
		case EWeaponType::EWT_MAX: EquippedWeaponName = FText::FromString(TEXT(""));
			break;
		}
		BlasterHUD->CharacterOverlay->EquippedWeaponName->SetText(EquippedWeaponName);
	}
}

void ABlasterPlayerController::SetHUDMatchCountdown(const float CountdownTime)
{
	EnsureBlasterHUD();

	if (HUDAndOverlayAreValid() && BlasterHUD->CharacterOverlay->MatchCountDownText)
	{
		if (CountdownTime < 0.f) // Prevent displaying negative time.
		{
			BlasterHUD->CharacterOverlay->MatchCountDownText->SetText(FText());
			return;
		}
		const int32 Minutes = FMath::FloorToInt(CountdownTime / 60.f);
		const int32 Seconds = CountdownTime - Minutes * 60.f;

		/* Play countdown animation in text if specified and the time remaining is 30 seconds or less. */
		if (Seconds <= 30 && Minutes == 0 && BlasterHUD->CharacterOverlay->CountdownAnimation)
		{
			BlasterHUD->CharacterOverlay->MatchCountDownText->SetColorAndOpacity(FLinearColor(1.0f, 0.0f, 0.0f, 1.0f));
			BlasterHUD->CharacterOverlay->PlayAnimation(BlasterHUD->CharacterOverlay->CountdownAnimation,
			                                            0.f, 30);
			// 30 loops is hardcoded for 30 seconds, it could be changed to a UPROPERTY variable if you want to.
		}

		const FString CountdownText = FString::Printf(TEXT("%02d:%02d"), Minutes, Seconds);
		BlasterHUD->CharacterOverlay->MatchCountDownText->SetText(FText::FromString(CountdownText));
	}
}

void ABlasterPlayerController::SetHUDAnnouncementCountdown(const float CountdownTime)
{
	EnsureBlasterHUD();

	const bool bHUDValid = BlasterHUD && BlasterHUD->Announcement && BlasterHUD->Announcement->WarmupTime;
	if (bHUDValid)
	{
		if (CountdownTime < 0.f) // Avoid displaying negative time.
		{
			BlasterHUD->Announcement->WarmupTime->SetText(FText());
			return;
		}
		const int32 Minutes = FMath::FloorToInt(CountdownTime / 60.f);
		const int32 Seconds = CountdownTime - Minutes * 60;

		const FString CountdownText = FString::Printf(TEXT("%02d:%02d"), Minutes, Seconds);
		BlasterHUD->Announcement->WarmupTime->SetText(FText::FromString(CountdownText));
	}
}

void ABlasterPlayerController::SetHUDSniperScope(const bool bIsAiming)
{
	EnsureBlasterHUD();

	if (!BlasterHUD->SniperScope)
	{
		BlasterHUD->AddSniperScope();
	}

	const bool bHUDValid = BlasterHUD && BlasterHUD->SniperScope && BlasterHUD->SniperScope->ScopeZoomIn;
	if (!bHUDValid)
	{
		return;
	}

	// Play aim animation (zoom in).
	if (bIsAiming)
	{
		BlasterHUD->SniperScope->SetVisibility(ESlateVisibility::Visible);
		BlasterHUD->SniperScope->PlayAnimation(BlasterHUD->SniperScope->ScopeZoomIn);
	}
	// Play reverse aim animation (zoom out).
	else
	{
		BlasterHUD->SniperScope->PlayAnimation(BlasterHUD->SniperScope->ScopeZoomIn, 0.f, 1,
		                                       EUMGSequencePlayMode::Reverse);
	}
}

void ABlasterPlayerController::SetHUDGrenades(const int32 Grenades)
{
	EnsureBlasterHUD();

	if (HUDAndOverlayAreValid() && BlasterHUD->CharacterOverlay->GrenadesText)
	{
		const FString GrenadesText = FString::Printf(TEXT("%d"), Grenades);
		BlasterHUD->CharacterOverlay->GrenadesText->SetText(FText::FromString(GrenadesText));
	}
}

void ABlasterPlayerController::SetHUDTime()
{
	float TimeLeft = 0.f;

	// 1. Calculate the remaining real time based on current match state.
	if (MatchState == MatchState::WaitingToStart)
	{
		TimeLeft = WarmupTime - (GetServerTime() - LevelStartingTime);
	}
	else if (MatchState == MatchState::InProgress)
	{
		TimeLeft = WarmupTime + MatchTime - (GetServerTime() - LevelStartingTime);
	}
	else if (MatchState == MatchState::Cooldown)
	{
		// The total amount of time until the end of the Cooldown.
		TimeLeft = CooldownTime + WarmupTime + MatchTime - (GetServerTime() - LevelStartingTime);
	}

	const uint32 SecondsLeft = FMath::CeilToInt(TimeLeft);

	// 2. Only update the HUD if the seconds changed to save a bit of performance.
	if (CountdownInt != SecondsLeft)
	{
		if (MatchState == MatchState::WaitingToStart || MatchState == MatchState::Cooldown)
		{
			// In Cooldown and Warmup we use to use the Announcement text.
			SetHUDAnnouncementCountdown(TimeLeft);
		}
		else if (MatchState == MatchState::InProgress)
		{
			SetHUDMatchCountdown(TimeLeft);
		}
	}

	CountdownInt = SecondsLeft;
	CheckTimeSync();
}

// ~Begin Time Sync interface.

void ABlasterPlayerController::ServerRequestServerTime_Implementation(const float TimeOfClientRequest)
{
	const float ServerTimeOfReceipt = GetWorld()->GetTimeSeconds();
	ClientReportServerTime(TimeOfClientRequest, ServerTimeOfReceipt);
}

void ABlasterPlayerController::ClientReportServerTime_Implementation(const float TimeOfClientRequest,
                                                                     const float TimeServerReceivedClientRequest)
{
	// The time it took for the client request to get to the server.
	const float RoundTripTime = GetWorld()->GetTimeSeconds() - TimeOfClientRequest;
	const float CurrentServerTime = TimeServerReceivedClientRequest + 0.5f * RoundTripTime;
	// Dividing by two is just an approximation.
	ClientServerDelta = CurrentServerTime - GetWorld()->GetTimeSeconds();
}

void ABlasterPlayerController::CheckTimeSync()
{
	if (IsLocalController())
	{
		ServerRequestServerTime(GetWorld()->GetTimeSeconds());
	}
}

float ABlasterPlayerController::GetServerTime() const
{
	if (HasAuthority()) return GetWorld()->GetTimeSeconds();
	return GetWorld()->GetTimeSeconds() + ClientServerDelta;
}

// ~End Time Sync interface.

void ABlasterPlayerController::ServerCheckMatchState_Implementation()
{
	// Get all time values from the server and get ready for announcement and match widgets.
	if (const auto BlasterGameMode = Cast<ABlasterGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		WarmupTime = BlasterGameMode->WarmupTime;
		MatchTime = BlasterGameMode->MatchTime;
		CooldownTime = BlasterGameMode->CooldownTime;
		MatchState = BlasterGameMode->GetMatchState();
		LevelStartingTime = BlasterGameMode->LevelStartingTime;
		ClientJoinMidGame(MatchState, WarmupTime, MatchTime, CooldownTime, LevelStartingTime);
	}
}

void ABlasterPlayerController::ClientJoinMidGame_Implementation(const FName& StateOfMatch, const float Warmup,
                                                                const float Match, const float Cooldown,
                                                                const float StartingTime)
{
	WarmupTime = Warmup;
	MatchTime = Match;
	CooldownTime = Cooldown;
	LevelStartingTime = StartingTime;
	MatchState = StateOfMatch; // Update the variable locally so that the client gets the info NOW. 
	OnMatchStateSet(MatchState);
	if (BlasterHUD && MatchState == MatchState::WaitingToStart)
	{
		BlasterHUD->AddAnnouncement();
	}
}

void ABlasterPlayerController::OnMatchStateSet(const FName& State)
{
	MatchState = State;

	// Just transitioned to gameplay.
	if (MatchState == MatchState::InProgress)
	{
		HandleMatchHasStarted();
	}
	// Match ended.
	else if (MatchState == MatchState::Cooldown)
	{
		HandleCooldown();
	}
}

void ABlasterPlayerController::RemovePauseMenu()
{
	bPauseMenuOpen = false;
	// BlasterHUD->PauseMenu->SetVisibility(ESlateVisibility::Hidden);
	BlasterHUD->PauseMenu->RemoveFromParent();
	// SetIgnoreMoveInput(false);
	// SetIgnoreLookInput(false);
	SetShowMouseCursor(false);
	const FInputModeGameOnly InputModeGameOnly;
	SetInputMode(InputModeGameOnly);
	if (auto BlasterCharacter = Cast<ABlasterCharacter>(GetPawn()))
	{
		BlasterCharacter->bDisableGameplay = false;
	}
}

void ABlasterPlayerController::ReturnToMainMenu()
{
	if (MainMenuLevel.IsNull()) return;

	auto GameInstance = GetGameInstance();
	if (!GameInstance) return;

	StoredMenuPath = MainMenuLevel.ToSoftObjectPath().GetLongPackageName();

	if (HasAuthority())
	// Please remember blaster project uses a listen-server approach when hosting, so this will be called from a ListenServer (player), not from a dedicated server. 
	{
		auto MultiplayerSubsystem = GameInstance->GetSubsystem<UMultiplayerSessionsSubsystem>();
		if (!MultiplayerSubsystem) return;

		MultiplayerSubsystem->MultiplayerOnDestroySessionComplete.AddDynamic(
			this, &ThisClass::OnDestroySessionForReturn);
		MultiplayerSubsystem->DestroySession();
	}
	else
	{
		ClientTravel(StoredMenuPath, TRAVEL_Absolute);
	}
}

void ABlasterPlayerController::OnDestroySessionForReturn(bool bWasSuccessful)
{
	auto GameInstance = GetGameInstance();
	if (!GameInstance) return;

	if (auto MultiplayerSubsystem = GameInstance->GetSubsystem<UMultiplayerSessionsSubsystem>())
	{
		MultiplayerSubsystem->MultiplayerOnDestroySessionComplete.RemoveDynamic(
			this, &ThisClass::OnDestroySessionForReturn);
	}
	if (auto World = GetWorld())
	{
		World->ServerTravel(StoredMenuPath);
	}
}

void ABlasterPlayerController::QuitGame()
{
	auto GameInstance = GetGameInstance();
	if (!GameInstance) return;

	if (HasAuthority())
	{
		auto MultiplayerSubsystem = GameInstance->GetSubsystem<UMultiplayerSessionsSubsystem>();
		if (MultiplayerSubsystem) MultiplayerSubsystem->DestroySession();
	}

	UKismetSystemLibrary::QuitGame(GetWorld(), this, EQuitPreference::Quit, false);
}

void ABlasterPlayerController::OnRep_MatchState()
{
	// Just transitioned to gameplay.
	if (MatchState == MatchState::InProgress)
	{
		HandleMatchHasStarted();
	}
	// Match ended.
	else if (MatchState == MatchState::Cooldown)
	{
		HandleCooldown();
	}
}

/** When match starts add character overlay and hide announcement text.*/
void ABlasterPlayerController::HandleMatchHasStarted()
{
	if (EnsureBlasterHUD())
	{
		BlasterHUD->AddCharacterOverlay();
		// Hide announcement text. 
		if (BlasterHUD->Announcement)
		{
			BlasterHUD->Announcement->SetVisibility(ESlateVisibility::Hidden);
		}
	}
}

void ABlasterPlayerController::HandleCooldown()
{
	if (EnsureBlasterHUD())
	{
		BlasterHUD->CharacterOverlay->RemoveFromParent(); // Remove overlay responsible for health, ammo, etc.

		const bool bHUDValid = BlasterHUD->Announcement &&
			BlasterHUD->Announcement->AnnouncementText &&
			BlasterHUD->Announcement->InfoText;

		if (bHUDValid)
		{
			BlasterHUD->Announcement->SetVisibility(ESlateVisibility::Visible); // Show announcement text.
			const FString AnnouncementText("New match starts in: ");
			// New text telling match will restart in a few seconds.
			BlasterHUD->Announcement->AnnouncementText->SetText(FText::FromString(AnnouncementText));

			DisplayWinner();
		}
	}
	// In cooldown state, disable gameplay movement and
	// in case the weapon is firing, stop firing. 
	auto BlasterCharacter = Cast<ABlasterCharacter>(GetPawn());
	if (BlasterCharacter && BlasterCharacter->GetCombatComponent())
	{
		BlasterCharacter->bDisableGameplay = true;
		BlasterCharacter->GetCombatComponent()->FireButtonPressed(false);
	}
	
	// Remove the pause menu in case it is displaying.
	if (EnsureBlasterHUD() && BlasterHUD->PauseMenu && bPauseMenuOpen)
	{
		BlasterHUD->PauseMenu->RemoveFromParent();
		bPauseMenuOpen = false;
	}
}

void ABlasterPlayerController::TogglePauseMenu()
{
	
	UE_LOG(LogPlayerController, Display, TEXT("IF YOU ARE READING THIS, INPUT IS WORKING PROPERLY. Is LocalController %d. BlasterHUD: %d."
										   "BlasterHud->PauseMenu: %d"),IsLocalController(),BlasterHUD? 1 :0, BlasterHUD->PauseMenu ? 1 : 0)
	
	if (!IsLocalController()) return; // Pause menu only on local player.
	
	if (auto GameState = GetWorld()->GetGameState<AGameState>())
	{
		if (GameState->GetMatchState() != MatchState::InProgress)
		{
			// Players should only be able to open pause menu if the match state is a gameplay state.
			// I also thought about removing IMC, or using a boolean, in case current approach does not work, try these or other options.
			return;
		}
	}
	
	if (!EnsureBlasterHUD() || !BlasterHUD->PauseMenuClass)
	{
		return;
	}

	bPauseMenuOpen = !bPauseMenuOpen;

	if (bPauseMenuOpen)
	{
		BlasterHUD->PauseMenu = CreateWidget<UPauseMenu>(this, BlasterHUD->PauseMenuClass);
		BlasterHUD->PauseMenu->AddToViewport();
		BlasterHUD->PauseMenu->OnCloseMenuRequested.AddUObject(this,
												   &ThisClass::RemovePauseMenu);
		BlasterHUD->PauseMenu->OnReturnToMenuRequested.AddUObject(this,
													  &ThisClass::ReturnToMainMenu);
		BlasterHUD->PauseMenu->OnQuitGameRequested.AddUObject(this, &ThisClass::QuitGame);
		
		// SetIgnoreMoveInput(true);
		// SetIgnoreLookInput(true);
		SetShowMouseCursor(true);
		FInputModeGameAndUI InputModeData;
		// This is to avoid a double click issue.
		InputModeData.SetWidgetToFocus(BlasterHUD->PauseMenu->TakeWidget());
		InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(InputModeData);
		if (auto BlasterCharacter = Cast<ABlasterCharacter>(GetPawn()))
		{
			BlasterCharacter->bDisableGameplay = true;
		}
	}
	else
	{
		RemovePauseMenu(); // This was made a function to be bound from the BlasterHUD class when pressing the ClosePauseMenu button.
	}
}

void ABlasterPlayerController::DisplayWinner() const
{
	const auto BlasterGameState = Cast<ABlasterGameState>(UGameplayStatics::GetGameState(this));
	const auto BlasterPlayerState = GetPlayerState<ABlasterPlayerState>();
	if (BlasterGameState && BlasterPlayerState)
	{
		const TArray<ABlasterPlayerState*> TopPlayers = BlasterGameState->TopScoringPlayers;
		FString InfoTextString;

		// Case NO WINNER.
		if (TopPlayers.Num() == 0)
		{
			InfoTextString = FString("There is no winner.");
		}
		// Case CURRENT PLAYER IS WINNER.
		else if (TopPlayers.Num() == 1 && TopPlayers[0] == BlasterPlayerState)
		{
			InfoTextString = FString("You are the winner!");
		}
		// Case ONE WINNER, BUT IT IS NOT THIS PLAYER.
		else if (TopPlayers.Num() == 1)
		{
			InfoTextString = FString::Printf(TEXT("Winner: \n %s"), *TopPlayers[0]->GetPlayerName());
		}
		// Case MORE THAN ONE WINNER.
		else if (TopPlayers.Num() > 1)
		{
			InfoTextString = FString("Players tied for the win:\n");
			for (const auto TiedPlayer : TopPlayers)
			{
				InfoTextString.Append(FString::Printf(TEXT("%s\n"), *TiedPlayer->GetPlayerName()));
			}
		}

		BlasterHUD->Announcement->InfoText->SetText(FText::FromString(InfoTextString));
	}
}

ABlasterHUD* ABlasterPlayerController::EnsureBlasterHUD()
{
	if (!IsValid(BlasterHUD))
	{
		BlasterHUD = Cast<ABlasterHUD>(GetHUD());
	}
	return BlasterHUD;
	// return BlasterHUD = BlasterHUD ? BlasterHUD.Get() : Cast<ABlasterHUD>(GetHUD());
}
