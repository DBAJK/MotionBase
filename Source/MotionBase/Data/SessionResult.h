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

	/**
	 * 모드 안의 세부 종목 식별자 (수비 전용: "Catch" / "Throw" / "Backup"). 없으면 NAME_None.
	 *
	 * 왜 필요한가: 수비 세 종목은 모두 Mode=Defense 로 저장되는데, 약점 축은 서로 완전히 다르다
	 * (포구=반응·유연성, 송구=구속·전환, 백업=판단). 이걸 구분하지 않으면
	 * UWeaknessDetector::AnalyzeTrend 가 세 종목의 축을 한 통에 섞어
	 * "송구를 5세션 했는데 백업 판단이 만성 약점" 같은 엉터리 추세를 만든다.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Session")
	FName DrillId = NAME_None;

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
