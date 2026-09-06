#include "Core/Defense/Backup/BackupPlaybook.h"

namespace
{
	FBackupPlay MakePlay(FName Id, EBattedBallZone Zone, EBattedBallKind Kind, ERunnerState Runners,
		bool bHasFrom, EFieldPosition From, EBaseType To, bool bRelay,
		const TCHAR* Situation, const TCHAR* RunnerText, float Weight = 1.0f)
	{
		FBackupPlay P;
		P.PlayId = Id;
		P.BallZone = Zone;
		P.BallKind = Kind;
		P.Runners = Runners;
		P.bHasThrowFrom = bHasFrom;
		P.ThrowFrom = From;
		P.bHasThrowTo = true;
		P.ThrowTo = To;
		P.bRelay = bRelay;
		P.Situation = Situation;
		P.RunnerText = RunnerText;
		P.Weight = Weight;
		return P;
	}

	/**
	 * BackUpFielder 전용 플레이 — "누구 뒤를 받치는가"만 있고 송구 목적지 베이스가 없다.
	 * MakePlay 와 달리 ThrowTo 인자를 안 받는다 — 실수로 더미값을 넣었다가 그 값을 필터로
	 * 쓰는 다른 규칙에 잘못 걸리는 사고(겪은 적 있음)를 원천 차단한다.
	 */
	FBackupPlay MakeFielderBackupPlay(FName Id, EBattedBallZone Zone, EFieldPosition FieldingPos,
		ERunnerState Runners, const TCHAR* Situation, const TCHAR* RunnerText)
	{
		FBackupPlay P;
		P.PlayId = Id;
		P.BallZone = Zone;
		P.BallKind = EBattedBallKind::FlyBall;
		P.Runners = Runners;
		P.bHasThrowFrom = true;
		P.ThrowFrom = FieldingPos;
		P.bHasThrowTo = false; // 이 플레이엔 "송구 목적지 베이스" 개념이 없다.
		P.Situation = Situation;
		P.RunnerText = RunnerText;
		return P;
	}

	FBackupAssignmentRule MakeBackUpBase(EFieldPosition Pos, const TCHAR* Explain, bool bKey = false)
	{
		FBackupAssignmentRule R;
		R.Position = Pos;
		R.Role = EBackupRole::BackUpBase;
		R.Explain = Explain;
		R.bKeyScenario = bKey;
		return R;
	}

	FBackupAssignmentRule MakeCoverBase(EFieldPosition Pos, EBaseType Anchor, const TCHAR* Explain, bool bKey = false)
	{
		FBackupAssignmentRule R;
		R.Position = Pos;
		R.Role = EBackupRole::CoverBase;
		R.AnchorBase = Anchor;
		R.Explain = Explain;
		R.bKeyScenario = bKey;
		return R;
	}

	FBackupAssignmentRule MakeCutoff(EFieldPosition Pos, const TCHAR* Explain, bool bKey = false)
	{
		FBackupAssignmentRule R;
		R.Position = Pos;
		R.Role = EBackupRole::CutoffRelay;
		R.Explain = Explain;
		R.bKeyScenario = bKey;
		return R;
	}

	FBackupAssignmentRule MakeBackUpFielder(EFieldPosition Pos, EFieldPosition Anchor, const TCHAR* Explain, bool bKey = false)
	{
		FBackupAssignmentRule R;
		R.Position = Pos;
		R.Role = EBackupRole::BackUpFielder;
		R.AnchorFielder = Anchor;
		R.Explain = Explain;
		R.bKeyScenario = bKey;
		return R;
	}

	/** 필터를 실은 채로 규칙을 복사한다 — Make* 헬퍼가 만든 기본 규칙에 조건을 덧씌운다. */
	FBackupAssignmentRule WithThrowFrom(FBackupAssignmentRule R, EFieldPosition From)
	{
		R.bFilterThrowFrom = true;
		R.ThrowFrom = From;
		return R;
	}
	FBackupAssignmentRule WithThrowTo(FBackupAssignmentRule R, EBaseType To)
	{
		R.bFilterThrowTo = true;
		R.ThrowTo = To;
		return R;
	}
	FBackupAssignmentRule WithBallZone(FBackupAssignmentRule R, EBattedBallZone Zone)
	{
		R.bFilterBallZone = true;
		R.BallZone = Zone;
		return R;
	}
	FBackupAssignmentRule WithNoThrowFrom(FBackupAssignmentRule R)
	{
		R.bRequireNoThrowFrom = true;
		return R;
	}
}

