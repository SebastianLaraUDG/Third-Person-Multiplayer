// Sebastian Lara. All rights reserved.


#include "BlasterCharacter.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "Blaster/BlasterComponents/CombatComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "Blaster/Weapon/Weapon.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "BlasterAnimInstance.h"
#include "Blaster/Blaster.h"
#include "Blaster/BlasterComponents/HealthComponent.h"
#include "Blaster/GameMode/BlasterGameMode.h"
#include "Blaster/PlayerController/BlasterPlayerController.h"
#include "TimerManager.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Blaster/BlasterComponents/BuffComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundCue.h"

#include "Blaster/PlayerState/BlasterPlayerState.h"

ABlasterCharacter::ABlasterCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// Create camera and SpringArm.
	SpringArmComponent = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	SpringArmComponent->SetupAttachment(GetMesh());
	SpringArmComponent->TargetArmLength = 600.0f;
	SpringArmComponent->bUsePawnControlRotation = true;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	Camera->SetupAttachment(SpringArmComponent, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false;

	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	// Make sure character can crouch.
	GetCharacterMovement()->GetNavAgentPropertiesRef().bCanCrouch = true;
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetMesh()->SetCollisionObjectType(ECC_SkeletalMesh);
	GetMesh()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	// Block on visibility channel to allow for crosshairs change to red color.
	GetCharacterMovement()->RotationRate = FRotator(0.f, 850.f, 0.f);

	// Overhead Widget.
	OverheadWidgetComp = CreateDefaultSubobject<UWidgetComponent>(TEXT("OverheadWidget"));
	OverheadWidgetComp->SetupAttachment(RootComponent);

	// Combat component.
	CombatComponent = CreateDefaultSubobject<UCombatComponent>(TEXT("CombatComponent"));
	CombatComponent->SetIsReplicated(true);

	TurningInPlace = ETurningInPlace::ETIP_NotTurning;
	SetNetUpdateFrequency(66.f);
	SetMinNetUpdateFrequency(33.f);

	// Health component.
	HealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));
	HealthComponent->SetIsReplicated(true);
	
	// Buff component.
	BuffComponent = CreateDefaultSubobject<UBuffComponent>(TEXT("BuffComponent"));
	BuffComponent->SetIsReplicated(true);

	// Always spawn. This is because there was an issue when a character should spawn but did not appear maybe because overlapping with something.
	SpawnCollisionHandlingMethod = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	// Dissolve Timeline component.
	DissolveTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("DissolveTimeline Component"));
	
	// Grenade mesh.
	GrenadeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GrenadeMesh"));
	GrenadeMesh->SetupAttachment(GetMesh(), FName("GrenadeSocket"));
	GrenadeMesh->SetCollisionEnabled(ECollisionEnabled::Type::NoCollision);
}

void ABlasterCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	RotateInPlace(DeltaTime);
	HideCharacterIfCameraClose();
	// Always update score and defeat text on hud. DEBUG
	if (auto BlasterPlayerState = GetPlayerState<ABlasterPlayerState>(); BlasterPlayerState && IsLocallyControlled())
	{
		if (BlasterPlayerState)
		{
			BlasterPlayerState->AddToScore(0.f);
			BlasterPlayerState->AddToDefeats(0);
		}
	}
}

void ABlasterCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	APlayerController* const PlayerController = Cast<APlayerController>(GetController());

	// Mapping context.
	if (UEnhancedInputLocalPlayerSubsystem* EnhancedInputSubsystem = ULocalPlayer::GetSubsystem<
		UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
	{
		EnhancedInputSubsystem->ClearAllMappings();
		EnhancedInputSubsystem->AddMappingContext(InputMappingContext, 0);

		// Equip Weapon Mapping context.
		EnhancedInputSubsystem->AddMappingContext(EquipWeaponMappingContext, 1);
		// Weapon combat mapping context.
		EnhancedInputSubsystem->AddMappingContext(WeaponCombatInputMappingContext, 2);
	}

	// Input actions.
	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EnhancedInput)
	{
		UE_LOG(LogTemp, Warning, TEXT("Enhanced Input Component is NULL in %s"), *GetNameSafe(this));
		return;
	}
	// Bind movement and camera rotation.
	EnhancedInput->BindAction(MoveInputAction, ETriggerEvent::Triggered, this, &ThisClass::Move);
	EnhancedInput->BindAction(TurnInputAction, ETriggerEvent::Triggered, this, &ThisClass::Turn);
	EnhancedInput->BindAction(JumpInputAction, ETriggerEvent::Started, this, &ThisClass::Jump);
	EnhancedInput->BindAction(JumpInputAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
	EnhancedInput->BindAction(CrouchInputAction, ETriggerEvent::Started, this, &ThisClass::HandleCrouchRequest);

	// Bind Equip Weapon.
	EnhancedInput->BindAction(EquipWeaponInputAction, ETriggerEvent::Triggered, this, &ThisClass::EquipButtonPressed);

	// Bind weapon combat.
	EnhancedInput->BindAction(AimInputAction, ETriggerEvent::Started, this, &ThisClass::AimStarted);
	EnhancedInput->BindAction(AimInputAction, ETriggerEvent::Completed, this, &ThisClass::AimStopped);
	EnhancedInput->BindAction(FireInputAction, ETriggerEvent::Started, this, &ThisClass::FireWeaponPressed);
	EnhancedInput->BindAction(FireInputAction, ETriggerEvent::Completed, this, &ThisClass::FireWeaponReleased);
	EnhancedInput->BindAction(ReloadInputAction, ETriggerEvent::Started, this, &ThisClass::ReloadButtonPressed);
	EnhancedInput->BindAction(ThrowGrenadeInputAction, ETriggerEvent::Started, this, &ThisClass::ThrowGrenadeButtonPressed);
}

void ABlasterCharacter::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Replicate overlapping weapon.
	DOREPLIFETIME_CONDITION(ABlasterCharacter, OverlappingWeapon, COND_OwnerOnly);
	DOREPLIFETIME(ABlasterCharacter, bDisableGameplay)
}

void ABlasterCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	if (CombatComponent)
	{
		CombatComponent->Character = this;
	}
	if (BuffComponent)
	{
		BuffComponent->Character = this;
	}
}

void ABlasterCharacter::PlayFireMontage(bool bAiming)
{
	if (!CombatComponent || !IsWeaponEquipped()) return;

	UAnimInstance* const AnimInstance = GetMesh()->GetAnimInstance();

	if (AnimInstance && FireWeaponMontage)
	{
		AnimInstance->Montage_Play(FireWeaponMontage);
		const FName SectionName = bAiming ? FName("RifleAim") : FName("RifleHip"); // TODO: make variables?
		AnimInstance->Montage_JumpToSection(SectionName);
	}
}

void ABlasterCharacter::PlayReloadMontage() const
{
	if (!CombatComponent || !CombatComponent->EquippedWeapon || !ReloadMontage) return;

	if (const auto AnimInstance = GetMesh()->GetAnimInstance())
	{
		AnimInstance->Montage_Play(ReloadMontage);
		FName SectionName;

		switch (CombatComponent->EquippedWeapon->GetWeaponType())
		{
		case EWeaponType::EWT_AssaultRifle: SectionName = FName("Rifle");
			break;
		case EWeaponType::EWT_RocketLauncher: SectionName = FName("RocketLauncher");
			// Specify section name if you implement new sections and animations.
			break;
		case EWeaponType::EWT_Pistol: SectionName = FName("Pistol");
			// Specify section name if you implement new sections and animations.
			break;
		case EWeaponType::EWT_SubmachineGun: SectionName = FName("Pistol");
			// Specify section name if you implement new sections and animations.
			break;
		case EWeaponType::EWT_Shotgun: SectionName = FName("Shotgun");
			// Specify section name if you implement new sections and animations.
			break;
		case EWeaponType::EWT_SniperRifle: SectionName = FName("SniperRifle");
			// Specify section name if you implement new sections and animations.
			break;
		case EWeaponType::EWT_GrenadeLauncher: SectionName = FName("GrenadeLauncher");
			// Specify section name if you implement new sections and animations.
			break;
		case EWeaponType::EWT_MAX: break;
		}
		AnimInstance->Montage_JumpToSection(SectionName);
	}
}

