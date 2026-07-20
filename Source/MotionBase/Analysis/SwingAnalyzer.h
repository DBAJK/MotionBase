#pragma once

#include "CoreMinimal.h"
#include "Data/SwingSample.h"
#include "Data/SwingMetrics.h"
#include "SwingAnalyzer.generated.h"

/**
 * 스윙 원시 궤적 → 물리 지표 추출 (계산 계층, UE 렌더/액터 비의존).
 *
 * 입력은 숫자(FSwingSample 배열)뿐, 출력도 숫자(FSwingMetrics)뿐이므로
 * 헤드셋·Vive 없이 단위 테스트 가능. static 순수 함수로 구현해 계산 버그를
 * 연출 버그와 분리해서 잡는다.
 *
 * 참고(측정 방식, CLAUDE §5):
 *  - 컨트롤러 속도 ≠ 배트 속도. 배트 헤드는 회전 채찍 효과로 손보다 빠름 (v_tip = v_hand + ω×r).
 *    → ABat 이 BatTip SceneComponent 월드 좌표를 샘플링하면 회전 효과가 자동 반영됨.
 *  - 컨택 순간 속도(peak 아님)를 사용. 링버퍼 최근 15~20 샘플 권장.
 */
UCLASS()
class MOTIONBASE_API USwingAnalyzer : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * 궤적 표본 배열에서 스윙 지표를 계산한다.
	 * @param Samples        시간순 정렬된 BatTip 궤적 (cm, 초).
	 * @param BallLocation   가상 공 위치 (cm). 컨택 거리/타이밍 판정 기준.
	 * @param IdealContactTime 이상적 컨택 시각 (초). 투구 도달 타이밍.
	 */
	UFUNCTION(BlueprintCallable, Category = "MotionBase|Analysis")
	static FSwingMetrics AnalyzeSwing(
		const TArray<FSwingSample>& Samples,
		const FVector& BallLocation,
		double IdealContactTime);

	/** 두 표본 사이 순간 속도 (m/s). 중앙차분/전진차분 위임용 헬퍼. */
	static float ComputeSpeedMps(const FSwingSample& A, const FSwingSample& B);

	/** 궤적에서 최대 속도 표본 인덱스와 값 (m/s). */
	static float FindPeakSpeed(const TArray<FSwingSample>& Samples, int32& OutIndex);
};