TArray<FBackupPlay> UBackupPlaybook::BuildPlayTable()
{
	TArray<FBackupPlay> P;

	// ── 그라운드볼 → 1루 (내야 방향별로 나눠 2루수/우익수 백업 분담을 살린다) ──
	// 스펙: "2루수: 우측 방향 아닌 송구의 1루 백업" / "우익수: 1루 뒤 백업".
	P.Add(MakePlay(TEXT("GB_SS_1B"), EBattedBallZone::InfieldLeft, EBattedBallKind::Grounder, ERunnerState::None,
		true, EFieldPosition::Short, EBaseType::First, false,
		TEXT("Routine grounder to shortstop"), TEXT("Bases empty")));
	P.Add(MakePlay(TEXT("GB_3B_1B"), EBattedBallZone::InfieldLeft, EBattedBallKind::Grounder, ERunnerState::None,
		true, EFieldPosition::Third, EBaseType::First, false,
		TEXT("Grounder to third base"), TEXT("Bases empty")));
	P.Add(MakePlay(TEXT("GB_2B_1B"), EBattedBallZone::InfieldRight, EBattedBallKind::Grounder, ERunnerState::None,
		true, EFieldPosition::Second, EBaseType::First, false,
		TEXT("Grounder to second base"), TEXT("Bases empty")));
	P.Add(MakePlay(TEXT("GB_SS_1B_R2"), EBattedBallZone::InfieldLeft, EBattedBallKind::Grounder, ERunnerState::Second,
		true, EFieldPosition::Short, EBaseType::First, false,
		TEXT("Grounder to shortstop"), TEXT("Runner on 2nd - catcher stays home")));
	P.Add(MakePlay(TEXT("GB_SS_1B_LINER"), EBattedBallZone::InfieldLeft, EBattedBallKind::LineDrive, ERunnerState::First,
		true, EFieldPosition::Short, EBaseType::First, false,
		TEXT("Sharp grounder to shortstop"), TEXT("Runner on 1st")));

	// ── 번트 → 1루수/3루수 이탈 시 커버 (핵심 시나리오: 1루수 이탈 시 2루수 1루 커버) ──
	P.Add(MakePlay(TEXT("BUNT_1B"), EBattedBallZone::BuntFirst, EBattedBallKind::Bunt, ERunnerState::First,
		true, EFieldPosition::First, EBaseType::First, false,
		TEXT("Bunt down the first-base line, 1B charges in"), TEXT("Runner on 1st")));
	P.Add(MakePlay(TEXT("SLOW_ROLLER_1B"), EBattedBallZone::InfieldRight, EBattedBallKind::Grounder, ERunnerState::Second,
		true, EFieldPosition::First, EBaseType::First, false,
		TEXT("Slow roller wide of first, 1B leaves the bag to field it"), TEXT("Runner on 2nd")));
	P.Add(MakePlay(TEXT("BUNT_3B"), EBattedBallZone::BuntThird, EBattedBallKind::Bunt, ERunnerState::First,
		true, EFieldPosition::Third, EBaseType::First, false,
		TEXT("Bunt down the third-base line, 3B charges in"), TEXT("Runner on 1st")));

	// ── 도루 / 병살 - 2루 커버 (스펙: 유격수 2루 커버·태그, 2루수 병살 커버) ──
	P.Add(MakePlay(TEXT("STEAL_2B"), EBattedBallZone::Steal, EBattedBallKind::Steal, ERunnerState::First,
		false, EFieldPosition::Short /*unused*/, EBaseType::Second, false,
		TEXT("Runner breaks for second, catcher throws"), TEXT("Runner on 1st")));
	P.Add(MakePlay(TEXT("DP_2B_FIELDS"), EBattedBallZone::InfieldRight, EBattedBallKind::Grounder, ERunnerState::First,
		true, EFieldPosition::Second, EBaseType::Second, false,
		TEXT("Grounder to second base, ball for the double play"), TEXT("Runner on 1st")));
	P.Add(MakePlay(TEXT("DP_SS_FIELDS"), EBattedBallZone::InfieldLeft, EBattedBallKind::Grounder, ERunnerState::First,
		true, EFieldPosition::Short, EBaseType::Second, false,
		TEXT("Grounder to shortstop, ball for the double play"), TEXT("Runner on 1st")));

	// ── 도루 - 3루 커버 (스펙: 3루수 3루 커버, 좌익수 3루 뒤 백업) ──
	P.Add(MakePlay(TEXT("STEAL_3B"), EBattedBallZone::Steal, EBattedBallKind::Steal, ERunnerState::Second,
		false, EFieldPosition::Short /*unused*/, EBaseType::Third, false,
		TEXT("Runner breaks for third, catcher throws"), TEXT("Runner on 2nd")));

	// ── 외야 안타 → 3루 송구 (좌익수 3루 뒤 백업 + 3루수 3루 커버) ──
	P.Add(MakePlay(TEXT("SINGLE_CF_3B"), EBattedBallZone::Center, EBattedBallKind::LineDrive, ERunnerState::First,
		true, EFieldPosition::Center, EBaseType::Third, false,
		TEXT("Single up the middle, throw going to third"), TEXT("Runner on 1st taking third")));
	P.Add(MakePlay(TEXT("SINGLE_RF_TO_3B"), EBattedBallZone::RightCenter, EBattedBallKind::LineDrive, ERunnerState::First,
		true, EFieldPosition::Right, EBaseType::Third, false,
		TEXT("Single to right-center, throw going to third"), TEXT("Runner on 1st taking third")));
	P.Add(MakePlay(TEXT("TRIPLE_RC_3B"), EBattedBallZone::RightCenter, EBattedBallKind::FlyBall, ERunnerState::Second,
		true, EFieldPosition::Right, EBaseType::Third, true,
		TEXT("Deep drive to right-center, relay going to third"), TEXT("Runner on 2nd")));
	P.Add(MakePlay(TEXT("GAP_CF_3B_DEEP"), EBattedBallZone::Center, EBattedBallKind::FlyBall, ERunnerState::First,
		true, EFieldPosition::Center, EBaseType::Third, true,
		TEXT("Deep drive off the wall in center, batter-runner digging for third"), TEXT("Runner on 1st")));
	// 주: "좌익수 자신이 처리해 3루로 던지는" 대칭 플레이는 일부러 안 만든다 — 그러면
	// "좌익수가 3루 뒤를 백업한다"는 블랭킷 규칙(아래 R11)이 좌익수 본인의 송구까지
	// 백업 대상으로 착각하는 자기충돌이 생긴다 (Resolve 필터로는 "ThrowFrom ≠ 나" 를
	// 표현할 수 없어, 아예 그런 플레이를 만들지 않는 쪽을 택했다).

	// ── 외야 안타/2루타 → 홈·2루 송구 중계 (1루수: 외야→내야 중계 커버 / 유격수: 좌·중견 중계) ──
	P.Add(MakePlay(TEXT("RELAY_HOME_RF"), EBattedBallZone::RightField, EBattedBallKind::LineDrive, ERunnerState::Second,
		true, EFieldPosition::Right, EBaseType::Home, true,
		TEXT("Single to right field, relay throw going home"), TEXT("Runner on 2nd scoring")));
	P.Add(MakePlay(TEXT("RELAY_2B_RF"), EBattedBallZone::RightField, EBattedBallKind::LineDrive, ERunnerState::None,
		true, EFieldPosition::Right, EBaseType::Second, true,
		TEXT("Base hit to right field, batter stretches it into a double"), TEXT("Bases empty")));
	P.Add(MakePlay(TEXT("DOUBLE_RF_HOME"), EBattedBallZone::RightLine, EBattedBallKind::LineDrive, ERunnerState::First,
		true, EFieldPosition::Right, EBaseType::Home, true,
		TEXT("Double into the right-field corner, relay throw going home"), TEXT("Runner on 1st scoring")));
	P.Add(MakePlay(TEXT("FLY_LC_SS_CUTOFF"), EBattedBallZone::LeftCenter, EBattedBallKind::FlyBall, ERunnerState::First,
		true, EFieldPosition::Center, EBaseType::Third, true,
		TEXT("Ball drops in the left-center gap, relay throw going to third"), TEXT("Runner on 1st")));
	P.Add(MakePlay(TEXT("DOUBLE_LF_HOME"), EBattedBallZone::LeftLine, EBattedBallKind::LineDrive, ERunnerState::First,
		true, EFieldPosition::Left, EBaseType::Home, true,
		TEXT("Double into the left-field corner, relay throw going home"), TEXT("Runner on 1st scoring")));

	// ── 외야 사령탑(중견수) — 코너 외야수 뒤 백업 (핵심 시나리오: 중견수 광범위 백업) ──
	// 송구 목적지 베이스가 없는 플레이라 MakeFielderBackupPlay 를 쓴다 (ThrowTo 더미값을
	// 안 만들어 다른 규칙의 ThrowTo 필터에 잘못 걸리는 사고를 원천 차단).
	P.Add(MakeFielderBackupPlay(TEXT("LEFT_GAP_CF_BACKUP"), EBattedBallZone::LeftField, EFieldPosition::Left,
		ERunnerState::First, TEXT("Sinking liner into shallow left, left fielder charges to make the play"), TEXT("Runner on 1st")));
	P.Add(MakeFielderBackupPlay(TEXT("RIGHT_GAP_CF_BACKUP"), EBattedBallZone::RightField, EFieldPosition::Right,
		ERunnerState::First, TEXT("Ball skips past the right fielder toward the corner"), TEXT("Runner on 1st")));
	P.Add(MakeFielderBackupPlay(TEXT("GAP_LC_CF_BACKUP_V2"), EBattedBallZone::LeftCenter, EFieldPosition::Left,
		ERunnerState::First, TEXT("Diving read in the left-center gap"), TEXT("Runner on 1st")));
	P.Add(MakeFielderBackupPlay(TEXT("GAP_RC_CF_BACKUP_V2"), EBattedBallZone::RightCenter, EFieldPosition::Right,
		ERunnerState::First, TEXT("Diving read in the right-center gap"), TEXT("Runner on 1st")));

	// ── 홀드 필러 (자기가 직접 처리하는 공 - 특별한 백업 콜이 없다) ──
	P.Add(MakePlay(TEXT("PITCHER_COMEBACKER"), EBattedBallZone::InfieldMiddle, EBattedBallKind::LineDrive, ERunnerState::None,
		false, EFieldPosition::Short /*unused*/, EBaseType::First, false,
		TEXT("Comebacker up the middle, pitcher fields and throws to first"), TEXT("Bases empty")));

	return P;
}

