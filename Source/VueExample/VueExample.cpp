// Fill out your copyright notice in the Description page of Project Settings.

#include "VueExample.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Camera/PlayerCameraManager.h"
#include "DrawDebugHelpers.h"
#include "HAL/PlatformProcess.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Modules/ModuleManager.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "NativeWebView.h"
#include "Subsystems/NativeWebLocalPlayerSubsystem.h"

namespace
{
	const FName HudSmokeViewName(TEXT("NativeWebHudSmoke"));
}

class FVueExampleModule final : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
		WorldTickHandle = FWorldDelegates::OnWorldPostActorTick.AddRaw(
			this, &FVueExampleModule::OnWorldPostActorTick);
		IConsoleManager& Console = IConsoleManager::Get();
		ConsoleCommands.Add(Console.RegisterConsoleCommand(TEXT("nativeweb.hud.enable"), TEXT("Enable the HUD fixture in this world."),
			FConsoleCommandWithWorldAndArgsDelegate::CreateRaw(this, &FVueExampleModule::EnableInWorld), ECVF_Cheat));
		ConsoleCommands.Add(Console.RegisterConsoleCommand(TEXT("nativeweb.hud.show"), TEXT("Show the HUD fixture in this world."),
			FConsoleCommandWithWorldAndArgsDelegate::CreateRaw(this, &FVueExampleModule::ShowInWorld), ECVF_Cheat));
		ConsoleCommands.Add(Console.RegisterConsoleCommand(TEXT("nativeweb.hud.hide"), TEXT("Hide the HUD fixture in this world."),
			FConsoleCommandWithWorldAndArgsDelegate::CreateRaw(this, &FVueExampleModule::HideInWorld), ECVF_Cheat));
		ConsoleCommands.Add(Console.RegisterConsoleCommand(TEXT("nativeweb.hud.close"), TEXT("Close the HUD fixture in this world."),
			FConsoleCommandWithWorldAndArgsDelegate::CreateRaw(this, &FVueExampleModule::CloseInWorld), ECVF_Cheat));
		ConsoleCommands.Add(Console.RegisterConsoleCommand(TEXT("nativeweb.hud.status"), TEXT("Log HUD fixture state in this world."),
			FConsoleCommandWithWorldAndArgsDelegate::CreateRaw(this, &FVueExampleModule::StatusInWorld), ECVF_Cheat));
		ConsoleCommands.Add(Console.RegisterConsoleCommand(TEXT("nativeweb.hud.recreate"), TEXT("Recreate the HUD fixture in this world."),
			FConsoleCommandWithWorldAndArgsDelegate::CreateRaw(this, &FVueExampleModule::RecreateInWorld), ECVF_Cheat));
	}

	virtual void ShutdownModule() override
	{
		if (WorldTickHandle.IsValid())
		{
			FWorldDelegates::OnWorldPostActorTick.Remove(WorldTickHandle);
			WorldTickHandle.Reset();
		}
		for (IConsoleObject* Command : ConsoleCommands)
			if (Command) IConsoleManager::Get().UnregisterConsoleObject(Command, false);
		ConsoleCommands.Reset();
		for (const TPair<TWeakObjectPtr<UNativeWebView>, FDelegateHandle>& Binding : MessageBindings)
			if (UNativeWebView* View = Binding.Key.Get()) View->OnMessage().Remove(Binding.Value);
		MessageBindings.Reset();
	}

