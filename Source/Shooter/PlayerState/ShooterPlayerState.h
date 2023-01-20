// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "ShooterPlayerState.generated.h"

/**
 * 
 */
UCLASS()
class SHOOTER_API AShooterPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray< FLifetimeProperty >& OutLifetimeProps) const override;

	/**
	* Replication notifies
	*/
	UFUNCTION()
	virtual void OnRep_Defeats();
	
	virtual void OnRep_Score() override;

	// updating score on server
	void AddToScore(float ScoreAmount);
	void AddToDefeats(int32 DefeatsAmount);

	void SetHUDDefeats();
	void SetHUDScore();

private:
	// using UPROPERTY() will ensure when uninitialized, the pointer will be a NULL point, 
	// instead of undefined behavior (chance of storing garbage information in the pointer) 
	// that could pass through the null check (such as if (Character))
	UPROPERTY()
	class AShooterCharacter* Character;

	UPROPERTY()
	class AShooterPlayerController* Controller;

	UPROPERTY(ReplicatedUsing = OnRep_Defeats)
	int32 Defeats;
};
