#include "Core/Defense/Backup/BackupGameState.h"
#include "Core/Defense/Backup/BackupPawn.h"
#include "Core/Defense/Backup/BackupPlaybook.h"
#include "MotionBase.h"
#include "Net/UnrealNetwork.h"

ABackupGameState::ABackupGameState()
{
	// GameStateBase 는 기본적으로 틱하지 않는다 — 시행 카운트다운을 여기서 돌리므로 켠다.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

void ABackupGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABackupGameState, TrialSerial);
	DOREPLIFETIME(ABackupGameState, CurrentPlayIndex);
	DOREPLIFETIME(ABackupGameState, TrialIndex);
	DOREPLIFETIME(ABackupGameState, TotalTrials);
	DOREPLIFETIME(ABackupGameState, bSessionOver);
	DOREPLIFETIME(ABackupGameState, bWaitingNext);
	DOREPLIFETIME(ABackupGameState, RegisteredPawnCount);
}

void ABackupGameState::RegisterPawn(ABackupPawn* Pawn, int32 InTotalTrials, float InFirstDelaySec, float InIntervalSec)
{
	if (!Pawn)
	{
		return;
	}

	// 클라이언트의 폰도 자기 자신을 부르지만, 진행 상태는 서버만 만든다.
	// (클라이언트는 복제된 TrialSerial 을 보고 따라올 뿐이다.)
	if (!HasAuthority())
	{
		return;
	}

	// 재등록(RestartSession 경로)은 세션을 다시 개시하지 않는다 — 재시작은 RequestRestart
	// 한 곳에서만 일어나야 한다. 여기서도 개시하면 타이머가 두 번 걸린다.
	const bool bAlreadyRegistered = RegisteredPawns.Contains(Pawn);

	RegisteredPawns.AddUnique(Pawn);
	RegisteredPawnCount = RegisteredPawns.Num();

	if (bAlreadyRegistered)
	{
		return;
	}

	// 첫 등록 폰의 튜닝값이 세션 전체의 값이 된다 — 협동에서 사람마다 시행 수가 다르면
	// 세션이 성립하지 않는다.
	if (RegisteredPawns.Num() == 1)
	{
		TotalTrials   = FMath::Max(InTotalTrials, 1);
		FirstDelaySec = FMath::Max(InFirstDelaySec, 0.2f);
		IntervalSec   = FMath::Max(InIntervalSec, 0.2f);
		PlayTableSize = UBackupPlaybook::BuildPlayTable().Num();

		TrialIndex = 0;
		TrialSerial = 0;
		CurrentPlayIndex = INDEX_NONE;
		bSessionOver = false;
		SharedBag.Reset();
		FinishedThisTrial.Reset();

		ScheduleNextTrial(FirstDelaySec);

		UE_LOG(LogMotionBase, Log, TEXT("[BackupGS] 세션 시작 — %d 시행, 첫 시행 %.1fs 뒤."),
			TotalTrials, FirstDelaySec);
	}
	else
	{
		UE_LOG(LogMotionBase, Log, TEXT("[BackupGS] 협동 참가 — 현재 %d명."), RegisteredPawns.Num());
	}
}

void ABackupGameState::UnregisterPawn(ABackupPawn* Pawn)
{
	if (!HasAuthority())
	{
		return;
	}

	RegisteredPawns.Remove(Pawn);
	FinishedThisTrial.Remove(Pawn);
	PruneRegisteredPawns();

	// 협동에서 단독으로 떨어지는 순간을 남긴다 — 부스에서 "왜 갑자기 혼자 규칙으로 도나"를
	// 로그로 확인할 수 있어야 한다.
	if (RegisteredPawns.Num() == 1)
	{
		UE_LOG(LogMotionBase, Log, TEXT("[BackupGS] 한 명만 남았다 — 단독 규칙으로 전환."));
	}

	// 나간 사람 때문에 "전원 보고"가 영영 안 차는 상황을 막는다 — 남은 전원이 이미
	// 보고했다면 즉시 진행한다.
	if (RegisteredPawns.Num() > 0 && !bSessionOver && !bWaitingNext
		&& FinishedThisTrial.Num() >= RegisteredPawns.Num())
	{
		AdvanceAfterTrial();
	}
}

void ABackupGameState::ReportTrialFinished(ABackupPawn* Pawn)
{
	if (!HasAuthority() || bSessionOver || bWaitingNext)
	{
		return;
	}

	if (Pawn)
	{
		FinishedThisTrial.Add(Pawn);
	}

	// 세기 전에 유령을 걷어낸다 — 안 하면 혼자인데도 오지 않을 보고를 기다리며 멈춘다.
	PruneRegisteredPawns();

	// ⚠️ 등록 폰이 0이면 진행하지 않는다. 아래 비교는 Num()==0 일 때 무조건 거짓이 되어
	//    "전원 보고 완료"로 통과해 버린다 — 플레이어가 하나도 없는데 시행이 넘어간다.
	if (RegisteredPawns.Num() == 0)
	{
		return;
	}

	// 전원이 끝나야 다음으로 — 먼저 끝낸 사람은 다른 사람이 도착하는 걸 보며 기다린다.
	// (혼자면 RegisteredPawns.Num()==1 이라 즉시 넘어간다 = 기존 싱글플레이 동작.)
	if (FinishedThisTrial.Num() < RegisteredPawns.Num())
	{
		return;
	}

	AdvanceAfterTrial();
}

