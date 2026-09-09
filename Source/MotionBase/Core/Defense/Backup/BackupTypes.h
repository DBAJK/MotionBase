#pragma once

#include "CoreMinimal.h"
#include "Data/MotionBaseTypes.h"   // EBaseType, EFieldPosition
#include "BackupTypes.generated.h"

/**
 * 타구가 떨어진 방향/구역. 백업 판단의 "무엇을 보고 결정하는가" 두 변수 중 하나
 * (다른 하나는 ERunnerState). Steal 은 타구가 아니라 도루 상황을 표시하는 특수값이다.
 */
UENUM(BlueprintType)
enum class EBattedBallZone : uint8
{
	InfieldLeft    UMETA(DisplayName = "내야 좌측 (3루-유격)"),
	InfieldMiddle  UMETA(DisplayName = "내야 중앙 (유격-2루)"),
	InfieldRight   UMETA(DisplayName = "내야 우측 (2루-1루)"),
	BuntFirst      UMETA(DisplayName = "1루 쪽 번트"),
	BuntThird      UMETA(DisplayName = "3루 쪽 번트"),
	LeftLine       UMETA(DisplayName = "좌측 파울선 인근"),
	LeftField      UMETA(DisplayName = "좌익"),
	LeftCenter     UMETA(DisplayName = "좌중간"),
	Center         UMETA(DisplayName = "중견"),
	RightCenter    UMETA(DisplayName = "우중간"),
	RightField     UMETA(DisplayName = "우익"),
	RightLine      UMETA(DisplayName = "우측 파울선 인근"),
	Backstop       UMETA(DisplayName = "백스톱 (포일·폭투)"),
	Steal          UMETA(DisplayName = "도루 (타구 없음)")
};

/** 타구 종류. Bunt/Steal/WildPitch 는 별도 처리가 필요해 Grounder 와 구분한다. */
UENUM(BlueprintType)
enum class EBattedBallKind : uint8
{
	Grounder,
	LineDrive,
	FlyBall,
	Bunt,
	Steal,
	WildPitch
};

/**
 * 주자 상황. 백업 판단의 두 번째 변수 — 어디로 송구가 갈지(따라서 누가 백업해야 하는지)를
 * 결정한다. (주자 상황 자체가 배치를 정하는 게 아니라, 송구 목적지를 정하고 그게 배치를 정한다.)
 */
UENUM(BlueprintType)
enum class ERunnerState : uint8
{
	None, First, Second, Third, FirstSecond, FirstThird, SecondThird, Loaded
};

/**
 * 백업 행동의 형태. 존 계산 방식이 역할마다 다르다 (FBaseballField::ResolveZone 참고).
 *   Hold          — 자기 자리를 지킨다 (백업 대상이 아닌 시행 — 무조건 뛰는 습관을 걸러낸다).
 *   BackUpBase    — 송구가 빠질 경우를 대비해 베이스 뒤에 선다.
 *   CoverBase     — 원래 그 자리를 지키던 야수가 빠져서 대신 베이스를 지킨다.
 *   CutoffRelay   — 외야수와 베이스 사이 중계 라인에 선다 (점이 아니라 선분 판정).
 *   BackUpFielder — 다른 야수(주로 외야수)의 뒤를 받쳐준다 (공을 놓칠 경우 대비).
 */
UENUM(BlueprintType)
enum class EBackupRole : uint8
{
	Hold,
	BackUpBase,
	CoverBase,
	CutoffRelay,
	BackUpFielder
};

/** 백업 위치 판단 시행 한 번의 판정 결과. */
UENUM(BlueprintType)
enum class EBackupOutcome : uint8
{
	Covered    UMETA(DisplayName = "성공"),
	TooSlow    UMETA(DisplayName = "시간 초과"),    // 방향은 맞았는데 늦게 도착
	WrongZone  UMETA(DisplayName = "다른 위치"),    // 시간 안에 도착했지만 엉뚱한 곳
	NoStart    UMETA(DisplayName = "무반응"),       // 아예 움직이지 않음 (Hold 시행이 아닌데)
	FalseStart UMETA(DisplayName = "성급한 출발")   // 큐 전에 움직였거나, Hold 시행인데 움직임
};

