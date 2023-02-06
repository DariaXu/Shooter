// Fill out your copyright notice in the Description page of Project Settings.


#include "CombatComponent.h"
#include "Components/SphereComponent.h"
#include "Shooter/Weapon/Weapon.h"
#include "Shooter/Character/ShooterCharacter.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "Shooter/PlayerController/ShooterPlayerController.h"
#include "Camera/CameraComponent.h"
#include "TimerManager.h"
#include "Sound/SoundCue.h"

// Sets default values for this component's properties
UCombatComponent::UCombatComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	//  true => tick every frame (calling TickComponent)
	PrimaryComponentTick.bCanEverTick = true;

	BaseWalkSpeed = 600.f;
	AimWalkSpeed = 450.f;
}

// Called when the game starts
void UCombatComponent::BeginPlay()
{
	Super::BeginPlay();

	if (Character) 
	{
		Character->GetCharacterMovement()->MaxWalkSpeed = BaseWalkSpeed;

		if (Character->GetFollowCamera())
		{
			DefaultFOV = Character->GetFollowCamera()->FieldOfView;
			CurrentFOV = DefaultFOV;
		}

		if (Character->HasAuthority())
		{
			// control by server
			InitializeCarriedAmmo();
		}
	}

}

// Called every frame
void UCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (Character && Character->IsLocallyControlled())
	{
		FHitResult HitResult;
		TraceUnderCrosshairs(HitResult);
		HitTarget = HitResult.ImpactPoint;

		SetHUDCrosshairs(DeltaTime);
		InterpFOV(DeltaTime);
	}
}

void UCombatComponent::SetController()
{
	if (Character->Controller)
	{
		Controller = Controller == nullptr ? Cast<AShooterPlayerController>(Character->Controller) : Controller;
	}
}

#pragma region Replicate variable
//================================================================================
// Replicate variable
//================================================================================

void UCombatComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UCombatComponent, EquippedWeapon);
	DOREPLIFETIME(UCombatComponent, bAiming);
	// will only replicate to the owning client
	DOREPLIFETIME_CONDITION(UCombatComponent, CarriedAmmo, COND_OwnerOnly);
	DOREPLIFETIME(UCombatComponent, CombatState);
}
#pragma endregion

#pragma region HUD
//================================================================================
// HUD
//================================================================================

void UCombatComponent::SetHUDCrosshairs(float DeltaTime)
{
	if (Character == nullptr || Character->Controller == nullptr) return;

	SetController();

	if (Controller)
	{
		HUD = HUD == nullptr ? Cast<AShooterHUD>(Controller->GetHUD()) : HUD;
		if (HUD)
		{
			if (EquippedWeapon)
			{
				HUDPackage.CrosshairsCenter = EquippedWeapon->CrosshairsCenter;
				HUDPackage.CrosshairsLeft = EquippedWeapon->CrosshairsLeft;
				HUDPackage.CrosshairsRight = EquippedWeapon->CrosshairsRight;
				HUDPackage.CrosshairsBottom = EquippedWeapon->CrosshairsBottom;
				HUDPackage.CrosshairsTop = EquippedWeapon->CrosshairsTop;
			}
			else
			{
				// should not draw anything to the screen if there is no equipped weapon
				HUDPackage.CrosshairsCenter = nullptr;
				HUDPackage.CrosshairsLeft = nullptr;
				HUDPackage.CrosshairsRight = nullptr;
				HUDPackage.CrosshairsBottom = nullptr;
				HUDPackage.CrosshairsTop = nullptr;
			}

			// Calculate crosshair spread base on moving velocity
			FVector2D WalkSpeedRange(0.f, Character->GetCharacterMovement()->MaxWalkSpeed);
			FVector2D VelocityMultiplierRange(0.f, 1.f);
			FVector Velocity = Character->GetVelocity();
			Velocity.Z = 0.f;

			// moving spread
			// [0, 600] normalize to [0, 1] (0 => not moving; 600 => moving with the max speed)
			// Velocity.Size(), the magnitude of the vector
			CrosshairVelocityFactor = FMath::GetMappedRangeValueClamped(WalkSpeedRange, VelocityMultiplierRange, Velocity.Size());
			if (Character->GetCharacterMovement()->IsFalling())
			{
				// spread more when the character is in the air
				// interp from current to 2.25 as the maximum value
				CrosshairInAirFactor = FMath::FInterpTo(CrosshairInAirFactor, 2.25f, DeltaTime, 2.25f);
			}
			else
			{
				// faster when hit the ground
				CrosshairInAirFactor = FMath::FInterpTo(CrosshairInAirFactor, 0.f, DeltaTime, 30.f);
			}

			// aiming spread
			if (bAiming)
			{
				// shrinking
				CrosshairAimFactor = FMath::FInterpTo(CrosshairAimFactor, -0.58f, DeltaTime, 30.f);
			}
			else
			{
				CrosshairAimFactor = FMath::FInterpTo(CrosshairAimFactor, 0.f, DeltaTime, 30.f);
			}

			// after firing, immediately shrink back to zero; was set at fire button pressed
			CrosshairShootingFactor = FMath::FInterpTo(CrosshairShootingFactor, 0.f, DeltaTime, 40.f);

			// 0.5f as the baseline spread
			HUDPackage.CrosshairSpread = 
				0.5f + 
				CrosshairVelocityFactor + 
				CrosshairInAirFactor +
				CrosshairAimFactor +
				CrosshairShootingFactor;

			HUD->SetHUDPackage(HUDPackage);
		}
	}
}