private:
	struct FOwnerTrace
	{
		TWeakObjectPtr<UWorld> World;
		FName WorldName = NAME_None;
		UPTRINT WorldIdentity = 0;
	};

	TArray<ULocalPlayer*> GetSelectedPlayers(UWorld* World, const TArray<FString>& Args) const
	{
		TArray<ULocalPlayer*> Result;
		UGameInstance* GameInstance = IsValid(World) ? World->GetGameInstance() : nullptr;
		if (!IsValid(GameInstance)) return Result;
		const int32 RequestedIndex = Args.IsEmpty() ? INDEX_NONE : FCString::Atoi(*Args[0]);
		for (ULocalPlayer* Player : GameInstance->GetLocalPlayers())
			if (IsValid(Player) && Player->GetWorld() == World &&
				(RequestedIndex == INDEX_NONE || Player->GetLocalPlayerIndex() == RequestedIndex))
				Result.Add(Player);
		return Result;
	}

	void EnableInWorld(const TArray<FString>& Args, UWorld* World)
	{
		for (ULocalPlayer* Player : GetSelectedPlayers(World, Args))
		{
			DisabledPlayers.Remove(Player);
			ExplicitlyEnabledPlayers.Add(Player);
		}
	}

	void ShowInWorld(const TArray<FString>& Args, UWorld* World)
	{
		for (ULocalPlayer* Player : GetSelectedPlayers(World, Args))
			if (UNativeWebLocalPlayerSubsystem* Subsystem = Player->GetSubsystem<UNativeWebLocalPlayerSubsystem>())
				if (UNativeWebView* View = Subsystem->FindView(HudSmokeViewName)) View->Show();
	}

	void HideInWorld(const TArray<FString>& Args, UWorld* World)
	{
		for (ULocalPlayer* Player : GetSelectedPlayers(World, Args))
			if (UNativeWebLocalPlayerSubsystem* Subsystem = Player->GetSubsystem<UNativeWebLocalPlayerSubsystem>())
				if (UNativeWebView* View = Subsystem->FindView(HudSmokeViewName)) View->Hide();
	}

	void CloseInWorld(const TArray<FString>& Args, UWorld* World)
	{
		for (ULocalPlayer* Player : GetSelectedPlayers(World, Args))
		{
			DisabledPlayers.Add(Player);
			if (UNativeWebLocalPlayerSubsystem* Subsystem = Player->GetSubsystem<UNativeWebLocalPlayerSubsystem>())
				Subsystem->DestroyView(HudSmokeViewName);
		}
	}

	void RecreateInWorld(const TArray<FString>& Args, UWorld* World)
	{
		EnableInWorld(Args, World);
		for (ULocalPlayer* Player : GetSelectedPlayers(World, Args)) FailedPlayers.Remove(Player);
	}

	void StatusInWorld(const TArray<FString>& Args, UWorld* World)
	{
		for (ULocalPlayer* Player : GetSelectedPlayers(World, Args))
		{
			UNativeWebLocalPlayerSubsystem* Subsystem = Player->GetSubsystem<UNativeWebLocalPlayerSubsystem>();
			UNativeWebView* View = IsValid(Subsystem) ? Subsystem->FindView(HudSmokeViewName) : nullptr;
			UE_LOG(LogTemp, Display,
				TEXT("NATIVEWEB_HUD_SMOKE_STATUS world=%s world_ptr=%p player=%d player_ptr=%p view_ptr=%p visible=%s mode=%s pending=%s fallback=%s failure=%s"),
				World ? *World->GetName() : TEXT("<null>"), World, Player->GetLocalPlayerIndex(), Player, View,
				View && View->IsVisible() ? TEXT("true") : TEXT("false"),
				View ? *UEnum::GetValueAsString(View->GetActivePresentationMode()) : TEXT("None"),
				View && View->IsPresentationPending() ? TEXT("true") : TEXT("false"),
				View && View->IsPresentationFallbackActive() ? TEXT("true") : TEXT("false"),
				View ? *View->GetPresentationFailureReason() : TEXT("<no view>"));
			if (View)
			{
				FNativeWebError ScriptError;
				const FString StatusScript = FString::Printf(
					TEXT("if(typeof globalThis.__nativeWebHudStarts!=='number'||typeof globalThis.__nativeWebHudClicks!=='number')throw new Error('HUD state is unavailable');nativeWeb.postString('view=%s world=%s pid=%u player=%d starts='+globalThis.__nativeWebHudStarts+' clicks='+globalThis.__nativeWebHudClicks,'hud-status')"),
					*HudSmokeViewName.ToString(), *World->GetName(),
					FPlatformProcess::GetCurrentProcessId(), Player->GetLocalPlayerIndex());
				if (!View->EvaluateScript(StatusScript, TEXT("NativeWebHudStatus"), ScriptError))
					UE_LOG(LogTemp, Warning, TEXT("NATIVEWEB_HUD_SMOKE_STATUS_SCRIPT_FAILED player=%d error=%s"),
						Player->GetLocalPlayerIndex(), *ScriptError.Message);
			}
		}
	}

	void OnHudSmokeMessage(const FNativeWebMessage& Message)
	{
		if (Message.Channel == FName(TEXT("hud-click")))
		{
			UE_LOG(LogTemp, Display, TEXT("NATIVEWEB_HUD_SMOKE_CLICK payload=%s"), *Message.String);
		}
		else if (Message.Channel == FName(TEXT("hud-status")))
		{
			UE_LOG(LogTemp, Display, TEXT("NATIVEWEB_HUD_SMOKE_JS_STATUS payload=%s"), *Message.String);
		}
	}

	void DrawColoredSmokeBackdrop(UWorld* World, ULocalPlayer* Player)
	{
		if (!FParse::Param(FCommandLine::Get(), TEXT("NativeWebHudSmokeColoredBackdrop")) ||
			!IsValid(World) || !IsValid(Player)) return;
		APlayerController* Controller = Player->GetPlayerController(World);
		APlayerCameraManager* Camera = Controller ? Controller->PlayerCameraManager : nullptr;
		if (!IsValid(Camera)) return;
		const FVector Forward = Camera->GetCameraRotation().Vector();
		const FVector PlaneLocation = Camera->GetCameraLocation() + Forward * 600.0f;
		DrawDebugSolidPlane(World, FPlane(Forward, 0.0f), PlaneLocation, 5000.0f,
			FColor::Magenta, false, 3600.0f, 0);
		UE_LOG(LogTemp, Display,
			TEXT("NATIVEWEB_HUD_SMOKE_COLORED_BACKDROP color=magenta world=%s player=%d"),
			*World->GetName(), Player->GetLocalPlayerIndex());
	}

	bool IsHudSmokeEnabled(const ULocalPlayer* Player) const
	{
		return Player && !DisabledPlayers.Contains(Player) &&
			(FParse::Param(FCommandLine::Get(), TEXT("NativeWebHudSmoke")) || ExplicitlyEnabledPlayers.Contains(Player));
	}

	void OnWorldPostActorTick(UWorld* World, ELevelTick, float)
	{
		if (!IsValid(World) || (World->WorldType != EWorldType::PIE && World->WorldType != EWorldType::Game)) return;
		UGameInstance* GameInstance = World->GetGameInstance();
		if (!IsValid(GameInstance)) return;
		bool bAnySmokeEnabled = false;
		for (ULocalPlayer* Player : GameInstance->GetLocalPlayers())
			bAnySmokeEnabled |= IsValid(Player) && Player->GetWorld() == World && IsHudSmokeEnabled(Player);
		if (bAnySmokeEnabled)
			if (AGameModeBase* GameMode = World->GetAuthGameMode()) GameMode->bUseSeamlessTravel = true;

		for (ULocalPlayer* Player : GameInstance->GetLocalPlayers())
		{
			if (!IsValid(Player) || Player->GetWorld() != World) continue;
			UNativeWebLocalPlayerSubsystem* Subsystem = Player->GetSubsystem<UNativeWebLocalPlayerSubsystem>();
			if (!IsValid(Subsystem)) continue;
			if (FParse::Param(FCommandLine::Get(), TEXT("NativeWebHudSmokeHideMain")))
			{
				if (UNativeWebView* MainView = Subsystem->FindView(FName(TEXT("Main"))))
				{
					TWeakObjectPtr<UNativeWebView>& HiddenMain = MainHiddenViews.FindOrAdd(Player);
					if (HiddenMain.Get() != MainView)
					{
						MainView->Hide();
						HiddenMain = MainView;
						UE_LOG(LogTemp, Display, TEXT("NATIVEWEB_HUD_SMOKE_HID_MAIN world=%s player=%d view_ptr=%p"),
							*World->GetName(), Player->GetLocalPlayerIndex(), MainView);
					}
				}
			}
			if (!IsHudSmokeEnabled(Player) || FailedPlayers.Contains(Player)) continue;
			if (UNativeWebView* ExistingView = Subsystem->FindView(HudSmokeViewName))
			{
				FOwnerTrace& LastOwner = LastWorldByPlayer.FindOrAdd(Player);
				const UPTRINT CurrentWorldIdentity = reinterpret_cast<UPTRINT>(World);
				if (LastOwner.WorldIdentity != 0 &&
					(LastOwner.WorldIdentity != CurrentWorldIdentity || LastOwner.WorldName != World->GetFName()))
				{
					FNativeWebError StateError;
					const bool bPersistentScriptState = ExistingView->EvaluateScript(
						TEXT("if(globalThis.__nativeWebHudStarts!==1||typeof globalThis.__nativeWebClick!=='function'||nativeWeb.setText('bridge-status','travel-preserved')!==true)throw new Error('HUD JS state was lost during travel')"),
						TEXT("NativeWebHudTravelCheck"), StateError);
					UE_LOG(LogTemp, Display,
						TEXT("NATIVEWEB_HUD_SMOKE_TRAVEL old_world=%s old_world_identity=%llu new_world=%s new_world_identity=%llu player=%d player_ptr=%p view_ptr=%p js_state=%s js_error=%s mode=%s fallback=%s"),
						*LastOwner.WorldName.ToString(), static_cast<unsigned long long>(LastOwner.WorldIdentity),
						*World->GetName(), static_cast<unsigned long long>(CurrentWorldIdentity), Player->GetLocalPlayerIndex(),
						Player, ExistingView, bPersistentScriptState ? TEXT("preserved") : TEXT("failed"), *StateError.Message,
						*UEnum::GetValueAsString(ExistingView->GetActivePresentationMode()),
						ExistingView->IsPresentationFallbackActive() ? TEXT("true") : TEXT("false"));
					DrawColoredSmokeBackdrop(World, Player);
				}
				LastOwner.World = World;
				LastOwner.WorldName = World->GetFName();
				LastOwner.WorldIdentity = CurrentWorldIdentity;
				continue;
			}

			FNativeWebSurfaceConfig Surface;
			if (FParse::Param(FCommandLine::Get(), TEXT("NativeWebHudSmokeOpaque")))
				Surface.ClearColor = FColor(0, 0, 0, 255);
			FNativeWebPresentationConfig Presentation;
			Presentation.PresentationMode = FParse::Param(FCommandLine::Get(), TEXT("NativeWebHudSmokeSlate"))
				? ENativeWebPresentationMode::Slate : ENativeWebPresentationMode::GpuViewport;
			Presentation.bFallbackToSlate = false;
			Presentation.ViewportZOrder = 10000;
			const FString Html = TEXT("<!doctype html><html><head><meta charset='utf-8'></head><body><div id='app'></div><span id='bridge-status'></span></body></html>");
			const FString Css = TEXT(
				"html,body{margin:0;width:100%;height:100%;background:transparent;color:white;font:18px sans-serif}"
				"body{display:grid;place-items:center}#bridge-status{display:none}"
				"#panel{min-width:360px;padding:24px 30px;border:3px solid #fff;border-radius:12px;"
				"background:rgba(24,92,190,.62);text-align:center}#count{min-width:240px;min-height:60px;font-size:18px;padding:10px 16px}");
			FNativeWebError Error;
			UNativeWebView* View = Subsystem->CreateInlineView(HudSmokeViewName, Html, Css, Surface, Presentation, Error);
			if (!IsValid(View))
			{
				UE_LOG(LogTemp, Error, TEXT("NATIVEWEB_HUD_SMOKE_CREATE_FAILED world=%s player=%d code=%s message=%s"),
					*World->GetName(), Player->GetLocalPlayerIndex(), *Error.Code.ToString(), *Error.Message);
				FailedPlayers.Add(Player);
				continue;
			}
			MessageBindings.Add(View, View->OnMessage().AddRaw(this, &FVueExampleModule::OnHudSmokeMessage));
			const FString Script = FString::Printf(
				TEXT("globalThis.__nativeWebHudStarts=(globalThis.__nativeWebHudStarts||0)+1;"
				"if(nativeWeb.setText('bridge-status','ready')!==true)throw new Error('NativeWeb text bridge failed');"
				"const root=nativeWeb.mountRoot('app');"
				"const panel=nativeWeb.createElement('main');nativeWeb.setAttribute(panel,'id','panel');nativeWeb.insertBefore(root,panel,null);"
				"const title=nativeWeb.createElement('strong');nativeWeb.insertBefore(panel,title,null);"
				"nativeWeb.insertBefore(title,nativeWeb.createText('NativeWeb HUD smoke'),null);"
				"const instance=nativeWeb.createElement('p');nativeWeb.insertBefore(panel,instance,null);"
				"nativeWeb.insertBefore(instance,nativeWeb.createText('view %s | %s | PID %u | player %d'),null);"
				"const starts=nativeWeb.createElement('p');nativeWeb.insertBefore(panel,starts,null);"
				"nativeWeb.insertBefore(starts,nativeWeb.createText('Starts: '),null);"
				"const startCount=nativeWeb.createText(String(globalThis.__nativeWebHudStarts));nativeWeb.insertBefore(starts,startCount,null);"
				"const clicks=nativeWeb.createElement('p');nativeWeb.insertBefore(panel,clicks,null);"
				"nativeWeb.insertBefore(clicks,nativeWeb.createText('Clicks: '),null);"
				"const clickCount=nativeWeb.createText('0');nativeWeb.insertBefore(clicks,clickCount,null);"
				"const button=nativeWeb.createElement('button');nativeWeb.setAttribute(button,'id','count');nativeWeb.insertBefore(panel,button,null);"
				"nativeWeb.insertBefore(button,nativeWeb.createText('Count click'),null);"
				"globalThis.__nativeWebHudClicks=0;globalThis.__nativeWebClick=(nodeId)=>{"
				"globalThis.__nativeWebHudClicks++;nativeWeb.setNodeText(clickCount,String(globalThis.__nativeWebHudClicks));"
				"if(nativeWeb.setText('bridge-status','clicks:'+globalThis.__nativeWebHudClicks)!==true||"
				"nativeWeb.postString('view=%s world=%s pid=%u player=%d clicks='+globalThis.__nativeWebHudClicks,'hud-click')!==true)"
				"throw new Error('NativeWeb click reporting failed');};"
				"if(nativeWeb.setAttribute(button,'class','nativeweb-click-'+button)!==true||"
				"nativeWeb.setClickHandler(button)!==true)throw new Error('NativeWeb click setup failed');"),
				*HudSmokeViewName.ToString(), *World->GetName(),
				FPlatformProcess::GetCurrentProcessId(), Player->GetLocalPlayerIndex(),
				*HudSmokeViewName.ToString(), *World->GetName(),
				FPlatformProcess::GetCurrentProcessId(), Player->GetLocalPlayerIndex());
			if (!View->EvaluateScript(Script, TEXT("NativeWebHudSmoke"), Error))
			{
				UE_LOG(LogTemp, Error, TEXT("NATIVEWEB_HUD_SMOKE_SCRIPT_FAILED world=%s player=%d message=%s"),
					*World->GetName(), Player->GetLocalPlayerIndex(), *Error.Message);
				Subsystem->DestroyView(HudSmokeViewName);
				FailedPlayers.Add(Player);
				continue;
			}
			UE_LOG(LogTemp, Display,
				TEXT("NATIVEWEB_HUD_SMOKE_READY world=%s world_ptr=%p player=%d player_ptr=%p view_ptr=%p mode=%s"),
				*World->GetName(), World, Player->GetLocalPlayerIndex(), Player, View,
				*UEnum::GetValueAsString(Presentation.PresentationMode));
			FOwnerTrace& Owner = LastWorldByPlayer.FindOrAdd(Player);
			Owner.World = World;
			Owner.WorldName = World->GetFName();
			Owner.WorldIdentity = reinterpret_cast<UPTRINT>(World);
			DrawColoredSmokeBackdrop(World, Player);
		}
	}

	FDelegateHandle WorldTickHandle;
	TArray<IConsoleObject*> ConsoleCommands;
	TMap<TWeakObjectPtr<ULocalPlayer>, FOwnerTrace> LastWorldByPlayer;
	TSet<TWeakObjectPtr<ULocalPlayer>> ExplicitlyEnabledPlayers;
	TSet<TWeakObjectPtr<ULocalPlayer>> DisabledPlayers;
	TSet<TWeakObjectPtr<ULocalPlayer>> FailedPlayers;
	TMap<TWeakObjectPtr<ULocalPlayer>, TWeakObjectPtr<UNativeWebView>> MainHiddenViews;
	TMap<TWeakObjectPtr<UNativeWebView>, FDelegateHandle> MessageBindings;
};

IMPLEMENT_PRIMARY_GAME_MODULE(FVueExampleModule, VueExample, "VueExample");