/**
 * 백업 케이스 1개(플레이) — 포지션과 무관한 "경기 상황" 그 자체.
 * 스펙의 1 케이스 = `타구 방향·종류` + `주자 상황` → (포지션별로 다른) 정답 백업 zone.
 * 정답은 여기 없다 — FBackupAssignmentRule 이 포지션별로 따로 정의한다.
 */
USTRUCT(BlueprintType)
struct FBackupPlay
{
	GENERATED_BODY()

	/** 안정 식별자 (로그·디버그용). */
	UPROPERTY(BlueprintReadOnly)
	FName PlayId;

	UPROPERTY(BlueprintReadOnly)
	EBattedBallZone BallZone = EBattedBallZone::Center;

	UPROPERTY(BlueprintReadOnly)
	EBattedBallKind BallKind = EBattedBallKind::Grounder;

	UPROPERTY(BlueprintReadOnly)
	ERunnerState Runners = ERunnerState::None;

	/** 이 플레이에 "누가 공을 처리해 던지는지"가 있는지 (도루·포일은 포수라 7개 포지션 밖). */
	UPROPERTY(BlueprintReadOnly)
	bool bHasThrowFrom = false;

	UPROPERTY(BlueprintReadOnly)
	EFieldPosition ThrowFrom = EFieldPosition::Short;

	/**
	 * 이번 송구(또는 커버 상황)의 목적지 베이스. bHasThrowTo=false 면 의미 없는 값이다.
	 * ⚠️ "값 없음"을 EBaseType 의 기존 값(예: Home) 으로 때우면 안 된다 — 그 값을 필터로
	 *    쓰는 다른 규칙이 이 플레이에 실수로 걸릴 수 있다 (예: BackUpFielder 전용 플레이에
	 *    ThrowTo=Home 을 더미로 넣었더니 "ThrowTo=Home" 규칙이 엉뚱하게 매칭된 적이 있다).
	 */
	UPROPERTY(BlueprintReadOnly)
	bool bHasThrowTo = true;

	UPROPERTY(BlueprintReadOnly)
	EBaseType ThrowTo = EBaseType::First;

	/** 중계(경유) 송구인지 — CutoffRelay 역할 존재 여부와는 별개로 상황 설명용. */
	UPROPERTY(BlueprintReadOnly)
	bool bRelay = false;

	/** 상황 설명 (영문 — 3D 텍스트 표기용). 예: "Single to left-center, runner on 1st taking third". */
	UPROPERTY(BlueprintReadOnly)
	FString Situation;

	/** 주자 상황 설명 (영문). 예: "Runner on 1st". */
	UPROPERTY(BlueprintReadOnly)
	FString RunnerText;

	/** 추첨 가중치. 흔한 플레이를 더 자주 내고 싶을 때 1보다 크게. */
	UPROPERTY(BlueprintReadOnly)
	float Weight = 1.0f;
};

/**
 * 한 포지션이 어떤 플레이를 만났을 때 해야 할 행동 (정답 규칙 1개).
 *
 * 필터(bFilter*)가 켜진 것만 매칭에 참여한다 — 더 많은 필터를 만족하는 규칙이 더 구체적이라
 * 우선한다 (FBackupPlaybook::Resolve). 하나도 안 맞으면 기본값 Hold 로 떨어진다.
 *
 * BackUpBase/CutoffRelay 의 좌표 앵커는 해석 시점의 FBackupPlay 에서 그대로 뽑아 쓴다
 * (송구가 가는 베이스/누가 던지는지). CoverBase/BackUpFielder 는 실제로 지키는 대상이
 * 송구 목적지와 다를 수 있어(예: "3루수가 번트 처리 → 유격수가 3루 커버" — 아웃 송구는
 * 1루로 가지만 지키는 건 3루) 이 구조체에 명시적으로 담는다.
 */
