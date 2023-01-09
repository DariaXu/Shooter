// Fill out your copyright notice in the Description page of Project Settings.


#include "ProjectileWeapon.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Projectile.h"

void AProjectileWeapon::Fire(const FVector& HitTarget)
{
    // this is fine to play the animation on all machine
	Super::Fire(HitTarget);

    // this ensure the affect of firing only happens on the server
    if (!HasAuthority()) return;

	const USkeletalMeshSocket* MuzzleFlashSocket = GetWeaponMesh()->GetSocketByName(FName("MuzzleFlash"));
	if (!MuzzleFlashSocket) return;
	
    // GetWeaponMesh() the skeletal mesh the weapon belongs to 
    // get where to spawn the projectile 
    FTransform SocketTransform = MuzzleFlashSocket->GetSocketTransform(GetWeaponMesh());

    // cast the owner to a pawn
    APawn* InstigatorPawn = Cast<APawn>(GetOwner());
    if (!ProjectileClass || !InstigatorPawn) return;

    UWorld* World = GetWorld();
    if (World)
    {
        // From muzzle flash socket transform location to hit location from TraceUnderCrosshairs
        FVector ToTarget = HitTarget - SocketTransform.GetLocation();
        // FRotator TargetRotation = ToTarget.Rotation();

        // spawn the projectile actor
        FActorSpawnParameters SpawnParams;
        // the newly spawn actor will be set to this owner
        // in this case, the charater that is currently firing this weapon
        SpawnParams.Owner = GetOwner();
        SpawnParams.Instigator = InstigatorPawn;

        World->SpawnActor<AProjectile>(
            ProjectileClass, 
            SocketTransform.GetLocation(), 
            ToTarget.Rotation(), 
            SpawnParams
        );
    }
	
}