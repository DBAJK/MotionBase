#include "Input/MockCameraPoseSource.h"

TArray<FCameraPoseFrame> UMockCameraPoseSource::BuildSwingSequence(double ContactTime, const FMockSwingPoseParams& Params)
{
	TArray<FCameraPoseFrame> Frames;

	const float Hz = FMath::Max(Params.SampleHz, 1.0f);
	const double Dt = 1.0 / Hz;
	const int32 NumPre = FMath::Max(FMath::RoundToInt(Params.PreContactSec * Hz), 1);
	const int32 NumPost = FMath::Max(FMath::RoundToInt(Params.PostContactSec * Hz), 0);
	const int32 Count = static_cast<int32>(EBodyLandmark::Count);

	// 기준 골격 자리 (cm, z-up).
	const FVector HipCenterStart(0.0f, 0.0f, 100.0f);
	const FVector NoseBase = HipCenterStart + FVector(0.0f, 0.0f, 150.0f);
	const float SpineHeight = 50.0f; // 골반중점→어깨중점 수직거리
	const float HipHalfWidth = 18.0f;
	const float ShoulderHalfWidth = 20.0f;

	// 각속도 피크가 지정 시각에 오도록 하는 smoothstep 램프 (피크=중심 t0).
	const double HalfWindow = 0.10;
	auto Ramp = [HalfWindow](double T, double T0) -> float
	{
		double U = (T - T0 + HalfWindow) / (2.0 * HalfWindow);
		U = FMath::Clamp(U, 0.0, 1.0);
		return static_cast<float>(U * U * (3.0 - 2.0 * U));
	};

	const double T0Hip = ContactTime - Params.HipPeakLeadSec;
	const double T0Shld = ContactTime - Params.ShoulderPeakLeadSec;
	const float TiltRad = FMath::DegreesToRadians(Params.SpineTiltDeg);
	const FVector SpineVec(SpineHeight * FMath::Tan(TiltRad), 0.0f, SpineHeight);

	Frames.Reserve(NumPre + NumPost + 1);
	for (int32 i = -NumPre; i <= NumPost; ++i)
	{
		const double T = ContactTime + i * Dt;

		FCameraPoseFrame F; // 생성자가 Landmarks 를 Count 크기로 채운다.
		F.TimeSeconds = T;
		F.bTracked = true;

		// 모든 관절에 동일 visibility (게이트 통과 + 신뢰도 = Visibility).
		for (int32 k = 0; k < Count; ++k)
		{
			F.Landmarks[k].Visibility = Params.Visibility;
			F.Landmarks[k].Position = HipCenterStart; // 미사용 관절 자리표시자
		}

		const float SH = Ramp(T, T0Hip);
		const float SS = Ramp(T, T0Shld);
		const float HipRad = FMath::DegreesToRadians(Params.HipRotationDeg * SH);
		const float ShldRad = FMath::DegreesToRadians(Params.ShoulderRotationDeg * SS);

		const FVector HipCenter = HipCenterStart + FVector(Params.WeightShiftCm * SH, 0.0f, 0.0f);
		const FVector ShldCenter = HipCenter + SpineVec;
		const FVector HipDir(FMath::Cos(HipRad), FMath::Sin(HipRad), 0.0f);
		const FVector ShldDir(FMath::Cos(ShldRad), FMath::Sin(ShldRad), 0.0f);

		auto Set = [&F, &Params](EBodyLandmark Joint, const FVector& Pos)
		{
			const int32 Idx = static_cast<int32>(Joint);
			F.Landmarks[Idx].Position = Pos;
			F.Landmarks[Idx].Visibility = Params.Visibility;
		};

		Set(EBodyLandmark::LeftHip,       HipCenter - HipDir * HipHalfWidth);
		Set(EBodyLandmark::RightHip,      HipCenter + HipDir * HipHalfWidth);
		Set(EBodyLandmark::LeftShoulder,  ShldCenter - ShldDir * ShoulderHalfWidth);
		Set(EBodyLandmark::RightShoulder, ShldCenter + ShldDir * ShoulderHalfWidth);
		Set(EBodyLandmark::Nose,          NoseBase + FVector(Params.HeadTravelCm * SS, 0.0f, 0.0f));

		Frames.Add(MoveTemp(F));
	}

	return Frames;
}
