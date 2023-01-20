// Fill out your copyright notice in the Description page of Project Settings.


#include "ShooterAnimInstance.h"
#include "ShooterCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Shooter/Weapon/Weapon.h"

void UShooterAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	ShooterCharacter = Cast<AShooterCharacter>(TryGetPawnOwner());
}

// calls every frame
void UShooterAnimInstance::NativeUpdateAnimation(float DeltaTime)
{
	Super::NativeUpdateAnimation(DeltaTime);

    if (ShooterCharacter == nullptr)
	{
		ShooterCharacter = Cast<AShooterCharacter>(TryGetPawnOwner());
	}
	
	if (ShooterCharacter == nullptr) return;

	// velocity calculation
    FVector Velocity = ShooterCharacter->GetVelocity();
	Velocity.Z = 0.f;
	Speed = Velocity.Size();

	// charater status
	bIsInAir = ShooterCharacter->GetCharacterMovement()->IsFalling();
	bIsAccelerating = ShooterCharacter->GetCharacterMovement()->GetCurrentAcceleration().Size() > 0.f ? true : false;
	bWeaponEquipped = ShooterCharacter->IsWeaponEquipped();
	EquippedWeapon = ShooterCharacter->GetEquippedWeapon();
	bIsCrouched = ShooterCharacter->bIsCrouched;
	bAiming = ShooterCharacter->IsAiming();
	TurningInPlace = ShooterCharacter->GetTurningInPlace();
	bRotateRootBone = ShooterCharacter->ShouldRotateRootBone();
	bElimmed = ShooterCharacter->IsElimmed();

	// Offset Yaw for Strafing (variables used are already replicated)
	// get the controller rotation (global rotation)
	FRotator AimRotation = ShooterCharacter->GetBaseAimRotation();
	// direction as paramater, show the rotation of current movement(global rotation)
	FRotator MovementRotation = UKismetMathLibrary::MakeRotFromX(ShooterCharacter->GetVelocity());
	// The difference between two vector
	// YawOffset = UKismetMathLibrary::NormalizedDeltaRotator(MovementRotation, AimRotation).Yaw;
	FRotator DeltaRot = UKismetMathLibrary::NormalizedDeltaRotator(MovementRotation, AimRotation);
	// will pick the shortest path for smoothness
	DeltaRotation = FMath::RInterpTo(DeltaRotation, DeltaRot, DeltaTime, 6.f);
	YawOffset = DeltaRotation.Yaw;

	// lean
	CharacterRotationLastFrame = CharacterRotationCurFrame;
	// rotation of the root component (capsule rotation)
	CharacterRotationCurFrame = ShooterCharacter->GetActorRotation();
	const FRotator Delta = UKismetMathLibrary::NormalizedDeltaRotator(CharacterRotationCurFrame, CharacterRotationLastFrame);
	// scale Delta up and let it be proportional to Delta time.
	const float Target = Delta.Yaw / DeltaTime;
	// to avoid jerkiness, for smooth transaction, interpret lean to target
	const float Interp = FMath::FInterpTo(Lean, Target, DeltaTime, 6.f);
	Lean = FMath::Clamp(Interp, -90.f, 90.f);

	// Aiming Offset
	AO_Yaw = ShooterCharacter->GetAO_Yaw();
	AO_Pitch = ShooterCharacter->GetAO_Pitch();

	if (bWeaponEquipped && EquippedWeapon && EquippedWeapon->GetWeaponMesh() && ShooterCharacter->GetMesh())
	{
		LeftHandTransform = EquippedWeapon->GetWeaponMesh()->GetSocketTransform(FName("LeftHandSocket"), ERelativeTransformSpace::RTS_World);
		FVector OutPosition;
		FRotator OutRotation;
		// right hand as the reference space
		ShooterCharacter->GetMesh()->TransformToBoneSpace(FName("hand_r"), LeftHandTransform.GetLocation(), FRotator::ZeroRotator, OutPosition, OutRotation);
		LeftHandTransform.SetLocation(OutPosition);
		LeftHandTransform.SetRotation(FQuat(OutRotation));

		if (ShooterCharacter->IsLocallyControlled())
		{
			bLocallyControlled = true;

			//right hand bone
			FTransform RightHandTransform = ShooterCharacter->GetMesh()->GetSocketTransform(FName("Hand_R"), ERelativeTransformSpace::RTS_World);
			// (RightHandTransform.GetLocation() - ShooterCharacter->GetHitTarget()) vector from hit target to right hand transform
			//  added up will become a vector point backward(opposite direction) from right hand transform
			FRotator LookAtRotation = UKismetMathLibrary::FindLookAtRotation(RightHandTransform.GetLocation(), RightHandTransform.GetLocation() + (RightHandTransform.GetLocation() - ShooterCharacter->GetHitTarget()));
			// for smoothing the transaction
			RightHandRotation = FMath::RInterpTo(RightHandRotation, LookAtRotation, DeltaTime, 40.f);

			// FVector RightHandX(FRotationMatrix(RightHandTransform.GetRotation().Rotator()).GetUnitAxis(EAxis::X));
			// DrawDebugLine(GetWorld(), RightHandTransform.GetLocation(), ShooterCharacter->GetHitTarget(), FColor::Blue);
		}

		// showing the different between crosshair and weapon pointing direction
		// FTransform MuzzleTipTransform = EquippedWeapon->GetWeaponMesh()->GetSocketTransform(FName("MuzzleFlash"), ERelativeTransformSpace::RTS_World);
		// // get the direction of where the x-axis of the muzzle tip pointing to 
		// FVector MuzzleX(FRotationMatrix(MuzzleTipTransform.GetRotation().Rotator()).GetUnitAxis(EAxis::X));
		// DrawDebugLine(GetWorld(), MuzzleTipTransform.GetLocation(), MuzzleTipTransform.GetLocation()+MuzzleX*100.f, FColor::Red);
		// DrawDebugLine(GetWorld(), MuzzleTipTransform.GetLocation(), ShooterCharacter->GetHitTarget(), FColor::Green);
		
	}
}

