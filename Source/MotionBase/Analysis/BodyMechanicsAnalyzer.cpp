#include "Analysis/BodyMechanicsAnalyzer.h"
#include "MotionBase.h"

namespace
{
	/** 두 관절이 모두 게이트를 통과하는지. */
	bool BothVisible(const FCameraPoseFrame& F, EBodyLandmark A, EBodyLandmark B, float MinVis)
	{
		return F.GetLandmark(A).Visibility >= MinVis
			&& F.GetLandmark(B).Visibility >= MinVis;
	}

	/** 좌우 관절의 중점. */
	FVector Midpoint(const FCameraPoseFrame& F, EBodyLandmark A, EBodyLandmark B)
	{
		return (F.GetLandmark(A).Position + F.GetLandmark(B).Position) * 0.5f;
	}
}

float UBodyMechanicsAnalyzer::HorizontalLineAngleDeg(const FVector& Left, const FVector& Right)
{
	const double Dx = Right.X - Left.X;
	const double Dy = Right.Y - Left.Y;
	return FMath::RadiansToDegrees(static_cast<float>(FMath::Atan2(Dy, Dx)));
}

float UBodyMechanicsAnalyzer::WrapDeg(float Deg)
{
	Deg = FMath::Fmod(Deg, 360.0f);
	if (Deg > 180.0f)  { Deg -= 360.0f; }
	if (Deg <= -180.0f) { Deg += 360.0f; }
	return Deg;
}

int32 UBodyMechanicsAnalyzer::FindFrameNearestTime(const TArray<FCameraPoseFrame>& Frames, double T)
{
	int32 Best = INDEX_NONE;
	double BestGap = TNumericLimits<double>::Max();
	for (int32 i = 0; i < Frames.Num(); ++i)
	{
		const double Gap = FMath::Abs(Frames[i].TimeSeconds - T);
		if (Gap < BestGap)
		{
			BestGap = Gap;
			Best = i;
		}
	}
	return Best;
}

float UBodyMechanicsAnalyzer::FindAngularVelocityPeak(
	const TArray<FCameraPoseFrame>& Frames,
	EBodyLandmark Left,
	EBodyLandmark Right,
	float MinVisibility,
	double& OutTime)
{
	OutTime = -1.0;

	// 1) 게이트 통과 프레임의 (시각, 언랩 각도) 시퀀스 수집.
	TArray<double> Times;
	TArray<float>  Angles;
	Times.Reserve(Frames.Num());
	Angles.Reserve(Frames.Num());

	float Unwrapped = 0.0f;
	bool bHavePrev = false;
	float PrevRaw = 0.0f;

	for (const FCameraPoseFrame& F : Frames)
	{
		if (!F.bTracked || !BothVisible(F, Left, Right, MinVisibility))
		{
			continue;
		}
		const float Raw = HorizontalLineAngleDeg(
			F.GetLandmark(Left).Position, F.GetLandmark(Right).Position);

		if (!bHavePrev)
		{
			Unwrapped = Raw;
			bHavePrev = true;
		}
		else
		{
			// 인접 프레임 각도 점프를 (-180,180]로 보정해 누적 (언랩).
			Unwrapped += WrapDeg(Raw - PrevRaw);
		}
		PrevRaw = Raw;

		Times.Add(F.TimeSeconds);
		Angles.Add(Unwrapped);
	}

	// 2) 중앙차분으로 각속도 피크 탐색.
	float Peak = 0.0f;
	for (int32 i = 1; i + 1 < Angles.Num(); ++i)
	{
		const double Dt = Times[i + 1] - Times[i - 1];
		if (Dt <= KINDA_SMALL_NUMBER)
		{
			continue;
		}
		const float AngVel = FMath::Abs((Angles[i + 1] - Angles[i - 1]) / static_cast<float>(Dt));
		if (AngVel > Peak)
		{
			Peak = AngVel;
			OutTime = Times[i];
		}
	}
	return Peak;
}

