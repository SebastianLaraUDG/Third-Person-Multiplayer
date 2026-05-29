// Sebastian Lara. All rights reserved.


#include "JumpPickup.h"

#include "Blaster/BlasterComponents/BuffComponent.h"

void AJumpPickup::OnBeginOverlapCallback(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
                                         UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (const auto OtherBuff = OtherActor->FindComponentByClass<UBuffComponent>())
	{
		OtherBuff->BuffJump(JumpZVelocityBuff, JumpBuffDuration);
	}
	Super::OnBeginOverlapCallback(OverlappedComponent, OtherActor, OtherComp, OtherBodyIndex, bFromSweep, SweepResult);
}