USTRUCT(BlueprintType)
struct FBackupAssignmentRule
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	EFieldPosition Position = EFieldPosition::First;

	UPROPERTY(BlueprintReadOnly)
	bool bFilterThrowTo = false;
	UPROPERTY(BlueprintReadOnly)
	EBaseType ThrowTo = EBaseType::First;

	UPROPERTY(BlueprintReadOnly)
	bool bFilterThrowFrom = false;
	UPROPERTY(BlueprintReadOnly)
	EFieldPosition ThrowFrom = EFieldPosition::Short;

	/**
	 * true 면 "누군가 필드에서 처리해 던지는 상황이 아닌"(도루·포일 — 포수가 던짐) 플레이에만
	 * 매칭된다. bFilterThrowFrom 은 "특정 포지션이 던졌을 때"를 걸러내는 반면, 이건 그 반대
	 * (아무도/포수가 던졌을 때)를 걸러낸다 — 두 조건을 하나의 필드로 표현할 수 없어 분리했다.
	 */
	UPROPERTY(BlueprintReadOnly)
	bool bRequireNoThrowFrom = false;

	/**
	 * true 면 **내가 공을 처리하는 플레이(ThrowFrom == 내 포지션)에는 매칭되지 않는다.**
	 *
	 * 왜 별도 플래그인가: bFilterThrowFrom 은 "특정 포지션이 던졌을 때만"이라는 **양성 조건**이라
	 * "나만 빼고 누구든"을 표현할 수 없다. 그게 없어서 외야 블랭킷 규칙(예: "좌익수는 3루 송구를
	 * 전부 백업한다")이 **좌익수 본인의 송구까지 백업 대상으로 착각**했고, 지금까지는
	 * "그런 플레이를 아예 저작하지 않는" 회피책으로 막아 왔다 (BuildPlayTable 주석 참고).
	 * 이 플래그가 그 구멍을 규칙 쪽에서 직접 막는다.
	 *
	 * ⚠️ 구체성(specificity) 점수에는 **넣지 않는다.** 이건 "이 규칙이 더 구체적인 상황을 다룬다"가
	 *    아니라 "이 규칙이 성립할 수 없는 경우를 걷어낸다"는 **안전장치**다. 점수에 넣으면 블랭킷
	 *    규칙이 갑자기 세밀한 규칙과 같은 우선순위가 되어 매칭이 뒤집힌다.
	 */
	UPROPERTY(BlueprintReadOnly)
	bool bExcludeSelfThrow = false;

	/**
	 * true 면 **번트 플레이에는 매칭되지 않는다.**
	 *
	 * 번트는 같은 "1루 송구"라도 커버가 통째로 달라진다 — 1루수가 대시해 들어오므로
	 * 1루 베이스는 2루수가 지킨다. 그래서 "1루로 송구가 가면 1루수가 베이스를 지킨다" 같은
	 * 내야 기본 로테이션 규칙은 번트에서 반드시 빠져야 한다.
	 *
	 * ⚠️ bExcludeSelfThrow 와 마찬가지로 구체성 점수에 넣지 않는다 — 안전장치이지 조건이 아니다.
	 */
	UPROPERTY(BlueprintReadOnly)
	bool bExcludeBunt = false;

	/** 타구 종류 필터 (번트 전용 규칙처럼 종류가 곧 조건인 경우). 구체성에 포함된다. */
	UPROPERTY(BlueprintReadOnly)
	bool bFilterBallKind = false;
	UPROPERTY(BlueprintReadOnly)
	EBattedBallKind BallKind = EBattedBallKind::Grounder;

	UPROPERTY(BlueprintReadOnly)
	bool bFilterBallZone = false;
	UPROPERTY(BlueprintReadOnly)
	EBattedBallZone BallZone = EBattedBallZone::Center;

	UPROPERTY(BlueprintReadOnly)
	EBackupRole Role = EBackupRole::Hold;

	/** CoverBase 전용 — 실제로 지키는 베이스 (송구 목적지와 다를 수 있음). */
	UPROPERTY(BlueprintReadOnly)
	EBaseType AnchorBase = EBaseType::First;

	/**
	 * BackUpBase 전용 — true 면 송구 목적지가 아니라 **AnchorBase 를 받친다.**
	 *
	 * 기본 BackUpBase 는 앵커를 Play.ThrowTo 에서 가져오는데, 그러면 "1루로 송구가 가는
	 * 동안 중견수는 2루를 받친다" 같은 **송구와 무관한 베이스 백업**을 표현할 수 없다.
	 * 내야 기본 로테이션에 꼭 필요해서 열어 둔 우회로다.
	 *
	 * 이 경우 백업 자리는 홈 반대쪽(외야 쪽)으로 잡는다 — 그 베이스로 오는 송구가 없으니
	 * "송구 라인 뒤"라는 기준 자체가 없고, 실제로도 뒤에서 받치는 건 외야 쪽이다.
	 */
	UPROPERTY(BlueprintReadOnly)
	bool bAnchorBaseOverride = false;

	/** BackUpFielder 전용 — 누구의 뒤를 받치는지. */
	UPROPERTY(BlueprintReadOnly)
	EFieldPosition AnchorFielder = EFieldPosition::Center;

	/** 정답 해설 (영문). 기존 4지선다 18문항의 Explain 이 여기로 이주했다. */
	UPROPERTY(BlueprintReadOnly)
	FString Explain;

	/** 난이도 핵심 시나리오 여부 (중견수 광범위 백업, 1루수 이탈 시 2루수 1루 커버 등). */
	UPROPERTY(BlueprintReadOnly)
	bool bKeyScenario = false;
};

