#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "BackupGameState.generated.h"

class ABackupPawn;

/**
 * 백업 드릴의 **시행 진행권**을 쥐는 게임 상태.
 *
 * 왜 폰이 아니라 여기인가: 협동 멀티플레이(1 VR + N PC)에서 전원이 **같은 타구를 같은
 * 순간에** 봐야 한다. 폰마다 따로 추첨하면(예전 구조) 각자 다른 상황을 보게 된다.
 *
 * ⚠️ **스탠드얼론에서 동작이 달라지면 안 된다.** `GameStateBase` 는 싱글플레이에서도 항상
 *    존재하고 `HasAuthority()` 도 true 라, 아래 서버 전용 로직이 싱글플레이에서 그대로
 *    돈다. 분기 없이 같은 코드로 두 경우를 만족시키는 게 이 설계의 요점이다.
 *
 * **복제하는 것**: 플레이 "인덱스"와 시행 카운터뿐이다. `FBackupPlay` 구조체는 복제하지
 * 않는다 — `UBackupPlaybook::BuildPlayTable()` 이 코드에 박힌 결정론적 데이터라 모든
 * 클라이언트가 동일한 테이블을 갖기 때문. 각 클라이언트는 인덱스만 받아 **자기 포지션
 * 기준으로** 정답을 로컬 계산한다(`BuildTrial`). 대역폭도, 규칙 저작 변경도 0.
 *
 * **복제하지 않는 것**: 판정·점수·저장. 입력이 로컬 HMD/컨트롤러라 로컬 계산이 정확하고
 * 지연도 없다. 부스 협동 콘텐츠라 치팅 방어는 하지 않는다 — 서버 권한 이동은 VR 에서
 * 지연으로 멀미를 만든다.
 */
UCLASS()
class MOTIONBASE_API ABackupGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	ABackupGameState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	// ── 폰이 부르는 등록/보고 (서버에서만 의미가 있다) ──

	/**
	 * 드릴 폰이 자신을 등록한다. 서버는 등록된 폰 수로 **혼자인지 여럿인지**를 판단해
	 * 추첨 규칙을 바꾼다(아래 DrawNextPlayIndex 주석 참고).
	 * 첫 등록 폰의 튜닝값(시행 수·간격)이 세션 전체의 값이 된다.
	 */
	void RegisterPawn(ABackupPawn* Pawn, int32 InTotalTrials, float InFirstDelaySec, float InIntervalSec);
	void UnregisterPawn(ABackupPawn* Pawn);

	/** 폰이 자기 시행을 끝냈다고 보고. 전원이 끝나면(또는 시간 초과) 다음 시행을 예약한다. */
	void ReportTrialFinished(ABackupPawn* Pawn);

	/** 세션 재시작 (종료 화면의 PLAY AGAIN). */
	void RequestRestart();

	// ── 조회 (클라이언트 포함 전원) ──

	/** 시행이 바뀔 때마다 1 증가. 폰은 이 값의 변화를 보고 새 시행을 시작한다. */
	int32 GetTrialSerial() const { return TrialSerial; }

	/** 이번 시행의 PlayTable 인덱스. 아직 시행 전이면 INDEX_NONE. */
	int32 GetCurrentPlayIndex() const { return CurrentPlayIndex; }

	int32 GetTrialIndex() const { return TrialIndex; }
	int32 GetTotalTrials() const { return TotalTrials; }
	bool  IsSessionOver() const { return bSessionOver; }
	bool  IsWaitingNext() const { return bWaitingNext; }

	/** 지금 협동 세션인가 (등록 폰 2 이상). 추첨 규칙과 UI 문구가 갈린다. */
	bool IsCoopSession() const { return RegisteredPawnCount >= 2; }

