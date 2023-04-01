// Fill out your copyright notice in the Description page of Project Settings.


#include "ShooterPlayerController.h"
#include "Shooter/HUD/ShooterHUD.h"
#include "Shooter/HUD/CharacterOverlay.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Shooter/Character/ShooterCharacter.h"
#include "Net/UnrealNetwork.h"
#include "Shooter/GameMode/ShooterGameMode.h"
#include "Shooter/PlayerState/ShooterPlayerState.h"
#include "Shooter/HUD/Announcement.h"
#include "Kismet/GameplayStatics.h"
#include "Shooter/ShooterComponents/CombatComponent.h"
#include "Shooter/Weapon/Weapon.h"
#include "Shooter/GameState/ShooterGameState.h"

void AShooterPlayerController::BeginPlay()
{
	Super::BeginPlay();

    // GetHUD() return AHUD
	ShooterHUD = Cast<AShooterHUD>(GetHUD());
	ServerCheckMatchState();
}

void AShooterPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty> &OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AShooterPlayerController, MatchState);
}

void AShooterPlayerController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	CheckTimeSync(DeltaTime);
	SetHUDTime();
	PollInit();
}

void AShooterPlayerController::ReceivedPlayer()
{
	Super::ReceivedPlayer();
	if (IsLocalController())
	{
		ServerRequestServerTime(GetWorld()->GetTimeSeconds());
	}
}

// Overridable native function for when this controller is asked to possess a pawn.
void AShooterPlayerController::OnPossess(APawn *InPawn)
{
	Super::OnPossess(InPawn);
	
	AShooterCharacter* ShooterCharacter = Cast<AShooterCharacter>(InPawn);
	if (ShooterCharacter)
	{
		SetHUDHealth(ShooterCharacter->GetHealth(), ShooterCharacter->GetMaxHealth());
		if (ShooterCharacter->GetCombat())
		{
			SetHUDGrenades(ShooterCharacter->GetCombat()->GetGrenades());
		}
	}
}

void AShooterPlayerController::PollInit()
{
	if (CharacterOverlay != nullptr) return;
	
	if (ShooterHUD && ShooterHUD->CharacterOverlay)
	{
		CharacterOverlay = ShooterHUD->CharacterOverlay;
		if (CharacterOverlay)
		{
			if (bInitializeHealth) SetHUDHealth(HUDHealth, HUDMaxHealth);
			if (bInitializeShield) SetHUDShield(HUDShield, HUDMaxShield);
			if (bInitializeScore) SetHUDScore(HUDScore);
			if (bInitializeDefeats) SetHUDDefeats(HUDDefeats);

			AShooterCharacter* ShooterCharacter = Cast<AShooterCharacter>(GetPawn());
			if (ShooterCharacter && ShooterCharacter->GetCombat())
			{
				if (bInitializeGrenades) SetHUDGrenades(ShooterCharacter->GetCombat()->GetGrenades());
			}
		}
	}
}

void AShooterPlayerController::SetShooterHUD()
{
	ShooterHUD = ShooterHUD == nullptr ? Cast<AShooterHUD>(GetHUD()) : ShooterHUD;
}

#pragma region Match State
void AShooterPlayerController::OnMatchStateSet(FName State)
{
	// this only happen on the server, called from game mode
	MatchState = State;

	if (MatchState == MatchState::InProgress)
	{
		HandleMatchHasStarted();
	}
	else if (MatchState == MatchState::Cooldown)
	{
		HandleCooldown();
	}
}

void AShooterPlayerController::OnRep_MatchState()
{
	if (MatchState == MatchState::InProgress)
	{
		HandleMatchHasStarted();
	}
	else if (MatchState == MatchState::Cooldown)
	{
		HandleCooldown();
	}
}

void AShooterPlayerController::ServerCheckMatchState_Implementation()
{
	AShooterGameMode* GameMode = Cast<AShooterGameMode>(UGameplayStatics::GetGameMode(this));
	if (GameMode)
	{
		// should get those value from the game mode
		WarmupTime = GameMode->WarmupTime;
		MatchTime = GameMode->MatchTime;
		CooldownTime = GameMode->CooldownTime;
		LevelStartingTime = GameMode->LevelStartingTime;
		MatchState = GameMode->GetMatchState();

		ClientJoinMidgame(MatchState, WarmupTime, MatchTime, LevelStartingTime, CooldownTime);
	}
}

