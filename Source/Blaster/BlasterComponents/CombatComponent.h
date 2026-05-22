// Sebastian Lara. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blaster/BlasterTypes/CombatState.h"
#include "Blaster/HUD/BlasterHUD.h"
#include "Blaster/Weapon/Weapon.h"
#include "Components/ActorComponent.h"
#include "Blaster/Weapon/WeaponTypes.h"
#include "CombatComponent.generated.h"

class ABlasterHUD;
class ABlasterPlayerController;
class AWeapon;
class ABlasterCharacter;
class AProjectileGrenade;

/*
 * Combat component. Responsible for all combat functionality.
 * To support left hand FABRIK make sure the Skeletal mesh contains a socket
 * "LeftHandSocket".
 * 
 * To be able to reload you must add a notify in you Montage and in your animation blueprint
 * implement that notify, adding the FinishReloading function node.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class BLASTER_API UCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	friend class ABlasterCharacter;
	UCombatComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	void EquipWeapon(AWeapon* WeaponToEquip);
	void Reload();
	void FireButtonPressed(bool bPressed);
	void PickupAmmo(EWeaponType WeaponType, int32 AmmoAmount);
	
	// Designed to be called from notifies. Set combat state to Unoccupied in server.
	UFUNCTION(BlueprintCallable)
	void FinishReloading();
	
	// Jump to the shotgun end anim montage section. Explicit jump section is "ShotgunEnd" (hardcoded).
	void JumpToShotgunEnd();
	
	// This takes care of changing combat state and attaches the equipped weapon to right hand.
	UFUNCTION(BlueprintCallable)
	void ThrowGrenadeFinished();
	
	// Hides the grenade mesh and TODO: spawn actual grenade object.
	UFUNCTION(BlueprintCallable)
	void LaunchGrenade();
	
	// Designed to be called from anim notifies.
	UFUNCTION(BlueprintCallable)
	void ShotgunShellReload();
	
	UPROPERTY(EditDefaultsOnly)
	TObjectPtr<USoundCue> ZoomInSniperRifle;
	
	UPROPERTY(EditDefaultsOnly)
	TObjectPtr<USoundCue> ZoomOutSniperRifle;

protected:
	virtual void BeginPlay() override;
	void SetAiming(bool NewAiming);

	UFUNCTION(Server, Reliable)
	void ServerSetAiming(bool NewAiming);

	UFUNCTION()
	void OnRep_EquippedWeapon();
	
	void Fire();

	UFUNCTION(Server, Reliable)
	void ServerFire(const FVector_NetQuantize& TraceHitTarget);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastFire(const FVector_NetQuantize& TraceHitTarget);

	void TraceUnderCrosshairs(FHitResult& TraceHitResult);
	
	void SetHUDCrosshairs(float DeltaTime);
	
	UFUNCTION(Server, Reliable)
	void ServerReload();
	
	// Play character reload montage.
	void HandleReload() const;

	// Calculates 
	int32 AmountToReload() const;
	
	void ThrowGrenade();

	UFUNCTION(Server, Reliable)
	void ServerThrowGrenade();
	
	UFUNCTION(Server, Reliable)
	void ServerLaunchGrenade(const FVector_NetQuantize& Target);
	
	UPROPERTY(EditAnywhere)
	TSubclassOf<AProjectileGrenade> GrenadeClass;
	
	void DropEquippedWeapon();
	void AttachActorToRightHand(const AActor* ActorToAttach) const;
	void AttachActorToLeftHand(const AActor* ActorToAttach) const;
	void UpdateCarriedAmmo();
	void PlayEquipWeaponSound() const;
	void ReloadEmptyWeapon();
	void ShowAttachedGrenade(const bool bShowGrenade) const;
	
private:
//	UPROPERTY()
	TObjectPtr<ABlasterCharacter> Character;
//	UPROPERTY()
	TObjectPtr<ABlasterPlayerController> Controller;
//	UPROPERTY()
	TObjectPtr<ABlasterHUD> HUD;

	// Hand Socket Name.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta =(AllowPrivateAccess = true))
	FName HandSocketName = FName("RightHandSocket");

	UPROPERTY(ReplicatedUsing=OnRep_EquippedWeapon, VisibleAnywhere, BlueprintReadOnly, Category = Combat,
		meta = (AllowPrivateAccess = true))
	TObjectPtr<AWeapon> EquippedWeapon;

	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = Combat, meta = (AllowPrivateAccess = true))
	bool bIsAiming;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Movement, meta = (AllowPrivateAccess = true))
	float BaseWalkSpeed;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Movement, meta = (AllowPrivateAccess = true))
	float AimWalkSpeed;

	bool bFireButtonPressed;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
	float TraceLength = 100000.f;
	
	// An offset in front of the start of the trace to avoid collision detection problems (in centimeters).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true, ClampMin = 0.1f, ClampMax = 1000.f, Units = "Centimeters"))
	float StartOfTraceOffset = 20.f;
	
	/*
	 * HUD and crosshairs.
	 */
	
	float CrosshairVelocityFactor;
	float CrosshairInAirFactor;
	float CrosshairAimFactor;
	float CrosshairShootingFactor;
	float CrosshairEnemyFactor;
	bool bAimingAtEnemy;
	/*
	 * Amount to shrink crosshairs while aiming at enemies.
	 * Small value means shrink crosshairs a bit and a high value shrinks crosshairs a lot.
	 * Be careful with very high values.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true, ClampMin = 0.1, ClampMax = 2.0))
	float AimAtEnemyShrinkFactor = 0.6f;
	
	
	FHUDPackage HUDPackage;
	
	/**
	 * Increment in crosshairs when shooting. For example a large value like 3 will have a dispersion effect like a heavy automatic gun.
	 * Note this only increments crosshairs displacement, not bullet trajectory.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true, ClampMin = 0))
	float CrosshairsShootingFactorIncrement = 0.5f;
	
	// The speed at which the crosshairs go back to normal positions.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
	float CrosshairShootingFactorBackToZeroInterpSpeed = 20.f;
	
	/**
	 *A negative value will put crosshairs closer to the center while aiming, a positive value while place them away from the center.
	 * 
	 * For example, a value to put crosshairs closer to center would be -0.4f.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
	float CrosshairsDisplacementWhileAiming = 0.58f;
	
	FVector HitTarget;
	
	/*
	 * Aiming and FOV
	 */
	
	// Field of view when not aiming; set to the camera's base FOV in BeginPlay.
	float DefaultFOV;
	
	float CurrentFOV;
	
	/* The speed to interp to zoom out. (If you want to change the zoom in speed,
	 you must change the ZoomInterpSpeed variable of the Weapon class.
	 */
	UPROPERTY(EditAnywhere, Category = Combat)
	float ZoomInterpSpeed = 20.f;
	
	void InterpFOV(const float DeltaTime);
	
	/* 
	 * Automatic fire
	 */
	FTimerHandle FireTimer;
	
	bool bCanFire = true;
	void StartFireTimer();
	void FireTimerFinished();

	bool CanFire() const;
	
	// Carried ammo for the currently-equipped weapon
	UPROPERTY(ReplicatedUsing = OnRep_CarriedAmmo)
	int32 CarriedAmmo;
	
	UFUNCTION()
	void OnRep_CarriedAmmo();
	
	TMap<EWeaponType, int32> CarriedAmmoMap;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true, ClampMin = 0))
	int32 MaxAmmo = 500;
	
	UPROPERTY(EditAnywhere)
	int32 StartingARAmmo = 30;
	
	UPROPERTY(EditAnywhere)
	int32 StartingRocketAmmo = 2;
	
	UPROPERTY(EditAnywhere, meta = (ClampMin = 0))
	int32 StartingPistolAmmo = 10;
	
	UPROPERTY(EditAnywhere, meta = (ClampMin = 0))
	int32 StartingSMGAmmo = 30;
	
	UPROPERTY(EditAnywhere, meta = (ClampMin = 0))
	int32 StartingShotgunAmmo = 5;
	
	UPROPERTY(EditAnywhere, meta = (ClampMin = 0))
	int32 StartingSniperAmmo = 6;
	
	UPROPERTY(EditAnywhere, meta = (ClampMin = 0))
	int32 StartingGrenadeLauncherAmmo = 6;
	
	void InitializeCarriedAmmo();
	
	UPROPERTY(ReplicatedUsing = OnRep_CombatState, VisibleAnywhere)
	ECombatState CombatState = ECombatState::ECS_Unoccupied;
	
	UFUNCTION()
	void OnRep_CombatState();
	
	void UpdateAmmoValues();
	void UpdateShotgunAmmoValues();
	
	UPROPERTY(ReplicatedUsing = OnRep_Grenades, VisibleAnywhere)
	int32 Grenades = 4;
	
	UFUNCTION()
	void OnRep_Grenades();
	
	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = 0))
	int32 MaxGrenades = 12;
	
	void UpdateHUDGrenades();
};
