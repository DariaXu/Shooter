// Fill out your copyright notice in the Description page of Project Settings.


#include "ShooterGameMode.h"
#include "Shooter/Character/ShooterCharacter.h"
#include "Shooter/PlayerController/ShooterPlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerStart.h"
#include "Shooter/PlayerState/ShooterPlayerState.h"

void AShooterGameMode::PlayerEliminated(AShooterCharacter *ElimmedCharacter, AShooterPlayerController *VictimController, AShooterPlayerController *AttackerController)
{
// 	if (AttackerController == nullptr || AttackerController->PlayerState == nullptr) return;
// 	if (VictimController == nullptr || VictimController->PlayerState == nullptr) return;

	AShooterPlayerState* AttackerPlayerState = AttackerController ? Cast<AShooterPlayerState>(AttackerController->PlayerState) : nullptr;
	AShooterPlayerState* VictimPlayerState = VictimController ? Cast<AShooterPlayerState>(VictimController->PlayerState) : nullptr;

	if (AttackerPlayerState && AttackerPlayerState != VictimPlayerState)
	{
		AttackerPlayerState->AddToScore(1.f);
	}

	if (VictimPlayerState)
	{
		// UE_LOG(LogTemp, Warning, TEXT("Defeat"));
		VictimPlayerState->AddToDefeats(1);
	}

    if (ElimmedCharacter)
	{
		ElimmedCharacter->Elim();
	}
}

void AShooterGameMode::RequestRespawn(ACharacter *ElimmedCharacter, AController *ElimmedController)
{
    if (ElimmedCharacter)
	{
        // detach from the controller, to allow the controller to repossess another one 
		ElimmedCharacter->Reset();
        // when the character destroy, the player controller will still survive
		ElimmedCharacter->Destroy();
	}

	if (ElimmedController)
	{
		TArray<AActor*> PlayerStarts;
        // get all actors of the player start class that exist in then world, hence using APlayerStart::StaticClass()
		UGameplayStatics::GetAllActorsOfClass(this, APlayerStart::StaticClass(), PlayerStarts);
        // randomly choose one 
		int32 Selection = FMath::RandRange(0, PlayerStarts.Num() - 1);

        // restart with player controller 
		RestartPlayerAtPlayerStart(ElimmedController, PlayerStarts[Selection]);
	}
}