void UCombatComponent::TraceUnderCrosshairs(FHitResult& TraceHitResult)
{
	FVector2D ViewportSize;
	if (GEngine && GEngine->GameViewport)
	{
		// get the view port size in screen space
		GEngine->GameViewport->GetViewportSize(ViewportSize);
	}

	// Cross hair location is the center of the view port
	FVector2D CrosshairPos(ViewportSize.X / 2.f, ViewportSize.Y / 2.f);

	// transfer 2D screen space to 3D world space
	FVector CrosshairWldPos;
	FVector CrosshairWldDir;
	// Regarding the GetPlayerController fuction, for each individual machine, player 0 is who currently playing the game 
	// the WorldContextObject is any object in this word to give the reference of which word is in
	bool bScreenToWorld = UGameplayStatics::DeprojectScreenToWorld(
		UGameplayStatics::GetPlayerController(this, 0),
		CrosshairPos, CrosshairWldPos, CrosshairWldDir
	);

	if (bScreenToWorld) // checking if the transform was successful
	{
		FVector Start = CrosshairWldPos;

		if (Character)
		{
			// moving the start point of the line trace from camera to 100 in front of the character 
			float DistanceToCharacter = (Character->GetActorLocation() - Start).Size();
			Start += CrosshairWldDir * (DistanceToCharacter + 100.f);
		}

		FVector End = Start + CrosshairWldDir * TRACE_LENGTH;

		// performing line trace, ECC_Visibility hit any visible object as the end result
		GetWorld()->LineTraceSingleByChannel(
			TraceHitResult,
			Start,
			End,
			ECollisionChannel::ECC_Visibility
		);

		if (!TraceHitResult.bBlockingHit)
		{
			// if nothing gets hit
			TraceHitResult.ImpactPoint = End;
		}
		// check if hit a charater
		// check if the actor is using the interface 
		if (TraceHitResult.GetActor() && TraceHitResult.GetActor()->Implements<UCrosshairInteractionInterface>())
		{
			HUDPackage.CrosshairsColor = FLinearColor::Red;
		}
		else
		{
			HUDPackage.CrosshairsColor = FLinearColor::White;
		}
	}
}
#pragma endregion HUD

#pragma region Weapon
//================================================================================
// Equip Weapon
//================================================================================
void UCombatComponent::OnRep_EquippedWeapon()
{
	// will be called on client
	if (EquippedWeapon && Character)
	{
		// to ensure the physic is properly set before attach actor
		EquippedWeapon->SetWeaponState(EWeaponState::EWS_Equipped);

		const USkeletalMeshSocket* HandSocket = Character->GetMesh()->GetSocketByName(FName("RightHandSocket"));
		if (HandSocket)
		{
			// this action of attaching the actor will propagate to clients
			HandSocket->AttachActor(EquippedWeapon, Character->GetMesh());
		}
		Character->GetCharacterMovement()->bOrientRotationToMovement = false;
		Character->bUseControllerRotationYaw = true;

		SetController();
		if (Controller)
		{
			Controller->SetHUDCarriedAmmo(CarriedAmmo);
			Controller->ShowHUDWeaponAmmo(true);
		}

		if (EquippedWeapon->EquipSound)
		{
			UGameplayStatics::PlaySoundAtLocation(
				this,
				EquippedWeapon->EquipSound,
				Character->GetActorLocation()
			);
		}	
	}
}

