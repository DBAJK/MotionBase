#pragma once

#include "CoreMinimal.h"
#include "Data/SwingMetrics.h"
#include "Data/TrainingFeedback.h"
#include "Data/SessionResult.h"
#include "Data/CameraPoseFrame.h"
#include "Scoring/ScoringService.h"
#include "WeaknessDetector.generated.h"

/**
 * 신체역학 약점 판별 임계값 (점수/판정 계층 소관 — FBodyMechanicsConfig 는 유효성 게이트일 뿐).
 * ⚠️ 아래 목표값은 전부 실측 캘리브레이션 대상. bCalibrated=false 면 리포트에 미보정 전파.
 */
USTRUCT(BlueprintType)
struct FBodyMechanicsScoringConfig
{
	GENERATED_BODY()

	/** X-factor 만점 목표 분리각 (°). 이 값 이상이면 만점, 0이면 0점 (선형). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BodyMechanics")
	float TargetSeparationDeg = 40.0f;

	/** 머리 이동 0점이 되는 상한 (cm). 작을수록 좋음. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BodyMechanics")
	float MaxHeadTravelCm = 8.0f;

	/** 체중 이동 만점 목표 (cm). 이 값 이상이면 만점. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BodyMechanics")
	float TargetWeightShiftCm = 15.0f;

	/**
	 * 신뢰도(Confidence)가 이 값 미만인 지표는 판별에서 제외 — 저조도·가림 프레임의
	 * 엉터리 수치가 약점으로 잡히지 않게 한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BodyMechanics")
	float MinConfidence = 0.5f;

	/** 실측 캘리브레이션 완료 여부. false 면 위 목표값은 개발용 플레이스홀더. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BodyMechanics")
	bool bCalibrated = false;
};

/**
 * 세션 지표 → 약점 리포트 (계산 계층, UE 렌더/액터 비의존).
 *
 * ⚠️ LLM 은 여기서 쓰이지 않는다. 약점 판별은 전적으로 결정론적 계산이다 —
 *    지표를 임계값·목표와 비교해 축별 수행도(0~1)와 심각도를 낸다.
 *    LLM 은 이 리포트를 받아 "문장으로 표현"만 한다(UAIFeedbackService).
 *
 * 임계값은 점수 계층과 동일한 FScoringConfig 상수를 재사용한다 → 채점과 진단이
 * 같은 기준으로 움직인다. 상수는 실측 캘리브레이션 대상(하드코딩 확정 금지).
 */
UCLASS()
class MOTIONBASE_API UWeaknessDetector : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * 타격 세션(여러 스윙) → 약점 리포트.
	 * @param History 한 세션의 스윙 지표들 (UScoringService::ScoreSession 과 같은 입력).
	 */
	UFUNCTION(BlueprintCallable, Category = "MotionBase|Feedback")
	static FWeaknessReport DetectSwing(const TArray<FSwingMetrics>& History, const FScoringConfig& Config);

	/**
	 * 세션의 신체역학 지표들을 판별해 기존 리포트에 약점 축을 덧붙인다.
	 * (스윙 지표 리포트에 이어 호출 → 신체역학 축까지 한 리포트에 합쳐 정렬된다.)
	 * bValid=false 이거나 신뢰도 미달인 지표는 제외한다. 유효 지표가 없으면 리포트를 건드리지 않는다.
	 * @param Report     DetectSwing 이 만든 리포트 (이 함수가 in-place 로 확장).
	 * @param BodyHistory 세션 내 스윙별 신체역학 지표.
	 * @param BodyConfig  판별 임계값 (미보정 시 Report.bUncalibrated 로 전파).
	 */
	UFUNCTION(BlueprintCallable, Category = "MotionBase|Feedback")
	static void AppendBodyMechanicsWeaknesses(
		UPARAM(ref) FWeaknessReport& Report,
		const TArray<FBodyMechanicsMetrics>& BodyHistory,
		const FBodyMechanicsScoringConfig& BodyConfig);

	/**
	 * 저장 이력을 훑어 축별 만성 약점·추세를 판별한다 (계산 계층).
	 * 최근 Window 개의 "리포트 있는" 해당 모드 세션을 시간순으로 모아, 축마다
	 * 세션별 수행도 시계열을 만들고 기울기(개선/악화)와 만성 여부를 낸다.
	 * @param History 저장된 전체 세션 (UModeManager::GetHistory — 이번 미저장 세션 제외).
	 * @param Mode    분석할 모드.
	 * @param Window  뒤에서부터 볼 최대 세션 수.
	 */
	UFUNCTION(BlueprintCallable, Category = "MotionBase|Feedback")
	static FChronicWeaknessReport AnalyzeTrend(const TArray<FSessionResult>& History, EGameModeId Mode, int32 Window = 5);

	/** 추세 표시 이름 (UMETA 는 패키징 빌드에서 사라지므로 직접 반환). */
	UFUNCTION(BlueprintPure, Category = "MotionBase|Feedback")
	static FText GetTrendDisplayName(EWeaknessTrend Trend);

	/** 축 표시 이름 (UMETA 는 패키징 빌드에서 사라지므로 직접 반환). */
	UFUNCTION(BlueprintPure, Category = "MotionBase|Feedback")
	static FText GetAxisDisplayName(EWeaknessAxis Axis);

	/**
	 * 리포트를 사람이 읽는 한 덩어리 텍스트로. (화면·LLM 프롬프트 공용)
	 * 숫자를 그대로 담으므로 LLM 이 값을 지어낼 필요가 없다.
	 */
	UFUNCTION(BlueprintCallable, Category = "MotionBase|Feedback")
	static FString SummarizeReport(const FWeaknessReport& Report);
};
