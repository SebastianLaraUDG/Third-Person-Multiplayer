// Sebastian Lara. All rights reserved.


#include "AmmoPickup.h"

#include "Blaster/BlasterComponents/CombatComponent.h"

void AAmmoPickup::OnBeginOverlapCallback(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
                                         UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (auto OtherCombatComponent = OtherActor->FindComponentByClass<UCombatComponent>())
	{
		OtherCombatComponent->PickupAmmo(WeaponType, AmmoAmount);
	}
	if (AmmoDataMap.Contains(WeaponType))
	{
		SoftPickupSound = AmmoDataMap[WeaponType].Sound;
	}
	
	Super::OnBeginOverlapCallback(OverlappedComponent, OtherActor, OtherComp, OtherBodyIndex, bFromSweep, SweepResult);
}

// TODO: rotating movement component here or in parent?