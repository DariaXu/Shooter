// Fill out your copyright notice in the Description page of Project Settings.


#include "Weapon.h"
#include "Components/SphereComponent.h"
#include "Components/WidgetComponent.h"
#include "Shooter/Character/ShooterCharacter.h"
#include "Net/UnrealNetwork.h"
#include "Animation/AnimationAsset.h"
#include "Components/SkeletalMeshComponent.h"
#include "Casing.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Kismet/KismetMathLibrary.h"
#include "Shooter/PlayerController/ShooterPlayerController.h"

// Sets default values
AWeapon::AWeapon()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
	// weapon class is replicated, which means it is spawn on the server and propagates to all clients, only the server has authority
	// if set to false, all machine will have authority (it will spawn on all machine independently)
	bReplicates = true;

	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	// WeaponMesh->SetupAttachment(RootComponent);
	SetRootComponent(WeaponMesh);

	// bounce with floor/wall etc
	WeaponMesh->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Block);
	// ignore pawn collision
	WeaponMesh->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Ignore);
	// startup with no collision
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// use to detect overlap with character, for pick up weapon
	AreaSphere = CreateDefaultSubobject<USphereComponent>(TEXT("AreaSphere"));
	AreaSphere->SetupAttachment(RootComponent);
	AreaSphere->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
	AreaSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	PickupWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("PickupWidget"));
	PickupWidget->SetupAttachment(RootComponent);
	// if (HasAuthority())
	// {
	// 	// enable only at server
	// 	AreaSphere->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	// 	AreaSphere->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Overlap);
	// }

}

// Called when the game starts or when spawned
void AWeapon::BeginPlay()
{
	Super::BeginPlay();

	// if (GetLocalRole() == ENetRole::ROlE_Authority)
	if (HasAuthority())
	{
		AreaSphere->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		AreaSphere->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Overlap);
		AreaSphere->OnComponentBeginOverlap.AddDynamic(this, &AWeapon::OnSphereOverlap);
		AreaSphere->OnComponentEndOverlap.AddDynamic(this,  &AWeapon::OnSphereEndOverlap);         
	}
	
	if (PickupWidget)
	{
		PickupWidget->SetVisibility(false);
	}
}

// Called every frame
void AWeapon::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AWeapon::OnRep_Owner()
{
	Super::OnRep_Owner();
	if (Owner == nullptr)
	{
		// when dropping weapon, also clear the character and controller pointer on the client
		ShooterOwnerCharacter = nullptr;
		ShooterOwnerController = nullptr;
	}
	else
	{
		// set on client
		SetHUDAmmo();
	}
}

#pragma region Replicate
//================================================================================
// Replicate 
//================================================================================

void AWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	// replicated variable
	DOREPLIFETIME(AWeapon, WeaponState);
	DOREPLIFETIME(AWeapon, Ammo);
}
#pragma endregion

#pragma region Weapon
//================================================================================
// Weapon Pick up
//================================================================================
void AWeapon::SetWeaponState(EWeaponState State)
{
	WeaponState = State;
	// will not copy to the client
	// Now it will be call on the client from combat onrep_equippedWeapon
	switch (WeaponState)
	{
		case EWeaponState::EWS_Equipped:
			ShowPickupWidget(false);
			// only on server
			AreaSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			// if set to true, will not be able to pickup
			WeaponMesh->SetSimulatePhysics(false);
			WeaponMesh->SetEnableGravity(false);
			WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			break;
		case EWeaponState::EWS_Dropped:
			if (HasAuthority())
			{
				// only do this on the server
				AreaSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			}
			WeaponMesh->SetSimulatePhysics(true);
			WeaponMesh->SetEnableGravity(true);
			WeaponMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			break;
	}
}

bool AWeapon::IsAmmoEmpty()
{
    return Ammo <= 0;
}

