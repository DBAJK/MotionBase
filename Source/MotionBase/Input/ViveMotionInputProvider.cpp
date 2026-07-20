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
	if (!bInitialized || !BatTip.IsValid())
	{
		return;
	}

	ElapsedSec += DeltaSeconds;

	// 배트 헤드의 월드 좌표 — 손 회전이 만드는 채찍 효과가 여기 이미 반영돼 있다.
	const FVector Location = BatTip->GetComponentLocation();
	const FRotator Rotation = BatTip->GetComponentRotation();

	FSwingSample Sample(ElapsedSec, Location, Rotation);

	// 위치 미분으로 속도(cm/s) 산출. 정본 속도는 USwingAnalyzer 가 다시 계산하지만,
	// 여기서도 채워두면 실시간 HUD·디버그에 바로 쓸 수 있다.
	if (bHasPrevious && DeltaSeconds > KINDA_SMALL_NUMBER)
	{
		Sample.Velocity = (Location - PreviousLocation) / DeltaSeconds;
	}

	PushRing(BatTipHistory, Sample, HistoryCapacity);

	PreviousLocation = Location;
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
