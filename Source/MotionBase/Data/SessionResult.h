#pragma once

#include "CoreMinimal.h"
#include "Data/MotionBaseTypes.h"
#include "Data/ScoreResult.h"
#include "SessionResult.generated.h"

/**
 * 세션 1회(여러 시도)의 집계 결과. 로컬 저장·AI 피드백의 단위.
 * 개별 시도는 FScoreResult, 그 묶음이 FSessionResult.
 */
USTRUCT(BlueprintType)
struct FSessionResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Session")
	EGameModeId Mode = EGameModeId::Batting;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Session")
	FDateTime StartedAt = FDateTime();

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Session")
	int32 AttemptCount = 0;

	/** 시도별 점수. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Session")
	TArray<FScoreResult> Attempts;

	/** 세션 집계 점수 (UScoringService::ScoreSession 결과). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Session")
	FScoreResult Average;

	/** 난이도 레벨. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Session")
	int32 DifficultyLevel = 1;
};
