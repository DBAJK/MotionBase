#include "Analysis/HitModel.h"

namespace
{
	// TODO(캘리브레이션): 아래 상수는 실측·플레이테스트로 조정 (하드코딩 확정 금지).
	constexpr float BaseLaunchAngleDeg   = 26.0f;   // 이상적 발사각
	constexpr float LaunchTimingGainDeg  = 80.0f;   // 타이밍 1초당 발사각 변화(도)
	// ⚠️ 좌우각 감도는 컨택 시간 창(`USwingAnalyzer::ContactTimeWindowSec`)과 맞물려 있다.
	//    감도가 낮으면 파울이 될 만큼 빗나간 스윙이 그 전에 헛스윙 처리돼 파울이 거의 안 나오고,
	//    높으면 창 안 대부분이 파울이 된다.
	//    현재: 45°/375 ≈ **0.12s** → 타이밍 오차 0.12s 를 넘는 컨택부터 파울 밴드.
	//    시간 창은 0.32s 이므로 페어(±0.12s) : 파울(0.12~0.32s) 로 갈린다.
	//    ※ 창을 0.22 → 0.32 로 넓힐 때 이 값은 그대로 뒀다. 파울 밴드가 넓어진 셈이라
	//      실기 플레이테스트에서 파울이 과하면 여기부터 낮출 것 (숫자 자체는 미보정).
	constexpr float SprayTimingGainDeg   = 375.0f;  // 타이밍 1초당 좌우각 변화(도)
	constexpr float MinLaunchDeg         = -15.0f;
	constexpr float MaxLaunchDeg         = 55.0f;
	constexpr float MaxSprayDeg          = 70.0f;
	constexpr float GravityMps2          = 9.8f;
	constexpr float CarryFactor          = 0.55f;   // 공기저항 근사 (진공 사거리 × 이 값)
	// ⚠️ 파울 라인 / 안타·홈런 비거리 임계는 **FScoringConfig 로 옮겼다**
	//    (FScoringConfig::ApplyDifficulty 가 난이도별로 조절한다). 여기 상수로 되돌리지 말 것 —
	//    되돌리면 난이도가 판정에 영향을 주지 못해 아마추어가 정타를 쳐도 아웃으로 찍힌다.
	constexpr float HomeRunMinLaunchDeg  = 15.0f;   // 이 발사각 구간에서만 담장을 넘는다
	constexpr float HomeRunMaxLaunchDeg  = 45.0f;
}

float UHitModel::ExitVelocityMps(const FSwingMetrics& M, const FScoringConfig& Config)
{
	if (!M.bContacted)
	{
		return 0.0f;
	}

	// 컨택 품질 0~1: 스위트스팟(0cm)일수록 1, 유효 반경 밖이면 0.
	const float Quality = FMath::Clamp(
		1.0f - (M.ContactDistanceCm / FMath::Max(Config.MaxContactDistanceCm, 1.0f)), 0.0f, 1.0f);

	// 배트 속도 → 타구 속도. 반발계수로 증폭, 컨택 품질로 감쇠.
	// (정확한 물리는 투구 속도까지 필요하지만, 여기선 컨택 속도 기반 간이 모델.)
	const float EV = M.ContactSpeedMps * (1.0f + Config.RestitutionCoeff) * Quality;
	return FMath::Max(EV, 0.0f);
}

FBattedBallResult UHitModel::Simulate(const FSwingMetrics& M, const FScoringConfig& Config)
{
	FBattedBallResult R;

	if (!M.bContacted)
	{
		R.Class = EHitClass::Whiff; // 나머지 필드는 기본 0
		return R;
	}

	R.ExitVelocityMps = ExitVelocityMps(M, Config);

	// 발사각: 늦으면(+) 땅볼 쪽, 이르면(−) 뜬공 쪽.
	R.LaunchAngleDeg = FMath::Clamp(
		BaseLaunchAngleDeg - M.TimingErrorSeconds * LaunchTimingGainDeg, MinLaunchDeg, MaxLaunchDeg);

	// 좌우각: 타이밍 오차에 비례. 극단 타이밍이면 파울 영역까지 간다.
	R.SprayAngleDeg = FMath::Clamp(M.TimingErrorSeconds * SprayTimingGainDeg, -MaxSprayDeg, MaxSprayDeg);

	R.bFair = FMath::Abs(R.SprayAngleDeg) <= Config.FoulLineDeg;

	// 비거리: 포물선 사거리 R = v^2 · sin(2θ) / g, 공기저항 계수 적용.
	const float AngleRad = FMath::DegreesToRadians(R.LaunchAngleDeg);
	const float VacuumRange = (R.ExitVelocityMps * R.ExitVelocityMps) * FMath::Sin(2.0f * AngleRad) / GravityMps2;
	R.CarryDistanceM = FMath::Max(VacuumRange * CarryFactor, 0.0f);

	// 판정.
	if (!R.bFair)
	{
		R.Class = EHitClass::Foul;
	}
	else if (R.CarryDistanceM >= Config.HomeRunDistanceM
		&& R.LaunchAngleDeg >= HomeRunMinLaunchDeg && R.LaunchAngleDeg <= HomeRunMaxLaunchDeg)
	{
		R.Class = EHitClass::HomeRun;
	}
	else if (R.CarryDistanceM >= Config.HitDistanceM)
	{
		R.Class = EHitClass::Hit;
	}
	else
	{
		R.Class = EHitClass::Out; // 약한 땅볼/뜬공
	}

	return R;
}

FText UHitModel::GetClassDisplayName(EHitClass Class)
{
	switch (Class)
	{
	case EHitClass::Whiff:   return FText::FromString(TEXT("헛스윙"));
	case EHitClass::Foul:    return FText::FromString(TEXT("파울"));
	case EHitClass::Out:     return FText::FromString(TEXT("아웃"));
	case EHitClass::Hit:     return FText::FromString(TEXT("안타"));
	case EHitClass::HomeRun: return FText::FromString(TEXT("홈런"));
	default:                 return FText::FromString(TEXT("?"));
	}
}