FBodyMechanicsMetrics UBodyMechanicsAnalyzer::Analyze(
	const TArray<FCameraPoseFrame>& Frames,
	double ContactTime,
	const FBodyMechanicsConfig& Config)
{
	FBodyMechanicsMetrics Out;

	// 유효(추적 성공) 프레임 수 게이트.
	int32 TrackedCount = 0;
	for (const FCameraPoseFrame& F : Frames)
	{
		if (F.bTracked) { ++TrackedCount; }
	}
	if (TrackedCount < Config.MinValidFrames)
	{
		UE_LOG(LogMotionBase, Verbose,
			TEXT("BodyMechanics: 유효 프레임 부족(%d/%d) — 미산출"),
			TrackedCount, Config.MinValidFrames);
		return Out; // bValid=false
	}

	const int32 ContactIdx = FindFrameNearestTime(Frames, ContactTime);
	if (ContactIdx == INDEX_NONE)
	{
		return Out;
	}
	const FCameraPoseFrame& CFrame = Frames[ContactIdx];

	// 시작 자세 = 첫 추적 프레임 (회전/이동의 기준).
	int32 StartIdx = INDEX_NONE;
	for (int32 i = 0; i < Frames.Num(); ++i)
	{
		if (Frames[i].bTracked) { StartIdx = i; break; }
	}
	const FCameraPoseFrame& SFrame = Frames[StartIdx];

	// ---- 1) 회전각 (시작 대비) & X-factor ----
	const float MinVis = Config.MinVisibility;
	const bool bHipsC = BothVisible(CFrame, EBodyLandmark::LeftHip, EBodyLandmark::RightHip, MinVis);
	const bool bShldC = BothVisible(CFrame, EBodyLandmark::LeftShoulder, EBodyLandmark::RightShoulder, MinVis);

	if (bHipsC && BothVisible(SFrame, EBodyLandmark::LeftHip, EBodyLandmark::RightHip, MinVis))
	{
		const float H0 = HorizontalLineAngleDeg(
			SFrame.GetLandmark(EBodyLandmark::LeftHip).Position,
			SFrame.GetLandmark(EBodyLandmark::RightHip).Position);
		const float H1 = HorizontalLineAngleDeg(
			CFrame.GetLandmark(EBodyLandmark::LeftHip).Position,
			CFrame.GetLandmark(EBodyLandmark::RightHip).Position);
		Out.HipRotationAtContactDeg = WrapDeg(H1 - H0);
	}
	if (bShldC && BothVisible(SFrame, EBodyLandmark::LeftShoulder, EBodyLandmark::RightShoulder, MinVis))
	{
		const float S0 = HorizontalLineAngleDeg(
			SFrame.GetLandmark(EBodyLandmark::LeftShoulder).Position,
			SFrame.GetLandmark(EBodyLandmark::RightShoulder).Position);
		const float S1 = HorizontalLineAngleDeg(
			CFrame.GetLandmark(EBodyLandmark::LeftShoulder).Position,
			CFrame.GetLandmark(EBodyLandmark::RightShoulder).Position);
		Out.ShoulderRotationAtContactDeg = WrapDeg(S1 - S0);
	}

	// X-factor: 컨택 순간 어깨-힙 라인 분리각 (프레임 내 상대각이라 시작 기준 불필요).
	if (bHipsC && bShldC)
	{
		const float HipAngle = HorizontalLineAngleDeg(
			CFrame.GetLandmark(EBodyLandmark::LeftHip).Position,
			CFrame.GetLandmark(EBodyLandmark::RightHip).Position);
		const float ShldAngle = HorizontalLineAngleDeg(
			CFrame.GetLandmark(EBodyLandmark::LeftShoulder).Position,
			CFrame.GetLandmark(EBodyLandmark::RightShoulder).Position);
		Out.HipShoulderSeparationDeg = FMath::Abs(WrapDeg(ShldAngle - HipAngle));
	}

	// ---- 2) Kinetic chain: 힙/어깨 각속도 피크 시각 vs 컨택 ----
	double HipPeakTime = -1.0, ShldPeakTime = -1.0;
	FindAngularVelocityPeak(Frames, EBodyLandmark::LeftHip, EBodyLandmark::RightHip, MinVis, HipPeakTime);
	FindAngularVelocityPeak(Frames, EBodyLandmark::LeftShoulder, EBodyLandmark::RightShoulder, MinVis, ShldPeakTime);

	if (HipPeakTime >= 0.0)
	{
		Out.HipPeakLeadMs = static_cast<float>((ContactTime - HipPeakTime) * 1000.0);
	}
	if (ShldPeakTime >= 0.0)
	{
		Out.ShoulderPeakLeadMs = static_cast<float>((ContactTime - ShldPeakTime) * 1000.0);
	}
	// 정상 순서: 힙 피크 → 어깨 피크 → 컨택 (모두 시간순 선행).
	Out.bKineticChainOrdered =
		(HipPeakTime >= 0.0 && ShldPeakTime >= 0.0)
		&& (HipPeakTime <= ShldPeakTime)
		&& (ShldPeakTime <= ContactTime);

	// ---- 3) 체중 이동: 골반 중점 수평 이동 (시작→컨택) ----
	if (bHipsC && BothVisible(SFrame, EBodyLandmark::LeftHip, EBodyLandmark::RightHip, MinVis))
	{
		const FVector P0 = Midpoint(SFrame, EBodyLandmark::LeftHip, EBodyLandmark::RightHip);
		const FVector P1 = Midpoint(CFrame, EBodyLandmark::LeftHip, EBodyLandmark::RightHip);
		Out.WeightShiftCm = FVector::Dist2D(P0, P1); // 수평면(XY)만
	}

	// ---- 4) 머리 이동량: 시작→컨택 구간 최대 코 변위 ----
	if (SFrame.GetLandmark(EBodyLandmark::Nose).Visibility >= MinVis)
	{
		const FVector Head0 = SFrame.GetLandmark(EBodyLandmark::Nose).Position;
		float MaxTravel = 0.0f;
		for (int32 i = StartIdx; i <= ContactIdx; ++i)
		{
			const FBodyLandmark& N = Frames[i].GetLandmark(EBodyLandmark::Nose);
			if (Frames[i].bTracked && N.Visibility >= MinVis)
			{
				MaxTravel = FMath::Max(MaxTravel, static_cast<float>(FVector::Dist(Head0, N.Position)));
			}
		}
		Out.HeadTravelCm = MaxTravel;
	}

	// ---- 5) 척추 기울기: 컨택 순간 골반중점→어깨중점 vs 수직 ----
	if (bHipsC && bShldC)
	{
		const FVector HipMid = Midpoint(CFrame, EBodyLandmark::LeftHip, EBodyLandmark::RightHip);
		const FVector ShldMid = Midpoint(CFrame, EBodyLandmark::LeftShoulder, EBodyLandmark::RightShoulder);
		const FVector Spine = ShldMid - HipMid;
		if (!Spine.IsNearlyZero())
		{
			const float CosTheta = FMath::Abs(Spine.GetSafeNormal().Z); // 수직 성분
			Out.SpineTiltDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(CosTheta, 0.0f, 1.0f)));
		}
	}

	// ---- 6) 신뢰도: 추적 프레임들의 평균 visibility ----
	double VisSum = 0.0;
	int32 VisN = 0;
	for (const FCameraPoseFrame& F : Frames)
	{
		if (!F.bTracked) { continue; }
		for (const FBodyLandmark& L : F.Landmarks)
		{
			VisSum += L.Visibility;
			++VisN;
		}
	}
	Out.Confidence = (VisN > 0) ? static_cast<float>(VisSum / VisN) : 0.0f;

	Out.bValid = true;
	return Out;
}
