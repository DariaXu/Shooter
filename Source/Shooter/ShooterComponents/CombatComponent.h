// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Shooter/HUD/ShooterHUD.h"
#include "Shooter/Weapon/WeaponTypes.h"
#include "Shooter/ShooterTypes/CombatState.h"
#include "CombatComponent.generated.h"

class AWeapon;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class SHOOTER_API UCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// because combat is made for character,  
	// give the character full access including all private, protected and public.
	friend class AShooterCharacter;
	// Sets default values for this component's properties
	UCombatComponent();
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/**
	* Weapon
	*/
	void EquipWeapon(AWeapon* WeaponToEquip);
	void SwapWeapons();
	
	/**
	* Firing 
	*/
	void FireBtnPressed(bool bPressed);

	/**
	* Reload 
	*/
	void Reload();
	
	UFUNCTION(BlueprintCallable)
	void FinishReloading();

	/**
	* Grenade 
	*/
	UFUNCTION(BlueprintCallable)
	void ThrowGrenadeFinished();

	UFUNCTION(BlueprintCallable)
	void LaunchGrenade();

	UFUNCTION(Server, Reliable)
	void ServerLaunchGrenade(const FVector_NetQuantize& Target);

	/**
	* Shotgun
	*/
	void JumpToShotgunEnd();
	
	UFUNCTION(BlueprintCallable)
	void ShotgunShellReload();

	/**
	* Pickups
	*/
	void PickupAmmo(EWeaponType WeaponType, int32 AmmoAmount);

protected: // for child class to inherence
	// Called when the game starts
	virtual void BeginPlay() override;

	/**
	* Weapon
	*/
	UFUNCTION()
	void OnRep_EquippedWeapon();

	UFUNCTION()
	void OnRep_SecondaryWeapon();

	void PlayEquipWeaponSound(AWeapon* WeaponToEquip);
	void DropEquippedWeapon();

	void AttachActorToRightHand(AActor* ActorToAttach);
	void AttachActorToLeftHand(AActor* ActorToAttach);
	void AttachActorToBackpack(AActor* ActorToAttach);

	void EquipPrimaryWeapon(AWeapon* WeaponToEquip);
	void EquipSecondaryWeapon(AWeapon* WeaponToEquip);

	/**
	* HUD
	*/
	void TraceUnderCrosshairs(FHitResult& TraceHitResult);
	void SetHUDCrosshairs(float DeltaTime);

	/**
	* Aiming
	*/
	void SetAiming(bool bIsAiming);
	// server RPC, can have input parameter
	UFUNCTION(Server, Reliable)
	void ServerSetAiming(bool bIsAiming);

	/**
	* Firing 
	*/
	void Fire();
	// server RPC
	// FVector_NetQuantize a more effective way to send vector through network
	UFUNCTION(Server, Reliable)
	void ServerFire(const FVector_NetQuantize& TraceHitTarget);
	// calling netmulticast on sever side will execute on both client and sever (Server broadcasting)
	UFUNCTION(NetMulticast, Reliable)
	void MulticastFire(const FVector_NetQuantize& TraceHitTarget);

	void ThrowGrenade();

	UFUNCTION(Server, Reliable)
	void ServerThrowGrenade();

	UPROPERTY(EditAnywhere)
	TSubclassOf<class AProjectile> GrenadeClass;

	/**
	* Reload 
	*/
	UFUNCTION(Server, Reliable)
	void ServerReload();
	void HandleReload();

	int32 AmountToReload();
	void UpdateCarriedAmmo();
	void ReloadEmptyWeapon();

	void UpdateShotgunAmmoValues();

	/**
	* Grenade 
	*/
	void ShowAttachedGrenade(bool bShowGrenade);

private:
	UPROPERTY()
	class AShooterCharacter* Character;
	UPROPERTY()
	class AShooterPlayerController* Controller;
	UPROPERTY()
	class AShooterHUD* HUD;

	UPROPERTY(ReplicatedUsing = OnRep_CombatState)
	ECombatState CombatState = ECombatState::ECS_Unoccupied;

	UFUNCTION()
	void OnRep_CombatState();

	void SetController();

	/**
	* Weapon
	*/
	UPROPERTY(ReplicatedUsing = OnRep_EquippedWeapon)
	AWeapon* EquippedWeapon;

	UPROPERTY(ReplicatedUsing = OnRep_SecondaryWeapon)
	AWeapon* SecondaryWeapon;

	/**
	* HUD and crosshairs
	*/
	float CrosshairVelocityFactor;
	float CrosshairInAirFactor;
	float CrosshairAimFactor;
	float CrosshairShootingFactor;

	FHUDPackage HUDPackage;

	UPROPERTY(EditAnywhere)
	float BaseWalkSpeed;

	UPROPERTY(EditAnywhere)
	float AimWalkSpeed;

	/**
	* Aiming
	*/
	UPROPERTY(Replicated)
	bool bAiming;
	// Aiming and FOV
	// Field of view when not aiming; set to the camera's base FOV in BeginPlay
	float DefaultFOV;
	float CurrentFOV;
	// default value 
	UPROPERTY(EditAnywhere, Category = Combat)
	float ZoomedFOV = 30.f;

	UPROPERTY(EditAnywhere, Category = Combat)
	float ZoomInterpSpeed = 20.f;

	void InterpFOV(float DeltaTime);

	/**
	* Firing 
	*/
	bool bFiredBtnPressed;
	FVector HitTarget;
	// Automatic Fire
	FTimerHandle FireTimer;
	bool bCanContinueFire = true;

	void StartFireTimer();
	void FireTimerFinished();

	bool CanFire();

	/**
	* Ammo 
	*/
	// Carried ammo for the currently-equipped weapon type
	UPROPERTY(ReplicatedUsing = OnRep_CarriedAmmo)
	int32 CarriedAmmo;

	UFUNCTION()
	void OnRep_CarriedAmmo();

	// key: Ammo type, Value: Amount of carried ammo for that type
	TMap<EWeaponType, int32> CarriedAmmoMap;

	void InitializeCarriedAmmo();

	UPROPERTY(EditAnywhere)
	int32 MaxCarriedAmmo = 500;

	UPROPERTY(EditAnywhere)
	int32 StartingARAmmo = 30;

	UPROPERTY(EditAnywhere)
	int32 StartingRocketAmmo = 0;

	UPROPERTY(EditAnywhere)
	int32 StartingPistolAmmo = 0;

	UPROPERTY(EditAnywhere)
	int32 StartingSMGAmmo = 0;

	UPROPERTY(EditAnywhere)
	int32 StartingShotgunAmmo = 0;

	UPROPERTY(EditAnywhere)
	int32 StartingSniperAmmo = 0;
	
	UPROPERTY(EditAnywhere)
	int32 StartingGrenadeLauncherAmmo = 0;

	/**
	* Grenades 
	*/
	UPROPERTY(ReplicatedUsing = OnRep_Grenades)
	int32 Grenades = 4;

	UFUNCTION()
	void OnRep_Grenades();

	UPROPERTY(EditAnywhere)
	int32 MaxGrenades = 4;

	void UpdateHUDGrenades();
	
	/**
	* Reload 
	*/
	void UpdateAmmoValues();

public:
	FORCEINLINE int32 GetGrenades() const { return Grenades; }
	bool ShouldSwapWeapons();
};
