// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PickupSpawnPoint.generated.h"

UCLASS()
class SHOOTER_API APickupSpawnPoint : public AActor
{
	GENERATED_BODY()
	
public:	
	APickupSpawnPoint();
	virtual void Tick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;

	// types of pickup
	UPROPERTY(EditAnywhere)
	TArray<TSubclassOf<class APickup>> PickupClasses;

	// store the actor after spawn it 
	UPROPERTY()
	APickup* SpawnedPickup;

	void SpawnPickup();
	
	// need to be UFUNCTION to successful bind the delegate, AActor* DestroyedActor is the required argument
	UFUNCTION()
	void StartSpawnPickupTimer(AActor* DestroyedActor);

	void SpawnPickupTimerFinished();
private:
	FTimerHandle SpawnPickupTimer;

	UPROPERTY(EditAnywhere)
	float SpawnPickupTimeMin;

	UPROPERTY(EditAnywhere)
	float SpawnPickupTimeMax;

public:	

};
