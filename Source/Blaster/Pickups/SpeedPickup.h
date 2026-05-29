// Sebastian Lara. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Pickup.h"
#include "SpeedPickup.generated.h"

/**
 * 
 */
UCLASS()
class BLASTER_API ASpeedPickup : public APickup
{
	GENERATED_BODY()
protected:
	virtual void OnBeginOverlapCallback(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) override;
private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Movement", meta = (AllowPrivateAccess = "true"))
	float BaseSpeedBuff = 1900.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Movement", meta = (AllowPrivateAccess = "true"))
	float CrouchSpeedBuff = 700.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Movement", meta = (AllowPrivateAccess = "true", ClampMin = 0, ForceUnits = Seconds))
	float SpeedBuffDuration = 4.f;
};