void UCombatComponent::EquipWeapon(AWeapon* WeaponToEquip)
{
	// will be called on server
	if (Character == nullptr || WeaponToEquip == nullptr) return;

	if (EquippedWeapon)
	{
		EquippedWeapon->Dropped();
	}

	EquippedWeapon = WeaponToEquip;
	// set to replicated at Weapon class
	// since this can't ensure the replicate call will be called on the client before the following attach actor replicate call,
	// (can't ensure setting the weapon physic will execute before attaching actor on the client side)
	// (if attachActor called before the replicate function, pick up will fail since the enable physic of the weapon is still true)
	// need to equip the weapon on client as well
	EquippedWeapon->SetWeaponState(EWeaponState::EWS_Equipped);

	const USkeletalMeshSocket* HandSocket = Character->GetMesh()->GetSocketByName(FName("RightHandSocket"));
	if (HandSocket)
	{
		// this action of attaching the actor will propagate to clients (replicate)
		HandSocket->AttachActor(EquippedWeapon, Character->GetMesh());
	}
	// auto replicated
	EquippedWeapon->SetOwner(Character);
	// take care setting up on the server side
	EquippedWeapon->SetHUDAmmo();

	// set carried ammo
	if (CarriedAmmoMap.Contains(EquippedWeapon->GetWeaponType()))
	{
		CarriedAmmo = CarriedAmmoMap[EquippedWeapon->GetWeaponType()];
	}

	SetController();
	if (Controller)
	{
		Controller->SetHUDCarriedAmmo(CarriedAmmo);
		Controller->ShowHUDWeaponAmmo(true);
	}
	
	if (EquippedWeapon->EquipSound)
	{
		UGameplayStatics::PlaySoundAtLocation(
			this,
			EquippedWeapon->EquipSound,
			Character->GetActorLocation()
		);
	}

	// Auto Reload
	if (EquippedWeapon->IsAmmoEmpty())
	{
		Reload();
	}

	Character->GetCharacterMovement()->bOrientRotationToMovement = false;
	Character->bUseControllerRotationYaw = true;

}

#pragma endregion

#pragma region Aiming
//================================================================================
// Aiming
//================================================================================
void UCombatComponent::SetAiming(bool bIsAiming)
{
	// having this statement will avoid client to wait for responds from sever 
	bAiming = bIsAiming;
	// since this function is a rpc server function, 
	// calling this on the client who own this charater, will result in calling this function on the server
	ServerSetAiming(bIsAiming);

	if (Character) 
	{
		Character->GetCharacterMovement()->MaxWalkSpeed = bIsAiming? AimWalkSpeed : BaseWalkSpeed;
	}
}

void UCombatComponent::ServerSetAiming_Implementation(bool bIsAiming)
{
	bAiming = bIsAiming;
	
	if (Character) 
	{
		Character->GetCharacterMovement()->MaxWalkSpeed = bIsAiming? AimWalkSpeed : BaseWalkSpeed;
	}
}

void UCombatComponent::InterpFOV(float DeltaTime)
{
	if (EquippedWeapon == nullptr) return;

	if (bAiming)
	{
		CurrentFOV = FMath::FInterpTo(CurrentFOV, EquippedWeapon->GetZoomedFOV(), DeltaTime, EquippedWeapon->GetZoomInterpSpeed());
	}
	else
	{
		// un zooming back to default are all at same speed for all weapons
		CurrentFOV = FMath::FInterpTo(CurrentFOV, DefaultFOV, DeltaTime, ZoomInterpSpeed);
	}

	if (Character && Character->GetFollowCamera())
	{
		Character->GetFollowCamera()->SetFieldOfView(CurrentFOV);
	}
}
#pragma endregion

#pragma region Firing
//================================================================================
// Firing
//================================================================================
void UCombatComponent::Fire()
{
	if (!CanFire()) return;

	// since we are tracing every frame, don't need this trace any more
	// FHitResult HitResult;
	// TraceUnderCrosshairs(HitResult);

	bCanContinueFire = false;
	// calling sever to fire
	ServerFire(HitTarget);

	if (EquippedWeapon)
	{
		CrosshairShootingFactor = .75f;
	}
	StartFireTimer();
}

void UCombatComponent::StartFireTimer()
{
	if (EquippedWeapon == nullptr || Character == nullptr) return;
	// set a timer
	Character->GetWorldTimerManager().SetTimer(
		FireTimer,
		this,
		&UCombatComponent::FireTimerFinished,
		EquippedWeapon->FireDelay
	);
}

void UCombatComponent::FireTimerFinished()
{
	if (EquippedWeapon == nullptr) return;
	bCanContinueFire = true;
	// if fire btn still pressed
	if (bFiredBtnPressed && EquippedWeapon->bAutomatic)
	{
		Fire();
	}

	// Auto Reload
	if (EquippedWeapon->IsAmmoEmpty())
	{
		Reload();
	}
}

