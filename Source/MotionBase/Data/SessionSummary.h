#pragma once

#include "CoreMinimal.h"
#include "Data/MotionBaseTypes.h"
#include "Data/ScoreResult.h"
#include "Data/TrainingFeedback.h"
#include "Data/CameraPose.h"
#include "Data/SessionResult.h"
#include "SessionSummary.generated.h"

/**
 * 세션 종료 결과 화면(HUD)이 그리는 한 판의 요약. 모드 폰이 채워 HUD 로 넘긴다.
 *
 * ⚠️ 표현용 스냅샷일 뿐, 판정/저장의 원본이 아니다. 점수·약점·드릴은 각 계층
 *    (UScoringService / UWeaknessDetector / UDrillCatalog)이 이미 산출한 값을 모아둔 것.
 */
USTRUCT(BlueprintType)
struct FSessionSummary
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Summary")
	EGameModeId Mode = EGameModeId::Batting;

	UPROPERTY(BlueprintReadWrite, Category = "Summary")
	EDifficultyLevel Difficulty = EDifficultyLevel::Amateur;

	/** 세션 집계 점수 (3축 + 총점). */
	UPROPERTY(BlueprintReadWrite, Category = "Summary")
	FScoreResult Score;

	/** 이 모드 역대 최고 총점. 기록이 없으면 음수(-1). */
	UPROPERTY(BlueprintReadWrite, Category = "Summary")
	float BestTotalScore = -1.0f;

	/** 이번 세션이 최고 기록을 넘었는지. */
	UPROPERTY(BlueprintReadWrite, Category = "Summary")
	bool bNewRecord = false;

	// ── 집계 카운트 ──
	UPROPERTY(BlueprintReadWrite, Category = "Summary") int32 SwingCount = 0;
	UPROPERTY(BlueprintReadWrite, Category = "Summary") int32 ContactCount = 0;
	UPROPERTY(BlueprintReadWrite, Category = "Summary") int32 HomeRunCount = 0;
	UPROPERTY(BlueprintReadWrite, Category = "Summary") int32 HitCount = 0;
	UPROPERTY(BlueprintReadWrite, Category = "Summary") int32 StrikeoutCount = 0;
	UPROPERTY(BlueprintReadWrite, Category = "Summary") int32 WalkCount = 0;

	/** 세션 최고 비거리 (m). 컨택한 타구 기준. */
	UPROPERTY(BlueprintReadWrite, Category = "Summary") float MaxCarryDistanceM = 0.0f;

	/** 컨택 타구 평균 비거리 (m). */
	UPROPERTY(BlueprintReadWrite, Category = "Summary") float AvgCarryDistanceM = 0.0f;

	/** 약점 리포트 (심각도 내림차순) — 상위 몇 개를 화면에 그린다. */
	UPROPERTY(BlueprintReadWrite, Category = "Summary")
	FWeaknessReport Report;

	/** 추천 드릴 (UDrillCatalog). */
	UPROPERTY(BlueprintReadWrite, Category = "Summary")
	TArray<FTrainingDrill> Drills;

	/**
	 * 과거 세션들에서 뽑은 만성 약점·추세 (UWeaknessDetector::AnalyzeTrend).
	 * 이번 세션 리포트(Report)가 "지금"이라면, 이건 "그동안"이다 — 결과 화면의
	 * 추세 섹션에 "타이밍: 3/5세션 · 개선 중" 같은 줄로 그린다. bValid=false 면 생략.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Summary")
	FChronicWeaknessReport Chronic;

	/** AI 코칭 문장 (없거나 미설정이면 상태 문구). */
	UPROPERTY(BlueprintReadWrite, Category = "Summary")
	FString CoachingText;

	/** AI 코칭 응답 대기 중이면 true → "생성 중..." 표시. */
	UPROPERTY(BlueprintReadWrite, Category = "Summary")
	bool bAwaitingCoaching = false;

	/**
	 * 최근 스윙의 신체역학 지표 (UBodyMechanicsAnalyzer 산출).
	 * 카메라(MediaPipe)가 없으면 Mock 포즈 소스로 채운다 → bMockBodyMechanics=true.
	 * bValid=false 면 결과 화면에서 "데이터 없음"으로 표시.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Summary")
	FBodyMechanicsMetrics BodyMechanics;

	/** 위 지표가 실측 카메라가 아니라 Mock 포즈 소스에서 나온 것인지. */
	UPROPERTY(BlueprintReadWrite, Category = "Summary")
	bool bMockBodyMechanics = false;

	/**
	 * 이 모드의 과거 기록 집계 (이번 세션 저장 전 기준). 결과 화면의 "기록/추세" 섹션 입력.
	 * 이번 총점을 PriorStats.AverageTotal·LastTotal 과 비교해 상승/하락을 보여준다.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Summary")
	FModeStats PriorStats;
};