void AShooterPlayerController::ClientJoinMidgame_Implementation(FName StateOfMatch, float Warmup, float Match, float StartingTime, float Cooldown)
{
	// informing the client of the match state when it joins.
	WarmupTime = Warmup;
	MatchTime = Match;
	CooldownTime = Cooldown;
	LevelStartingTime = StartingTime;
	MatchState = StateOfMatch;

	OnMatchStateSet(MatchState);
	
	if (ShooterHUD && MatchState == MatchState::WaitingToStart)
	{
		// add announcement only when we are at the warm up statw
		ShooterHUD->AddAnnouncement();
	}
}
#pragma endregion

#pragma region Match Start
void AShooterPlayerController::HandleMatchHasStarted()
{
	SetShooterHUD();
	if (ShooterHUD)
	{
		if(ShooterHUD->CharacterOverlay == nullptr) ShooterHUD->AddCharacterOverlay();
		if (ShooterHUD->Announcement)
		{
			ShooterHUD->Announcement->SetVisibility(ESlateVisibility::Hidden);
		}
	}
}
#pragma endregion

#pragma region Match End
void AShooterPlayerController::HandleCooldown()
{
	SetShooterHUD();
	if (ShooterHUD)
	{
		ShooterHUD->CharacterOverlay->RemoveFromParent();

		bool bHUDValid = ShooterHUD->Announcement && 
			ShooterHUD->Announcement->AnnouncementText && 
			ShooterHUD->Announcement->InfoText;

		if (bHUDValid)
		{
			ShooterHUD->Announcement->SetVisibility(ESlateVisibility::Visible);
			FString AnnouncementText("New Match Starts In:");
			ShooterHUD->Announcement->AnnouncementText->SetText(FText::FromString(AnnouncementText));

			DisplayMatchResult();
		}
	}
	AShooterCharacter* ShooterCharacter = Cast<AShooterCharacter>(GetPawn());
	if (ShooterCharacter && ShooterCharacter->GetCombat())
	{
		ShooterCharacter->bDisableGameplay = true;
		ShooterCharacter->GetCombat()->FireBtnPressed(false);
	}
}

void AShooterPlayerController::DisplayMatchResult()
{
	AShooterGameState* ShooterGameState = Cast<AShooterGameState>(UGameplayStatics::GetGameState(this));
	AShooterPlayerState* ShooterPlayerState = GetPlayerState<AShooterPlayerState>();
	
	if (ShooterGameState && ShooterPlayerState)
	{
		TArray<AShooterPlayerState*> TopPlayers = ShooterGameState->TopScoringPlayers;
		FString InfoTextString;
		if (TopPlayers.Num() == 0)
		{
			InfoTextString = FString("There is no winner.");
		}
		else if (TopPlayers.Num() == 1 && TopPlayers[0] == ShooterPlayerState)
		{
			InfoTextString = FString("You are the winner!");
		}
		else if (TopPlayers.Num() == 1)
		{
			InfoTextString = FString::Printf(TEXT("Winner: \n%s"), *TopPlayers[0]->GetPlayerName());
		}
		else if (TopPlayers.Num() > 1)
		{
			InfoTextString = FString("Players tied for the win:\n");
			for (auto TiedPlayer : TopPlayers)
			{
				InfoTextString.Append(FString::Printf(TEXT("%s\n"), *TiedPlayer->GetPlayerName()));
			}
		}

		ShooterHUD->Announcement->InfoText->SetText(FText::FromString(InfoTextString));
	}
}
#pragma endregion

#pragma region Overlay HUD
void AShooterPlayerController::SetHUDHealth(float Health, float MaxHealth)
{
    // set if not set yet
	SetShooterHUD();

	// at the beginning of the game, CharacterOverlay will not be set
    // checking if valid
	bool bHUDValid = ShooterHUD && 
		ShooterHUD->CharacterOverlay && 
		ShooterHUD->CharacterOverlay->HealthBar && 
		ShooterHUD->CharacterOverlay->HealthText;

	if (bHUDValid)
	{
		const float HealthPercent = Health / MaxHealth;
		ShooterHUD->CharacterOverlay->HealthBar->SetPercent(HealthPercent);

        // CeilToInt round to int
		FString HealthText = FString::Printf(TEXT("%d/%d"), FMath::CeilToInt(Health), FMath::CeilToInt(MaxHealth));
        // setText taking FText
		ShooterHUD->CharacterOverlay->HealthText->SetText(FText::FromString(HealthText));
	}
	else
	{
		bInitializeHealth = true;
		HUDHealth = Health;
		HUDMaxHealth = MaxHealth;
	}
}