TArray<FBackupAssignmentRule> UBackupPlaybook::BuildRuleTable()
{
	TArray<FBackupAssignmentRule> R;

	// ── 1루 백업 (스펙: 2루수=비우측 송구, 우익수=일반) ──
	R.Add(WithThrowFrom(WithThrowTo(
		MakeBackUpBase(EFieldPosition::Second, TEXT("The throw to first is coming from the left side - get over and back it up.")),
		EBaseType::First), EFieldPosition::Short));
	R.Add(WithThrowFrom(WithThrowTo(
		MakeBackUpBase(EFieldPosition::Second, TEXT("The throw to first is coming from third - get over and back it up.")),
		EBaseType::First), EFieldPosition::Third));
	R.Add(WithThrowTo(
		MakeBackUpBase(EFieldPosition::Right, TEXT("The right fielder backs up every throw to first base.")),
		EBaseType::First));

	// ── 1루 커버 (1루수 이탈 시 2루수가 대신 지킨다 — 핵심 시나리오) ──
	R.Add(WithBallZone(
		MakeCoverBase(EFieldPosition::Second, EBaseType::First,
			TEXT("Once the first baseman leaves the bag, the second baseman covers first."), true),
		EBattedBallZone::BuntFirst));
	R.Add(WithThrowFrom(WithThrowTo(
		MakeCoverBase(EFieldPosition::Second, EBaseType::First,
			TEXT("The first baseman is pulled off the bag - covering first is the second baseman's job."), true),
		EBaseType::First), EFieldPosition::First));

	// ── 3루 커버 (번트 처리 시 유격수가 대신 지킨다) ──
	R.Add(WithBallZone(
		MakeCoverBase(EFieldPosition::Short, EBaseType::Third,
			TEXT("When the third baseman fields the bunt, the shortstop covers third.")),
		EBattedBallZone::BuntThird));

	// ── 2루 커버/태그 (도루·병살 — 스펙: 유격수 커버/태그, 미들 인필더 중 안 던진 쪽이 커버) ──
	// ⚠️ Short 의 두 규칙을 "ThrowTo=Second 블랭킷" 하나로 두면 안 된다 — DP_SS_FIELDS
	//    (유격수 본인이 처리해 2루로 던지는 병살)에도 걸려 "자기가 던진 걸 자기가 커버"라는
	//    말이 안 되는 자기충돌이 생긴다. 그래서 "포수가 던짐(도루)"과 "2루수가 처리함(병살)"
	//    두 경우만 명시적으로 걸어 유격수 자신이 던지는 경우를 자동으로 제외한다.
	R.Add(WithNoThrowFrom(WithThrowTo(
		MakeCoverBase(EFieldPosition::Short, EBaseType::Second,
			TEXT("On a throw to second, the shortstop takes the bag for the tag.")),
		EBaseType::Second)));
	R.Add(WithThrowFrom(WithThrowTo(
		MakeCoverBase(EFieldPosition::Short, EBaseType::Second,
			TEXT("The second baseman fielded it, so the shortstop covers the bag for the pivot.")),
		EBaseType::Second), EFieldPosition::Second));
	R.Add(WithThrowFrom(WithThrowTo(
		MakeCoverBase(EFieldPosition::Second, EBaseType::Second,
			TEXT("The shortstop fielded it, so the second baseman covers the bag for the pivot.")),
		EBaseType::Second), EFieldPosition::Short));
	// 2루수 자신이 처리한 병살(DP_2B_FIELDS)·유격수 자신이 처리한 병살(DP_SS_FIELDS)은
	// 위 규칙들이 "본인이 던지는 쪽"을 걸러내므로 그 위치만 자동으로 Hold 로 떨어진다.

	R.Add(WithThrowTo(
		MakeBackUpBase(EFieldPosition::Center, TEXT("The center fielder backs up second base on steals and relay throws."), true),
		EBaseType::Second));

	// ── 3루 커버/백업 (스펙: 3루수 3루 커버, 좌익수 3루 뒤 백업) ──
	R.Add(WithThrowTo(
		MakeCoverBase(EFieldPosition::Third, EBaseType::Third,
			TEXT("The third baseman takes the bag for the play.")),
		EBaseType::Third));
	R.Add(WithThrowTo(
		MakeBackUpBase(EFieldPosition::Left, TEXT("The left fielder backs up throws to third.")),
		EBaseType::Third));

	// ── 중계(cutoff) — 1루수: 외야→내야 전반 / 유격수: 좌·중견 방면 ──
	R.Add(WithThrowTo(WithThrowFrom(
		MakeCutoff(EFieldPosition::First, TEXT("On a throw from the right side to home, line up as the cutoff man.")),
		EFieldPosition::Right), EBaseType::Home));
	R.Add(WithThrowTo(WithThrowFrom(
		MakeCutoff(EFieldPosition::First, TEXT("On a throw from the right side to second, line up as the cutoff man.")),
		EFieldPosition::Right), EBaseType::Second));
	R.Add(WithThrowTo(WithThrowFrom(
		MakeCutoff(EFieldPosition::First, TEXT("On a throw from the right side to third, trail as the safety cutoff.")),
		EFieldPosition::Right), EBaseType::Third));
	R.Add(WithThrowTo(WithThrowFrom(
		MakeCutoff(EFieldPosition::Short, TEXT("On a throw from center or left to third, the shortstop is the cutoff man.")),
		EFieldPosition::Center), EBaseType::Third));
	R.Add(WithThrowTo(WithThrowFrom(
		MakeCutoff(EFieldPosition::Short, TEXT("On a throw from left to home, the shortstop is the cutoff man.")),
		EFieldPosition::Left), EBaseType::Home));

	// ── 외야 사령탑 — 중견수가 코너 외야수 뒤를 받친다 (핵심 시나리오) ──
	R.Add(WithThrowFrom(
		MakeBackUpFielder(EFieldPosition::Center, EFieldPosition::Left,
			TEXT("The center fielder is the outfield captain - back up the left fielder too."), true),
		EFieldPosition::Left));
	R.Add(WithThrowFrom(
		MakeBackUpFielder(EFieldPosition::Center, EFieldPosition::Right,
			TEXT("The center fielder is the outfield captain - back up the right fielder too."), true),
		EFieldPosition::Right));

	return R;
}

