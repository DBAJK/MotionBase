#include "Core/ModeManager.h"
#include "MotionBase.h"
#include "Save/MotionBaseSaveGame.h"
#include "Kismet/GameplayStatics.h"

void UModeManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 슬롯이 있으면 로드, 없으면 새 저장 오브젝트를 만든다.
	const FString Slot = UMotionBaseSaveGame::DefaultSlotName;
	const uint32 User = UMotionBaseSaveGame::DefaultUserIndex;

	if (UGameplayStatics::DoesSaveGameExist(Slot, User))
	{
		SaveData = Cast<UMotionBaseSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, User));
	}

	if (!SaveData)
	{
		SaveData = Cast<UMotionBaseSaveGame>(
			UGameplayStatics::CreateSaveGameObject(UMotionBaseSaveGame::StaticClass()));
	}

	UE_LOG(LogMotionBase, Log, TEXT("ModeManager: 저장 로드 — 누적 세션 %d건"),
		SaveData ? SaveData->History.Num() : 0);
}

void UModeManager::Deinitialize()
{
	// 앱 종료·레벨 정리 시점에 폰이 flush 하지 못한 세션이 남아 있을 수 있다.
	// FinalizeSession 이 빈 세션은 스스로 걸러내므로 무조건 한 번 호출해도 안전하다.
	// (여기선 집계 평균을 다시 구하지 않고, 시도들만 확정 저장한다.)
	FinalizeSession(FScoreResult());

	Super::Deinitialize();
}

void UModeManager::SetActiveMode(EGameModeId NewMode, EDifficultyLevel NewDifficulty)
{
	const bool bChanged = (NewMode != ActiveMode);
	ActiveMode = NewMode;
	ActiveDifficulty = NewDifficulty;

	// 모드 진입 = 새 세션. 같은 모드를 다시 고른 경우에도 반드시 비운다.
	SessionResults.Reset();
	SessionStartedAt = FDateTime::Now();

	UE_LOG(LogMotionBase, Log, TEXT("ModeManager: 모드 진입 → %s / 난이도 %s (세션 초기화)"),
		*GetModeDisplayName(NewMode).ToString(), *GetDifficultyDisplayName(NewDifficulty).ToString());

	if (bChanged)
	{
		OnModeChanged.Broadcast(NewMode);
	}
}

void UModeManager::ClearSessionResults()
{
	SessionResults.Reset();
}

void UModeManager::RecordResult(const FScoreResult& Result)
{
	FScoreResult Stored = Result;

	// 점수 계층은 모드를 모르므로 ModeId 가 비어 온다. 여기서 채워야
	// 저장·AI 피드백 단계에서 "어느 모드 기록인지"를 잃지 않는다.
	if (Stored.ModeId.IsNone())
	{
		Stored.ModeId = GetModeIdName(ActiveMode);
	}

	SessionResults.Add(Stored);

	UE_LOG(LogMotionBase, Log, TEXT("ModeManager: 결과 기록 (mode=%s total=%.1f) 누적 %d건"),
		*Stored.ModeId.ToString(), Stored.TotalScore, SessionResults.Num());

	// TODO: PlayResultLogger / SaveGame 로 영속화, 일정 누적 시 AI 피드백 트리거.
}

TArray<EGameModeId> UModeManager::GetMenuModes()
{
	return {
		EGameModeId::BodyScan,
		EGameModeId::ReactionSpeed,
		EGameModeId::Defense,
		EGameModeId::BaseRunning,
		EGameModeId::Batting,
		EGameModeId::FunctionalFitness
	};
}

FText UModeManager::GetModeDisplayName(EGameModeId Mode)
{
	switch (Mode)
	{
	case EGameModeId::BodyScan:          return FText::FromString(TEXT("신체 인식 (셋업)"));
	case EGameModeId::ReactionSpeed:     return FText::FromString(TEXT("반응속도 훈련"));
	case EGameModeId::Defense:           return FText::FromString(TEXT("수비 훈련"));
	case EGameModeId::BaseRunning:       return FText::FromString(TEXT("베이스 러닝"));
	case EGameModeId::Batting:           return FText::FromString(TEXT("타격 훈련"));
	case EGameModeId::FunctionalFitness: return FText::FromString(TEXT("기능성 피트니스"));
	default:                             return FText::FromString(TEXT("알 수 없는 모드"));
	}
}

