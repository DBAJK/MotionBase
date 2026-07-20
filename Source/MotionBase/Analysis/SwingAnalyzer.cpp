#include "Analysis/SwingAnalyzer.h"
#include "MotionBase.h"

float USwingAnalyzer::ComputeSpeedMps(const FSwingSample& A, const FSwingSample& B)
{
	const double Dt = B.TimeSeconds - A.TimeSeconds;
	if (Dt <= KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}
	const float DistCm = static_cast<float>(FVector::Dist(A.TipLocation, B.TipLocation));
	// cm/s → m/s
	return (DistCm / static_cast<float>(Dt)) / 100.0f;
}

float USwingAnalyzer::FindPeakSpeed(const TArray<FSwingSample>& Samples, int32& OutIndex)
{
	OutIndex = INDEX_NONE;
	float Peak = 0.0f;
	for (int32 i = 1; i < Samples.Num(); ++i)
	{
		const float Speed = ComputeSpeedMps(Samples[i - 1], Samples[i]);
		if (Speed > Peak)
		{
			Peak = Speed;
			OutIndex = i;
		}
	}
	return Peak;
}

FSwingMetrics USwingAnalyzer::AnalyzeSwing(
	const TArray<FSwingSample>& Samples,
	const FVector& BallLocation,
	double IdealContactTime)
{
	FSwingMetrics Out;

	if (Samples.Num() < 2)
	{
		UE_LOG(LogMotionBase, Verbose, TEXT("AnalyzeSwing: 표본 부족(%d) — 헛스윙 처리"), Samples.Num());
		return Out;
	}

	// 1) 피크 속도
	int32 PeakIdx = INDEX_NONE;
	Out.PeakSpeedMps = FindPeakSpeed(Samples, PeakIdx);

	// 2) 공에 가장 가까운 표본 = 컨택 후보
	int32 ContactIdx = INDEX_NONE;
	float MinDistCm = TNumericLimits<float>::Max();
	for (int32 i = 0; i < Samples.Num(); ++i)
	{
		const float D = static_cast<float>(FVector::Dist(Samples[i].TipLocation, BallLocation));
		if (D < MinDistCm)
		{
			MinDistCm = D;
			ContactIdx = i;
		}
	}

	Out.ContactDistanceCm = MinDistCm;

	// TODO(캘리브레이션): 유효 컨택 반경은 실측 스위트스팟 크기로 조정 (하드코딩 금지).
	constexpr float ContactRadiusCm = 15.0f;
	Out.bContacted = (MinDistCm <= ContactRadiusCm);

	if (ContactIdx != INDEX_NONE)
	{
		// 3) 컨택 순간 속도 (peak 아님)
		const int32 Prev = FMath::Max(0, ContactIdx - 1);
		const int32 Next = FMath::Min(Samples.Num() - 1, ContactIdx + 1);
		Out.ContactSpeedMps = ComputeSpeedMps(Samples[Prev], Samples[Next]);

		// 4) 타이밍 오차: 컨택 시각 - 이상 시각
		Out.TimingErrorSeconds = static_cast<float>(Samples[ContactIdx].TimeSeconds - IdealContactTime);
	}

	// TODO: 스윙 평면각 — 궤적 주성분(PCA) 또는 시작→컨택 벡터의 수평 대비 각도.
	Out.SwingPlaneAngleDeg = 0.0f;

	return Out;
}
