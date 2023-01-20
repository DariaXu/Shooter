// Fill out your copyright notice in the Description page of Project Settings.


#include "ShooterPlayerState.h"
#include "Shooter/Character/ShooterCharacter.h"
#include "Shooter/PlayerController/ShooterPlayerController.h"
#include "Net/UnrealNetwork.h"

void AShooterPlayerState::GetLifetimeReplicatedProps(TArray< FLifetimeProperty >& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AShooterPlayerState, Defeats);
}

void AShooterPlayerState::OnRep_Score()
{
    // will be call only on client
	Super::OnRep_Score();
    
	Character = Character == nullptr ? Cast<AShooterCharacter>(GetPawn()) : Character;

	if ((Character && Character->Controller) || Controller)
	{
		Controller = Controller == nullptr ? Cast<AShooterPlayerController>(Character->Controller) : Controller;
		if (Controller)
		{
            UE_LOG(LogTemp, Warning, TEXT("Score Updating on client"));
			Controller->SetHUDScore(GetScore());
		}
	}
}

void AShooterPlayerState::AddToScore(float ScoreAmount)
{
    // only call this on the server

    // only update the score on the server
	SetScore(GetScore() + ScoreAmount);

	Character = Character == nullptr ? Cast<AShooterCharacter>(GetPawn()) : Character;

	if ((Character && Character->Controller) || Controller)
	{
		Controller = Controller == nullptr ? Cast<AShooterPlayerController>(Character->Controller) : Controller;
		if (Controller)
		{
			Controller->SetHUDScore(GetScore());
		}
	}
}

void AShooterPlayerState::OnRep_Defeats()
{
	Character = Character == nullptr ? Cast<AShooterCharacter>(GetPawn()) : Character;
	if ((Character && Character->Controller) || Controller)
	{
		Controller = Controller == nullptr ? Cast<AShooterPlayerController>(Character->Controller) : Controller;
		if (Controller)
		{
			Controller->SetHUDDefeats(Defeats);
		}
	}
}

void AShooterPlayerState::AddToDefeats(int32 DefeatsAmount)
{
	Defeats += DefeatsAmount;
    // UE_LOG(LogTemp, Warning, TEXT("Defeat:%d"), Defeats);

	Character = Character == nullptr ? Cast<AShooterCharacter>(GetPawn()) : Character;

    // if (!Character->Controller) UE_LOG(LogTemp, Warning, TEXT("Defeat: Not Controller"));

	if ((Character && Character->Controller) || Controller)
	{
		Controller = Controller == nullptr ? Cast<AShooterPlayerController>(Character->Controller) : Controller;
		if (Controller)
		{
            
			Controller->SetHUDDefeats(Defeats);
		}
	}
}

