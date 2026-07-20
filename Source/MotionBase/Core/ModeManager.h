#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Data/MotionBaseTypes.h"
#include "Data/ScoreResult.h"
#include "ModeManager.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnModeChanged, EGameModeId, NewMode);

/**
 * 6개 게임 모드 전환/상태 관리. (Game Logic Layer)
 *
 * GameInstanceSubsystem 으로 두어 레벨 전환에도 세션 상태를 유지.
 * 모드별 채점 결과를 누적하고 Play Result Logger / SaveGame 으로 흘려보낸다.
 */
UCLASS()
class MOTIONBASE_API UModeManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "MotionBase|Mode")
	void SetActiveMode(EGameModeId NewMode);

	UFUNCTION(BlueprintPure, Category = "MotionBase|Mode")
	EGameModeId GetActiveMode() const { return ActiveMode; }

	/** 한 판(모드 세션)의 결과를 기록. */
	UFUNCTION(BlueprintCallable, Category = "MotionBase|Mode")
	void RecordResult(const FScoreResult& Result);

	UPROPERTY(BlueprintAssignable, Category = "MotionBase|Mode")
	FOnModeChanged OnModeChanged;

private:
	UPROPERTY()
	EGameModeId ActiveMode = EGameModeId::BodyScan;

	/** 세션 내 누적 결과 (저장/피드백 입력). */
	UPROPERTY()
	TArray<FScoreResult> SessionResults;
};