void AShooterPlayerController::SetHUDShield(float Shield, float MaxShield)
{
	SetShooterHUD();
	bool bHUDValid = ShooterHUD &&
		ShooterHUD->CharacterOverlay &&
		ShooterHUD->CharacterOverlay->ShieldBar &&
		ShooterHUD->CharacterOverlay->ShieldText;
	if (bHUDValid)
	{
		const float ShieldPercent = Shield / MaxShield;
		ShooterHUD->CharacterOverlay->ShieldBar->SetPercent(ShieldPercent);
		FString ShieldText = FString::Printf(TEXT("%d/%d"), FMath::CeilToInt(Shield), FMath::CeilToInt(MaxShield));
		ShooterHUD->CharacterOverlay->ShieldText->SetText(FText::FromString(ShieldText));
	}
	else
	{
		bInitializeShield = true;
		HUDShield = Shield;
		HUDMaxShield = MaxShield;
	}
}

void AShooterPlayerController::SetHUDScore(float Score)
{
	SetShooterHUD();
	bool bHUDValid = ShooterHUD && 
		ShooterHUD->CharacterOverlay && 
		ShooterHUD->CharacterOverlay->ScoreAmount;

	if (bHUDValid)
	{
		FString ScoreText = FString::Printf(TEXT("%d"), FMath::FloorToInt(Score));
		ShooterHUD->CharacterOverlay->ScoreAmount->SetText(FText::FromString(ScoreText));
	}
	else
	{
		bInitializeScore = true;
		HUDScore = Score;
	}
}

void AShooterPlayerController::SetHUDDefeats(int32 Defeats)
{
	SetShooterHUD();
	bool bHUDValid = ShooterHUD && 
		ShooterHUD->CharacterOverlay && 
		ShooterHUD->CharacterOverlay->DefeatsAmount;

	if (bHUDValid)
	{
		FString DefeatsText = FString::Printf(TEXT("%d"), Defeats);
		ShooterHUD->CharacterOverlay->DefeatsAmount->SetText(FText::FromString(DefeatsText));
	}
	else
	{
		bInitializeDefeats = true;
		HUDDefeats = Defeats;
	}
}

void AShooterPlayerController::SetHUDWeaponAmmo(int32 Ammo)
{
	SetShooterHUD();
	bool bHUDValid = ShooterHUD && 
		ShooterHUD->CharacterOverlay && 
		ShooterHUD->CharacterOverlay->WeaponAmmoAmount;

	if (bHUDValid)
	{
		FString AmmoText = FString::Printf(TEXT("%d"), Ammo);
		ShooterHUD->CharacterOverlay->WeaponAmmoAmount->SetText(FText::FromString(AmmoText));
	}
}

void AShooterPlayerController::ShowHUDWeaponAmmo(bool bIfShow)
{
	SetShooterHUD();
	bool bHUDValid = ShooterHUD && 
		ShooterHUD->CharacterOverlay && 
		ShooterHUD->CharacterOverlay->WeaponAmmoAmount;
 
	ESlateVisibility VisibleState = bIfShow ? (ESlateVisibility::Visible) : (ESlateVisibility::Hidden);
	if (bHUDValid)
	{
		ShooterHUD->CharacterOverlay->WeaponAmmoText->SetVisibility(VisibleState);
		ShooterHUD->CharacterOverlay->WeaponAmmoAmount->SetVisibility(VisibleState);
		ShooterHUD->CharacterOverlay->Slash->SetVisibility(VisibleState);
		ShooterHUD->CharacterOverlay->CarriedAmmoAmount->SetVisibility(VisibleState);
	}
}

void AShooterPlayerController::SetHUDCarriedAmmo(int32 Ammo)
{
	SetShooterHUD();
	bool bHUDValid = ShooterHUD && 
		ShooterHUD->CharacterOverlay && 
		ShooterHUD->CharacterOverlay->CarriedAmmoAmount;

	if (bHUDValid)
	{
		FString AmmoText = FString::Printf(TEXT("%d"), Ammo);
		ShooterHUD->CharacterOverlay->CarriedAmmoAmount->SetText(FText::FromString(AmmoText));
	}
}

void AShooterPlayerController::SetHUDGrenades(int32 Grenades)
{
	SetShooterHUD();
	bool bHUDValid = ShooterHUD &&
		ShooterHUD->CharacterOverlay &&
		ShooterHUD->CharacterOverlay->GrenadesText;
		
	if (bHUDValid)
	{
		FString GrenadesText = FString::Printf(TEXT("%d"), Grenades);
		ShooterHUD->CharacterOverlay->GrenadesText->SetText(FText::FromString(GrenadesText));
	}
	else
	{
		bInitializeGrenades = true;
		HUDGrenades = Grenades;
	}
}
#pragma endregion