/**
 * 해석된 백업 존 — 실제 월드 좌표/판정 형태로 굳힌 정답.
 * FBaseballField::ResolveZone 이 FBackupAssignmentRule + FBackupPlay 로부터 계산한다.
 */
USTRUCT(BlueprintType)
struct FBackupZone
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	EBackupRole Role = EBackupRole::Hold;

	/** 점 형태 역할(BackUpBase/CoverBase/BackUpFielder/Hold)의 중심. */
	UPROPERTY(BlueprintReadOnly)
	FVector Center = FVector::ZeroVector;

	/** CutoffRelay 전용 — 유효 구간의 양 끝점 (선분 판정). */
	UPROPERTY(BlueprintReadOnly)
	FVector SegmentA = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly)
	FVector SegmentB = FVector::ZeroVector;

	/** 성공 반경(점 역할) 또는 회랑 반폭(CutoffRelay). cm. */
	UPROPERTY(BlueprintReadOnly)
	float RadiusCm = 200.0f;

	UPROPERTY(BlueprintReadOnly)
	FString Explain;

	UPROPERTY(BlueprintReadOnly)
	bool bKeyScenario = false;

	/** 플레이어 XY 위치 ↔ 이 존까지의 최단 거리 (cm). CutoffRelay 는 선분 거리. */
	float DistanceTo(const FVector& PlayerPos) const
	{
		if (Role == EBackupRole::CutoffRelay)
		{
			return FMath::PointDistToSegment(PlayerPos, SegmentA, SegmentB);
		}
		return FVector::Dist2D(PlayerPos, Center);
	}

	/** 반경/회랑 안에 있는지. */
	bool Contains(const FVector& PlayerPos) const
	{
		return DistanceTo(PlayerPos) <= RadiusCm;
	}

	/** 방향 판단(1단계)에 쓸 대표 좌표 — CutoffRelay 는 구간 중점을 쓴다. */
	FVector RepresentativePoint() const
	{
		return (Role == EBackupRole::CutoffRelay) ? FMath::Lerp(SegmentA, SegmentB, 0.5f) : Center;
	}
};

