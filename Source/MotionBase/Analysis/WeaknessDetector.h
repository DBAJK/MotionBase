#pragma once

#include "CoreMinimal.h"
#include "Data/SwingMetrics.h"
#include "Data/TrainingFeedback.h"
#include "Scoring/ScoringService.h"
#include "WeaknessDetector.generated.h"

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
