// Sebastian Lara. All rights reserved.


#include "HealthPickup.h"

#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Blaster/BlasterComponents/HealthComponent.h"


AHealthPickup::AHealthPickup()
{
	PrimaryActorTick.bCanEverTick = false;
	
	bReplicates = true;
	
	PickupEffectComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("Pickup Effect Comp"));
	PickupEffectComponent->SetupAttachment(RootComponent);
}
void AHealthPickup::Destroyed()
{
	UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, PickupEffect, GetActorLocation(), GetActorRotation());
	Super::Destroyed();
}


void AHealthPickup::OnBeginOverlapCallback(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (auto HealthComp = OtherActor->FindComponentByClass<UHealthComponent>())
	{
		HealthComp->StartHealing(HealAmount, HealTime);
	}
	Super::OnBeginOverlapCallback(OverlappedComponent, OtherActor, OtherComp, OtherBodyIndex, bFromSweep, SweepResult);
}