// Sebastian Lara. All rights reserved.


#include "Pickup.h"

#include "Components/SphereComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundCue.h"

APickup::APickup()
{
	PrimaryActorTick.bCanEverTick = false;
	
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Pickup root")));
	
	SphereComponent = CreateDefaultSubobject<USphereComponent>(TEXT("Sphere Comp"));
	SphereComponent->SetupAttachment(RootComponent);
	SphereComponent->SetCollisionEnabled(ECollisionEnabled::Type::QueryOnly);
	SphereComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	SphereComponent->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn,ECR_Overlap);
	SphereComponent->SetSphereRadius(150.f);
	
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Pickup Mesh"));
	Mesh->SetupAttachment(SphereComponent);
	Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	Mesh->SetRenderCustomDepth(true);
	Mesh->SetCustomDepthStencilValue(250);
	
	SetReplicates(true);
}

void APickup::Destroyed()
{
	Super::Destroyed();
	
	UGameplayStatics::PlaySoundAtLocation(this, PickupSound, GetActorLocation());
}

void APickup::BeginPlay()
{
	Super::BeginPlay();
	
	if (HasAuthority())
	{
		SphereComponent->OnComponentBeginOverlap.AddUniqueDynamic(this, &ThisClass::OnBeginOverlapCallback); // testing with Unique for the first time, not for a specific reason.
	}
}

void APickup::OnBeginOverlapCallback(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
}
