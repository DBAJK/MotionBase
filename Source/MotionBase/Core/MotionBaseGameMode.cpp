#include "Core/MotionBaseGameMode.h"
#include "MotionBase.h"
#include "Core/ModeManager.h"
#include "Core/ModeSelectPawn.h"
#include "Testing/SwingTestPawn.h"
#include "Core/Defense/DefensePawn.h"
#include "UI/ModeSelectHUD.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Core/Defense/CatchBall/CatchBallPawn.h"
#include "Core/Defense/Throw/ThrowPawn.h"
#include "Core/Defense/Cover/CoverPawn.h"

AMotionBaseGameMode::AMotionBaseGameMode()
{
	PrimaryActorTick.bCanEverTick = false;

	// 시작 화면부터 진입. 모드 선택 후 폰을 교체한다.
	DefaultPawnClass = AModeSelectPawn::StaticClass();
	HUDClass = AModeSelectHUD::StaticClass();
}

TSubclassOf<APawn> AMotionBaseGameMode::GetPawnClassForMode(EGameModeId Mode) const
{
	switch (Mode)
	{
	case EGameModeId::Batting:
		// Stage 1 화면 테스트 폰. Vive 배선 후 ABat 기반 VR 폰으로 교체 예정.
		return ASwingTestPawn::StaticClass();

	case EGameModeId::Defense:
		return ADefensePawn::StaticClass();
		
	default:
		// 나머지 모드는 미구현 (ROADMAP Phase 3~4).
		return nullptr;
	}
}

bool AMotionBaseGameMode::StartMode(EGameModeId Mode)
{
	if (!UModeManager::IsModeImplemented(Mode))
	{
		UE_LOG(LogMotionBase, Log, TEXT("GameMode: %s 은(는) 아직 준비 중인 모드입니다."),
			*UModeManager::GetModeDisplayName(Mode).ToString());
		return false;
	}

	const TSubclassOf<APawn> PawnClass = GetPawnClassForMode(Mode);
	if (!PawnClass)
	{
		// IsModeImplemented 와 폰 등록이 어긋난 경우 — 둘 다 갱신해야 한다.
		UE_LOG(LogMotionBase, Warning,
			TEXT("GameMode: %s 은(는) 구현됨으로 표시됐지만 폰이 등록되지 않았습니다. GetPawnClassForMode 를 확인하세요."),
			*UModeManager::GetModeDisplayName(Mode).ToString());
		return false;
	}

	// 모드 진입 = 새 세션. 이전 모드의 누적 결과를 비운다.
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UModeManager* ModeManager = GI->GetSubsystem<UModeManager>())
		{
			ModeManager->SetActiveMode(Mode);
		}
	}

	RequestPawnSwap(PawnClass);

	UE_LOG(LogMotionBase, Log, TEXT("GameMode: 모드 시작 → %s"),
		*UModeManager::GetModeDisplayName(Mode).ToString());
	return true;
}

void AMotionBaseGameMode::ReturnToModeSelect()
{
	UE_LOG(LogMotionBase, Log, TEXT("GameMode: 시작 화면으로 복귀"));
	RequestPawnSwap(AModeSelectPawn::StaticClass());
}

void AMotionBaseGameMode::RequestPawnSwap(TSubclassOf<APawn> NewPawnClass)
{
	if (!NewPawnClass || !GetWorld())
	{
		return;
	}

	PendingPawnClass = NewPawnClass;
	GetWorldTimerManager().SetTimerForNextTick(this, &AMotionBaseGameMode::ApplyPendingPawnSwap);
}

void AMotionBaseGameMode::ApplyPendingPawnSwap()
{
	// 한 틱 안에 요청이 겹쳐도 마지막 것 한 번만 반영한다.
	const TSubclassOf<APawn> NewPawnClass = PendingPawnClass;
	PendingPawnClass = nullptr;

	UWorld* World = GetWorld();
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!NewPawnClass || !World || !PC)
	{
		return;
	}

	APawn* OldPawn = PC->GetPawn();

	// 새 폰을 기존 폰 자리에 놓는다 (없으면 원점). 시점이 튀지 않고,
	// 모드 폰이 자기 위치 기준으로 배치하는 액터(APitchingZone 등)도 그대로 맞는다.
	const FTransform SpawnTM = OldPawn ? OldPawn->GetActorTransform() : FTransform::Identity;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	APawn* NewPawn = World->SpawnActor<APawn>(NewPawnClass, SpawnTM, Params);
	if (!NewPawn)
	{
		// 생성 실패 시 기존 폰을 그대로 둔다 — 조작 불능 상태로 빠지지 않게.
		UE_LOG(LogMotionBase, Warning, TEXT("GameMode: 폰 생성 실패 (%s) — 전환을 취소합니다."),
			*NewPawnClass->GetName());
		return;
	}

	if (OldPawn)
	{
		PC->UnPossess();
	}

	PC->Possess(NewPawn);

	// 빙의를 옮긴 뒤에 파괴한다. 이전 폰의 EndPlay 에서
	// 자기가 만든 액터(APitchingZone 등)를 정리한다.
	if (OldPawn)
	{
		OldPawn->Destroy();
	}
}

bool AMotionBaseGameMode::StartDefenseDrill(int32 DrillIndex)
{
	// 종목별 폰 결정. 지금은 0=포구만 실제 구현, 나머지는 자리표시자(DefensePawn).
	TSubclassOf<APawn> PawnClass = nullptr;
	switch (DrillIndex)
	{
	case 0: // 포구
		PawnClass = ACatchBallPawn::StaticClass();
		break;
	case 1: // 송구
		PawnClass = AThrowPawn::StaticClass();
		break;
	case 3: // 백업(커버)
		PawnClass = ACoverPawn::StaticClass();
		break;
	default: // 송구/풋워크/백업 — 아직 미구현
		UE_LOG(LogMotionBase, Log, TEXT("GameMode: 수비 세부 종목 %d 은(는) 아직 준비 중입니다."), DrillIndex);
		return false;
	}

	// 세션 진입 처리 (모드는 Defense 로 기록).
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UModeManager* MM = GI->GetSubsystem<UModeManager>())
		{
			MM->SetActiveMode(EGameModeId::Defense);
		}
	}

	RequestPawnSwap(PawnClass);
	UE_LOG(LogMotionBase, Log, TEXT("GameMode: 수비 종목 시작 → %d"), DrillIndex);
	return true;
}
