#pragma once

#include "CoreMinimal.h"
#include "Data/SwingMetrics.h"
#include "Data/ScoreResult.h"
#include "ScoringService.generated.h"

/**
 * 3축 채점 파라미터 (모드별 가중치 + 캘리브레이션 상수).
 *
 * ⚠️ 기준 상수(σt, d_max, v_min, v_target)는 실측 데이터로 캘리브레이션 필수.
 *    아래 기본값은 개발용 플레이스홀더 — 하드코딩 확정 금지.
 */
USTRUCT(BlueprintType)
struct FScoringConfig
{
	GENERATED_BODY()

	/** 가중치 (합=1 권장). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weights")
	float WeightAccuracy = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weights")
	float WeightEfficiency = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weights")
	float WeightConsistency = 0.25f;

	/** 타이밍 오차 가우시안 표준편차 (초). 작을수록 엄격. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration")
	float TimingSigmaSeconds = 0.05f;

	/** 정확도 0점이 되는 최대 컨택 거리 (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration")
	float MaxContactDistanceCm = 30.0f;

	/** 효율 0점 하한 배트 속도 (m/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration")
	float MinBatSpeedMps = 15.0f;

	/** 효율 만점 목표 배트 속도 (m/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration")
	float TargetBatSpeedMps = 35.0f;

	/** 배트-공 반발계수 (가상 타구 추정용 간이 모델). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration")
	float RestitutionCoeff = 0.5f;

	/** 타구 속도 만점 기준 (m/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration")
	float ExitVelocityTargetMps = 40.0f;

	/** 일관성 0점이 되는 표준편차 상한. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration")
	float ConsistencySigmaMax = 0.35f;

	/**
	 * 실측 캘리브레이션 완료 여부.
	 * ⚠️ false면 위 상수는 전부 예시값 → FScoreResult.bUncalibrated 로 전파되어
	 *    결과 화면에 "미보정"으로 표시된다. 제출 전 반드시 실측으로 채울 것.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration")
	bool bCalibrated = false;

	/** 가중치 합을 1로 보정. */
	void Normalize()
	{
		const float Sum = WeightAccuracy + WeightEfficiency + WeightConsistency;
		if (Sum > KINDA_SMALL_NUMBER)
		{
			WeightAccuracy /= Sum;
			WeightEfficiency /= Sum;
			WeightConsistency /= Sum;
		}
	}
};

/**
 * 지표(FSwingMetrics) → 3축 점수(FScoreResult) 변환. (점수 계층)
 * 타격·투구가 공유하는 3축 구조. 순수 계산 — UE 액터/렌더 비의존.
 */
UCLASS()
class MOTIONBASE_API UScoringService : public UObject
{
	GENERATED_BODY()

public:
	/** 단일 스윙 채점. Consistency는 이력이 필요하므로 ScoreSession 사용 권장. */
	UFUNCTION(BlueprintCallable, Category = "MotionBase|Scoring")
	static FScoreResult ScoreSwing(const FSwingMetrics& Metrics, const FScoringConfig& Config);

	/**
	 * 세션(여러 스윙) 채점. 일관성 = 지표 표준편차 기반.
	 * @param History 한 세션의 스윙 지표들.
	 */
	UFUNCTION(BlueprintCallable, Category = "MotionBase|Scoring")
	static FScoreResult ScoreSession(const TArray<FSwingMetrics>& History, const FScoringConfig& Config);

	/** 정확도 축 (0~1): 타이밍 가우시안 감쇠 × 컨택 거리 감쇠. */
	static float EvalAccuracy(const FSwingMetrics& M, const FScoringConfig& Config);

	/** 효율 축 (0~1): 배트 속도 정규화. */
	static float EvalEfficiency(const FSwingMetrics& M, const FScoringConfig& Config);
};
