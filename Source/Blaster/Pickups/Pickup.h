// Sebastian Lara. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Pickup.generated.h"

class USphereComponent;

/*
 * Pickup actor base class.
 * This actor destroys when overlapping with a pawn and plays a 3D sound.
 */
UCLASS(Abstract)
class BLASTER_API APickup : public AActor
{
	GENERATED_BODY()
	
public:	
	APickup();
	virtual void Destroyed() override;
protected:
	virtual void BeginPlay() override;
	
	// This calls Destroy so you should only use at the end.
	UFUNCTION()
	virtual void OnBeginOverlapCallback(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	
protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<USphereComponent> SphereComponent;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UStaticMeshComponent> Mesh;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
	TSoftObjectPtr<USoundCue> SoftPickupSound;
};
