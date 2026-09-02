#pragma once

#include "CoreMinimal.h"
#include "Data/SwingMetrics.h"
#include "Data/ScoreResult.h"
#include "Data/MotionBaseTypes.h"
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

	/**
	 * 가중치. 합이 1이 아니어도 된다 — 채점 함수가 항상 합으로 나눠 정규화한다.
	 * (예전엔 세션 채점만 정규화를 안 해서, 에디터에서 가중치를 만지면 총점이 100을 넘었다.)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weights")
	float WeightAccuracy = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weights")
	float WeightEfficiency = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weights")
	float WeightConsistency = 0.25f;

	/** 타이밍 오차 가우시안 표준편차 (초). 작을수록 엄격. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration")
	float TimingSigmaSeconds = 0.05f;

	/**
	 * 정확도 0점이 되는 최대 컨택 거리 (cm).
	 *
	 * ⚠️ **`USwingAnalyzer::ContactRadiusCm` 이상이어야 한다.** 이 값이 더 작으면
	 *    그 사이 구간(예전: 반경 32 / 상한 30 → 30~32cm)이 "맞긴 맞았는데 정확도 0 ·
	 *    타구속도 0 · 비거리 0m" 인 죽은 밴드가 된다. 컨택 반경을 넓힐 땐 여기도 같이 올릴 것.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration")
	float MaxContactDistanceCm = 32.0f;

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

	// ── 타구 판정 임계 (난이도가 조절하는 값) ────────────────────────────────
	//
	// ⚠️ **채점(정확도·효율·일관성)에는 영향을 주지 않는다.** 난이도를 낮췄다고 점수가
	//    올라가면 기록·만성 약점 추세가 난이도끼리 섞여 의미를 잃는다. 난이도는
	//    "이 타구를 안타로 불러 줄 것인가"(연출·판정)만 관대하게 만든다.
	//
	// ⚠️ 값 자체는 실측 캘리브레이션 대상 (CLAUDE §규칙) — 하드코딩 확정 아님.

	/** 이 비거리(m) 이상이면 안타. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitJudge")
	float HitDistanceM = 30.0f;

	/** 이 비거리(m) 이상 + 발사각 조건을 만족하면 홈런. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitJudge")
	float HomeRunDistanceM = 100.0f;

	/** |좌우각| 이 값(도)을 넘으면 파울. 크면 페어 존이 넓어진다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitJudge")
	float FoulLineDeg = 45.0f;

	/**
	 * 난이도별 타구 판정 관대도를 적용한다 (위 세 값만 바꾼다).
	 *
	 * 왜 필요한가: 간이 타구 모델은 투구 속도를 반영하지 않아 타구속도가 실제보다 낮게
	 * 나온다. 그 상태로 "비거리 30m 이상 = 안타"를 요구하면 **아마추어가 잘 맞혀도 대부분
	 * 아웃**으로 찍혀 훈련 피드백이 죽는다. 모델을 부풀리는 대신(=점수 오염) 판정선을
	 * 난이도에 맞춰 내린다.
	 */
	void ApplyDifficulty(EDifficultyLevel Level);

	/**
	 * 실측 캘리브레이션 완료 여부.
	 * ⚠️ false면 위 상수는 전부 예시값 → FScoreResult.bUncalibrated 로 전파되어
	 *    결과 화면에 "미보정"으로 표시된다. 제출 전 반드시 실측으로 채울 것.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration")
	bool bCalibrated = false;

	/**
	 * 3축 가중치의 합. 채점 함수가 총점을 이 값으로 나눠 0~100 스케일을 지킨다.
	 * (예전의 Normalize() 를 대신한다 — 아무도 호출하지 않는 보정 함수보다,
	 *  쓰는 쪽에서 항상 나누는 편이 설정을 어떻게 만져도 깨지지 않는다.)
	 */
	float WeightSum() const
	{
		return FMath::Max(WeightAccuracy + WeightEfficiency + WeightConsistency, KINDA_SMALL_NUMBER);
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

	// ── 수비 계열 (포구·송구·백업) ──
	//
	// ⚠️ 수비는 아직 **3축 채점 모델이 없다.** 타격의 정확도·효율·일관성을 수비에 억지로
	//    끼워 맞추면(예: 구속을 '효율'로) 근거 없는 숫자가 기록에 남는다. 그래서 여기서는
	//    성공/실패만 점수로 남기고, 실제 분석 내용은 각 폰이 만드는 FWeaknessReport 가 진다.
	//    Details 에 원시 측정값을 실어두므로 나중에 실측 캘리브레이션 후 모델을 얹을 수 있다.

	/**
	 * 수비 시도 1건 → 점수. 성공 100 / 실패 0.
	 * @param bSuccess 포구 성공 / 목표 zone 도달 / 백업 정답
	 * @param Details  원시 측정값 (예: "TimingError" -> -0.08, "ReleaseKmh" -> 78)
	 */
	UFUNCTION(BlueprintCallable, Category = "MotionBase|Scoring")
	static FScoreResult ScoreDefenseAttempt(bool bSuccess, const TMap<FName, float>& Details);

	/** 수비 세션 집계 → 총점 = 성공률 × 100. 시도가 0이면 bValid=false. */
	UFUNCTION(BlueprintCallable, Category = "MotionBase|Scoring")
	static FScoreResult ScoreDefenseSession(int32 SuccessCount, int32 AttemptCount);

	/** 정확도 축 (0~1): 타이밍 가우시안 감쇠 × 컨택 거리 감쇠. */
	static float EvalAccuracy(const FSwingMetrics& M, const FScoringConfig& Config);

	/** 효율 축 (0~1): 배트 속도 정규화. */
	static float EvalEfficiency(const FSwingMetrics& M, const FScoringConfig& Config);
};
