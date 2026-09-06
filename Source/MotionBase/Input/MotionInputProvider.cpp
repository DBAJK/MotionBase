#include "Input/MotionInputProvider.h"
#include "MotionBase.h"

bool UMockMotionInputProvider::Initialize()
{
	bInitialized = true;
	ResetPlayback();
	return true;
}

void UMockMotionInputProvider::Shutdown()
{
	bInitialized = false;
	BatTipHistory.Reset();
	BodyPoseHistory.Reset();
}

void UMockMotionInputProvider::ResetPlayback()
{
	ElapsedSec = 0.0f;
	BatTipIndex = 0;
	BodyPoseIndex = 0;
	BatTipHistory.Reset();
	BodyPoseHistory.Reset();
}

void UMockMotionInputProvider::AdvancePlayback()
{
	// TimeSec 이 경과시간에 도달한 샘플을 모두 소비 → 프레임레이트 무관.
	while (BatTipSequence.IsValidIndex(BatTipIndex)
		&& BatTipSequence[BatTipIndex].TimeSeconds <= ElapsedSec)
	{
		PushBatTipRing(BatTipHistory, BatTipSequence[BatTipIndex]);
		++BatTipIndex;
	}

	while (BodyPoseSequence.IsValidIndex(BodyPoseIndex)
		&& BodyPoseSequence[BodyPoseIndex].TimeSec <= ElapsedSec)
	{
		PushRing(BodyPoseHistory, BodyPoseSequence[BodyPoseIndex], HistoryCapacity);
		++BodyPoseIndex;
	}
}

void UMockMotionInputProvider::Tick(float DeltaSeconds)
{
	if (!bInitialized || !bPlaybackMode)
	{
		return;
	}

	ElapsedSec += DeltaSeconds;
	AdvancePlayback();
}

void UMockMotionInputProvider::PlayAll()
{
	// 단위 테스트용: 시간 진행 없이 전체를 즉시 소비.
	for (; BatTipSequence.IsValidIndex(BatTipIndex); ++BatTipIndex)
	{
		PushBatTipRing(BatTipHistory, BatTipSequence[BatTipIndex]);
	}
	for (; BodyPoseSequence.IsValidIndex(BodyPoseIndex); ++BodyPoseIndex)
	{
		PushRing(BodyPoseHistory, BodyPoseSequence[BodyPoseIndex], HistoryCapacity);
	}
}

bool UMockMotionInputProvider::IsPlaybackFinished() const
{
	return !BatTipSequence.IsValidIndex(BatTipIndex)
		&& !BodyPoseSequence.IsValidIndex(BodyPoseIndex);
}

void UMockMotionInputProvider::FeedBatTipSequence(const TArray<FSwingSample>& InSamples)
{
	BatTipSequence = InSamples;
	BatTipIndex = 0;
	BatTipHistory.Reset();
	// 주입 직후에도 읽을 수 있도록 현재 시각까지의 샘플을 즉시 반영.
	AdvancePlayback();
}

void UMockMotionInputProvider::FeedBodyPoseSequence(const TArray<FBodyPoseSample>& InPoses)
{
	BodyPoseSequence = InPoses;
	BodyPoseIndex = 0;
	BodyPoseHistory.Reset();
	AdvancePlayback();
}

bool UMockMotionInputProvider::GetBatTipSample(FSwingSample& OutSample) const
{
	if (BatTipHistory.Num() == 0)
	{
		return false;
	}
	OutSample = BatTipHistory.Last();
	return true;
}

bool UMockMotionInputProvider::GetBatTipHistory(TArray<FSwingSample>& OutSamples) const
{
	if (BatTipHistory.Num() == 0)
	{
		return false;
	}
	OutSamples = BatTipHistory;
	return true;
}

bool UMockMotionInputProvider::GetBodyPose(FBodyPoseSample& OutPose) const
{
	if (BodyPoseHistory.Num() == 0)
	{
		return false;
	}
	OutPose = BodyPoseHistory.Last();
	return true;
}

bool UMockMotionInputProvider::GetBodyPoseHistory(TArray<FBodyPoseSample>& OutPoses) const
{
	if (BodyPoseHistory.Num() == 0)
	{
		return false;
	}
	OutPoses = BodyPoseHistory;
	return true;
}

bool UMockMotionInputProvider::GetBodyScan(float& OutHeightCm, float& OutReachRadiusCm) const
{
	OutHeightCm = MockHeightCm;
	OutReachRadiusCm = MockReachRadiusCm;
	return true;
}
