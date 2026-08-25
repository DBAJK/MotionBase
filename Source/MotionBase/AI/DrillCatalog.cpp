#include "AI/DrillCatalog.h"

namespace
{
	FTrainingDrill MakeDrill(EWeaknessAxis Axis, const TCHAR* Name, const TCHAR* Desc, const TCHAR* Cue)
	{
		FTrainingDrill D;
		D.TargetAxis = Axis;
		D.Name = Name;
		D.Description = Desc;
		D.FocusCue = Cue;
		return D;
	}
}

TArray<FTrainingDrill> UDrillCatalog::DrillsForAxis(EWeaknessAxis Axis)
{
	switch (Axis)
	{
	case EWeaknessAxis::ContactRate:
		return {
			MakeDrill(EWeaknessAxis::ContactRate, TEXT("Tracking drill"),
				TEXT("Follow the ball with your eyes from release to impact, watch 10 pitches (no swing)."),
				TEXT("Watch the ball longer")),
			MakeDrill(EWeaknessAxis::ContactRate, TEXT("Soft-toss contact"),
				TEXT("Focus only on making contact with slow, close soft tosses, 20 balls."),
				TEXT("Contact first")),
		};

	case EWeaknessAxis::Timing:
		return {
			MakeDrill(EWeaknessAxis::Timing, TEXT("Rhythm step drill"),
				TEXT("Repeat a consistent front-foot step timed to the pitcher's release, 15 swings."),
				TEXT("Release = step")),
			MakeDrill(EWeaknessAxis::Timing, TEXT("Variable-speed soft toss"),
				TEXT("Mix slow and fast tosses, adjust your timing to the ball."),
				TEXT("Wait for the ball")),
		};

	case EWeaknessAxis::ContactAccuracy:
		return {
			MakeDrill(EWeaknessAxis::ContactAccuracy, TEXT("Tee precision contact"),
				TEXT("Hit the center of a stationary tee ball on the bat's sweet spot, 20 swings."),
				TEXT("Barrel center")),
			MakeDrill(EWeaknessAxis::ContactAccuracy, TEXT("Zone soft toss"),
				TEXT("Toss to split high/low and inside/outside zones, make clean contact in each."),
				TEXT("Aim per zone")),
		};

	case EWeaknessAxis::BatSpeed:
		return {
			MakeDrill(EWeaknessAxis::BatSpeed, TEXT("Rotational power throw"),
				TEXT("Throw a medicine ball hard in the hitting direction to build lower-body/core rotation, 10 reps x3."),
				TEXT("Rotate from the legs")),
			MakeDrill(EWeaknessAxis::BatSpeed, TEXT("Resistance-band swing"),
				TEXT("Swing against band resistance while keeping your path, accelerate through, 12 swings x3."),
				TEXT("Accelerate at impact")),
		};

	case EWeaknessAxis::Consistency:
		return {
			MakeDrill(EWeaknessAxis::Consistency, TEXT("Fixed-routine reps"),
				TEXT("Repeat the same setup-step-swing routine for 10 swings, check the feel each time."),
				TEXT("Repeat the same move")),
			MakeDrill(EWeaknessAxis::Consistency, TEXT("Checkpoint swing"),
				TEXT("Slow swings checking posture checkpoints (grip, elbow, rotation) every time."),
				TEXT("Check your posture")),
		};

	case EWeaknessAxis::HipShoulderSeparation:
		return {
			MakeDrill(EWeaknessAxis::HipShoulderSeparation, TEXT("Hip-lead separation drill"),
				TEXT("Keep the upper body back and open the hips first, 10 slow swings to store the coil."),
				TEXT("Hips first, shoulders back")),
			MakeDrill(EWeaknessAxis::HipShoulderSeparation, TEXT("Band coil hold"),
				TEXT("Fix the upper body with a band and rotate only the lower body to feel the separation, 8 reps x3."),
				TEXT("Upper and lower apart")),
		};

	case EWeaknessAxis::HeadStability:
		return {
			MakeDrill(EWeaknessAxis::HeadStability, TEXT("Eyes-fixed tee batting"),
				TEXT("Stare at one point on the tee ball until impact and keep the head still, 15 swings."),
				TEXT("Never leave the ball")),
			MakeDrill(EWeaknessAxis::HeadStability, TEXT("Head-still mirror drill"),
				TEXT("Swing in front of a mirror/video and check the head does not sway in height or sideways, 10 swings."),
				TEXT("Keep head height")),
		};

	case EWeaknessAxis::KineticChain:
		return {
			MakeDrill(EWeaknessAxis::KineticChain, TEXT("Step-hip-hand sequence drill"),
				TEXT("Exaggerate the front-foot land -> hip turn -> hands order slowly, 12 swings to learn the sequence."),
				TEXT("Transfer bottom-up")),
			MakeDrill(EWeaknessAxis::KineticChain, TEXT("Med-ball rotation throw"),
				TEXT("Wind up from the legs and throw, ingraining hip -> torso -> arm order, 10 reps x3."),
				TEXT("Start from the legs")),
		};

	case EWeaknessAxis::WeightShift:
		return {
			MakeDrill(EWeaknessAxis::WeightShift, TEXT("Back-to-front load drill"),
				TEXT("Load onto the back foot then shift to the front foot with the swing, isolate and repeat, 12 swings."),
				TEXT("Back to front")),
			MakeDrill(EWeaknessAxis::WeightShift, TEXT("Step-through batting"),
				TEXT("Push weight forward with a light step while hitting, keep balance after moving, 10 swings."),
				TEXT("Plant the front foot")),
		};

	// ── Fielding (catch) fitness drills ──
	case EWeaknessAxis::CatchReaction:
		return {
			MakeDrill(EWeaknessAxis::CatchReaction, TEXT("Reaction catch drill"),
				TEXT("Catch balls thrown without warning or bounced off a wall immediately, 20 reps."),
				TEXT("Hands before you think")),
			MakeDrill(EWeaknessAxis::CatchReaction, TEXT("Light reaction touch"),
				TEXT("Reach and touch a randomly firing cue (light / partner's hand), 30s x3."),
				TEXT("React on the cue")),
		};

	case EWeaknessAxis::UpperBodyFlex:
		return {
			MakeDrill(EWeaknessAxis::UpperBodyFlex, TEXT("Thoracic-shoulder rotation stretch"),
				TEXT("Twist the upper body far side to side, hold 10s each, 5 per side - build reaching range."),
				TEXT("Extend your reach range")),
			MakeDrill(EWeaknessAxis::UpperBodyFlex, TEXT("Band overhead reach"),
				TEXT("Hold a band and sweep the arms in a big overhead circle, 12 reps x2."),
				TEXT("Widen the range")),
		};

	case EWeaknessAxis::FootSpeed:
		return {
			MakeDrill(EWeaknessAxis::FootSpeed, TEXT("Ladder quick steps"),
				TEXT("Step through a ladder/line raising your foot turnover, 30s x3."),
				TEXT("Short, fast steps")),
			MakeDrill(EWeaknessAxis::FootSpeed, TEXT("Side shuffle"),
				TEXT("Shuffle side to side fast in a low stance, 10m x4 - positioning quickness."),
				TEXT("Low and fast")),
		};

	// ── Fielding (throw) drills ──
	case EWeaknessAxis::ThrowAccuracy:
		return {
			MakeDrill(EWeaknessAxis::ThrowAccuracy, TEXT("Target line throws"),
				TEXT("Throw to a chest-high target from 20m, 20 reps - step straight at the target every time."),
				TEXT("Front foot points at the base")),
			MakeDrill(EWeaknessAxis::ThrowAccuracy, TEXT("One-hop to the bag"),
				TEXT("From long range, aim a deliberate one-hop into the receiver's glove, 15 reps."),
				TEXT("Low miss, never high")),
		};

	case EWeaknessAxis::ArmStrength:
		return {
			MakeDrill(EWeaknessAxis::ArmStrength, TEXT("Long toss ladder"),
				TEXT("Build distance 15m -> 25m -> 35m and come back down, 5 throws per step."),
				TEXT("Carry, do not aim")),
			MakeDrill(EWeaknessAxis::ArmStrength, TEXT("Med-ball crow hop throw"),
				TEXT("Crow-hop and throw a 2kg med ball with the whole body, 8 reps x3."),
				TEXT("Legs and trunk, not the arm")),
		};

	case EWeaknessAxis::TransferQuick:
		return {
			MakeDrill(EWeaknessAxis::TransferQuick, TEXT("Glove-to-hand transfer reps"),
				TEXT("Catch and move the ball to the throwing hand at the chest, 30 reps - no throw, transfer only."),
				TEXT("Bring it to the chest, not the ear")),
			MakeDrill(EWeaknessAxis::TransferQuick, TEXT("Quick-release footwork"),
				TEXT("Catch - right/left step - release as one motion, 20 reps at short range."),
				TEXT("Feet start with the catch")),
		};

	// ── Fielding (backup judgment) drills ──
	case EWeaknessAxis::BackupJudgment:
		return {
			MakeDrill(EWeaknessAxis::BackupJudgment, TEXT("Position backup walkthrough"),
				TEXT("For your position, walk the backup path for each batted-ball direction, 10 cases."),
				TEXT("Ball direction decides the base")),
			MakeDrill(EWeaknessAxis::BackupJudgment, TEXT("Runner-situation card review"),
				TEXT("Fix the runners (none / 1st / 2nd) and say your backup base out loud for each, 15 cases."),
				TEXT("Runners decide the throw")),
		};

	case EWeaknessAxis::DecisionSpeed:
		return {
			MakeDrill(EWeaknessAxis::DecisionSpeed, TEXT("Call-it-out reaction"),
				TEXT("A partner calls a situation, you name the backup base within 2 seconds, 20 reps."),
				TEXT("Decide before you move")),
			MakeDrill(EWeaknessAxis::DecisionSpeed, TEXT("Pre-pitch routine"),
				TEXT("Before every pitch, say your job for each batted-ball direction, one full inning."),
				TEXT("Decide it before the pitch")),
		};

	default:
		return {};
	}
}

