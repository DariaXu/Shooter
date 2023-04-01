// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BuffComponent.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class SHOOTER_API UBuffComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UBuffComponent();

	friend class AShooterCharacter;

	/** 
	* Health buff
	*/
	void Heal(float HealAmount, float HealingTime);

	/** 
	* Speed buff
	*/
	void BuffSpeed(float BuffBaseSpeed, float BuffCrouchSpeed, float BuffTime);
	void SetInitialSpeeds(float BaseSpeed, float CrouchSpeed);

	/** 
	* Jump buff
	*/
	void BuffJump(float BuffJumpVelocity, float BuffTime);
	void SetInitialJumpVelocity(float Velocity);

	/** 
	* Shield buff
	*/
	void ReplenishShield(float ShieldAmount, float ReplenishTime);

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	void HealRampUp(float DeltaTime);
	void ShieldRampUp(float DeltaTime);

private:
	UPROPERTY()
	class AShooterCharacter* Character;

	/** 
	* Health buff
	*/
	bool bHealing = false;
	float HealingRate = 0.f;
	float AmountToHeal = 0.f;

	/** 
	* Speed buff
	*/
	FTimerHandle SpeedBuffTimer;
	void ResetSpeeds();
	float InitialBaseSpeed;
	float InitialCrouchSpeed;

	UFUNCTION(NetMulticast, Reliable)
	void MulticastSpeedBuff(float BaseSpeed, float CrouchSpeed);

	/** 
	* Jump buff
	*/
	FTimerHandle JumpBuffTimer;
	void ResetJump();
	float InitialJumpVelocity;

	UFUNCTION(NetMulticast, Reliable)
	void MulticastJumpBuff(float JumpVelocity);

	/** 
	* Shield buff
	*/
	bool bReplenishingShield = false;
	float ShieldReplenishRate = 0.f;
	float ShieldReplenishAmount = 0.f;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
};