/** 이번 시행의 확정된 조건 — 플레이 + 내 포지션 + 정답 + 방향 판단용 후보들 + 제한 시간. */
USTRUCT(BlueprintType)
struct FBackupTrial
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FBackupPlay Play;

	UPROPERTY(BlueprintReadOnly)
	EFieldPosition Position = EFieldPosition::First;

	UPROPERTY(BlueprintReadOnly)
	FBackupZone CorrectZone;

	/**
	 * 방향 판단(1단계)의 후보 집합 — **"이 포지션이 플레이북 전체에서 배정받을 수 있는 존들"**
	 * 로 한정한다 (플레이가 다른 포지션에게 주는 존까지 넣으면 안 된다 — 외야에서는 그 존들이
	 * 서로 각도상 거의 구분이 안 돼 오판정을 유발한다. 설계 노트 참고).
	 */
	UPROPERTY(BlueprintReadOnly)
	TArray<FBackupZone> CandidateZones;

	/** CandidateZones 중 정답 인덱스. */
	UPROPERTY(BlueprintReadOnly)
	int32 CorrectCandidateIndex = INDEX_NONE;

	/** 도착 제한 시간 (초) — 거리에서 파생 (FBaseballField::DeriveTimeLimit). */
	UPROPERTY(BlueprintReadOnly)
	float TimeLimitSec = 5.0f;

	/** 이 시행이 "정답 = 제자리"인 Hold 시행인지. */
	UPROPERTY(BlueprintReadOnly)
	bool bIsHoldTrial = false;
};

/** 백업 위치 판단 한 시행의 최종 판정 결과. */
USTRUCT(BlueprintType)
struct FBackupResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	EBackupOutcome Outcome = EBackupOutcome::NoStart;

	/** 1단계: 방향이 맞았는지. */
	UPROPERTY(BlueprintReadOnly)
	bool bDirectionCorrect = false;

	/** 1단계: 후보 간 마진이 좁아 방향 판정을 자문(advisory)으로만 썼는지. */
	UPROPERTY(BlueprintReadOnly)
	bool bAmbiguousDirection = false;

	/** 측정 지표: 판단(이동 시작)까지 걸린 시간 (초). 안 움직였으면 -1. */
	UPROPERTY(BlueprintReadOnly)
	float DecisionTimeSec = -1.0f;

	/** 도착까지 걸린 시간 (초, 큐 시점 기준). 도착 못 하면 제한 시간 값. */
	UPROPERTY(BlueprintReadOnly)
	float ArrivalTimeSec = -1.0f;

	/** 2단계: 직선거리 / 누적 이동거리 (0~1). 낮을수록 헤맨 것. 이동 없으면 -1. */
	UPROPERTY(BlueprintReadOnly)
	float PathEfficiency = -1.0f;

	UPROPERTY(BlueprintReadOnly)
	bool bKeyScenario = false;

	/**
	 * 이 시행에서 내가 맡았어야 할 역할. 집계에 **꼭 필요하다** — 정답률만으로는
	 * "백업은 되는데 중계(cutoff)를 못 선다" 같은 편중이 안 보인다. 역할마다 가르치는
	 * 내용이 달라서 코칭 문장이 갈려야 하는 축이다.
	 */
	UPROPERTY(BlueprintReadOnly)
	EBackupRole Role = EBackupRole::Hold;

	/** 이 시행의 상황 문구 ("우전 안타, 3루 송구" 등). 어떤 상황을 틀렸는지 짚기 위한 것. */
	UPROPERTY(BlueprintReadOnly)
	FString Situation;

	/** 정답이 "제자리 유지"였던 시행인가. Hold 는 성공/실패의 의미가 다르다. */
	UPROPERTY(BlueprintReadOnly)
	bool bHoldTrial = false;

	bool IsSuccess() const { return Outcome == EBackupOutcome::Covered; }
};