#pragma region Countdown HUD
void AShooterPlayerController::SetHUDMatchCountdown(float CountdownTime)
{
	SetShooterHUD();
	bool bHUDValid = ShooterHUD &&
		ShooterHUD->CharacterOverlay &&
		ShooterHUD->CharacterOverlay->MatchCountdownText;

	if (bHUDValid)
	{
		if (CountdownTime < 0.f)
		{
			ShooterHUD->CharacterOverlay->MatchCountdownText->SetText(FText());
			return;
		}
		int32 Minutes = FMath::FloorToInt(CountdownTime / 60.f);
		int32 Seconds = CountdownTime - Minutes * 60;

		FString CountdownText = FString::Printf(TEXT("%02d:%02d"), Minutes, Seconds);
		ShooterHUD->CharacterOverlay->MatchCountdownText->SetText(FText::FromString(CountdownText));
	}
}

void AShooterPlayerController::SetHUDAnnouncementCountdown(float CountdownTime)
{
	SetShooterHUD();
	bool bHUDValid = ShooterHUD &&
		ShooterHUD->Announcement &&
		ShooterHUD->Announcement->WarmupTime;

	if (bHUDValid)
	{
		if (CountdownTime < 0.f)
		{
			ShooterHUD->Announcement->WarmupTime->SetText(FText());
			return;
		}
		int32 Minutes = FMath::FloorToInt(CountdownTime / 60.f);
		int32 Seconds = CountdownTime - Minutes * 60;

		FString CountdownText = FString::Printf(TEXT("%02d:%02d"), Minutes, Seconds);
		ShooterHUD->Announcement->WarmupTime->SetText(FText::FromString(CountdownText));
	}
}

#pragma endregion

#pragma region Time
void AShooterPlayerController::CheckTimeSync(float DeltaTime)
{
	TimeSyncRunningTime += DeltaTime;
	if (IsLocalController() && TimeSyncRunningTime > TimeSyncFrequency)
	{
		ServerRequestServerTime(GetWorld()->GetTimeSeconds());
		TimeSyncRunningTime = 0.f;
	}
}

void AShooterPlayerController::ServerRequestServerTime_Implementation(float TimeOfClientRequest)
{
	float ServerTime = GetWorld()->GetTimeSeconds();
	ClientReportServerTime(TimeOfClientRequest, ServerTime);
}

void AShooterPlayerController::ClientReportServerTime_Implementation(float TimeOfClientRequest, float TimeServerReceivedClientRequest)
{
	float RoundTripTime = GetWorld()->GetTimeSeconds() - TimeOfClientRequest;

	// Assume to time from client to server and server to client is same
	float CurrentServerTime = TimeServerReceivedClientRequest + (0.5f * RoundTripTime);
	// The time different between server and client
	ClientServerDelta = CurrentServerTime - GetWorld()->GetTimeSeconds();
}

float AShooterPlayerController::GetServerTime()
{
	// GetWorld()->GetTimeSeconds() the time has passed since the game started, will be different on different machines
    if (HasAuthority()) return GetWorld()->GetTimeSeconds();
	else return GetWorld()->GetTimeSeconds() + ClientServerDelta;
}

void AShooterPlayerController::SetHUDTime()
{
	// GetServerTime() time since the game launch (all time)
	// uint32 SecondsLeft = FMath::CeilToInt(MatchTime - GetServerTime());
	float TimeLeft = 0.f;
	if (MatchState == MatchState::WaitingToStart) TimeLeft = WarmupTime - (GetServerTime() - LevelStartingTime);
	else if (MatchState == MatchState::InProgress) TimeLeft = (WarmupTime + MatchTime) - (GetServerTime() - LevelStartingTime);
	else if (MatchState == MatchState::Cooldown) TimeLeft = (CooldownTime + WarmupTime + MatchTime) - (GetServerTime() - LevelStartingTime);

	if (HasAuthority())
	{
		// if it is on server, GetWorld()->GetTimeSeconds() might be off since the host might get timed logger when waiting
		ShooterGameMode = ShooterGameMode == nullptr ? Cast<AShooterGameMode>(UGameplayStatics::GetGameMode(this)) : ShooterGameMode;
		if (ShooterGameMode)
		{
			// TimeLeft = ShooterGameMode->GetCountdownTime()+LevelStartingTime;
			TimeLeft = ShooterGameMode->GetCountdownTime();
		}
	}

	uint32 SecondsLeft = FMath::CeilToInt(TimeLeft);

	if (CountdownInt != SecondsLeft)
	{
		// Updated when the second left updated
		// SetHUDMatchCountdown(MatchTime - GetServerTime());
		if (MatchState == MatchState::WaitingToStart || MatchState == MatchState::Cooldown)
		{
			SetHUDAnnouncementCountdown(TimeLeft);
		}
		if (MatchState == MatchState::InProgress)
		{
			SetHUDMatchCountdown(TimeLeft);
		}
	}

	CountdownInt = SecondsLeft;
}
#pragma endregion