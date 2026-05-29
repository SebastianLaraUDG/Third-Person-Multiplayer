// Sebastian Lara. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BuffComponent.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogBuffs, Log, All);

class USoundCue;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class BLASTER_API UBuffComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBuffComponent();
	virtual void BeginPlay() override;
	
	// ----------------------------------------------------------------------------------------------------------------------------
	// Speed related.
	void SetInitialSpeeds(const float BaseSpeed, const float CrouchSpeed);
	
	// This sets the speed to the corresponding value (not addition).
	// Setting Duration to a negative value will be clamped to zero.
	UFUNCTION(BlueprintCallable)
	void BuffSpeed(const float BuffBaseSpeed, const float BuffCrouchSpeed, const float BuffSpeedDuration);
	// ----------------------------------------------------------------------------------------------------------------------------
	
	
	// ----------------------------------------------------------------------------------------------------------------------------
	// Jump related.
	void SetInitialJumpVelocity(const float Velocity);
	
	// This sets the jump velocity (not addition).
	// Setting Duration to a negative value will be clamped to zero.
	UFUNCTION(BlueprintCallable)
	void BuffJump(const float Velocity, const float JumpBuffDuration);
	// ----------------------------------------------------------------------------------------------------------------------------
	
	UPROPERTY()
	TObjectPtr<ACharacter> Character;
private:
	
	UFUNCTION(Client, Reliable)
	void ClientPlayLocalSound(USoundBase* Sound) const;
	
	// ----------------------------------------------------------------------------------------------------------------------------
	// Speed related.
	float InitialBaseSpeed;
	float InitialCrouchSpeed;
	FTimerHandle SpeedBuffTimer;
	
	void SetMovementSpeeds(const float BaseSpeed, const float CrouchSpeed) const;
	
	UFUNCTION(NetMulticast, Reliable)
	void MulticastSpeedBuff(const float BaseSpeed, const float CrouchSpeed) const;
	
	// Sound to play when the speed buff ends.
	UPROPERTY(EditAnywhere)
	TObjectPtr<USoundCue> SpeedBuffEndSound;
	// ----------------------------------------------------------------------------------------------------------------------------
	
	
	// ----------------------------------------------------------------------------------------------------------------------------
	// Jump related.
	FTimerHandle JumpBuffTimer;
	float InitialJumpZVelocity;
	
	void SetJumpVelocity(const float JumpVelocity) const;
	
	UFUNCTION(NetMulticast, Reliable)
	void MulticastJumpBuff(const float JumpVelocity) const;
	
	// Sound to play when the jump buff ends.
	UPROPERTY(EditAnywhere)
	TObjectPtr<USoundCue> JumpBuffEndSound;
	// ----------------------------------------------------------------------------------------------------------------------------
};
