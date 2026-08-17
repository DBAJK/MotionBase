#pragma once

#include "CoreMinimal.h"
#include "Data/MotionBaseTypes.h"
#include "Data/ScoreResult.h"
#include "Data/TrainingFeedback.h"
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

	/**
	 * 이 세션의 약점 리포트 (UWeaknessDetector 결정론적 산출).
	 * 세션마다 저장해 두면 여러 세션에 걸친 만성 약점·개선 추세를 계산할 수 있다.
	 * bValid=false 면 리포트 없이 저장된 세션(옛 기록·표본 부족).
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Session")
	FWeaknessReport Report;
};

/**
 * 한 모드의 누적 기록 집계 (저장 이력에서 계산). 결과 화면의 "기록/추세" 표시 입력.
 * ⚠️ "현재 진행 중이나 아직 저장되지 않은 세션"은 포함하지 않는다 — 그래서 결과 화면이
 *    이번 판을 과거 기록과 비교("평균 대비", "직전 대비", "신기록")할 수 있다.
 */
USTRUCT(BlueprintType)
struct FModeStats
{
	GENERATED_BODY()

	/** 이 모드로 저장된 세션 수. */
	UPROPERTY(BlueprintReadWrite, Category = "Stats")
	int32 SessionCount = 0;

	/** 역대 최고 총점. 기록이 없으면 음수(-1). */
	UPROPERTY(BlueprintReadWrite, Category = "Stats")
	float BestTotal = -1.0f;

	/** 평균 총점. */
	UPROPERTY(BlueprintReadWrite, Category = "Stats")
	float AverageTotal = 0.0f;

	/** 직전(가장 최근) 세션 총점. 없으면 음수(-1). */
	UPROPERTY(BlueprintReadWrite, Category = "Stats")
	float LastTotal = -1.0f;

	/** 최근 세션 총점들 (오래된→최신 순). 추세 미니 그래프용. */
	UPROPERTY(BlueprintReadWrite, Category = "Stats")
	TArray<float> RecentTotals;
};