void ABlasterCharacter::PlayElimMontage() const
{
	/*
	UAnimInstance* const AnimInstance = GetMesh()->GetAnimInstance();
	if (ElimMontage && AnimInstance)
	{
		AnimInstance->Montage_Play(ElimMontage);
	}
	*/
	PlayMontage(ElimMontage);
}

void ABlasterCharacter::PlayThrowGrenadeMontage() const
{
	PlayMontage(ThrowGrenadeMontage);
}

void ABlasterCharacter::PlayMontage(UAnimMontage* const Montage) const
{
	UAnimInstance* const AnimInstance = GetMesh()->GetAnimInstance();
	if (Montage && AnimInstance)
	{
		AnimInstance->Montage_Play(Montage);
	}
}

void ABlasterCharacter::PlayHitReactMontage() const
{
	if (!CombatComponent || !IsWeaponEquipped()) return;
	if (CombatComponent->CombatState != ECombatState::ECS_Unoccupied)/*if (CombatComponent->CombatState == ECombatState::ECS_Reloading)*/ return; // There is the case where the character received damage while reloading (found this while receiving damage from a grenade explosion)
																			// so it started playing Hit react montage therefore aborting the reload montage so character would never be able to shoot or reload ever again.
																			// This happened again with the throw grenade animation.
	
	if (const auto AnimInstance = GetMesh()->GetAnimInstance(); AnimInstance && HitReactMontage)
	{
		AnimInstance->Montage_Play(HitReactMontage);
		const FName SectionName("FromFront"); // TEMP: add more sections to play here in code.
		AnimInstance->Montage_JumpToSection(SectionName);
	}
}

void ABlasterCharacter::OnRep_ReplicatedMovement()
{
	Super::OnRep_ReplicatedMovement();
	SimProxiesTurn();
	TimeSinceLastMovementReplication = 0.f;
}

void ABlasterCharacter::Destroyed()
{
	Super::Destroyed();

	// Destroy bot niagara after this character dies.
	if (ElimBotComponent)
	{
		ElimBotComponent->DestroyComponent();
	}

	const auto BlasterGameMode = Cast<ABlasterGameMode>(UGameplayStatics::GetGameMode(this));
	const bool bMatchNotInProgress = BlasterGameMode && BlasterGameMode->GetMatchState() != MatchState::InProgress;

	// If the character is holding a weapon when it gets destroyed, also destroy the weapon to
	// prevent the remaining weapon from floating in the air.
	if (CombatComponent && CombatComponent->EquippedWeapon && bMatchNotInProgress)
	{
		CombatComponent->EquippedWeapon->Destroy();
	}
}

void ABlasterCharacter::BeginPlay()
{
	Super::BeginPlay();

	// Bind health changed.
	// We don't bind it only on server to allow for HitMontage play for every character in all machines.
	if (HealthComponent)
	{
		HealthComponent->OnHealthChanged.AddUObject(this, &ABlasterCharacter::OnHealthChanged);
	}
	if (GrenadeMesh) // Hide attached grenade mesh.
	{
		GrenadeMesh->SetVisibility(false);
	}
	// Initialize HUD health values.
	UpdateHUD();
}

void ABlasterCharacter::Move(const FInputActionValue& Value)
{
	if (bDisableGameplay) return;

	const FVector2D Val = Value.Get<FVector2D>();

	if (Controller != nullptr && !Val.IsZero())
	{
		const FRotator YawRotation(0.0f, Controller->GetControlRotation().Yaw, 0.0f);
		const FVector ForwardDirection(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X));
		const FVector RightDirection(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y));
		AddMovementInput(ForwardDirection, Val.X);
		AddMovementInput(RightDirection, Val.Y);
	}
}