bool UCombatComponent::CanFire()
{
    if (EquippedWeapon == nullptr) return false;
	if (EquippedWeapon->IsAmmoEmpty()) return false;
	if (!bCanContinueFire) return false;
	if (CombatState != ECombatState::ECS_Unoccupied) return false;
	return true;
}

// will always call on local machine
void UCombatComponent::FireBtnPressed(bool bPressed)
{
	bFiredBtnPressed = bPressed;

	if (bFiredBtnPressed && EquippedWeapon) 
	{
		Fire();
	}
}

// called from client and executed from the server
void UCombatComponent::ServerFire_Implementation(const FVector_NetQuantize& TraceHitTarget)
{
	MulticastFire(TraceHitTarget);
}

// called from sever and will execute on both client and sever
// if called from client, will only execute on client(useless)
void UCombatComponent::MulticastFire_Implementation(const FVector_NetQuantize& TraceHitTarget)
{
	if (EquippedWeapon == nullptr) return;
	
	// since bFiredBtnPressed is only set on client side
	if (Character && CombatState == ECombatState::ECS_Unoccupied)
	{
		// not replicated
		Character->PlayFireMontage(bAiming);
		EquippedWeapon->Fire(TraceHitTarget);
	}
}
#pragma endregion

#pragma region Ammo and Reload
void UCombatComponent::InitializeCarriedAmmo()
{
	CarriedAmmoMap.Emplace(EWeaponType::EWT_AssaultRifle, StartingARAmmo);
	CarriedAmmoMap.Emplace(EWeaponType::EWT_RocketLauncher, StartingRocketAmmo);
	CarriedAmmoMap.Emplace(EWeaponType::EWT_Pistol, StartingPistolAmmo);
	CarriedAmmoMap.Emplace(EWeaponType::EWT_SubmachineGun, StartingSMGAmmo);
}

void UCombatComponent::Reload()
{
	// check on client
	if (CarriedAmmo > 0 && CombatState != ECombatState::ECS_Reloading)
	{
		// will be execute only on the server
		ServerReload();
	}
}

void UCombatComponent::HandleReload()
{
	// what will execute when reloading (on all machine)
	Character->PlayReloadMontage();
}

void UCombatComponent::FinishReloading()
{
	if (Character == nullptr) return;
	if (Character->HasAuthority())
	{
		CombatState = ECombatState::ECS_Unoccupied;
		UpdateAmmoValues();
	}
	if (bFiredBtnPressed)
	{
		// fire button will only be true on the server
		Fire();
	}
}

void UCombatComponent::ServerReload_Implementation()
{
	// server RPC
	if (Character == nullptr || EquippedWeapon == nullptr) return;
	CombatState = ECombatState::ECS_Reloading;
	HandleReload();
}

void UCombatComponent::OnRep_CarriedAmmo()
{
	SetController();
	if (Controller)
	{
		Controller->SetHUDCarriedAmmo(CarriedAmmo);
	}
}

void UCombatComponent::OnRep_CombatState()
{
	// on client
	switch (CombatState)
	{
		case ECombatState::ECS_Reloading:
			HandleReload();
			break;

		case ECombatState::ECS_Unoccupied:
			if (bFiredBtnPressed)
			{
				Fire();
			}
			break;
	}
}

int32 UCombatComponent::AmountToReload()
{
    if (EquippedWeapon == nullptr) return 0;

	int32 RoomInMag = EquippedWeapon->GetMagCapacity() - EquippedWeapon->GetAmmo();

	if (CarriedAmmoMap.Contains(EquippedWeapon->GetWeaponType()))
	{
		int32 AmountCarried = CarriedAmmoMap[EquippedWeapon->GetWeaponType()];
		int32 Least = FMath::Min(RoomInMag, AmountCarried);
		return FMath::Clamp(RoomInMag, 0, Least);
	}

	// doesn't have any carried ammo for current weapon type
	return 0;
}

void UCombatComponent::UpdateAmmoValues()
{
	if (Character == nullptr || EquippedWeapon == nullptr) return;

	int32 ReloadAmount = AmountToReload();
	if (CarriedAmmoMap.Contains(EquippedWeapon->GetWeaponType()))
	{
		CarriedAmmoMap[EquippedWeapon->GetWeaponType()] -= ReloadAmount;
		CarriedAmmo = CarriedAmmoMap[EquippedWeapon->GetWeaponType()];
	}

	SetController();
	if (Controller)
	{
		// updating on the sever, client HUD will be set when ammo replicate
		Controller->SetHUDCarriedAmmo(CarriedAmmo);
	}
	EquippedWeapon->AddAmmo(ReloadAmount);
}

#pragma endregion
