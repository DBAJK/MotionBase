#include "Misc/AutomationTest.h"
#include "Analysis/BodyMechanicsAnalyzer.h"

#if WITH_DEV_AUTOMATION_TESTS

// UBodyMechanicsAnalyzer 순수 로직 단위 테스트 — 카메라·헤드셋 없이 헤드리스 실행.
// 실행: 에디터 Session Frontend > Automation > "MotionBase.Analysis.BodyMechanics"
//   또는 커맨드라인 -ExecCmds="Automation RunTests MotionBase.Analysis.BodyMechanics; Quit"

namespace
{
	/** 수평면(XY)에서 각도 θ(도)로 회전한 좌·우 관절 위치를 중심 C·반길이 r 로 만든다. */
	void SetSegment(FCameraPoseFrame& F, EBodyLandmark Left, EBodyLandmark Right,
		const FVector& C, float HalfLen, float AngleDeg, float Visibility)
	{
		const float Rad = FMath::DegreesToRadians(AngleDeg);
		const FVector Dir(FMath::Cos(Rad), FMath::Sin(Rad), 0.0f);
		F.Landmarks[static_cast<int32>(Right)].Position = C + Dir * HalfLen;
		F.Landmarks[static_cast<int32>(Right)].Visibility = Visibility;
		F.Landmarks[static_cast<int32>(Left)].Position = C - Dir * HalfLen;
		F.Landmarks[static_cast<int32>(Left)].Visibility = Visibility;
	}

	/** 0→1 로 부드럽게 증가, Center 에서 기울기(=각속도) 최대인 램프. */
	float SmoothRamp(float T, float Center, float Width)
	{
		return FMath::Clamp((T - Center) / Width + 0.5f, 0.0f, 1.0f);
	}

	/**
	 * 정상 kinetic chain 스윙 합성:
	 * 힙 회전 램프 중심 = HipCenter, 어깨 = ShldCenter( > HipCenter ), 컨택 = 0.20s.
	 * → 힙 각속도 피크가 어깨보다, 어깨가 컨택보다 앞선다.
	 * 힙이 어깨보다 더 크게 열리므로(HipMaxDeg > ShldMaxDeg) 컨택 순간 X-factor 분리각이 남는다.
	 */
	TArray<FCameraPoseFrame> BuildChainSwing(float HipCenter, float ShldCenter,
		float HipMaxDeg, float ShldMaxDeg)
	{
		constexpr int32 N = 40;
		constexpr float Dt = 0.01f;      // 100 fps 합성
		constexpr float Width = 0.06f;

		TArray<FCameraPoseFrame> Frames;
		Frames.Reserve(N);
		for (int32 i = 0; i < N; ++i)
		{
			const float T = i * Dt;
			FCameraPoseFrame F; // 생성자가 Landmarks 를 Count 만큼 채움
			F.TimeSeconds = T;
			F.bTracked = true;

			const float HipAngle  = HipMaxDeg  * SmoothRamp(T, HipCenter, Width);
			const float ShldAngle = ShldMaxDeg * SmoothRamp(T, ShldCenter, Width);

			// 골반: z=100, 앞으로(X+) 체중 이동. 어깨: z=140. (척추 수직 근처)
			const FVector HipCenterPos(20.0f * SmoothRamp(T, HipCenter, Width), 0.0f, 100.0f);
			SetSegment(F, EBodyLandmark::LeftHip, EBodyLandmark::RightHip, HipCenterPos, 15.0f, HipAngle, 1.0f);
			SetSegment(F, EBodyLandmark::LeftShoulder, EBodyLandmark::RightShoulder,
				FVector(HipCenterPos.X, 0.0f, 140.0f), 20.0f, ShldAngle, 1.0f);

			// 코(머리): 거의 고정.
			F.Landmarks[static_cast<int32>(EBodyLandmark::Nose)].Position = FVector(0.0f, 0.0f, 160.0f);
			F.Landmarks[static_cast<int32>(EBodyLandmark::Nose)].Visibility = 1.0f;

			Frames.Add(MoveTemp(F));
		}
		return Frames;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBodyMechanicsChainTest,
	"MotionBase.Analysis.BodyMechanics.KineticChain",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBodyMechanicsChainTest::RunTest(const FString& Parameters)
{
	FBodyMechanicsConfig Config; // 기본 게이트 (MinVisibility 0.5, MinValidFrames 5)
	const double ContactTime = 0.20;

	// 힙 램프 중심 0.08s, 어깨 0.13s → 힙 먼저, 어깨 뒤, 컨택 0.20s.
	// 컨택 시 힙 55° / 어깨 40° → X-factor 분리각 ≈ 15° 잔존.
	const TArray<FCameraPoseFrame> Frames = BuildChainSwing(0.08f, 0.13f, 55.0f, 40.0f);
	const FBodyMechanicsMetrics M = UBodyMechanicsAnalyzer::Analyze(Frames, ContactTime, Config);

	TestTrue(TEXT("지표 유효"), M.bValid);
	TestTrue(TEXT("kinetic chain 정상 순서(힙→어깨→컨택)"), M.bKineticChainOrdered);
	TestTrue(TEXT("힙 피크 선행 > 어깨 피크 선행 (힙이 더 일찍)"), M.HipPeakLeadMs > M.ShoulderPeakLeadMs);
	TestTrue(TEXT("힙 피크가 컨택보다 앞섬"), M.HipPeakLeadMs > 0.0f);
	TestTrue(TEXT("X-factor 분리각 존재"), M.HipShoulderSeparationDeg > 1.0f);
	TestTrue(TEXT("체중 이동 감지"), M.WeightShiftCm > 1.0f);
	TestTrue(TEXT("머리 거의 고정"), M.HeadTravelCm < 5.0f);
	TestTrue(TEXT("신뢰도 높음"), M.Confidence > 0.9f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBodyMechanicsInsufficientTest,
	"MotionBase.Analysis.BodyMechanics.Insufficient",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBodyMechanicsInsufficientTest::RunTest(const FString& Parameters)
{
	FBodyMechanicsConfig Config;

	// 프레임 부족 → 미산출(bValid=false).
	TArray<FCameraPoseFrame> Few;
	for (int32 i = 0; i < 3; ++i)
	{
		FCameraPoseFrame F;
		F.TimeSeconds = i * 0.01;
		F.bTracked = true;
		Few.Add(MoveTemp(F));
	}
	const FBodyMechanicsMetrics M = UBodyMechanicsAnalyzer::Analyze(Few, 0.02, Config);
	TestFalse(TEXT("프레임 부족 시 미산출"), M.bValid);

	// 빈 입력도 안전.
	const FBodyMechanicsMetrics Empty = UBodyMechanicsAnalyzer::Analyze({}, 0.0, Config);
	TestFalse(TEXT("빈 입력 미산출"), Empty.bValid);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