FBackupAssignmentRule UBackupPlaybook::Resolve(const TArray<FBackupAssignmentRule>& RuleTable,
	const FBackupPlay& Play, EFieldPosition Position)
{
	const FBackupAssignmentRule* Best = nullptr;
	int32 BestSpecificity = -1;

	for (const FBackupAssignmentRule& Rule : RuleTable)
	{
		if (Rule.Position != Position) { continue; }

		if (Rule.bFilterThrowTo && (!Play.bHasThrowTo || Rule.ThrowTo != Play.ThrowTo)) { continue; }
		if (Rule.bFilterThrowFrom && (!Play.bHasThrowFrom || Rule.ThrowFrom != Play.ThrowFrom)) { continue; }
		if (Rule.bRequireNoThrowFrom && Play.bHasThrowFrom) { continue; }
		if (Rule.bFilterBallZone && Rule.BallZone != Play.BallZone) { continue; }

		const int32 Specificity =
			(Rule.bFilterThrowTo ? 1 : 0) + (Rule.bFilterThrowFrom ? 1 : 0) +
			(Rule.bFilterBallZone ? 1 : 0) + (Rule.bRequireNoThrowFrom ? 1 : 0);
		if (Specificity > BestSpecificity)
		{
			BestSpecificity = Specificity;
			Best = &Rule;
		}
	}

	if (Best)
	{
		return *Best;
	}

	// 매칭되는 규칙이 없다 = 이 공은 당신 담당이 아니다. 항상 유효한 기본값으로 떨어진다.
	FBackupAssignmentRule Fallback;
	Fallback.Position = Position;
	Fallback.Role = EBackupRole::Hold;
	Fallback.Explain = TEXT("No special backup job here - hold your position and stay ready.");
	return Fallback;
}

