#pragma once

#include "CoreMinimal.h"
#include "Data/SwingMetrics.h"
#include "Data/ScoreResult.h"
#include "Data/MotionBaseTypes.h"
#include "Data/SessionResult.h"
#include "Data/OverallScore.h"
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
 * 수비 3축 채점 가중치. FScoringConfig(타격)와 대칭이지만, 정확도·효율의 실제 계산은
 * 종목(포구/송구/백업)마다 다른 원시 지표를 쓰므로 여기 담지 않는다 — 각 폰이 이미 갖고 있는
 * 기준값(CatchRadius, TargetReleaseKmh, TargetTransferSec, TargetDecisionSec 등)을 단일
 * 출처로 참조해 0~1 로 정규화한 뒤, 그 결과만 UScoringService::ScoreDefenseSession3Axis 에 넘긴다.
 */
USTRUCT(BlueprintType)
struct FDefenseScoringConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weights")
	float WeightAccuracy = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weights")
	float WeightEfficiency = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weights")
	float WeightConsistency = 0.25f;

	/** 실측 캘리브레이션 완료 여부. false면 FScoreResult.bUncalibrated 로 전파된다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration")
	bool bCalibrated = false;

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
	// 시도 1건은 여전히 성공/실패만 남긴다(ScoreDefenseAttempt) — 원시 측정값은 이미 Details 에
	// 실려 있고, 그걸 종목별 0~1 정확도·효율로 바꾸는 계산은 각 폰(자기 기준값을 쥔 쪽)의 몫이다.
	// 세션 집계(ScoreDefenseSession3Axis)만 타격과 동일한 원칙(가중치 재정규화, 표본<2 면
	// 일관성 축 제외)으로 3축 총점을 낸다 — 이전엔 여기서도 성공률×100 이 전부였다.

	/**
	 * 수비 시도 1건 → 점수. 성공 100 / 실패 0.
	 * @param bSuccess 포구 성공 / 목표 zone 도달 / 백업 정답
	 * @param Details  원시 측정값 (예: "TimingError" -> -0.08, "ReleaseKmh" -> 78)
	 */
	UFUNCTION(BlueprintCallable, Category = "MotionBase|Scoring")
	static FScoreResult ScoreDefenseAttempt(bool bSuccess, const TMap<FName, float>& Details);

	/**
	 * 수비 세션 3축 집계. "정확도·효율이 무엇인가"는 종목마다 다르므로(포구=거리/타이밍,
	 * 송구=거리/구속·전환시간, 백업=정답여부/판단시간) 그 계산은 호출부가 이미 0~1 로 정규화해
	 * 넘긴다 — 여기선 타격의 ScoreSession 과 동일한 집계 원칙만 공유한다.
	 *
	 * @param AccuracyPerAttempt    시도별 정확도(0~1). 실패 시도는 0.
	 * @param EfficiencyPerAttempt  시도별 효율(0~1). Accuracy 와 같은 길이.
	 * @param ConsistencyBasisPerAttempt 일관성 표준편차를 낼 기준값(0~1). 해당 시도가 편차 계산
	 *        대상이 아니면(예: 실패 시도) 음수를 넣는다 — 내부에서 0 이상만 골라 쓴다.
	 *        포구/송구는 보통 Accuracy 와 같은 배열을 넘기고, 백업은 판단시간 기반 Efficiency
	 *        를 넘긴다(백업의 정확도는 이진값이라 편차가 늘 0이 되어 무의미하기 때문).
	 */
	UFUNCTION(BlueprintCallable, Category = "MotionBase|Scoring")
	static FScoreResult ScoreDefenseSession3Axis(const TArray<float>& AccuracyPerAttempt,
		const TArray<float>& EfficiencyPerAttempt, const TArray<float>& ConsistencyBasisPerAttempt,
		const FDefenseScoringConfig& Config);

	/**
	 * 수비 세션 집계 → 총점 = 성공률 × 100. 시도가 0이면 bValid=false.
	 * ⚠️ 레거시 — 3축 근거 없이 성공률만 본다. 새 코드는 ScoreDefenseSession3Axis 를 쓸 것.
	 */
	UFUNCTION(BlueprintCallable, Category = "MotionBase|Scoring")
	static FScoreResult ScoreDefenseSession(int32 SuccessCount, int32 AttemptCount);

	/**
	 * 저장 이력 전체 → 공격 50 + 수비 50 = 100점 종합.
	 *
	 * 규칙 (전부 의도적으로 정한 것이라 바꿀 때 주의):
	 *  - **종목별 최고점**을 쓴다. 한 번 망친 판이 종합을 계속 깎지 않게 하고 재도전 동기를 준다.
	 *  - **난이도 계수**를 곱한 뒤 종목 상한으로 clamp — Beginner 는 만점에 도달할 수 없고,
	 *    Pro 는 더 낮은 원점수로 도달한다.
	 *  - **미실시 종목은 0 이 아니라 제외**하고 실시한 종목 기준으로 100 환산한다
	 *    (절대 점수가 필요하면 FOverallScore::RawTotal).
	 *  - 유효하지 않은 세션(bValid=false)은 무시한다.
	 *
	 * @param Categories 집계할 종목 정의. **UModeManager::BuildOverallCategories() 로 만들 것** —
	 *                   종목 식별자를 여기서 새로 적으면 저장 ID 가 바뀔 때 조용히 어긋난다.
	 */
	UFUNCTION(BlueprintCallable, Category = "MotionBase|Scoring")
	static FOverallScore ComputeOverall(const TArray<FSessionResult>& History,
		const TArray<FOverallCategoryDef>& Categories, const FOverallScoreConfig& Config);

	/** 정확도 축 (0~1): 타이밍 가우시안 감쇠 × 컨택 거리 감쇠. */
	static float EvalAccuracy(const FSwingMetrics& M, const FScoringConfig& Config);

	/** 효율 축 (0~1): 배트 속도 정규화. */
	static float EvalEfficiency(const FSwingMetrics& M, const FScoringConfig& Config);
};