protected:
	/**
	 * 다음 시행의 플레이 인덱스를 고른다. **서버 전용.**
	 *
	 * ⚠️ 혼자일 때와 여럿일 때 규칙이 다르다 —
	 *  - **혼자**: 등록된 그 폰에게 위임한다. 폰이 자기 포지션 기준으로 "내가 할 일이 있는
	 *    플레이"를 편향 추첨한다(HoldTrialFraction). 이게 없으면 1루수 혼자 플레이할 때
	 *    대부분의 시행에서 아무 일도 일어나지 않는다.
	 *  - **여럿**: 전체 테이블에서 그냥 뽑는다. 하나의 타구에 7명이 동시에 걸려 있으니
	 *    각자에게 할 일이 있기도 없기도 하다 — 그게 실제 야구이고, "가만히 있는 것도
	 *    정답일 수 있다"는 훈련 타당성이 보정 없이 공짜로 나온다.
	 */
	int32 DrawNextPlayIndex();

	/** 여럿일 때 쓰는 전체 테이블 무반복 주머니. 고갈되면 다시 채운다. */
	int32 DrawFromSharedBag();

	/** 서버: 다음 시행을 DelaySec 뒤로 예약한다. */
	void ScheduleNextTrial(float DelaySec);

	/** 서버: 예약된 시행을 실제로 시작(인덱스 확정 + 시리얼 증가). */
	void BeginNextTrial();

	/** 서버: 한 시행이 끝난 뒤의 공통 처리 (시행 번호 증가 → 다음 예약 또는 세션 종료). */
	void AdvanceAfterTrial();

	/**
	 * 무효해진 등록(파괴된 폰의 약참조)을 걷어낸다. **인원수를 세기 전에 반드시 부른다.**
	 *
	 * 왜 필요한가: 인원수가 협동 여부와 "전원 보고했는가"를 가른다. 유령이 하나 남아 있으면
	 * **혼자 하는데도 협동으로 오인해 오지 않을 보고를 영원히 기다린다** — 즉 혼자 플레이가
	 * 통째로 멈춘다. 협동은 실제로 붙어 있는 기기 수로만 판단해야 한다.
	 */
	void PruneRegisteredPawns();

	/** 서버: 세션 종료 처리. */
	void FinishSession();

	// ── 복제 상태 ──

	/** 시행 교체 신호. 같은 플레이가 연달아 나와도 새 시행임을 구분하려면 카운터가 필요하다. */
	UPROPERTY(Replicated)
	int32 TrialSerial = 0;

	UPROPERTY(Replicated)
	int32 CurrentPlayIndex = INDEX_NONE;

	UPROPERTY(Replicated)
	int32 TrialIndex = 0;

	UPROPERTY(Replicated)
	int32 TotalTrials = 6;

	UPROPERTY(Replicated)
	bool bSessionOver = false;

	UPROPERTY(Replicated)
	bool bWaitingNext = false;

	// ── 서버 전용 (복제하지 않음) ──

	/** 등록된 드릴 폰들. 서버에서만 채워진다. */
	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<ABackupPawn>> RegisteredPawns;

	/** 복제해서 클라이언트도 협동 여부를 알 수 있게 한다 (RegisteredPawns 자체는 서버 전용). */
	UPROPERTY(Replicated)
	int32 RegisteredPawnCount = 0;

	/** 이번 시행을 끝냈다고 보고한 폰들. 전원이 차면 다음 시행으로 넘어간다. */
	TSet<TWeakObjectPtr<ABackupPawn>> FinishedThisTrial;

	/** 다음 시행까지 남은 시간(서버 카운트다운). */
	float NextTrialTimer = 0.0f;

	/**
	 * 한 시행의 절대 상한(초). 협동에서 누군가 헤드셋을 벗거나 멈춰버려도 세션이 영원히
	 * 멈추지 않게 하는 안전장치 — 이 시간이 지나면 미보고 폰을 무시하고 진행한다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Backup|Coop")
	float TrialHardTimeoutSec = 20.0f;

	float TrialElapsedSec = 0.0f;

	float FirstDelaySec = 1.5f;
	float IntervalSec = 3.0f;

	/** 여럿일 때 쓰는 무반복 주머니 (PlayTable 인덱스). */
	TArray<int32> SharedBag;

	/** 플레이 테이블 크기 — 주머니를 채울 때만 쓴다. 폰이 등록하며 알려준다. */
	int32 PlayTableSize = 0;
};
