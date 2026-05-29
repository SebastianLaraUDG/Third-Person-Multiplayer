// Sebastian Lara. All rights reserved.


#include "BuffComponent.h"

#include "CombatComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundCue.h"

DEFINE_LOG_CATEGORY(LogBuffs)

UBuffComponent::UBuffComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	
	SetIsReplicatedByDefault(true);
}

void UBuffComponent::BeginPlay()
{
	Super::BeginPlay();
	
}

//~ Begin Speed Buff section.

void UBuffComponent::SetInitialSpeeds(const float BaseSpeed, const float CrouchSpeed)
{
	InitialBaseSpeed = BaseSpeed;
	InitialCrouchSpeed = CrouchSpeed;
	UE_LOG(LogBuffs, Display, TEXT("Initial Speeds: %f Base. %f crouched"), InitialBaseSpeed, InitialCrouchSpeed)
}

void UBuffComponent::BuffSpeed(const float BuffBaseSpeed, const float BuffCrouchSpeed, const float BuffSpeedDuration)
{
	if (!Character || !Character->GetCharacterMovement())
	{
		UE_LOG(LogBuffs, Error, TEXT("Tried to buff speed but either character or character movement is null. Files: %s"), *GetNameSafe(this));
		return;
	}
	UE_LOG(LogBuffs, Display, TEXT("Buff speed started"))
	
	// Set speeds.
	SetMovementSpeeds(BuffBaseSpeed, BuffCrouchSpeed);
	MulticastSpeedBuff(BuffBaseSpeed, BuffCrouchSpeed);
	
	// Start timer.
	FTimerDelegate ResetSpeedsDelegate = FTimerDelegate::CreateLambda(
	[this]
	{
		Character->GetCharacterMovement()->MaxWalkSpeed = InitialBaseSpeed;
		Character->GetCharacterMovement()->MaxWalkSpeedCrouched = InitialCrouchSpeed;
		UE_LOG(LogBuffs, Display, TEXT(" Restored Initial Speeds: %f Base. %f crouched"), Character->GetCharacterMovement()->MaxWalkSpeed, Character->GetCharacterMovement()->MaxWalkSpeedCrouched)
		MulticastSpeedBuff(InitialBaseSpeed, InitialCrouchSpeed);
		UE_LOG(LogBuffs, Display, TEXT("Speed back to normal"))
		ClientPlayLocalSound(SpeedBuffEndSound);
	});
	const float BuffDuration = BuffSpeedDuration < 0.f ? 0.0f : BuffSpeedDuration;
	UE_LOG(LogBuffs,Display,TEXT("Calculated duration: %f"),BuffDuration);
	Character->GetWorldTimerManager().SetTimer(SpeedBuffTimer, ResetSpeedsDelegate, BuffDuration,false);
}

void UBuffComponent::SetInitialJumpVelocity(const float Velocity)
{
	InitialJumpZVelocity = Velocity;
}

void UBuffComponent::BuffJump(const float Velocity, const float JumpBuffDuration)
{
	if (!Character || !Character->GetCharacterMovement())
	{
		UE_LOG(LogBuffs, Error, TEXT("Tried to buff JUMP but either character or character movement is null. Files: %s"), *GetNameSafe(this));
		return;
	}
	UE_LOG(LogBuffs, Display, TEXT("Buff Jump started"))
	
	// Set Velocity.
	SetJumpVelocity(Velocity);
	MulticastJumpBuff(Velocity);
	
	// Start timer.
	FTimerDelegate ResetJumpDelegate = FTimerDelegate::CreateLambda(
		[this]
		{
			Character->GetCharacterMovement()->JumpZVelocity = InitialJumpZVelocity;
		UE_LOG(LogBuffs, Display, TEXT(" Restored Initial JUMP VEL: %f "), Character->GetCharacterMovement()->JumpZVelocity)
			MulticastJumpBuff(InitialJumpZVelocity);
			UE_LOG(LogBuffs, Display, TEXT("JUMP VEL back to normal"))
			ClientPlayLocalSound(JumpBuffEndSound);
		});
	const float BuffDuration = JumpBuffDuration < 0.f ? 0.0f : JumpBuffDuration;
	UE_LOG(LogBuffs,Display,TEXT("Calculated duration: %f"), BuffDuration);
	Character->GetWorldTimerManager().SetTimer(JumpBuffTimer, ResetJumpDelegate, BuffDuration, false);
}

void UBuffComponent::SetMovementSpeeds(const float BaseSpeed, const float CrouchSpeed) const
{
	if (Character && Character->GetCharacterMovement())
	{
		Character->GetCharacterMovement()->MaxWalkSpeed = BaseSpeed;
		Character->GetCharacterMovement()->MaxWalkSpeedCrouched = CrouchSpeed;
		UE_LOG(LogBuffs, Display, TEXT("Buffed speeds. Curr value: %f BASE, %f Crouch"), Character->GetCharacterMovement()->MaxWalkSpeed, Character->GetCharacterMovement()->MaxWalkSpeedCrouched)
	}
}

void UBuffComponent::ClientPlayLocalSound_Implementation(USoundBase* Sound) const
{
	if (Sound)
	{
		UGameplayStatics::PlaySound2D(this, Sound);
	}
}

void UBuffComponent::MulticastSpeedBuff_Implementation(const float BaseSpeed, const float CrouchSpeed) const
{
	SetMovementSpeeds(BaseSpeed, CrouchSpeed);
	
	if (Character)
	{
		if (const auto CombatComponent = Character->FindComponentByClass<UCombatComponent>())
		{
			CombatComponent->SetSpeeds(BaseSpeed, CrouchSpeed);
		}
	}
}

//~ End Speed Buff section.



//~ Begin Jump Buff section.
void UBuffComponent::SetJumpVelocity(const float JumpVelocity) const
{
	if (Character && Character->GetCharacterMovement())
	{
		Character->GetCharacterMovement()->JumpZVelocity = JumpVelocity;
	}
}

void UBuffComponent::MulticastJumpBuff_Implementation(const float JumpVelocity) const
{
	if (Character && Character->GetCharacterMovement())
	SetJumpVelocity(JumpVelocity);
}

//~ End Jump Buff section.
