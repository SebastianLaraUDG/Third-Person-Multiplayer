// Sebastian Lara. All rights reserved.


#include "HostLobbyGameMode.h"

#include "Blaster/PlayerController/LobbyPlayerController.h"
#include "GameFramework/GameStateBase.h"

AHostLobbyGameMode::AHostLobbyGameMode()
{
	bUseSeamlessTravel = true;
}

void AHostLobbyGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	
	if (bHasShownHostWidget || !NewPlayer->IsLocalController()) return;
	
	if (auto LobbyPlayerController = Cast<ALobbyPlayerController>(NewPlayer))
	{
		LobbyPlayerController->ClientShowHostWidget();
	}
	
	bHasShownHostWidget = true;
}

void AHostLobbyGameMode::TryStartMatch()
{
	UE_LOG(LogGameMode, Warning, TEXT("Level path: %s | IsNull: %d"), *Level.ToString(), Level.IsNull() ? 1 : 0);
	UE_LOG(LogGameMode, Warning, TEXT("PackageName: %s"), *Level.ToSoftObjectPath().GetLongPackageName());
	
	if (Level.IsNull())
	{
		UE_LOG(LogGameMode, Error, TEXT("ERROR: Level is null. %s"), *GetNameSafe(this))
		return;
	}
	
	const int32 Players = GetWorld()->GetGameState()->PlayerArray.Num();
	if (Players < 2)
	{
		UE_LOG(LogGameMode, Display, TEXT("ERROR: Match should contain at least 2 players. NOT ENOUGH PLAYERS. %s"), *GetNameSafe(this))
		return;
	}
	
	bUseSeamlessTravel = true;
	if (auto World = GetWorld())
	{
		World->ServerTravel(Level.ToSoftObjectPath().GetLongPackageName()); // "%s?listen is not needed here since Lobby level (host button) already uses this prefix and therefore becomes a listen server.
	}
}
