#include "Input/ViveMotionInputProvider.h"
#include "MotionBase.h"
#include "Components/SceneComponent.h"
#include "MotionControllerComponent.h"

void UViveMotionInputProvider::SetTrackedComponents(USceneComponent* InBatTip, UMotionControllerComponent* InController)
{
	BatTip = InBatTip;
	Controller = InController;
}

bool UViveMotionInputProvider::Initialize()
{
	if (!BatTip.IsValid())
	{
		UE_LOG(LogMotionBase, Warning,
			TEXT("ViveProvider: BatTip 컴포넌트가 없습니다. SetTrackedComponents() 를 먼저 호출하세요."));
		return false;
	}

	bInitialized = true;
	ElapsedSec = 0.0f;
	bHasPrevious = false;
	BatTipHistory.Reset();
	return true;
}

void UViveMotionInputProvider::Shutdown()
{
	bInitialized = false;
	bHasPrevious = false;
	BatTipHistory.Reset();
}

bool UViveMotionInputProvider::IsTracking() const
{
	return bInitialized && Controller.IsValid() && Controller->IsTracked();
}

void UViveMotionInputProvider::Tick(float DeltaSeconds)
{
	if (!bInitialized || !BatTip.IsValid() || DeltaSeconds <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	ElapsedSec += DeltaSeconds;

	const FVector TipWorld = BatTip->GetComponentLocation();
	const FRotator TipRot = BatTip->GetComponentRotation();

	// 손(컨트롤러) 월드 트랜스폼. 컨트롤러가 없으면 BatTip 을 손으로 간주(r=0 → 위치 미분과 동일).
	UMotionControllerComponent* MC = Controller.Get();
	const FVector HandWorld = MC ? MC->GetComponentLocation() : TipWorld;
	const FQuat HandQuat = MC ? MC->GetComponentQuat() : BatTip->GetComponentQuat();

	FSwingSample Sample(ElapsedSec, TipWorld, TipRot);

	if (bHasPrevious)
	{
		// 1) v_hand — 손(컨트롤러) 위치 미분. 손은 느리게 움직여 프레임 차분으로도 정확하다.
		//    (MotionControllerComponent::GetLinearVelocity 는 protected 라 외부에서 못 쓴다.)
		const FVector HandVel = (HandWorld - PreviousHandLocation) / DeltaSeconds;

		// 2) ω (rad/s, 월드) — 프레임 간 쿼터니언 델타. 90fps 에선 프레임당 회전 <180° 라 모호하지 않다.
		const FQuat DeltaQ = (HandQuat * PreviousHandQuat.Inverse()).GetNormalized();
		FVector Axis;
		float Angle;
		DeltaQ.ToAxisAndAngle(Axis, Angle);
		if (Angle > PI)
		{
			Angle -= 2.0f * PI; // 최단 회전
		}
		const FVector Omega = Axis * (Angle / DeltaSeconds);

		// 3) v_tip = v_hand + ω × r   (r = 손→배트헤드, cm) → 회전 채찍 효과 반영
		const FVector R = TipWorld - HandWorld;
		Sample.Velocity = HandVel + FVector::CrossProduct(Omega, R);
	}
	// 첫 프레임은 속도 0 (분석기가 위치 차분으로 폴백).

	PushBatTipRing(BatTipHistory, Sample);

	PreviousHandLocation = HandWorld;
	PreviousHandQuat = HandQuat;
	bHasPrevious = true;
}

bool UViveMotionInputProvider::GetBatTipSample(FSwingSample& OutSample) const
{
	if (BatTipHistory.Num() == 0)
	{
		return false;
	}
	OutSample = BatTipHistory.Last();
	return true;
}

bool UViveMotionInputProvider::GetBatTipHistory(TArray<FSwingSample>& OutSamples) const
{
	if (BatTipHistory.Num() == 0)
	{
		return false;
	}
	OutSamples = BatTipHistory;
	return true;
}
