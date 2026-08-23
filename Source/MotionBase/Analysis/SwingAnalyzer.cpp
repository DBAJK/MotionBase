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

	// 트래커가 per-sample 속도를 제공하면(Vive provider: v_tip = v_hand + ω×r) 그 값을 우선.
	// Mock/키보드 경로는 Velocity=0 이라 위치 차분(FindPeakSpeed) 결과를 그대로 쓴다.
	float ReportedPeakCmps = 0.0f;
	for (const FSwingSample& S : Samples)
	{
		ReportedPeakCmps = FMath::Max(ReportedPeakCmps, static_cast<float>(S.Velocity.Size()));
	}
	const bool bHasReportedVelocity = (ReportedPeakCmps > KINDA_SMALL_NUMBER);
	if (bHasReportedVelocity)
	{
		Out.PeakSpeedMps = ReportedPeakCmps / 100.0f;
	}

	// TODO(캘리브레이션): 아래 세 상수는 실측 스위트스팟/스윙 특성으로 조정 (하드코딩 금지).
	constexpr float ContactRadiusCm     = 15.0f;   // 유효 컨택 반경
	constexpr float MinSwingSpeedMps     = 8.0f;   // 이 속도 미만은 '스윙 아님'(정지·미세이동)
	constexpr float ContactTimeWindowSec = 0.15f;  // 공이 플레이트에 있는 순간 부근만 컨택 가능

	// 2) 컨택 후보 탐색.
	//    공에 가장 가까운 표본을 찾되, **두 관문**을 통과한 표본만 본다:
	//    (a) 시간 창 — 공이 실제로 플레이트에 있는 순간(IdealContactTime) 부근이어야 한다.
	//        너무 이르거나 늦게 지나간 궤적은 공이 거기 없었으므로 컨택이 아니다.
	//    (b) 동작 게이트 — 배트가 실제로 움직이고 있어야 한다.
	//        정지한 배트가 공 근처에 있다는 이유만으로 컨택 판정되던 결함(Vive 실기의 핵심 위험)을 막는다.
	//    참고용으로 전체 최소 거리(OverallMinCm)도 따로 기록한다 — 헛스윙이어도 얼마나 가까웠는지.
	int32 ContactIdx = INDEX_NONE;
	float ContactDistCm = TNumericLimits<float>::Max();
	float OverallMinCm = TNumericLimits<float>::Max();

	for (int32 i = 0; i < Samples.Num(); ++i)
	{
		const float D = static_cast<float>(FVector::Dist(Samples[i].TipLocation, BallLocation));
		OverallMinCm = FMath::Min(OverallMinCm, D);

		// (a) 시간 창
		const double TimeGap = FMath::Abs(Samples[i].TimeSeconds - IdealContactTime);
		if (TimeGap > ContactTimeWindowSec)
		{
			continue;
		}

		// (b) 동작 게이트 (중앙차분 속도)
		const int32 P = FMath::Max(0, i - 1);
		const int32 Nx = FMath::Min(Samples.Num() - 1, i + 1);
		if (ComputeSpeedMps(Samples[P], Samples[Nx]) < MinSwingSpeedMps)
		{
			continue;
		}

		if (D < ContactDistCm)
		{
			ContactDistCm = D;
			ContactIdx = i;
		}
	}

	if (ContactIdx != INDEX_NONE)
	{
		Out.ContactDistanceCm = ContactDistCm;
		Out.bContacted = (ContactDistCm <= ContactRadiusCm);

		// 3) 컨택 순간 속도 (peak 아님)
		const int32 Prev = FMath::Max(0, ContactIdx - 1);
		const int32 Next = FMath::Min(Samples.Num() - 1, ContactIdx + 1);
		// 컨택 순간 속도: 트래커 제공값 우선(정확), 없으면 위치 중앙차분.
		const float ReportedContactCmps = static_cast<float>(Samples[ContactIdx].Velocity.Size());
		Out.ContactSpeedMps = (ReportedContactCmps > KINDA_SMALL_NUMBER)
			? ReportedContactCmps / 100.0f
			: ComputeSpeedMps(Samples[Prev], Samples[Next]);

		// 4) 타이밍 오차: 컨택 시각 - 이상 시각
		Out.TimingErrorSeconds = static_cast<float>(Samples[ContactIdx].TimeSeconds - IdealContactTime);
	}
	else
	{
		// 시간 창 안에서 스윙 동작이 없었다 → 헛스윙. 거리만 참고용으로 남긴다.
		Out.ContactDistanceCm = OverallMinCm;
		// bContacted=false, ContactSpeedMps=0, TimingErrorSeconds=0 (기본값 유지)
	}

	// TODO: 스윙 평면각 — 궤적 주성분(PCA) 또는 시작→컨택 벡터의 수평 대비 각도.
	Out.SwingPlaneAngleDeg = 0.0f;

	return Out;
}
