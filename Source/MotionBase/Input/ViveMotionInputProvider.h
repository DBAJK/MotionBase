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
 *  - 따라서 손(MotionController)의 속도를 읽지 않고, **자식 BatTip 의 월드 좌표를
 *    매 프레임 미분**한다 → 회전 효과가 자동 반영된다.
 *  - ⚠️ UE 5.7+ 에서 폐기된 `Get Motion Controller Data` 계열 API를 쓰지 않으므로
 *    버전 변화에 안전하다.
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

	bool bHasPrevious = false;
	FVector PreviousLocation = FVector::ZeroVector;
};