void ABlasterCharacter::Turn(const FInputActionValue& Value)
{
	if (bDisableGameplay) return;
	const FVector2D Val = Value.Get<FVector2D>();
	AddControllerYawInput(Val.X);
	// Negate Y axis.
	AddControllerPitchInput(-Val.Y);
}

void ABlasterCharacter::HandleCrouchRequest()
{
	if (bDisableGameplay) return;
	IsCrouched() ? UnCrouch() : Crouch();
}

void ABlasterCharacter::EquipButtonPressed()
{
	if (bDisableGameplay) return;

	if (CombatComponent)
	{
		// Case: Called from the server.
		if (HasAuthority())
		{
			CombatComponent->EquipWeapon(OverlappingWeapon);
		}
		// Case: called from a client.
		else
		{
			ServerEquipButtonPressed();
		}
	}
}

void ABlasterCharacter::AimStarted()
{
	if (bDisableGameplay) return;
	if (CombatComponent)
	{
		CombatComponent->SetAiming(true);
	}
}

void ABlasterCharacter::AimStopped()
{
	if (bDisableGameplay) return;
	if (CombatComponent)
	{
		CombatComponent->SetAiming(false);
	}
}

void ABlasterCharacter::FireWeaponPressed()
{
	if (bDisableGameplay) return;
	if (CombatComponent)
	{
		CombatComponent->FireButtonPressed(true);
		UE_LOG(LogTemp, Display, TEXT("Character started trying to shoot."));
	}
}

void ABlasterCharacter::FireWeaponReleased()
{
	if (bDisableGameplay) return;
	if (CombatComponent)
	{
		CombatComponent->FireButtonPressed(false);
		UE_LOG(LogTemp, Display, TEXT("Character stopped shooting."));
	}
}

void ABlasterCharacter::ReloadButtonPressed()
{
	if (bDisableGameplay) return;
	if (CombatComponent)
	{
		CombatComponent->Reload();
	}
}

void ABlasterCharacter::ThrowGrenadeButtonPressed()
{
	if (bDisableGameplay) return;
	if (CombatComponent)
	{
		CombatComponent->ThrowGrenade();
	}
}

float ABlasterCharacter::CalculateSpeed() const
{
	FVector Velocity = GetVelocity();
	Velocity.Z = 0.f;
	return Velocity.Size();
}

void ABlasterCharacter::AimOffset(float DeltaTime)
{
	if (!IsWeaponEquipped()) return;

	const float Speed = CalculateSpeed();
	const bool bIsInAir = GetCharacterMovement()->IsFalling();

	if (Speed == 0.f && !bIsInAir) // Standing still, not jumping.
	{
		bRotateRootBone = true;
		FRotator CurrentRotation = FRotator(0.f, GetBaseAimRotation().Yaw, 0.f);
		FRotator DeltaAimRotation = UKismetMathLibrary::NormalizedDeltaRotator(CurrentRotation, StartingAimRotation);
		AO_Yaw = DeltaAimRotation.Yaw;

		if (TurningInPlace == ETurningInPlace::ETIP_NotTurning) // Not turning.
		{
			InterpAO_Yaw = AO_Yaw;
		}
		bUseControllerRotationYaw = true;
		TurnInPlace(DeltaTime);
	}
	if (Speed > 0.f || bIsInAir) // Running or jumping
	{
		bRotateRootBone = false;
		StartingAimRotation = FRotator(0.f, GetBaseAimRotation().Yaw, 0.f);
		AO_Yaw = 0.f;
		bUseControllerRotationYaw = true;
		TurningInPlace = ETurningInPlace::ETIP_NotTurning;
	}
	CalculateAO_Pitch();
}

