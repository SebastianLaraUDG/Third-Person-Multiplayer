// Sebastian Lara. All rights reserved.


#include "SpeedPickup.h"

#include "Blaster/BlasterComponents/BuffComponent.h"

void ASpeedPickup::OnBeginOverlapCallback(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
                                          UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (auto OtherBuffComp = OtherActor->FindComponentByClass<UBuffComponent>())
	{
		OtherBuffComp->BuffSpeed(BaseSpeedBuff, CrouchSpeedBuff, SpeedBuffDuration);
	}
	Super::OnBeginOverlapCallback(OverlappedComponent, OtherActor, OtherComp, OtherBodyIndex, bFromSweep, SweepResult);
}
