#include "Core/ModeManager.h"
#include "MotionBase.h"

void UModeManager::SetActiveMode(EGameModeId NewMode)
{
	if (NewMode == ActiveMode)
	{
		return;
	}
	ActiveMode = NewMode;
	UE_LOG(LogMotionBase, Log, TEXT("ModeManager: 모드 전환 → %d"), static_cast<int32>(NewMode));
	OnModeChanged.Broadcast(NewMode);
}

void UModeManager::RecordResult(const FScoreResult& Result)
{
	SessionResults.Add(Result);
	UE_LOG(LogMotionBase, Log, TEXT("ModeManager: 결과 기록 (mode=%s total=%.1f) 누적 %d건"),
		*Result.ModeId.ToString(), Result.TotalScore, SessionResults.Num());

	// TODO: PlayResultLogger / SaveGame 로 영속화, 일정 누적 시 AI 피드백 트리거.
}
