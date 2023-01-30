// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "ShooterGameState.generated.h"

/**
 * 
 */
UCLASS()
class SHOOTER_API AShooterGameState : public AGameState
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	void UpdateTopScore(class AShooterPlayerState* ScoringPlayer);

	UPROPERTY(Replicated)
	TArray<AShooterPlayerState*> TopScoringPlayers;
private:

	float TopScore = 0.f;
};
