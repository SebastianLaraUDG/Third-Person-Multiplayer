// Sebastian Lara. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blaster/HUD/BlasterHUD.h"
#include "GameFramework/PlayerController.h"
#include "Blaster/Weapon/WeaponTypes.h"
#include "BlasterPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;
class ABlasterHUD;
/**
 * 
 */
UCLASS()
class BLASTER_API ABlasterPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void ReceivedPlayer() override; // Sync with server clock as soon as possible.
	virtual void ClientRestart_Implementation(class APawn* NewPawn) override;
	/**
	 * HUD
	 */
	void SetHUDHealth(float Health, float MaxHealth);
	void SetHUDScore(const float Score);
	void SetHUDDefeats(const int32 Defeats);
	void SetHUDWeaponAmmo(const int32 Ammo);
	void SetHUDWeaponCarriedAmmo(const int32 CarriedAmmo);
	void SetHUDEquippedWeaponName(const EWeaponType WeaponType);
	void SetHUDMatchCountdown(const float CountdownTime); // If only 30 seconds left, display some text animation and set text color to red.
	void SetHUDAnnouncementCountdown(const float CountdownTime); // Same as SetHUDMatchCountdown, but with a different TextBlock. TODO: checks and calculations could be refactorized into a separate function to make cleaner code.
	void SetHUDSniperScope(const bool bIsAiming);
	void SetHUDGrenades(const int32 Grenades);
	
	void OnMatchStateSet(const FName& State);
	
	// Pause.
	
	void RemovePauseMenu();
	
	UFUNCTION(BlueprintCallable)
	void ReturnToMainMenu();
	
	UFUNCTION(BlueprintCallable)
	void QuitGame();
	
	UFUNCTION()
	void OnDestroySessionForReturn(bool bWasSuccessful);
	
	UPROPERTY(EditDefaultsOnly, Category = Input)
	TObjectPtr<UInputMappingContext> UIMappingContext;
	
	UPROPERTY(EditDefaultsOnly, Category = Input)
	TObjectPtr<UInputAction> PauseInputAction;

	UPROPERTY(EditDefaultsOnly, Category = Levels)
	TSoftObjectPtr<UWorld> MainMenuLevel;
	
protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void OnPossess(APawn* InPawn) override;
	
	// Set HUD time and check time sync.
	// It displays time according to match or announcement case.
	void SetHUDTime();
	
	virtual float GetServerTime() const; // Synced with server world clock.
	
	/**
	 * Sync time between client and server.
	 */
	
	// Requests the current server time, passing in the client's time when the request was sent.
	UFUNCTION(Server, Reliable)
	void ServerRequestServerTime(const float TimeOfClientRequest);
	
	// Reports the current server time to the client in response to ServerRequestServerTime
	UFUNCTION(Client, Reliable)
	void ClientReportServerTime(const float TimeOfClientRequest, const float TimeServerReceivedClientRequest);
	
	float ClientServerDelta = 0.f; // Difference between client and server time.
	
	// Time sync and time HUD text update frequency. It is recommended to set this at a low value. Greater values will increase delay between updates.
	// TODO: I don't like putting this here. It should be in the game mode or game state.
	UPROPERTY(EditAnywhere, Category = Time, meta = (ClampMin = 0.f))
	float TimeSyncFrequency = 0.2f;
	
	
	void CheckTimeSync();
	
	UFUNCTION(Server, Reliable)
	void ServerCheckMatchState();
	
	UFUNCTION(Client, Reliable)
	void ClientJoinMidGame(const FName& StateOfMatch, const float Warmup, const float Match, const float Cooldown, const float StartingTime);
	
	/* Different options to display: No winner, You are the winner, Sole winner, Top winners of the match. */
	virtual void DisplayWinner() const;
	
private:
	// UPROPERTY()
	TObjectPtr<ABlasterHUD> BlasterHUD;
	
	float LevelStartingTime = 0.f;
	float MatchTime = 0.f;
	float WarmupTime = 0.f;
	float CooldownTime = 0.f;
	
	uint32 CountdownInt = 0;
	FTimerHandle CountdownTimer;
	
	bool bPauseMenuOpen = false;
	FString StoredMenuPath;
	
	/** Match state. */
	
	UPROPERTY(ReplicatedUsing = OnRep_MatchState)
	FName MatchState;
	
	UFUNCTION()
	void OnRep_MatchState();
	
	// Add character overlay to player HUD when match starts.
	void HandleMatchHasStarted();
	// Hide character overlay and show announcement text.
	void HandleCooldown();
	
	void TogglePauseMenu();
	
	// Avoid code duplication with these functions.
	ABlasterHUD* EnsureBlasterHUD();
	
	bool HUDAndOverlayAreValid() const { return BlasterHUD && BlasterHUD->CharacterOverlay; }
};