bool UBackupPlaybook::IsEligibleForPosition(const TArray<FBackupAssignmentRule>& RuleTable,
	const FBackupPlay& Play, EFieldPosition Position)
{
	return Resolve(RuleTable, Play, Position).Role != EBackupRole::Hold;
}

FBackupTrial UBackupPlaybook::BuildTrial(const FBaseballField& Field, const TArray<FBackupAssignmentRule>& RuleTable,
	EFieldPosition Position, const FBackupPlay& Play)
{
	FBackupTrial Trial;
	Trial.Play = Play;
	Trial.Position = Position;

	const FBackupAssignmentRule Matched = Resolve(RuleTable, Play, Position);
	Trial.CorrectZone = Field.ResolveZone(Matched, Play);
	Trial.bIsHoldTrial = (Matched.Role == EBackupRole::Hold);

	// ── 방향 판단(1단계) 후보 — "이 포지션이 플레이북 전체에서 배정받을 수 있는 존들" ──
	// 이 플레이가 다른 포지션에게 주는 존은 넣지 않는다: 넣으면 외야에서 서로 각도差가
	// 너무 좁아져 트랙패드 노이즈로 오판정이 난다 (설계 노트 — 실좌표 검산으로 발견한 결함).
	struct FDistinctAnchor
	{
		EBackupRole Role;
		EBaseType AnchorBase;
		EFieldPosition AnchorFielder;
		FString Explain;
		bool bKeyScenario;

		bool operator==(const FDistinctAnchor& O) const
		{
			return Role == O.Role && AnchorBase == O.AnchorBase && AnchorFielder == O.AnchorFielder;
		}
	};

	TArray<FDistinctAnchor> Distinct;
	for (const FBackupAssignmentRule& Rule : RuleTable)
	{
		if (Rule.Position != Position || Rule.Role == EBackupRole::Hold) { continue; }

		FDistinctAnchor A{ Rule.Role, Rule.AnchorBase, Rule.AnchorFielder, Rule.Explain, Rule.bKeyScenario };
		if (!Distinct.Contains(A)) { Distinct.Add(A); }
	}

	Trial.CandidateZones.Reserve(Distinct.Num() + 1);
	for (const FDistinctAnchor& A : Distinct)
	{
		FBackupAssignmentRule R;
		R.Position = Position;
		R.Role = A.Role;
		R.AnchorBase = A.AnchorBase;
		R.AnchorFielder = A.AnchorFielder;
		R.Explain = A.Explain;
		R.bKeyScenario = A.bKeyScenario;

		// 이번 플레이의 실제 지리(누가 던지는지 등)로 해석한다 — BackUpBase/CutoffRelay 는
		// Play 에서 앵커를 가져오므로, 같은 역할이라도 매번 방향이 이번 상황에 맞게 나온다.
		const FBackupZone Zone = Field.ResolveZone(R, Play);
		Trial.CandidateZones.Add(Zone);
		if (A == FDistinctAnchor{ Matched.Role, Matched.AnchorBase, Matched.AnchorFielder, FString(), false })
		{
			Trial.CorrectCandidateIndex = Trial.CandidateZones.Num() - 1;
		}
	}

	// Hold 는 방향 후보에 늘 포함한다 — "이 공은 내 담당이 아니다"도 유효한 판단이다.
	FBackupAssignmentRule HoldRule;
	HoldRule.Position = Position;
	HoldRule.Role = EBackupRole::Hold;
	const FBackupZone HoldZone = Field.ResolveZone(HoldRule, Play);
	Trial.CandidateZones.Add(HoldZone);
	if (Trial.bIsHoldTrial)
	{
		Trial.CorrectCandidateIndex = Trial.CandidateZones.Num() - 1;
	}

	// ── 제한 시간 — 큐 지점(내 수비 위치)에서 정답 존까지 거리로 파생 ──
	const FVector MySpot = Field.GetFieldingSpot(Position);
	const float PathDistance = Trial.CorrectZone.DistanceTo(MySpot) > KINDA_SMALL_NUMBER
		? FVector::Dist2D(MySpot, Trial.CorrectZone.RepresentativePoint())
		: 0.0f;
	Trial.TimeLimitSec = Field.DeriveTimeLimit(PathDistance);

	return Trial;
}

