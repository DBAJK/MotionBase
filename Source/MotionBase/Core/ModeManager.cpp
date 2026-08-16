#include "Core/ModeManager.h"
#include "MotionBase.h"

void UModeManager::SetActiveMode(EGameModeId NewMode)
{
	const bool bChanged = (NewMode != ActiveMode);
	ActiveMode = NewMode;

	// 모드 진입 = 새 세션. 같은 모드를 다시 고른 경우에도 반드시 비운다.
	SessionResults.Reset();

	UE_LOG(LogMotionBase, Log, TEXT("ModeManager: 모드 진입 → %s (세션 초기화)"),
		*GetModeDisplayName(NewMode).ToString());

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
	return Mode == EGameModeId::Batting
		|| Mode == EGameModeId::Defense;
}
