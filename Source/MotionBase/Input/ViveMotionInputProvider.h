#pragma once

#include "CoreMinimal.h"
#include "Input/MotionInputProvider.h"
#include "ViveMotionInputProvider.generated.h"

class USceneComponent;
class UMotionControllerComponent;

/**
 * HTC Vive 컨트롤러 provider (OpenXR).
 *
 * 측정 방식 (브리프 §5):
 *  - 컨트롤러 속도 ≠ 배트 속도. 배트 헤드는 회전 채찍 효과로 손보다 빠르다 (v_tip = v_hand + ω×r).
 *  - **v_hand** = 손(컨트롤러) 위치 미분. 손은 반경이 작아 느리게 움직이므로 프레임 차분으로도
 *    정확하다. (배트 헤드를 직접 차분하면 빠른 스윙에서 코드(chord)로 뭉개지는 것과 대비.)
 *  - **ω** = 컨트롤러 회전의 프레임 간 쿼터니언 델타에서 산출(rad/s). 90fps 에선 프레임당
 *    회전이 작아(<180°) 축-각 분해가 모호하지 않다. (FRotator 각속도 API는 고속 스윙에서
 *    180°/s 넘어가면 손실이 커 쓰지 않는다.)
 *  - **r** = 손(컨트롤러) → 배트 헤드 벡터. → 프레임 차분보다 빠른 스윙에서 훨씬 정확.
 *  - ⚠️ UE 5.7+ 폐기된 `Get Motion Controller Data` 계열 미사용 → 버전 변화에 안전.
 *
 * 컴포넌트는 액터(ABat)가 소유하고, 이 provider 는 그 트랜스폼을 샘플링만 한다.
 */
UCLASS(BlueprintType)
class MOTIONBASE_API UViveMotionInputProvider : public UMotionInputProvider
{
	GENERATED_BODY()

public:
	/** ABat 이 자신의 BatTip / MotionController 를 등록한다. */
	void SetTrackedComponents(USceneComponent* InBatTip, UMotionControllerComponent* InController);

	virtual bool Initialize() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void Shutdown() override;

	virtual EInputSource GetSourceType() const override { return EInputSource::ViveController; }
	virtual bool IsTracking() const override;
	virtual float GetElapsedSeconds() const override { return ElapsedSec; }

	virtual bool GetBatTipSample(FSwingSample& OutSample) const override;
	virtual bool GetBatTipHistory(TArray<FSwingSample>& OutSamples) const override;

	/** Vive 컨트롤러만으로는 전신 스캔 불가 → LiDAR provider 담당. */
	virtual bool GetBodyScan(float& OutHeightCm, float& OutReachRadiusCm) const override { return false; }

private:
	TWeakObjectPtr<USceneComponent> BatTip;
	TWeakObjectPtr<UMotionControllerComponent> Controller;

	TArray<FSwingSample> BatTipHistory;

	bool bInitialized = false;
	float ElapsedSec = 0.0f;

	// 이전 프레임 상태 (v_hand·ω 산출용)
	bool bHasPrevious = false;
	FVector PreviousHandLocation = FVector::ZeroVector;
	FQuat PreviousHandQuat = FQuat::Identity;
};
