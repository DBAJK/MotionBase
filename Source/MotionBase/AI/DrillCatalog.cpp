#include "AI/DrillCatalog.h"

namespace
{
	// Desc = 수행 방법, Benefit = 무엇이 좋아지는지, Prescription = 수행량.
	// ⚠️ 수행량은 Desc 에 섞어 쓰지 않는다 — 두 곳에 적히면 어긋나고, LLM 이 어느 쪽을
	//    인용할지 흔들린다. 횟수·세트는 Prescription 한 곳에만 존재한다.
	FTrainingDrill MakeDrill(EWeaknessAxis Axis, const TCHAR* Name, const TCHAR* Desc, const TCHAR* Cue,
		const TCHAR* Benefit, const TCHAR* Prescription, const TCHAR* PrescriptionShort)
	{
		FTrainingDrill D;
		D.TargetAxis = Axis;
		D.Name = Name;
		D.Description = Desc;
		D.FocusCue = Cue;
		D.Benefit = Benefit;
		D.Prescription = Prescription;
		D.PrescriptionShort = PrescriptionShort;
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
				TEXT("Follow the ball with your eyes from release to impact, no swing."),
				TEXT("Watch the ball longer"),
				TEXT("Trains eye tracking and pitch recognition, so the barrel meets a ball you actually saw"),
				TEXT("3 sets x 10 pitches"), TEXT("3x10")),
			MakeDrill(EWeaknessAxis::ContactRate, TEXT("Soft-toss contact"),
				TEXT("Focus only on making contact with slow, close soft tosses."),
				TEXT("Contact first"),
				TEXT("Builds hand-eye coordination and a repeatable contact point"),
				TEXT("3 sets x 20 balls"), TEXT("3x20")),
		};

	case EWeaknessAxis::Timing:
		return {
			MakeDrill(EWeaknessAxis::Timing, TEXT("Rhythm step drill"),
				TEXT("Repeat a consistent front-foot step timed to the pitcher's release."),
				TEXT("Release = step"),
				TEXT("Syncs your load and stride to the release, so the swing starts on time"),
				TEXT("3 sets x 15 swings"), TEXT("3x15")),
			MakeDrill(EWeaknessAxis::Timing, TEXT("Variable-speed soft toss"),
				TEXT("Mix slow and fast tosses, adjust your timing to the ball."),
				TEXT("Wait for the ball"),
				TEXT("Improves timing adjustment against changing pitch speeds"),
				TEXT("3 sets x 15 tosses"), TEXT("3x15")),
		};

	case EWeaknessAxis::ContactAccuracy:
		return {
			MakeDrill(EWeaknessAxis::ContactAccuracy, TEXT("Tee precision contact"),
				TEXT("Hit the center of a stationary tee ball on the bat's sweet spot."),
				TEXT("Barrel center"),
				TEXT("Sharpens barrel control and sweet-spot accuracy"),
				TEXT("3 sets x 20 swings"), TEXT("3x20")),
			MakeDrill(EWeaknessAxis::ContactAccuracy, TEXT("Zone soft toss"),
				TEXT("Toss to split high/low and inside/outside zones, make clean contact in each."),
				TEXT("Aim per zone"),
				TEXT("Extends barrel accuracy to every zone, not just your comfortable one"),
				TEXT("4 zones x 10 swings"), TEXT("4x10")),
		};

	case EWeaknessAxis::BatSpeed:
		return {
			MakeDrill(EWeaknessAxis::BatSpeed, TEXT("Deadlift"),
				TEXT("Hip-hinge lift with a barbell or kettlebell, back flat, drive the floor away - "
					"go light and keep the form before adding load."),
				TEXT("Hinge at the hips, not the back"),
				TEXT("Builds hip extension power and core stability, the base every rotational swing pushes off"),
				TEXT("3 sets x 15 reps"), TEXT("3x15")),
			MakeDrill(EWeaknessAxis::BatSpeed, TEXT("Rotational power throw"),
				TEXT("Throw a medicine ball hard in the hitting direction using the lower body and core."),
				TEXT("Rotate from the legs"),
				TEXT("Develops the lower-body and core rotational power that turns into bat speed"),
				TEXT("3 sets x 10 throws"), TEXT("3x10")),
			MakeDrill(EWeaknessAxis::BatSpeed, TEXT("Resistance-band swing"),
				TEXT("Swing against band resistance while keeping your path, accelerate through."),
				TEXT("Accelerate at impact"),
				TEXT("Trains acceleration through the contact zone under load"),
				TEXT("3 sets x 12 swings"), TEXT("3x12")),
		};

	case EWeaknessAxis::Consistency:
		return {
			MakeDrill(EWeaknessAxis::Consistency, TEXT("Fixed-routine reps"),
				TEXT("Repeat the same setup-step-swing routine, check the feel each time."),
				TEXT("Repeat the same move"),
				TEXT("Turns the setup-step-swing sequence into a routine, cutting swing-to-swing variance"),
				TEXT("3 sets x 10 swings"), TEXT("3x10")),
			MakeDrill(EWeaknessAxis::Consistency, TEXT("Checkpoint swing"),
				TEXT("Slow swings checking posture checkpoints (grip, elbow, rotation) every time."),
				TEXT("Check your posture"),
				TEXT("Locks in posture checkpoints so the same swing repeats under pressure"),
				TEXT("3 sets x 10 slow swings"), TEXT("3x10")),
		};

	case EWeaknessAxis::HipShoulderSeparation:
		return {
			MakeDrill(EWeaknessAxis::HipShoulderSeparation, TEXT("Hip-lead separation drill"),
				TEXT("Keep the upper body back and open the hips first, slow swings to store the coil."),
				TEXT("Hips first, shoulders back"),
				TEXT("Increases hip-shoulder separation, so the torso stores and releases more elastic energy"),
				TEXT("3 sets x 10 slow swings"), TEXT("3x10")),
			MakeDrill(EWeaknessAxis::HipShoulderSeparation, TEXT("Band coil hold"),
				TEXT("Fix the upper body with a band and rotate only the lower body to feel the separation."),
				TEXT("Upper and lower apart"),
				TEXT("Builds the trunk strength to hold separation instead of spinning as one piece"),
				TEXT("3 sets x 8 reps"), TEXT("3x8")),
		};

	case EWeaknessAxis::HeadStability:
		return {
			MakeDrill(EWeaknessAxis::HeadStability, TEXT("Eyes-fixed tee batting"),
				TEXT("Stare at one point on the tee ball until impact and keep the head still."),
				TEXT("Never leave the ball"),
				TEXT("Keeps the head and eye line still, so the ball stays in focus through contact"),
				TEXT("3 sets x 15 swings"), TEXT("3x15")),
			MakeDrill(EWeaknessAxis::HeadStability, TEXT("Head-still mirror drill"),
				TEXT("Swing in front of a mirror or video and check the head does not sway in height or sideways."),
				TEXT("Keep head height"),
				TEXT("Removes the head sway that moves your contact point from swing to swing"),
				TEXT("3 sets x 10 swings"), TEXT("3x10")),
		};

	case EWeaknessAxis::KineticChain:
		return {
			MakeDrill(EWeaknessAxis::KineticChain, TEXT("Step-hip-hand sequence drill"),
				TEXT("Exaggerate the front-foot land -> hip turn -> hands order slowly to learn the sequence."),
				TEXT("Transfer bottom-up"),
				TEXT("Orders the kinetic chain (land - hips - hands) so power transfers bottom-up instead of leaking"),
				TEXT("3 sets x 12 swings"), TEXT("3x12")),
			MakeDrill(EWeaknessAxis::KineticChain, TEXT("Med-ball rotation throw"),
				TEXT("Wind up from the legs and throw, ingraining the hip -> torso -> arm order."),
				TEXT("Start from the legs"),
				TEXT("Ingrains the hip-torso-arm firing order under load"),
				TEXT("3 sets x 10 throws"), TEXT("3x10")),
		};

	case EWeaknessAxis::WeightShift:
		return {
			MakeDrill(EWeaknessAxis::WeightShift, TEXT("Back-to-front load drill"),
				TEXT("Load onto the back foot then shift to the front foot with the swing, isolate and repeat."),
				TEXT("Back to front"),
				TEXT("Trains a full back-to-front weight transfer, adding drive without extra arm effort"),
				TEXT("3 sets x 12 swings"), TEXT("3x12")),
			MakeDrill(EWeaknessAxis::WeightShift, TEXT("Step-through batting"),
				TEXT("Push weight forward with a light step while hitting, keep balance after moving."),
				TEXT("Plant the front foot"),
				TEXT("Teaches you to hold balance while the weight moves forward"),
				TEXT("3 sets x 10 swings"), TEXT("3x10")),
		};

	// ── Fielding (catch) fitness drills ──
	case EWeaknessAxis::CatchReaction:
		return {
			MakeDrill(EWeaknessAxis::CatchReaction, TEXT("Reaction catch drill"),
				TEXT("Catch balls thrown without warning or bounced off a wall immediately."),
				TEXT("Hands before you think"),
				TEXT("Shortens the reaction time from seeing the ball to getting the glove there"),
				TEXT("3 sets x 20 catches"), TEXT("3x20")),
			MakeDrill(EWeaknessAxis::CatchReaction, TEXT("Light reaction touch"),
				TEXT("Reach and touch a randomly firing cue (light / partner's hand)."),
				TEXT("React on the cue"),
				TEXT("Trains the cue-to-first-movement delay itself, isolated from catching technique"),
				TEXT("3 sets x 30 seconds"), TEXT("3x30s")),
		};

	case EWeaknessAxis::UpperBodyFlex:
		return {
			MakeDrill(EWeaknessAxis::UpperBodyFlex, TEXT("Thoracic-shoulder rotation stretch"),
				TEXT("Twist the upper body far side to side, holding each end position."),
				TEXT("Extend your reach range"),
				TEXT("Opens thoracic and shoulder rotation, widening the range you can still catch in"),
				TEXT("2 sets x 5 per side (10s hold)"), TEXT("2x5/side")),
			MakeDrill(EWeaknessAxis::UpperBodyFlex, TEXT("Band overhead reach"),
				TEXT("Hold a band and sweep the arms in a big overhead circle."),
				TEXT("Widen the range"),
				TEXT("Restores overhead shoulder range for balls above the head"),
				TEXT("2 sets x 12 reps"), TEXT("2x12")),
		};

	case EWeaknessAxis::FootSpeed:
		return {
			MakeDrill(EWeaknessAxis::FootSpeed, TEXT("Ladder quick steps"),
				TEXT("Step through a ladder or line raising your foot turnover."),
				TEXT("Short, fast steps"),
				TEXT("Raises foot turnover, so the first step toward the ball comes quicker"),
				TEXT("3 sets x 30 seconds"), TEXT("3x30s")),
			MakeDrill(EWeaknessAxis::FootSpeed, TEXT("Side shuffle"),
				TEXT("Shuffle side to side fast in a low stance."),
				TEXT("Low and fast"),
				TEXT("Builds the lateral quickness that fixes your fielding position before the ball arrives"),
				TEXT("4 sets x 10m"), TEXT("4x10m")),
		};

	// ── Fielding (throw) drills ──
	case EWeaknessAxis::ThrowAccuracy:
		return {
			MakeDrill(EWeaknessAxis::ThrowAccuracy, TEXT("Target line throws"),
				TEXT("Throw to a chest-high target from 20m, stepping straight at the target every time."),
				TEXT("Front foot points at the base"),
				TEXT("Aligns the stride and release line with the target, tightening throw accuracy"),
				TEXT("3 sets x 20 throws"), TEXT("3x20")),
			MakeDrill(EWeaknessAxis::ThrowAccuracy, TEXT("One-hop to the bag"),
				TEXT("From long range, aim a deliberate one-hop into the receiver's glove."),
				TEXT("Low miss, never high"),
				TEXT("Trains the low miss - a one-hop is still catchable, a high throw costs a base"),
				TEXT("3 sets x 15 throws"), TEXT("3x15")),
		};

	case EWeaknessAxis::ArmStrength:
		return {
			MakeDrill(EWeaknessAxis::ArmStrength, TEXT("Long toss ladder"),
				TEXT("Build distance 15m -> 25m -> 35m and come back down."),
				TEXT("Carry, do not aim"),
				TEXT("Builds throwing distance and arm endurance through a progressive range"),
				TEXT("6 steps x 5 throws"), TEXT("6x5")),
			MakeDrill(EWeaknessAxis::ArmStrength, TEXT("Med-ball crow hop throw"),
				TEXT("Crow-hop and throw a 2kg med ball with the whole body."),
				TEXT("Legs and trunk, not the arm"),
				TEXT("Adds whole-body throwing power from the legs and trunk instead of overloading the arm"),
				TEXT("3 sets x 8 throws"), TEXT("3x8")),
		};

	case EWeaknessAxis::TransferQuick:
		return {
			MakeDrill(EWeaknessAxis::TransferQuick, TEXT("Glove-to-hand transfer reps"),
				TEXT("Catch and move the ball to the throwing hand at the chest - no throw, transfer only."),
				TEXT("Bring it to the chest, not the ear"),
				TEXT("Cuts the glove-to-hand transfer time before the throw even starts"),
				TEXT("3 sets x 30 transfers"), TEXT("3x30")),
			MakeDrill(EWeaknessAxis::TransferQuick, TEXT("Quick-release footwork"),
				TEXT("Catch - right/left step - release as one motion at short range."),
				TEXT("Feet start with the catch"),
				TEXT("Merges catch, step and release into one motion for a faster release"),
				TEXT("3 sets x 20 reps"), TEXT("3x20")),
		};

	// ── Fielding (backup judgment) drills ──
	// ⚠️ 판단 훈련이다. 처방의 단위는 세트·횟수가 아니라 **상황 케이스 수**다 —
	//    여기에 근력·컨디셔닝 처방이 섞이면 Backup 도메인 프롬프트의 금지 규칙과 충돌한다.
	case EWeaknessAxis::BackupJudgment:
		return {
			MakeDrill(EWeaknessAxis::BackupJudgment, TEXT("Position backup walkthrough"),
				TEXT("For your position, walk the backup path for each batted-ball direction."),
				TEXT("Ball direction decides the base"),
				TEXT("Maps every batted-ball direction to your backup base until the answer is automatic"),
				TEXT("2 sets x 10 cases"), TEXT("2x10")),
			MakeDrill(EWeaknessAxis::BackupJudgment, TEXT("Runner-situation card review"),
				TEXT("Fix the runners (none / 1st / 2nd) and say your backup base out loud for each."),
				TEXT("Runners decide the throw"),
				TEXT("Links the runner situation to the throw destination that decides your job"),
				TEXT("3 sets x 15 cases"), TEXT("3x15")),
		};

	case EWeaknessAxis::DecisionSpeed:
		return {
			MakeDrill(EWeaknessAxis::DecisionSpeed, TEXT("Call-it-out reaction"),
				TEXT("A partner calls a situation, you name the backup base within 2 seconds."),
				TEXT("Decide before you move"),
				TEXT("Shortens the gap between the situation and your decision, before the feet move"),
				TEXT("2 sets x 20 calls"), TEXT("2x20")),
			MakeDrill(EWeaknessAxis::DecisionSpeed, TEXT("Pre-pitch routine"),
				TEXT("Before every pitch, say your job for each batted-ball direction."),
				TEXT("Decide it before the pitch"),
				TEXT("Moves the decision to before the pitch, so you react instead of think"),
				TEXT("1 full inning, every pitch"), TEXT("1 inning")),
		};

	// ── Fielding (backup route efficiency) drills — 판단 훈련. 컨디셔닝 처방 아님. ──
	case EWeaknessAxis::RouteEfficiency:
		return {
			MakeDrill(EWeaknessAxis::RouteEfficiency, TEXT("Straight-line walkthrough"),
				TEXT("Walk the exact backup path at half speed, noticing every place you drift off line."),
				TEXT("Pick the spot, then go straight"),
				TEXT("Removes drift from the route so you take the shortest path to the spot"),
				TEXT("2 sets x 8 routes"), TEXT("2x8")),
			MakeDrill(EWeaknessAxis::RouteEfficiency, TEXT("Call-and-commit"),
				TEXT("Say the backup base out loud before your first step, then don't change your mind mid-route."),
				TEXT("Decide once, commit fully"),
				TEXT("Stops mid-route direction changes by forcing the call before the first step"),
				TEXT("3 sets x 15 reps"), TEXT("3x15")),
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
				TEXT("Hold current feel"),
				TEXT("Maintains the fielding timing you already have instead of rebuilding it"),
				TEXT("2 sets x 10 game-speed reps"), TEXT("2x10")));
		}
		else
		{
			Out.Add(MakeDrill(EWeaknessAxis::Consistency, TEXT("Keep game feel"),
				TEXT("Keep taking live batting at game pace to maintain your current balance."),
				TEXT("Hold current feel"),
				TEXT("Maintains the swing balance and timing you already have"),
				TEXT("3 sets x 10 live swings"), TEXT("3x10")));
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
