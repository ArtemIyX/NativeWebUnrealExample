#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "NativeWebAssetExampleGameMode.generated.h"

class UNativeWebView;

UCLASS()
class VUEEXAMPLE_API ANativeWebAssetExampleGameMode final : public AGameModeBase
{
	GENERATED_BODY()

public:
	virtual void PostLogin(APlayerController* NewPlayer) override;

private:
	void OnAppViewComplete(UNativeWebView* View, const struct FNativeWebError& Error);
	void CheckRenderHandoff();
	void FinishSmokeTest(int32 ExitCode, const TCHAR* Marker, const FString& Message);

	UPROPERTY(Transient)
	TObjectPtr<UNativeWebView> LaunchView;

	FTimerHandle RenderHandoffTimeoutHandle;
	FTimerHandle RenderHandoffPollHandle;
	bool bLaunchRequested = false;
	bool bSmokeTest = false;
};
