#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/MotionBaseTypes.h"
#include "Data/SwingSample.h"
#include "Data/SwingMetrics.h"
#include "Bat.generated.h"

class UMotionControllerComponent;
class USceneComponent;
class UMotionInputProvider;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSwingCompleted, const FSwingMetrics&, Metrics);

/**
 * 배트 액터. (Actor 계층 — 입력 수집·연출·충돌)
 *
 * 입력은 UMotionInputProvider 를 통해서만 받는다:
 *   - EInputSource::ViveController → UViveMotionInputProvider (실기)
 *   - EInputSource::Mock          → UMockMotionInputProvider (PC 개발/테스트)
 * 덕분에 Vive 없이도 동일 코드 경로로 스윙 로직을 검증할 수 있다.
 *
 * MotionController 자식으로 BatTip(배트 길이 오프셋)을 두고, provider 가 그 월드 좌표를
 * 미분해 회전 채찍 효과(v_tip = v_hand + ω×r)를 반영한 배트 헤드 속도를 낸다.
 *
 * ⚠️ SteamVR 구형 플러그인은 UE 5.1 폐기 → OpenXR 플러그인 사용.
 */
UCLASS()
class MOTIONBASE_API ABat : public AActor
{
	GENERATED_BODY()

public:
	ABat();

	virtual void Tick(float DeltaSeconds) override;

	/**
	 * 스윙 캡처 시작.
	 * @param InBallLocation        가상 공 월드 위치 (cm)
	 * @param InIdealContactWorldTime 이상적 컨택 시각 (월드 시간, 초) — APitchingZone 이 제공
	 */
	UFUNCTION(BlueprintCallable, Category = "MotionBase|Bat")
	void BeginSwingCapture(const FVector& InBallLocation, double InIdealContactWorldTime);

	/** 스윙 종료 → provider 히스토리 분석 → OnSwingCompleted 발행. */
	UFUNCTION(BlueprintCallable, Category = "MotionBase|Bat")
	FSwingMetrics EndSwingCaptureAndAnalyze();

	/** 컨트롤러가 실제로 추적되고 있는지 (HUD 표시용). */
	UFUNCTION(BlueprintPure, Category = "MotionBase|Bat")
	bool IsTracking() const;

	UPROPERTY(BlueprintAssignable, Category = "MotionBase|Bat")
	FOnSwingCompleted OnSwingCompleted;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 손(컨트롤러) 추적. */
	UPROPERTY(VisibleAnywhere, Category = "MotionBase|Bat")
	TObjectPtr<UMotionControllerComponent> MotionController;

	/** 배트 헤드. MotionController 자식, 배트 길이만큼 오프셋. */
	UPROPERTY(VisibleAnywhere, Category = "MotionBase|Bat")
	TObjectPtr<USceneComponent> BatTip;

	/** 어떤 입력 소스를 쓸지. Vive 없이 테스트하려면 Mock 으로. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MotionBase|Bat")
	EInputSource InputSource = EInputSource::ViveController;

	/** 링버퍼 크기 (최근 N 샘플). 컨택 순간/피크/평면각 추출용. */
	UPROPERTY(EditAnywhere, Category = "MotionBase|Bat", meta = (ClampMin = "8"))
	int32 RingBufferSize = 20;

	/** 실제 입력 소스. BeginPlay 에서 InputSource 에 따라 생성된다. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "MotionBase|Bat")
	TObjectPtr<UMotionInputProvider> InputProvider;

private:
	bool bCapturing = false;
	FVector BallLocation = FVector::ZeroVector;

	/** 이상적 컨택 시각을 provider 시간축으로 변환해둔 값. */
	double IdealContactProviderTime = 0.0;
};