TArray<FTrainingDrill> UDrillCatalog::Recommend(const FWeaknessReport& Report, int32 MaxDrills)
{
	TArray<FTrainingDrill> Out;
	MaxDrills = FMath::Max(MaxDrills, 1);

	if (!Report.bValid)
	{
		return Out;
	}

	// 약점이 없으면 유지용 기본 드릴.
	// ⚠️ 모드에 맞는 것을 줘야 한다 — 수비 세션 끝에 "라이브 배팅을 계속하세요"가 나오면
	//    추천 전체의 신뢰가 무너진다 (약점 0개는 잘한 세션이라 오히려 자주 나온다).
	if (Report.Weaknesses.Num() == 0)
	{
		if (Report.Mode == EGameModeId::Defense)
		{
			Out.Add(MakeDrill(EWeaknessAxis::CatchReaction, TEXT("Keep the fielding routine"),
				TEXT("Nothing stands out - keep taking game-speed reps and hold this feel."),
				TEXT("Hold current feel")));
		}
		else
		{
			Out.Add(MakeDrill(EWeaknessAxis::Consistency, TEXT("Keep game feel"),
				TEXT("Keep taking live batting at game pace to maintain your current balance."),
				TEXT("Hold current feel")));
		}
		return Out;
	}

	// 시급한 약점부터(리포트가 이미 정렬됨) 드릴을 뽑되, 이름 중복은 건너뛴다.
	TSet<FString> Seen;
	for (const FWeakness& W : Report.Weaknesses)
	{
		for (const FTrainingDrill& D : DrillsForAxis(W.Axis))
		{
			if (Out.Num() >= MaxDrills)
			{
				return Out;
			}
			if (!Seen.Contains(D.Name))
			{
				Seen.Add(D.Name);
				Out.Add(D);
			}
		}
	}
	return Out;
}

