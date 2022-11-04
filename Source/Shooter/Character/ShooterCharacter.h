// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ShooterCharacter.generated.h"

UCLASS()
class SHOOTER_API AShooterCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	AShooterCharacter();
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	virtual void PostInitializeComponents() override;

	// register variable to be replicated
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	void MoveForward(float Value);
	void MoveRight(float Value);
	void Turn(float Value);
	void LookUp(float Value);

	void EquipBtnPressed();
	void CrouchBtnPressed();
	void AimBtnPressed();
	void AimBtnReleased();
	
private:
	UPROPERTY(VisibleAnywhere, Category = Camera)
	class USpringArmComponent* CameraBoom;

	UPROPERTY(VisibleAnywhere, Category = Camera)
	class UCameraComponent* FollowCamera;

	//  meta = (AllowPrivateAccess = "true") means the blueprint can access private variable from c++ class
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	class UWidgetComponent* OverheadWidget;

	// notify(call) whenever OverlappingWeapon replicate happen
	// replication happens from server to client(one way)
	// hence, this notification function will only get called on the client side
	// and only client side will show the pick up widget
	UPROPERTY(ReplicatedUsing = OnRep_OverlappingWeapon)
	class AWeapon* OverlappingWeapon;
	
	// this function can only have a parameter with the type that being replicated，
	// will automaticstore the last replicated variable into the parameter before current replication happen
	UFUNCTION()
	void OnRep_OverlappingWeapon(AWeapon* LastWeapon);

	UPROPERTY(VisibleAnywhere)
	class UCombatComponent* Combat;

	// UFUNCTION(Server) is RPCs (Remote Procedure Calls) are functions that are called locally, 
	// but executed remotely on another machine (separate from the calling machine). 
	// reliable RPC is guaranty to be called: if sending information from client to server,
	// reliable allows client to get confirmation when the server receives the RPC;
	// if didn't get this confirmation, resent the RPC
	UFUNCTION(Server, Reliable)
	void ServerEquipButtonPressed();

public:
	// change only when the overlapping weapon changed on the server, once it changed, it will be replicated to all clients
	void SetOverlappingWeapon(AWeapon* Weapon);

	bool IsWeaponEquipped();
	bool IsAiming();
};