void AWeapon::OnRep_WeaponState()
{
	// called when WeaponState changed
	// for the action that didn't copy to the client
	switch (WeaponState)
	{
		case EWeaponState::EWS_Equipped:
			ShowPickupWidget(false);
			// AreaSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);	
			WeaponMesh->SetSimulatePhysics(false);
			WeaponMesh->SetEnableGravity(false);
			WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			break;
		case EWeaponState::EWS_Dropped:
			WeaponMesh->SetSimulatePhysics(true);
			WeaponMesh->SetEnableGravity(true);
			WeaponMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			break;
	}
}

// only get calls on the server
void AWeapon::OnSphereOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult
	)
{
	AShooterCharacter* ShooterCharacter = Cast<AShooterCharacter>(OtherActor);

	if (ShooterCharacter)
	{
		// Cast successfully 
		ShooterCharacter->SetOverlappingWeapon(this);
	}
}

void AWeapon::OnSphereEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex
	)
{
	AShooterCharacter* ShooterCharacter = Cast<AShooterCharacter>(OtherActor);
	if (ShooterCharacter)
	{
		ShooterCharacter->SetOverlappingWeapon(nullptr);
	}
}


void AWeapon::ShowPickupWidget(bool bShowWidget)
{
	if (PickupWidget)
	{
		PickupWidget->SetVisibility(bShowWidget);
	}
}

void AWeapon::Dropped()
{
	SetWeaponState(EWeaponState::EWS_Dropped);
	FDetachmentTransformRules DetachRules(EDetachmentRule::KeepWorld, true);
	WeaponMesh->DetachFromComponent(DetachRules);

	if (ShooterOwnerController)
	{
		ShooterOwnerController->ShowHUDWeaponAmmo(false);
	}
	SetOwner(nullptr);

	// clear it on the server side
	ShooterOwnerCharacter = nullptr;
	ShooterOwnerController = nullptr;
}
#pragma endregion

#pragma region Firing and Ammo
//================================================================================
// Firing and Ammo
//================================================================================
void AWeapon::Fire(const FVector& HitTarget)
{
	if (FireAnimation)
	{
		// playing animation using skelton mesh 
		// not replicated
		WeaponMesh->PlayAnimation(FireAnimation, false);
	}

	if (CasingClass)
	{
		const USkeletalMeshSocket* AmmoEjectSocket = WeaponMesh->GetSocketByName(FName("AmmoEject"));
		if (AmmoEjectSocket)
		{
			FTransform SocketTransform = AmmoEjectSocket->GetSocketTransform(WeaponMesh);
			FRotator RandomRotator = UKismetMathLibrary::RandomRotator();

			UWorld* World = GetWorld();
			if (World)
			{
				World->SpawnActor<ACasing>(
					CasingClass,
					SocketTransform.GetLocation(),
					// SocketTransform.GetRotation().Rotator()
					RandomRotator
				);
			}
		}
	}
	SpendRound();
}


void AWeapon::OnRep_Ammo()
{
	ShooterOwnerCharacter = ShooterOwnerCharacter == nullptr ? Cast<AShooterCharacter>(GetOwner()) : ShooterOwnerCharacter;
	SetHUDAmmo();
}

void AWeapon::SpendRound()
{
	Ammo = FMath::Clamp(Ammo - 1, 0, MagCapacity);
	SetHUDAmmo();
}

void AWeapon::SetHUDAmmo()
{
	ShooterOwnerCharacter = ShooterOwnerCharacter == nullptr ? Cast<AShooterCharacter>(GetOwner()) : ShooterOwnerCharacter;
	if (ShooterOwnerCharacter)
	{
		ShooterOwnerController = ShooterOwnerController == nullptr ? Cast<AShooterPlayerController>(ShooterOwnerCharacter->Controller) : ShooterOwnerController;
		if (ShooterOwnerController)
		{
			ShooterOwnerController->SetHUDWeaponAmmo(Ammo);
		}
	}
}

#pragma endregion