void ABlasterCharacter::CalculateAO_Pitch()
{
	AO_Pitch = GetBaseAimRotation().Pitch;

	// Convert compressed value to a valid range [-90,90].
	if (AO_Pitch > 90.f && !IsLocallyControlled())
	{
		// Map pitch from the range [270,360) to [-90,0)
		const FVector2D InRange(270.f, 360.f);
		const FVector2D OutRange(-90.f, 0.f);
		AO_Pitch = FMath::GetMappedRangeValueClamped(InRange, OutRange, AO_Pitch);
	}
}

void ABlasterCharacter::SimProxiesTurn()
{
	// if (!CombatComponent || !CombatComponent->EquippedWeapon) return;
	if (!IsWeaponEquipped()) return;

	// Simulated proxies do not rotate bones.
	bRotateRootBone = false;

	// Is moving.
	if (CalculateSpeed() > 0.f)
	{
		TurningInPlace = ETurningInPlace::ETIP_NotTurning;
		return;
	}

	ProxyRotationLastFrame = ProxyRotation;
	ProxyRotation = GetActorRotation();
	// Delta Yaw rotation between frames.
	const float ProxyYaw = UKismetMathLibrary::NormalizedDeltaRotator(ProxyRotation, ProxyRotationLastFrame).Yaw;

	// Delta rotation exceeds threshold.
	if (FMath::Abs(ProxyYaw) > TurnThreshold)
	{
		// Turned right.
		if (ProxyYaw > TurnThreshold)
		{
			TurningInPlace = ETurningInPlace::ETIP_Right;
		}
		// Turned left.
		else if (ProxyYaw < -TurnThreshold)
		{
			TurningInPlace = ETurningInPlace::ETIP_Left;
		}
		// Not turning.
		else
		{
			TurningInPlace = ETurningInPlace::ETIP_NotTurning;
		}
		return;
	}
	TurningInPlace = ETurningInPlace::ETIP_NotTurning;
}

void ABlasterCharacter::Jump()
{
	if (bDisableGameplay) return;
	if (bIsCrouched)
	{
		UnCrouch();
	}
	else
	{
		Super::Jump();
	}
}

void ABlasterCharacter::RotateInPlace(float DeltaTime)
{
	if (bDisableGameplay)
	{
		// Disable rotation in place.
		bUseControllerRotationYaw = false;
		TurningInPlace = ETurningInPlace::ETIP_NotTurning;
		return;
	}

	/*
	 * Aim offsets for both local players and simulated proxies.
	 * 
	 */

	// Only autonomous and authority proxies controlled by a player will calculate aim offsets.
	if (GetLocalRole() > ROLE_SimulatedProxy && IsLocallyControlled())
	{
		AimOffset(DeltaTime);
	}
	// Simulated proxies will update their Aim offsets every certain amount of time determined regardless of whether they moved or not.
	else
	{
		TimeSinceLastMovementReplication += DeltaTime;
		if (TimeSinceLastMovementReplication > TimeForMovementReplicationUpdate)
		{
			OnRep_ReplicatedMovement();
		}
		CalculateAO_Pitch();
	}
}

void ABlasterCharacter::TurnInPlace(float DeltaTime)
{
	if (AO_Yaw > 90.f)
	{
		TurningInPlace = ETurningInPlace::ETIP_Right;
	}
	else if (AO_Yaw < -90.f)
	{
		TurningInPlace = ETurningInPlace::ETIP_Left;
	}

	if (TurningInPlace != ETurningInPlace::ETIP_NotTurning) // We are turning.
	{
		InterpAO_Yaw = FMath::FInterpTo(InterpAO_Yaw, 0.f, DeltaTime, TurningInterpolationSpeed);
		AO_Yaw = InterpAO_Yaw;
		if (FMath::Abs(AO_Yaw) < 15.0f)
		{
			TurningInPlace = ETurningInPlace::ETIP_NotTurning;
			// Reset Starting Aim Rotation.
			StartingAimRotation = FRotator(0.f, GetBaseAimRotation().Yaw, 0.f);
		}
	}
}