void ABackupGameState::PruneRegisteredPawns()
{
	RegisteredPawns.RemoveAll([](const TWeakObjectPtr<ABackupPawn>& P) { return !P.IsValid(); });

	for (auto It = FinishedThisTrial.CreateIterator(); It; ++It)
	{
		if (!It->IsValid())
		{
			It.RemoveCurrent();
		}
	}

	RegisteredPawnCount = RegisteredPawns.Num();
}

void ABackupGameState::AdvanceAfterTrial()
{
	if (!HasAuthority())
	{
		return;
	}

	++TrialIndex;
	FinishedThisTrial.Reset();

	if (TrialIndex >= TotalTrials)
	{
		FinishSession();
		return;
	}

	ScheduleNextTrial(IntervalSec);
}

void ABackupGameState::RequestRestart()
{
	if (!HasAuthority())
	{
		return;
	}

	TrialIndex = 0;
	CurrentPlayIndex = INDEX_NONE;
	bSessionOver = false;
	SharedBag.Reset();
	FinishedThisTrial.Reset();
	TrialElapsedSec = 0.0f;

	ScheduleNextTrial(FirstDelaySec);
}

void ABackupGameState::ScheduleNextTrial(float DelaySec)
{
	bWaitingNext = true;
	NextTrialTimer = FMath::Max(DelaySec, 0.0f);
}

void ABackupGameState::BeginNextTrial()
{
	if (!HasAuthority())
	{
		return;
	}

	if (TrialIndex >= TotalTrials)
	{
		FinishSession();
		return;
	}

	const int32 Index = DrawNextPlayIndex();
	if (Index == INDEX_NONE)
	{
		// 저작 데이터가 비어 있는 극단적 경우 — 세션을 매달아두지 않고 끝낸다.
		UE_LOG(LogMotionBase, Warning, TEXT("[BackupGS] 뽑을 플레이가 없다 — 세션 종료."));
		FinishSession();
		return;
	}

	CurrentPlayIndex = Index;
	bWaitingNext = false;
	FinishedThisTrial.Reset();
	TrialElapsedSec = 0.0f;

	// ⚠️ 시리얼은 **마지막에** 올린다. 폰은 이 값의 변화만 보고 시행을 시작하므로,
	//    CurrentPlayIndex 가 확정되기 전에 올리면 한 프레임 동안 이전 플레이로 시작한다.
	++TrialSerial;
}

void ABackupGameState::FinishSession()
{
	bSessionOver = true;
	bWaitingNext = false;
	CurrentPlayIndex = INDEX_NONE;
}

int32 ABackupGameState::DrawNextPlayIndex()
{
	// 추첨 규칙이 인원수로 갈리므로, 세기 전에 유령을 걷어낸다.
	PruneRegisteredPawns();

	// 혼자면 그 폰에게 위임한다 — 자기 포지션 기준 편향 추첨이라야 대부분의 시행에서
	// 할 일이 생긴다 (헤더 주석의 규칙 참고).
	if (RegisteredPawns.Num() == 1)
	{
		if (ABackupPawn* Solo = RegisteredPawns[0].Get())
		{
			return Solo->DrawSoloPlayIndex();
		}
	}

	return DrawFromSharedBag();
}

int32 ABackupGameState::DrawFromSharedBag()
{
	if (PlayTableSize <= 0)
	{
		PlayTableSize = UBackupPlaybook::BuildPlayTable().Num();
	}
	if (PlayTableSize <= 0)
	{
		return INDEX_NONE;
	}

	if (SharedBag.Num() == 0)
	{
		SharedBag.Reserve(PlayTableSize);
		for (int32 i = 0; i < PlayTableSize; ++i)
		{
			SharedBag.Add(i);
		}
	}

	const int32 Pick = FMath::RandRange(0, SharedBag.Num() - 1);
	const int32 Index = SharedBag[Pick];
	SharedBag.RemoveAtSwap(Pick);
	return Index;
}

void ABackupGameState::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// 진행권은 서버에만 있다. 클라이언트는 복제된 값을 따라올 뿐이다.
	if (!HasAuthority() || bSessionOver)
	{
		return;
	}

	// ⚠️ 참가자가 하나도 없으면 세션을 진행시키지 않는다.
	//    폰이 EndPlay 없이 사라지는 경로(레벨 정리, 강제 GC)가 있는데, 그대로 두면 아래
	//    하드 타임아웃이 매번 걸려 **플레이어 없이 시행이 끝까지 소진**된다.
	//    (배열이 최대 7개라 매 틱 정리해도 비용은 무시할 수준이다.)
	PruneRegisteredPawns();
	if (RegisteredPawns.Num() == 0)
	{
		return;
	}

	if (bWaitingNext)
	{
		NextTrialTimer -= DeltaSeconds;
		if (NextTrialTimer <= 0.0f)
		{
			BeginNextTrial();
		}
		return;
	}

	// 시행 중 — 누군가 헤드셋을 벗거나 멈춰도 세션이 영원히 멈추지 않게 하는 안전장치.
	if (CurrentPlayIndex != INDEX_NONE)
	{
		TrialElapsedSec += DeltaSeconds;
		if (TrialElapsedSec >= TrialHardTimeoutSec)
		{
			UE_LOG(LogMotionBase, Warning,
				TEXT("[BackupGS] 시행 %d 이 %.0fs 를 넘겨 미보고 인원을 무시하고 진행한다 (%d/%d 보고)."),
				TrialIndex + 1, TrialHardTimeoutSec, FinishedThisTrial.Num(), RegisteredPawns.Num());
			AdvanceAfterTrial();
		}
	}
}
