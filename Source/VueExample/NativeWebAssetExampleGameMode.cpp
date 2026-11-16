#include "NativeWebAssetExampleGameMode.h"

#include "Assets/NativeWebAppAsset.h"
#include "Engine/LocalPlayer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "NativeWebView.h"
#include "Subsystems/NativeWebLocalPlayerSubsystem.h"
#include "TimerManager.h"

namespace
{
	constexpr float RenderHandoffTimeoutSeconds = 30.0f;
	constexpr float RenderHandoffPollSeconds = 0.1f;
}

void ANativeWebAssetExampleGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	if (bLaunchRequested || !IsValid(NewPlayer) || !NewPlayer->IsLocalController()) return;
	bLaunchRequested = true;
	bSmokeTest = FParse::Param(FCommandLine::Get(), TEXT("NativeWebAssetExampleSmoke"));
	ULocalPlayer* LocalPlayer = Cast<ULocalPlayer>(NewPlayer->Player);
	UNativeWebLocalPlayerSubsystem* Subsystem = IsValid(LocalPlayer)
		? LocalPlayer->GetSubsystem<UNativeWebLocalPlayerSubsystem>() : nullptr;
	if (!IsValid(Subsystem))
	{
		FNativeWebError Error;
		Error.Code = TEXT("MissingSubsystem");
		Error.Message = TEXT("No NativeWeb local-player subsystem was available to the example game mode.");
		OnAppViewComplete(nullptr, Error);
		return;
	}
	const FSoftObjectPath AppPath(TEXT("/Game/NativeWeb/AssetExample/NA_App.NA_App"));
	GetWorldTimerManager().SetTimer(RenderHandoffTimeoutHandle, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		if (bSmokeTest)
		{
			FinishSmokeTest(1, TEXT("NATIVEWEB_ASSET_EXAMPLE_FAILURE"), TEXT("Timed out before app launch and render handoff completed."));
		}
		else
		{
			GetWorldTimerManager().ClearTimer(RenderHandoffPollHandle);
			UE_LOG(LogTemp, Error, TEXT("NATIVEWEB_ASSET_EXAMPLE_FAILURE timed out waiting for rendered texture handoff."));
		}
	}), RenderHandoffTimeoutSeconds, false);
	TWeakObjectPtr<ANativeWebAssetExampleGameMode> WeakThis(this);
	FNativeWebPresentationConfig Presentation;
	Presentation.PresentationMode = ENativeWebPresentationMode::Slate;
	Subsystem->CreateAppViewAsync(TSoftObjectPtr<UNativeWebAppAsset>(AppPath), TEXT("NativeWebAssetExample"),
	                              FNativeWebSurfaceConfig(), Presentation,
	                              FNativeWebCreateAppViewComplete::CreateLambda(
		                              [WeakThis](UNativeWebView* View, const FNativeWebError& Error)
		                              {
			                              if (ANativeWebAssetExampleGameMode* GameMode = WeakThis.Get())
			                              {
				                              GameMode->OnAppViewComplete(View, Error);
			                              }
		                              }));
}

void ANativeWebAssetExampleGameMode::OnAppViewComplete(UNativeWebView* View, const FNativeWebError& Error)
{
	if (!IsValid(View))
	{
		GetWorldTimerManager().ClearTimer(RenderHandoffTimeoutHandle);
		GetWorldTimerManager().ClearTimer(RenderHandoffPollHandle);
		UE_LOG(LogTemp, Error, TEXT("NATIVEWEB_ASSET_EXAMPLE_FAILURE code=%s source=%s message=%s"),
		       *Error.Code.ToString(), *Error.SourceName, *Error.Message);
		if (bSmokeTest) FinishSmokeTest(1, TEXT("NATIVEWEB_ASSET_EXAMPLE_FAILURE"), Error.Message);
		return;
	}
	LaunchView = View;
	GetWorldTimerManager().SetTimer(RenderHandoffPollHandle,
	                                FTimerDelegate::CreateUObject(this, &ANativeWebAssetExampleGameMode::CheckRenderHandoff),
	                                RenderHandoffPollSeconds, true);
}

void ANativeWebAssetExampleGameMode::CheckRenderHandoff()
{
	if (!IsValid(LaunchView)) return;
	UTextureRenderTarget2D* Texture = LaunchView->GetTexture();
	if (!IsValid(Texture) || LaunchView->GetActivePresentationMode() != ENativeWebPresentationMode::Slate) return;
	UE_LOG(LogTemp, Display, TEXT("NATIVEWEB_ASSET_EXAMPLE_SUCCESS view=%s texture=%s presentation=Slate"),
	       *LaunchView->GetName(), *Texture->GetName());
	GetWorldTimerManager().ClearTimer(RenderHandoffPollHandle);
	GetWorldTimerManager().ClearTimer(RenderHandoffTimeoutHandle);
	if (bSmokeTest) FinishSmokeTest(0, TEXT("NATIVEWEB_ASSET_EXAMPLE_SUCCESS"), TEXT("Rendered output texture reached the Slate presenter."));
}

void ANativeWebAssetExampleGameMode::FinishSmokeTest(int32 ExitCode, const TCHAR* Marker, const FString& Message)
{
	if (GetWorld())
	{
		GetWorldTimerManager().ClearTimer(RenderHandoffTimeoutHandle);
		GetWorldTimerManager().ClearTimer(RenderHandoffPollHandle);
	}
	if (ExitCode == 0)
	{
		UE_LOG(LogTemp, Display, TEXT("%s exit=%d message=%s"), Marker, ExitCode, *Message);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("%s exit=%d message=%s"), Marker, ExitCode, *Message);
	}
	FPlatformMisc::RequestExitWithStatus(false, ExitCode);
}