void ABlasterCharacter::HideCharacterIfCameraClose() const
{
	if (!IsLocallyControlled()) return;

	const bool bCameraTooCloseToCharacter = (Camera->GetComponentLocation() - GetActorLocation()).Size() <
		CameraThreshold;

	// In case camera is too close to character, mesh must not be visible.
	GetMesh()->SetVisibility(!bCameraTooCloseToCharacter);

	// Weapon has a valid mesh.
	if (CombatComponent && CombatComponent->EquippedWeapon && CombatComponent->EquippedWeapon->GetMesh())
	{
		// In case camera is too close to character, character should not see weapon.
		CombatComponent->EquippedWeapon->GetMesh()->SetOwnerNoSee(bCameraTooCloseToCharacter);
	}
}

void ABlasterCharacter::OnHealthChanged(float NewHealth, float DeltaHealth, AController* InstigatorController)
{
	UpdateHUD();
	
	if (DeltaHealth < 0) // Play only when receiving damage (a positive value means "healing").
	{
		PlayHitReactMontage();
	}
	
	const auto BlasterGameMode = GetWorld()->GetAuthGameMode<ABlasterGameMode>();

	// Register death in game mode.
	if (HealthComponent->GetCurrentHealth() == 0.f && BlasterGameMode)
	{
		if (!BlasterPlayerController)
		{
			BlasterPlayerController = Cast<ABlasterPlayerController>(Controller);
		}
		const auto AttackerController = Cast<ABlasterPlayerController>(InstigatorController
		);
		BlasterGameMode->PlayerEliminated(this,
		                                  BlasterPlayerController ? BlasterPlayerController : nullptr,
		                                  AttackerController ? AttackerController : nullptr
		);
	}
}


void ABlasterCharacter::UpdateHUD()
{
	// Only local players need to update and see HUD.
	if (!IsLocallyControlled()) return;

	// Make sure controller is valid.
	if (!BlasterPlayerController)
	{
		BlasterPlayerController = Cast<ABlasterPlayerController>(Controller);
	}
	if (!BlasterPlayerController) return;
	// Double check to avoid issues with server travel. TODO: improve both readability and process itself.
	BlasterPlayerController->SetHUDHealth(HealthComponent->GetCurrentHealth(), HealthComponent->GetMaxHealth());
}

void ABlasterCharacter::Elim()
{
	// Drop weapon.
	if (CombatComponent && CombatComponent->EquippedWeapon)
	{
		CombatComponent->EquippedWeapon->Drop();
	}
	MulticastElim();

	// Start respawn timer.
	GetWorldTimerManager().SetTimer(ElimTimer, this, &ABlasterCharacter::ElimTimerFinished, ElimDelay);
}

void ABlasterCharacter::ElimTimerFinished()
{
	if (const auto BlasterGameMode = GetWorld()->GetAuthGameMode<ABlasterGameMode>())
	{
		BlasterGameMode->RequestRespawn(this, Controller);
	}
}

void ABlasterCharacter::UpdateDissolveMaterial(float DissolveValue)
{
	if (DynamicDissolveMaterialInstance)
	{
		DynamicDissolveMaterialInstance->SetScalarParameterValue(TEXT("Dissolve"), DissolveValue);
		// This value is hard coded to the material of this project. Make sure name is correct.
	}
}

void ABlasterCharacter::StartDissolve()
{
	DissolveTrack.BindDynamic(this, &ABlasterCharacter::UpdateDissolveMaterial);
	if (DissolveCurve && DissolveTimeline)
	{
		DissolveTimeline->AddInterpFloat(DissolveCurve, DissolveTrack);
		DissolveTimeline->Play();
	}
}

