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
	/** "내가 던지는 플레이엔 걸리지 않는다" 안전장치 — 블랭킷 규칙에 붙인다. */
	FBackupAssignmentRule WithoutSelfThrow(FBackupAssignmentRule R)
	{
		R.bExcludeSelfThrow = true;
		return R;
	}
	/** "번트엔 걸리지 않는다" 안전장치 — 내야 기본 로테이션 규칙에 붙인다. */
	FBackupAssignmentRule WithoutBunt(FBackupAssignmentRule R)
	{
		R.bExcludeBunt = true;
		return R;
	}
	FBackupAssignmentRule WithBallKind(FBackupAssignmentRule R, EBattedBallKind Kind)
	{
		R.bFilterBallKind = true;
		R.BallKind = Kind;
		return R;
	}
	/** BackUpBase 가 송구 목적지 대신 지정한 베이스를 받치게 한다. */
	FBackupAssignmentRule BackingUpBase(FBackupAssignmentRule R, EBaseType Anchor)
	{
		R.bAnchorBaseOverride = true;
		R.AnchorBase = Anchor;
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
	// 주: "좌익수 자신이 처리해 3루로 던지는" 대칭 플레이는 여기 없다. 예전엔 그게
	// **필수 회피책**이었다 — "좌익수가 3루 뒤를 백업한다"는 블랭킷 규칙이 좌익수 본인의
	// 송구까지 백업 대상으로 착각하는데, Resolve 필터로 "ThrowFrom ≠ 나"를 표현할 수 없었다.
	// 지금은 그 블랭킷 규칙들에 bExcludeSelfThrow 가 붙어 규칙 쪽에서 직접 막으므로,
	// 필요하면 그런 플레이를 추가해도 안전하다 (아직 훈련 가치가 낮아 안 넣었을 뿐이다).

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
	// 위 좌중간 플레이의 좌우 대칭 — 우익수에게도 "내 갭을 중견수가 잡는" 상황이 있어야
	// 갭 백업 규칙(BuildRuleTable 맨 아래)이 실제로 뽑힌다. 없으면 그 규칙은 죽은 데이터가 된다.
	P.Add(MakePlay(TEXT("GAP_RC_CF_TO_3B"), EBattedBallZone::RightCenter, EBattedBallKind::FlyBall, ERunnerState::First,
		true, EFieldPosition::Center, EBaseType::Third, true,
		TEXT("Ball drops in the right-center gap, relay throw going to third"), TEXT("Runner on 1st")));
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

	// ══ 내야 기본 로테이션 — 땅볼이 나오면 매번 도는 임무 ══════════════════════════
	//
	// 예전엔 이 블록이 통째로 없었다. 번트·병살·도루·중계 같은 **특수 상황부터** 저작하는
	// 바람에, 정작 야구에서 가장 자주 나오는 "평범한 내야 땅볼"에서는 7개 포지션 중 5개가
	// Hold(= 내 일 아님)로 떨어졌다. 실제로는 전원이 각자 갈 곳이 있다.
	//
	// ⚠️ 전부 WithoutBunt 다. 번트는 1루수가 대시해 들어와 커버가 통째로 달라진다
	//    (아래 번트 블록이 따로 담당한다).
	// ⚠️ 전부 WithoutSelfThrow 다 — 공을 처리하는 본인은 이 로테이션에서 빠진다.

	// 1루수: 송구를 받으러 베이스로. (예전엔 Hold 라 수비 위치에 그냥 서 있었다.)
	R.Add(WithoutBunt(WithoutSelfThrow(WithThrowTo(
		MakeCoverBase(EFieldPosition::First, EBaseType::First,
			TEXT("Any throw to first is yours - get to the bag and give a target.")),
		EBaseType::First))));

	// 미들 인필더: 공을 처리하지 않은 쪽이 2루를 지킨다 (송구가 빠졌을 때 진루 저지).
	// ⚠️ 여기가 "유격수 땅볼인데 2루수가 1루를 백업한다"였던 자리다. 1루 송구 백업은
	//    우익수 담당이고(아래), 2루수의 실제 임무는 2루 커버다.
	R.Add(WithoutBunt(WithThrowFrom(WithThrowTo(
		MakeCoverBase(EFieldPosition::Second, EBaseType::Second,
			TEXT("The shortstop is making the play - you have second base behind it.")),
		EBaseType::First), EFieldPosition::Short)));
	R.Add(WithoutBunt(WithThrowFrom(WithThrowTo(
		MakeCoverBase(EFieldPosition::Second, EBaseType::Second,
			TEXT("Third is making the play - you have second base behind it.")),
		EBaseType::First), EFieldPosition::Third)));
	// ⚠️ 유격수 쪽은 WithoutBunt 를 걸지 않는다 — 번트도 주자가 2루로 가므로 커버가 필요하고,
	//    ThrowFrom 필터가 이미 "1루 쪽 번트"만 남기기 때문에 3루 쪽 번트와 섞이지 않는다.
	R.Add(WithThrowFrom(WithThrowTo(
		MakeCoverBase(EFieldPosition::Short, EBaseType::Second,
			TEXT("The second baseman is making the play - you have second base behind it.")),
		EBaseType::First), EFieldPosition::Second));
	R.Add(WithThrowFrom(WithThrowTo(
		MakeCoverBase(EFieldPosition::Short, EBaseType::Second,
			TEXT("The first baseman is making the play - you have second base behind it.")),
		EBaseType::First), EFieldPosition::First));

	// 3루수: 3루를 비우지 않는다.
	R.Add(WithoutSelfThrow(WithThrowTo(
		MakeCoverBase(EFieldPosition::Third, EBaseType::Third,
			TEXT("Stay home - third base is yours even when the play is going to first.")),
		EBaseType::First)));

	// 중견수: 2루 뒤를 받친다 (송구가 1루로 가도, 빠지면 주자가 노리는 건 2루다).
	// 송구 목적지(1루)가 아닌 베이스를 받치는 케이스라 앵커를 규칙에서 지정한다.
	R.Add(WithThrowTo(BackingUpBase(
		MakeBackUpBase(EFieldPosition::Center, TEXT("Throw is going to first - drift in behind second in case it gets away.")),
		EBaseType::Second), EBaseType::First));

	// 우익수: 1루 뒤 백업 (번트 포함 — 1루 송구는 무조건 받친다).
	R.Add(WithoutSelfThrow(WithThrowTo(
		MakeBackUpBase(EFieldPosition::Right, TEXT("The right fielder backs up every throw to first base.")),
		EBaseType::First)));

	// ── 1루 커버 (1루수 이탈 시 2루수가 대신 지킨다 — 핵심 시나리오) ──
	// 번트는 어느 쪽이든 1루수가 대시해 들어온다 → 1루 베이스는 2루수 몫이다.
	// (예전엔 BallZone=BuntFirst 만 걸려 있어 3루 쪽 번트에서 1루가 비었다.)
	R.Add(WithBallKind(
		MakeCoverBase(EFieldPosition::Second, EBaseType::First,
			TEXT("Once the first baseman charges the bunt, the second baseman covers first."), true),
		EBattedBallKind::Bunt));
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

	// 유격수가 커버로 들어가면 2루수는 송구 뒤를 받친다 (빠지면 주자가 3루까지 간다).
	R.Add(WithNoThrowFrom(WithThrowTo(
		MakeBackUpBase(EFieldPosition::Second, TEXT("The shortstop takes the tag - you get behind the bag for the overthrow.")),
		EBaseType::Second)));

	R.Add(WithoutSelfThrow(WithThrowTo(
		MakeBackUpBase(EFieldPosition::Center, TEXT("The center fielder backs up second base on steals and relay throws."), true),
		EBaseType::Second)));

	// ── 3루 커버/백업 (스펙: 3루수 3루 커버, 좌익수 3루 뒤 백업) ──
	R.Add(WithoutSelfThrow(WithThrowTo(
		MakeCoverBase(EFieldPosition::Third, EBaseType::Third,
			TEXT("The third baseman takes the bag for the play.")),
		EBaseType::Third)));
	R.Add(WithoutSelfThrow(WithThrowTo(
		MakeBackUpBase(EFieldPosition::Left, TEXT("The left fielder backs up throws to third.")),
		EBaseType::Third)));

	// ══ 중계(cutoff) — **사이드별 관례** ═══════════════════════════════════════════
	//
	//   홈 송구 : 좌익수 발신 → 3루수 / 우익·중견 발신 → 1루수
	//   3루 송구: 전부 유격수
	//   2루 송구: 타구가 간 쪽 미들 인필더가 중계, 반대쪽이 베이스 커버
	//
	// ⚠️ 컷오프 배정은 팀·리그마다 다르게 가르치는 영역이다. 여기 있는 건 "사이드별"
	//    관례이며, 바꾸려면 이 블록만 통째로 갈아끼우면 된다 (다른 규칙과 얽혀 있지 않다).

	// ── 홈 송구 ──
	R.Add(WithThrowTo(WithThrowFrom(
		MakeCutoff(EFieldPosition::First, TEXT("Throw home from right field - you are the cutoff man.")),
		EFieldPosition::Right), EBaseType::Home));
	R.Add(WithThrowTo(WithThrowFrom(
		MakeCutoff(EFieldPosition::First, TEXT("Throw home from center field - you are the cutoff man.")),
		EFieldPosition::Center), EBaseType::Home));
	R.Add(WithThrowTo(WithThrowFrom(
		MakeCutoff(EFieldPosition::Third, TEXT("Throw home from left field - you are the cutoff man on that side.")),
		EFieldPosition::Left), EBaseType::Home));
	// 3루수가 컷오프로 나가면 3루가 빈다 — 유격수가 대신 지킨다.
	R.Add(WithThrowTo(WithThrowFrom(
		MakeCoverBase(EFieldPosition::Short, EBaseType::Third,
			TEXT("The third baseman is the cutoff on this one - you have third base.")),
		EFieldPosition::Left), EBaseType::Home));

	// ── 3루 송구 — 어느 외야수가 던지든 유격수가 중계 ──
	R.Add(WithThrowTo(WithThrowFrom(
		MakeCutoff(EFieldPosition::Short, TEXT("Throw going to third - the shortstop is the cutoff man.")),
		EFieldPosition::Center), EBaseType::Third));
	R.Add(WithThrowTo(WithThrowFrom(
		MakeCutoff(EFieldPosition::Short, TEXT("Throw going to third - the shortstop is the cutoff man.")),
		EFieldPosition::Left), EBaseType::Third));
	R.Add(WithThrowTo(WithThrowFrom(
		MakeCutoff(EFieldPosition::Short, TEXT("Throw going to third - the shortstop is the cutoff man.")),
		EFieldPosition::Right), EBaseType::Third));

	// ── 2루 송구 — 타구 쪽 미들 인필더가 중계, 반대쪽이 베이스 커버 ──
	R.Add(WithThrowTo(WithThrowFrom(
		MakeCutoff(EFieldPosition::Second, TEXT("Ball is on your side - go out as the relay man to second.")),
		EFieldPosition::Right), EBaseType::Second));
	R.Add(WithThrowTo(WithThrowFrom(
		MakeCoverBase(EFieldPosition::Short, EBaseType::Second,
			TEXT("The second baseman is going out for the relay - you have the bag.")),
		EFieldPosition::Right), EBaseType::Second));
	R.Add(WithThrowTo(WithThrowFrom(
		MakeCutoff(EFieldPosition::Short, TEXT("Ball is on your side - go out as the relay man to second.")),
		EFieldPosition::Left), EBaseType::Second));
	R.Add(WithThrowTo(WithThrowFrom(
		MakeCoverBase(EFieldPosition::Second, EBaseType::Second,
			TEXT("The shortstop is going out for the relay - you have the bag.")),
		EFieldPosition::Left), EBaseType::Second));

	// ── 외야 사령탑 — 중견수가 코너 외야수 뒤를 받친다 (핵심 시나리오) ──
	R.Add(WithThrowFrom(
		MakeBackUpFielder(EFieldPosition::Center, EFieldPosition::Left,
			TEXT("The center fielder is the outfield captain - back up the left fielder too."), true),
		EFieldPosition::Left));
	R.Add(WithThrowFrom(
		MakeBackUpFielder(EFieldPosition::Center, EFieldPosition::Right,
			TEXT("The center fielder is the outfield captain - back up the right fielder too."), true),
		EFieldPosition::Right));

	// ── 외야 갭 백업 — 내 갭의 타구를 옆 외야수가 처리하면, 먼 베이스가 아니라 그 뒤를 받친다 ──
	//
	// 이게 없으면 "좌중간 타구를 중견수가 잡는데 좌익수는 3루 뒤 백업"이 정답이 된다 (블랭킷
	// 규칙 ThrowTo=Third 에 걸려서). 공이 바로 옆에 떨어졌는데 40m 떨어진 베이스로 달려가라는
	// 뜻이라 야구로도 틀리고, 플레이어가 룰을 의심하게 되는 자리였다.
	//
	// 구체성 2(ThrowFrom + BallZone) > 블랭킷 1(ThrowTo) 이라 매칭이 확정적으로 이쪽을 이긴다
	// — 동점이면 규칙 선언 순서에 좌우돼 취약해지므로, BallZone 까지 걸어 두는 게 핵심이다.
	// 반대로 갭이 아닌 타구(중견 정면 등)에서는 이 규칙이 안 걸려 기존대로 베이스를 백업한다.
	R.Add(WithBallZone(WithThrowFrom(
		MakeBackUpFielder(EFieldPosition::Left, EFieldPosition::Center,
			TEXT("The ball is in your gap and the center fielder is taking it - back him up, don't run to the bag."), true),
		EFieldPosition::Center), EBattedBallZone::LeftCenter));
	R.Add(WithBallZone(WithThrowFrom(
		MakeBackUpFielder(EFieldPosition::Right, EFieldPosition::Center,
			TEXT("The ball is in your gap and the center fielder is taking it - back him up, don't run to the bag."), true),
		EFieldPosition::Center), EBattedBallZone::RightCenter));

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

		if (Rule.bFilterBallKind && Rule.BallKind != Play.BallKind) { continue; }

		// 안전장치: 내가 던지는 공을 내가 백업/커버한다는 말이 안 되는 매칭을 걷어낸다.
		if (Rule.bExcludeSelfThrow && Play.bHasThrowFrom && Play.ThrowFrom == Position) { continue; }

		// 안전장치: 번트는 커버가 통째로 달라진다 (1루수가 대시 → 1루는 2루수가 지킨다).
		if (Rule.bExcludeBunt && Play.BallKind == EBattedBallKind::Bunt) { continue; }

		// ⚠️ bExcludeSelfThrow 는 구체성에 넣지 않는다 — "더 구체적인 상황"이 아니라
		//    "성립할 수 없는 경우를 걷어내는" 안전장치라서다 (BackupTypes.h 주석 참고).
		//    넣으면 블랭킷 규칙이 세밀한 규칙과 같은 순위가 되어 매칭이 뒤집힌다.
		const int32 Specificity =
			(Rule.bFilterThrowTo ? 1 : 0) + (Rule.bFilterThrowFrom ? 1 : 0) +
			(Rule.bFilterBallZone ? 1 : 0) + (Rule.bRequireNoThrowFrom ? 1 : 0) +
			(Rule.bFilterBallKind ? 1 : 0);
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

int32 UBackupPlaybook::PositionNumber(EFieldPosition Pos)
{
	// 투수 1 · 포수 2 는 이 모드에 등장하지 않는다 (백업 판단 대상 7개 포지션만).
	switch (Pos)
	{
	case EFieldPosition::First:  return 3;
	case EFieldPosition::Second: return 4;
	case EFieldPosition::Third:  return 5;
	case EFieldPosition::Short:  return 6;
	case EFieldPosition::Left:   return 7;
	case EFieldPosition::Center: return 8;
	case EFieldPosition::Right:  return 9;
	default:                     return 0;
	}
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
