// Sebastian Lara. All rights reserved.


#include "BuffComponent.h"

UBuffComponent::UBuffComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	
	SetIsReplicatedByDefault(true);
}

void UBuffComponent::BeginPlay()
{
	Super::BeginPlay();
	
}