FName UModeManager::GetModeIdName(EGameModeId Mode)
{
	switch (Mode)
	{
	case EGameModeId::BodyScan:          return TEXT("BodyScan");
	case EGameModeId::ReactionSpeed:     return TEXT("ReactionSpeed");
	case EGameModeId::Defense:           return TEXT("Defense");
	case EGameModeId::BaseRunning:       return TEXT("BaseRunning");
	case EGameModeId::Batting:           return TEXT("Batting");
	case EGameModeId::FunctionalFitness: return TEXT("FunctionalFitness");
	default:                             return NAME_None;
	}
}

FText UModeManager::GetModeDescription(EGameModeId Mode)
{
	switch (Mode)
	{
	case EGameModeId::BodyScan:
		return FText::FromString(TEXT("키·팔 길이·활동 범위를 측정해 개인 난이도 기준선을 만듭니다. (LiDAR)"));
	case EGameModeId::ReactionSpeed:
		return FText::FromString(TEXT("바닥에 점등되는 타깃을 밟아 반응속도와 콤보를 겨룹니다. (LiDAR)"));
	case EGameModeId::Defense:
		return FText::FromString(TEXT("좌우 이동·점프·숙이기로 타구를 처리합니다. (LiDAR 전신)"));
	case EGameModeId::BaseRunning:
		return FText::FromString(TEXT("주루 타이밍과 이동 속도로 세이프/아웃을 판정합니다. (LiDAR)"));
	case EGameModeId::Batting:
		return FText::FromString(TEXT("날아오는 공에 타이밍을 맞춰 스윙합니다. 정확도·효율·일관성 3축 채점."));
	case EGameModeId::FunctionalFitness:
		return FText::FromString(TEXT("관절각과 반복 횟수를 측정하는 야구 특화 피트니스. (LiDAR)"));
	default:
		return FText::GetEmpty();
	}
}

bool UModeManager::IsModeImplemented(EGameModeId Mode)
{
	// ROADMAP Phase 1 기준: 타격만 플레이 가능. 나머지는 Phase 3~4 에서 열린다.
	// 모드를 구현하면 여기에 추가하고 AMotionBaseGameMode::GetPawnClassForMode 에도 폰을 등록할 것.
	return Mode == EGameModeId::Batting;
}

TArray<EDifficultyLevel> UModeManager::GetMenuDifficulties()
{
	return { EDifficultyLevel::Beginner, EDifficultyLevel::Amateur, EDifficultyLevel::Pro };
}

FText UModeManager::GetDifficultyDisplayName(EDifficultyLevel Level)
{
	switch (Level)
	{
	case EDifficultyLevel::Beginner: return FText::FromString(TEXT("초보"));
	case EDifficultyLevel::Amateur:  return FText::FromString(TEXT("아마추어"));
	case EDifficultyLevel::Pro:      return FText::FromString(TEXT("프로"));
	default:                         return FText::FromString(TEXT("알 수 없음"));
	}
}

FText UModeManager::GetDifficultyDescription(EDifficultyLevel Level)
{
	switch (Level)
	{
	case EDifficultyLevel::Beginner:
		return FText::FromString(TEXT("느린 직구 위주, 변화구 없음, 여유로운 간격. 타이밍 감을 익히는 단계."));
	case EDifficultyLevel::Amateur:
		return FText::FromString(TEXT("보통 구속에 변화구가 섞입니다. 코스도 넓어집니다."));
	case EDifficultyLevel::Pro:
		return FText::FromString(TEXT("빠른 구속과 잦은 변화구, 짧은 간격. 실전에 가까운 난이도."));
	default:
		return FText::GetEmpty();
	}
}

FName UModeManager::GetDifficultyIdName(EDifficultyLevel Level)
{
	switch (Level)
	{
	case EDifficultyLevel::Beginner: return TEXT("Beginner");
	case EDifficultyLevel::Amateur:  return TEXT("Amateur");
	case EDifficultyLevel::Pro:      return TEXT("Pro");
	default:                         return NAME_None;
	}
}
