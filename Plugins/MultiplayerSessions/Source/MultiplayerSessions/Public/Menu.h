// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Interfaces/OnlineSessionInterface.h"

#include "Menu.generated.h"

/**
 * 
 */
UCLASS()
class MULTIPLAYERSESSIONS_API UMenu : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable)
	void MenuSetup(
		int32 NumOfPublicConnection=4, 
		FString TypeOfMatch=FString(TEXT("FreeForAll")), 
		FString LobbyPath=FString("/Game/ThirdPerson/Maps/Lobby")
		);

protected:
	virtual bool Initialize() override;
	virtual void OnLevelRemovedFromWorld(ULevel* InLevel, UWorld* InWorld) override;

	UFUNCTION()
	void OnCreateSession(bool bWasSuccessful);

	void OnFindSessions(const TArray<FOnlineSessionSearchResult>& SessionResult, bool bWasSuccessful);
	void OnJoinSession(EOnJoinSessionCompleteResult::Type Result);
	
	UFUNCTION()
	void OnDestroySession(bool bWasSuccessful);
	
	UFUNCTION()
	void OnStartSession(bool bWasSuccessful);


private:
	void PrintDebugMsg(FColor color, FString msg);

	UPROPERTY(meta = (BindWidget))
	class UButton* HostBtn;

	UPROPERTY(meta = (BindWidget))
	UButton* JoinBtn;

	UFUNCTION()
	void HostBtnClicked();

	UFUNCTION()
	void JoinBtnClicked();

	class UMultiplayerSessionSubsystem* MultiplayerSessionSubsystem;

	void MenuTearDown();

	int32 NumPublicConnection{4};
	FString MatchType{TEXT("FreeForAll")};
	FString Path2Lobby;
};
