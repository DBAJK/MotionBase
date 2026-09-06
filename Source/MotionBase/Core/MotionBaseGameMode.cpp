#include "Core/MotionBaseGameMode.h"
#include "MotionBase.h"
#include "Core/ModeManager.h"
#include "Core/ModeSelectPawn.h"
#include "Testing/SwingTestPawn.h"
#include "Testing/VRBattingPawn.h"
#include "Testing/ViveBringupPawn.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "UI/ModeSelectHUD.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Core/Defense/CatchBall/CatchBallPawn.h"
#include "Core/Defense/Throw/ThrowPawn.h"
#include "Core/Defense/Backup/BackupPawn.h"
#include "AI/AICoachingPawn.h"

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
		// HMD 가 연결돼 있으면 VR 타격 폰(컨트롤러 스윙), 아니면 화면 테스트 폰(키보드).
		// → 베이스 스테이션/헤드셋 없이도 PC 개발·시연이 안 막힌다.
		if (UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayEnabled())
		{
			return AVRBattingPawn::StaticClass();
		}
		return ASwingTestPawn::StaticClass();

	// ⚠️ 수비(Defense)는 여기 없다 — 세부 종목(포구/송구/백업)을 고르지 않으면 띄울 폰이
	//    정해지지 않기 때문. 진입은 항상 StartDefenseDrill 을 거친다.
	//    (예전엔 빈 스텁 ADefensePawn 을 돌려줬는데, 모드 선택이 항상 종목 서브메뉴로 가므로
	//     한 번도 실행되지 않는 죽은 경로였다.)
	default:
		// 나머지 모드는 미구현 (ROADMAP Phase 3~4).
		return nullptr;
	}
}

bool AMotionBaseGameMode::StartMode(EGameModeId Mode, EDifficultyLevel Difficulty, EBattingStance Stance)
{
	if (!UModeManager::IsModeImplemented(Mode))
	{
		UE_LOG(LogMotionBase, Log, TEXT("GameMode: %s 은(는) 아직 준비 중인 모드입니다."),
			*UModeManager::GetModeDisplayName(Mode).ToString());
		return false;
	}

	// 수비는 세부 종목을 고르기 전엔 진입할 수 없다 (StartDefenseDrill 이 진입 경로).
	if (Mode == EGameModeId::Defense)
	{
		UE_LOG(LogMotionBase, Warning,
			TEXT("GameMode: 수비는 세부 종목이 필요합니다 — StartDefenseDrill(0=포구/1=송구/2=백업) 을 쓰세요."));
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

	// 모드 진입 = 새 세션. 모드+난이도를 저장하고 이전 누적 결과를 비운다.
	// (모드 폰이 BeginPlay 에서 난이도를 읽어 파라미터에 반영한다.)
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UModeManager* ModeManager = GI->GetSubsystem<UModeManager>())
		{
			ModeManager->SetActiveMode(Mode, Difficulty, Stance);
		}
	}

	RequestPawnSwap(PawnClass);

	UE_LOG(LogMotionBase, Log, TEXT("GameMode: 모드 시작 → %s / %s / %s"),
		*UModeManager::GetModeDisplayName(Mode).ToString(),
		*UModeManager::GetDifficultyDisplayName(Difficulty).ToString(),
		*UModeManager::GetStanceDisplayName(Stance).ToString());
	return true;
}

void AMotionBaseGameMode::StartAICoaching()
{
	// 읽기 전용 리뷰 — 세션 상태(SetActiveMode)를 건드리지 않고 폰만 교체한다.
	UE_LOG(LogMotionBase, Log, TEXT("GameMode: AI 코칭(운동 추천) 화면 진입"));
	RequestPawnSwap(AAICoachingPawn::StaticClass());
}

void AMotionBaseGameMode::ReturnToModeSelect()
{
	UE_LOG(LogMotionBase, Log, TEXT("GameMode: 시작 화면으로 복귀"));
	RequestPawnSwap(AModeSelectPawn::StaticClass());
}

void AMotionBaseGameMode::StartViveBringup()
{
	UE_LOG(LogMotionBase, Log, TEXT("GameMode: Vive 브링업 진단 진입"));
	RequestPawnSwap(AViveBringupPawn::StaticClass());
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

bool AMotionBaseGameMode::StartDefenseDrill(int32 DrillIndex, EFieldPosition Position)
{
	// 종목별 폰 결정 (0=포구, 1=송구, 2=백업). 풋워크·반응속도는 메뉴에서 제외됨.
	TSubclassOf<APawn> PawnClass = nullptr;
	switch (DrillIndex)
	{
	case 0: // 포구
		PawnClass = ACatchBallPawn::StaticClass();
		break;
	case 1: // 송구
		PawnClass = AThrowPawn::StaticClass();
		break;
	case 2: // 백업 위치 판단 — 실제 이동 훈련 (ABackupPawn, 4지선다 퀴즈 ACoverPawn 대체)
		PawnClass = ABackupPawn::StaticClass();
		break;
	default:
		UE_LOG(LogMotionBase, Log, TEXT("GameMode: 수비 세부 종목 %d 은(는) 아직 준비 중입니다."), DrillIndex);
		return false;
	}

	// 세션 진입 처리 (모드는 Defense, 세부 종목은 DrillId 로 기록).
	// 순서 주의: SetActiveMode 가 새 세션을 열며 세부 종목·포지션을 비우므로 그다음에 지정한다.
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UModeManager* MM = GI->GetSubsystem<UModeManager>())
		{
			MM->SetActiveMode(EGameModeId::Defense);
			MM->SetActiveDrill(UModeManager::GetDefenseDrillIdName(DrillIndex));
			if (DrillIndex == 2)
			{
				MM->SetActiveFieldPosition(Position);
			}
		}
	}

	RequestPawnSwap(PawnClass);
	UE_LOG(LogMotionBase, Log, TEXT("GameMode: 수비 종목 시작 → %d"), DrillIndex);
	return true;
}
