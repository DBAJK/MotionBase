#pragma once

#include "CoreMinimal.h"
#include "Data/SwingMetrics.h"
#include "Data/BattedBall.h"
#include "Scoring/ScoringService.h"
#include "HitModel.generated.h"

/**
 * 스윙 지표 → 가상 타구 결과 (계산 계층, UE 렌더/액터 비의존, 결정론적).
 *
 * 간이 물리 모델:
 *   타구속도 = 컨택속도 × 반발계수 × 컨택품질(스위트스팟 근접도)
 *   발사각   = 기준각 − 타이밍오차·감도  (늦으면 땅볼, 이르면 뜬공)
 *   좌우각   = 타이밍오차·감도           (극단 타이밍 → 파울)
 *   비거리   = 포물선 사거리 × 공기저항 계수
 *   판정     = 파울/거리/발사각으로 홈런·안타·아웃 분류
 *
 * ⚠️ 상수는 실측 캘리브레이션 대상(하드코딩 확정 금지). 발사각은 실측 배트 평면각
 *    (FSwingMetrics::SwingPlaneAngleDeg, 현재 TODO=0)이 들어오면 그것으로 대체할 것.
 */
UCLASS()
class MOTIONBASE_API UHitModel : public UObject
{
	GENERATED_BODY()

public:
	/** 타구 속도만 (m/s). 효율 점수(UScoringService::EvalEfficiency)가 공유한다. */
	UFUNCTION(BlueprintCallable, Category = "MotionBase|Hit")
	static float ExitVelocityMps(const FSwingMetrics& M, const FScoringConfig& Config);

	/** 전체 타구 결과 (속도·각도·비거리·판정). */
	UFUNCTION(BlueprintCallable, Category = "MotionBase|Hit")
	static FBattedBallResult Simulate(const FSwingMetrics& M, const FScoringConfig& Config);

	/** 판정 표시 이름 (UMETA 는 패키징에서 사라지므로 직접 반환). */
	UFUNCTION(BlueprintPure, Category = "MotionBase|Hit")
	static FText GetClassDisplayName(EHitClass Class);
};