TArray<FTrainingDrill> UDrillCatalog::RecommendWithHistory(
	const FWeaknessReport& Report, const FChronicWeaknessReport& Chronic, int32 MaxDrills)
{
	// 이력이 없으면 단발 추천과 동일하게 — 신규 사용자/첫 세션 경로.
	if (!Chronic.bValid)
	{
		return Recommend(Report, MaxDrills);
	}

	TArray<FTrainingDrill> Out;
	MaxDrills = FMath::Max(MaxDrills, 1);

	if (!Report.bValid)
	{
		return Out;
	}

	// 축별 만성 추세를 빠르게 찾기 위한 색인.
	TMap<EWeaknessAxis, const FAxisTrend*> TrendByAxis;
	for (const FAxisTrend& T : Chronic.Trends)
	{
		TrendByAxis.Add(T.Axis, &T);
	}

	// 이번 세션 약점을 (현재 심각도 + 만성 가중)으로 재정렬한다.
	// 만성 가중: 반복 등장 비율 * 0.5, 악화면 +0.25 더. 단발 심각도(0~1)와 같은 스케일.
	struct FRanked { EWeaknessAxis Axis; float Priority; float Severity; int32 Appearances; };
	TArray<FRanked> Ranked;
	for (const FWeakness& W : Report.Weaknesses)
	{
		FRanked R;
		R.Axis = W.Axis;
		R.Severity = W.Severity;
		R.Appearances = 0;
		float Boost = 0.0f;
		if (const FAxisTrend* const* Found = TrendByAxis.Find(W.Axis))
		{
			const FAxisTrend* T = *Found;
			R.Appearances = T->AppearanceCount;
			if (T->WindowSize > 0)
			{
				Boost += 0.5f * (static_cast<float>(T->AppearanceCount) / T->WindowSize);
			}
			if (T->Trend == EWeaknessTrend::Worsening) { Boost += 0.25f; }
		}
		R.Priority = W.Severity + Boost;
		Ranked.Add(R);
	}

	// 우선순위 내림차순 — 만성·악화 약점이 앞으로.
	Ranked.Sort([](const FRanked& A, const FRanked& B) { return A.Priority > B.Priority; });

	// 축마다 드릴을 뽑되, 반복 처방된 축은 등장 횟수만큼 로테이션해 다른 드릴을 낸다.
	// (같은 약점에 매번 같은 운동만 나오면 질린다 — 카탈로그를 돌려 쓴다.)
	TSet<FString> Seen;
	for (const FRanked& R : Ranked)
	{
		if (Out.Num() >= MaxDrills)
		{
			break;
		}
		const TArray<FTrainingDrill> Drills = DrillsForAxis(R.Axis);
		if (Drills.Num() == 0)
		{
			continue;
		}
		const int32 Start = R.Appearances % Drills.Num();
		// 로테이션 시작점부터 한 바퀴 돌며 아직 안 뽑힌 첫 드릴을 고른다.
		for (int32 k = 0; k < Drills.Num(); ++k)
		{
			const FTrainingDrill& D = Drills[(Start + k) % Drills.Num()];
			if (!Seen.Contains(D.Name))
			{
				Seen.Add(D.Name);
				Out.Add(D);
				break;
			}
		}
	}

	return Out;
}
