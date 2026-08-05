#pragma once

#include "CoreMinimal.h"
#include "Data/MotionBaseTypes.h"
#include "TrainingFeedback.generated.h"

/**
 * 약점 판별 → 운동 추천 → (선택) AI 코칭 파이프라인의 계층 간 계약.
 *
 * 핵심 원칙: **약점을 찾는 것은 결정론적 계산(UWeaknessDetector)**, LLM은 그 위에
 * 코칭 문장만 얹는 표현 계층이다. 덕분에 네트워크·API 키 없이도 약점 리포트와
 * 추천 드릴이 나오고, 단위 테스트가 가능하며, "왜 이 조언이 나왔는가"에 숫자로 답할 수 있다.
 *
 * 계약은 **모드 무관**이다 — 타격은 스윙 지표로, 베이스 러닝은 전신 지표로 같은
 * FWeaknessReport 를 채운다. FScoreResult 가 모드 무관인 것과 같은 패턴.
 */

/** 약점 축. 점수 3축보다 세분화 — 코칭은 구체적일수록 좋다. */
UENUM(BlueprintType)
enum class EWeaknessAxis : uint8
{
	ContactRate     UMETA(DisplayName = "컨택률"),        // 헛스윙이 잦음
	Timing          UMETA(DisplayName = "타이밍"),        // 컨택 순간이 도달 시각과 어긋남
	ContactAccuracy UMETA(DisplayName = "컨택 정확도"),   // 스위트스팟에서 벗어남
	BatSpeed        UMETA(DisplayName = "배트 스피드"),   // 임팩트 속도 부족
	Consistency     UMETA(DisplayName = "일관성")         // 시도별 편차가 큼
};

/** 한 축의 약점. */
USTRUCT(BlueprintType)
struct FWeakness
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Feedback")
	EWeaknessAxis Axis = EWeaknessAxis::Timing;

	/** 이 축 수행도 0~1 (높을수록 잘함). */
	UPROPERTY(BlueprintReadWrite, Category = "Feedback")
	float Score = 0.0f;

	/** 심각도 0~1 (= 1 - Score, 높을수록 나쁨). 정렬·우선순위 기준. */
	UPROPERTY(BlueprintReadWrite, Category = "Feedback")
	float Severity = 0.0f;

	/** 근거 문자열 (실측 숫자 포함). 예: "평균 타이밍 오차 120 ms (목표 ±50 ms)". */
	UPROPERTY(BlueprintReadWrite, Category = "Feedback")
	FString Evidence;
};

/** 세션 1회의 약점 분석 결과. 결정론적으로 산출된다. */
USTRUCT(BlueprintType)
struct FWeaknessReport
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Feedback")
	EGameModeId Mode = EGameModeId::Batting;

	UPROPERTY(BlueprintReadWrite, Category = "Feedback")
	int32 AttemptCount = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Feedback")
	int32 ContactCount = 0;

	/** 심각도 내림차순 정렬. 앞쪽이 가장 시급한 약점. */
	UPROPERTY(BlueprintReadWrite, Category = "Feedback")
	TArray<FWeakness> Weaknesses;

	/** 표본이 있어 분석이 유효한지. */
	UPROPERTY(BlueprintReadWrite, Category = "Feedback")
	bool bValid = false;

	/** 점수 기준 상수가 미보정 예시값인지. LLM 프롬프트에 전파해 단정을 막는다. */
	UPROPERTY(BlueprintReadWrite, Category = "Feedback")
	bool bUncalibrated = true;
};

/** 추천 운동(드릴) 한 개. 카탈로그(UDrillCatalog)에서 온다 — LLM 자유생성 아님. */
USTRUCT(BlueprintType)
struct FTrainingDrill
{
	GENERATED_BODY()

	/** 드릴 이름. */
	UPROPERTY(BlueprintReadWrite, Category = "Feedback")
	FString Name;

	/** 어떤 약점을 겨냥하는지. */
	UPROPERTY(BlueprintReadWrite, Category = "Feedback")
	EWeaknessAxis TargetAxis = EWeaknessAxis::Timing;

	/** 수행 방법 한 줄. */
	UPROPERTY(BlueprintReadWrite, Category = "Feedback")
	FString Description;

	/** 핵심 포커스 큐 (짧게). */
	UPROPERTY(BlueprintReadWrite, Category = "Feedback")
	FString FocusCue;
};