void ABlasterCharacter::MulticastElim_Implementation()
{
	if (BlasterPlayerController)
	{
		// Reset ammo text to zero.
		BlasterPlayerController->SetHUDWeaponAmmo(0);
		// Clean equipped weapon name.
		BlasterPlayerController->SetHUDEquippedWeaponName(EWeaponType::EWT_MAX);
	}
	PlayElimMontage();

	/* Start dissolve effect. */

	// Set the dynamic dissolve material for each mesh material.
	if (DissolveMaterialInstance)
	{
		DynamicDissolveMaterialInstance = UMaterialInstanceDynamic::Create(DissolveMaterialInstance, this);
		for (auto Index = 0; Index < GetMesh()->GetNumMaterials(); ++Index)
		{
			GetMesh()->SetMaterial(Index, DynamicDissolveMaterialInstance);
			DynamicDissolveMaterialInstance->
				SetScalarParameterValue(TEXT("Dissolve"), 0.55f /*Completely dissolved material. */);
			// These values are hard coded to the material of this project. Make sure names are correct.
			DynamicDissolveMaterialInstance->SetScalarParameterValue(TEXT("Glow"), 200.f);
		}
	}
	// and start the dissolving effect.
	StartDissolve();
	// Disable character movement.
	GetCharacterMovement()->DisableMovement();
	GetCharacterMovement()->StopMovementImmediately();

	// Disable character movement
	// and stop firing.
	bDisableGameplay = true;
	if (CombatComponent)
	{
		CombatComponent->FireButtonPressed(false);
	}

	// Disable collision.
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Spawn elim bot.
	if (ElimBotEffect)
	{
		const FVector ElimBotSpawnPoint(GetActorLocation().X, GetActorLocation().Y, GetActorLocation().Z + 200.f);
		// Spawn location is 2 meters above character.
		ElimBotComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			this, ElimBotEffect, ElimBotSpawnPoint, GetActorRotation());
	}
	if (ElimBotSound)
	{
		UGameplayStatics::SpawnSoundAtLocation(this, ElimBotSound, GetActorLocation());
	}
	
	// Remove sniper rifle aiming on death.
	const bool bHideSniperScope = IsLocallyControlled() && 
		CombatComponent && 
		CombatComponent->bIsAiming && 
		CombatComponent->EquippedWeapon && 
		CombatComponent->EquippedWeapon->GetWeaponType() == EWeaponType::EWT_SniperRifle;
	if (bHideSniperScope)
	{
		BlasterPlayerController->SetHUDSniperScope(false);
	}
}

void ABlasterCharacter::OnRep_OverlappingWeapon(AWeapon* LastWeapon)
{
	if (OverlappingWeapon)
	{
		OverlappingWeapon->ShowPickupWidget(true);
	}
	// Hide widget if end overlap.
	if (LastWeapon)
	{
		LastWeapon->ShowPickupWidget(false);
	}
}

void ABlasterCharacter::ServerEquipButtonPressed_Implementation()
{
	if (CombatComponent)
	{
		CombatComponent->EquipWeapon(OverlappingWeapon);
	}
}

bool ABlasterCharacter::IsWeaponEquipped() const
{
	return (CombatComponent && CombatComponent->EquippedWeapon);
}

bool ABlasterCharacter::IsAiming() const
{
	return (CombatComponent && CombatComponent->bIsAiming);
}

void ABlasterCharacter::SetOverlappingWeapon(AWeapon* Weapon)
{
	if (OverlappingWeapon)
	{
		OverlappingWeapon->ShowPickupWidget(false);
	}
	OverlappingWeapon = Weapon;
	if (IsLocallyControlled() && OverlappingWeapon)
	{
		OverlappingWeapon->ShowPickupWidget(true);
	}
}

AWeapon* ABlasterCharacter::GetEquippedWeapon() const
{
	return CombatComponent ? CombatComponent->EquippedWeapon : nullptr;
}

FVector ABlasterCharacter::GetHitTarget() const
{
	return CombatComponent ? CombatComponent->HitTarget : FVector();
}

ECombatState ABlasterCharacter::GetCombatState() const
{
	if (!CombatComponent) return ECombatState::ECS_MAX;
	return CombatComponent->CombatState;
}
