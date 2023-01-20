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
	SetHUDScore();
}

void AShooterPlayerState::AddToScore(float ScoreAmount)
{
    // only call this on the server
    // only update the score on the server
	SetScore(GetScore() + ScoreAmount);
	SetHUDScore();
}

void AShooterPlayerState::OnRep_Defeats()
{
	SetHUDDefeats();
}

void AShooterPlayerState::AddToDefeats(int32 DefeatsAmount)
{
	Defeats += DefeatsAmount;
    // UE_LOG(LogTemp, Warning, TEXT("Defeat:%d"), Defeats);
	SetHUDDefeats();
	
}

void AShooterPlayerState::SetHUDDefeats()
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

void AShooterPlayerState::SetHUDScore()
{
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