bool UBackupPlaybook::ValidateCoverage(const TArray<FBackupPlay>& PlayTable, const TArray<FBackupAssignmentRule>& RuleTable,
	int32 MinNonHoldPerPosition, TArray<FString>& OutErrors)
{
	bool bOk = true;
	for (EFieldPosition Pos : AllPositions())
	{
		int32 Count = 0;
		for (const FBackupPlay& Play : PlayTable)
		{
			if (IsEligibleForPosition(RuleTable, Play, Pos)) { ++Count; }
		}
		if (Count < MinNonHoldPerPosition)
		{
			bOk = false;
			OutErrors.Add(FString::Printf(TEXT("%s has only %d non-Hold play(s), need >= %d"),
				*PositionName(Pos), Count, MinNonHoldPerPosition));
		}
	}
	return bOk;
}

const TArray<EFieldPosition>& UBackupPlaybook::AllPositions()
{
	static const TArray<EFieldPosition> All = {
		EFieldPosition::First, EFieldPosition::Second, EFieldPosition::Short, EFieldPosition::Third,
		EFieldPosition::Left, EFieldPosition::Center, EFieldPosition::Right
	};
	return All;
}

FString UBackupPlaybook::PositionName(EFieldPosition Pos)
{
	switch (Pos)
	{
	case EFieldPosition::First:  return TEXT("1B");
	case EFieldPosition::Second: return TEXT("2B");
	case EFieldPosition::Short:  return TEXT("SS");
	case EFieldPosition::Third:  return TEXT("3B");
	case EFieldPosition::Left:   return TEXT("LF");
	case EFieldPosition::Center: return TEXT("CF");
	case EFieldPosition::Right:  return TEXT("RF");
	default:                     return TEXT("?");
	}
}
