#pragma once

#include "CoreMinimal.h"
#include "ScoreResult.generated.h"

/**
 * 3축 채점 결과. (점수 계층 → UI / 로컬 저장 / AI 피드백)
 * TotalScore = w_a·Accuracy + w_e·Efficiency + w_c·Consistency
 *
 * 각 축은 0~1 정규화. 가중치는 모드별로 UScoringService 가 조정.
 */
USTRUCT(BlueprintType)
struct FScoreResult
{
	GENERATED_BODY()

	/** 정확도: 타이밍 오차(가우시안 감쇠) + 컨택 거리. 0~1. */
	UPROPERTY(BlueprintReadWrite, Category = "Score")
	float Accuracy = 0.0f;

	/** 효율: 배트 스피드 정규화 + 가상 타구 결과. 0~1. */
	UPROPERTY(BlueprintReadWrite, Category = "Score")
	float Efficiency = 0.0f;

	/** 일관성: 지표별 표준편차(작을수록 고득점). 0~1. */
	UPROPERTY(BlueprintReadWrite, Category = "Score")
	float Consistency = 0.0f;

	/** 가중합 최종 점수. 0~100 스케일. */
	UPROPERTY(BlueprintReadWrite, Category = "Score")
	float TotalScore = 0.0f;

	/** 어느 모드에서 산출된 결과인지. */
	UPROPERTY(BlueprintReadWrite, Category = "Score")
	FName ModeId = NAME_None;

	/** 계산이 유효한지 (샘플 부족·헛스윙 등이면 false). */
	UPROPERTY(BlueprintReadWrite, Category = "Score")
	bool bValid = false;

	/**
	 * 캘리브레이션 미완료 상태에서 산출된 점수인지.
	 * 예시 상수가 그대로 제출되는 사고를 막는 장치 — true면 UI에 "미보정" 표시.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Score")
	bool bUncalibrated = true;

	/** 세부 지표 (디버그·AI 피드백 프롬프트 입력). 예: "BatSpeedMps" -> 26.4 */
	UPROPERTY(BlueprintReadWrite, Category = "Score")
	TMap<FName, float> Details;
};
