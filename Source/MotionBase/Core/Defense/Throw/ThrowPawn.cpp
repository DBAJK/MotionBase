#include "Core/Defense/Throw/ThrowPawn.h"
#include "Core/Defense/Throw/ThrowJudge.h"
#include "Core/Defense/CatchBall/CatchBall.h"
#include "Core/MotionBaseGameMode.h"
#include "MotionBase.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "MotionControllerComponent.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/PlayerController.h"
#include "Core/Defense/Throw/ThrowHUD.h"
#include "UI/ModeSelectHUD.h"
#include "UI/VRInfoPanel.h"
#include "AI/DrillCatalog.h"
#include "AI/AIFeedbackService.h"
#include "Analysis/WeaknessDetector.h"
#include "Scoring/ScoringService.h"
#include "Core/ModeManager.h"

namespace
{
	// TextRender 는 자동 줄바꿈이 없다 → 글자수로 하드 랩.
	TArray<FString> ThrowWrap(const FString& In, int32 MaxCharsPerLine, int32 MaxLines)
	{
		TArray<FString> Lines;
		int32 i = 0;
		const int32 Len = In.Len();
		while (i < Len && Lines.Num() < MaxLines)
		{
			Lines.Add(In.Mid(i, MaxCharsPerLine));
			i += MaxCharsPerLine;
		}
		if (i < Len && Lines.Num() > 0)
		{
			Lines.Last().Append(TEXT(" …"));
		}
		return Lines;
	}
}

AThrowPawn::AThrowPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	Capsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	Capsule->InitCapsuleSize(34.0f, 88.0f);
	SetRootComponent(Capsule);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Capsule);
	Camera->SetRelativeLocation(FVector(0.0f, 0.0f, 70.0f));
	Camera->bUsePawnControlRotation = false;

	// VR 송구 손 = 오른손 컨트롤러 + 손에 든 공.
	ThrowController = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("ThrowController"));
	ThrowController->SetupAttachment(Capsule);
	ThrowController->MotionSource = FName(TEXT("Right"));

	BallInHandMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BallInHandMesh"));
	BallInHandMesh->SetupAttachment(ThrowController);
	BallInHandMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sph(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (Sph.Succeeded())
	{
		BallInHandMesh->SetStaticMesh(Sph.Object);
		BallInHandMesh->SetRelativeScale3D(FVector(0.10f));
	}

	// VR 상태 패널 — 캡슐 루트에 월드 고정(정면 +X, Y=0, 눈높이쯤). 헤드락 아님.
	VrPanel = CreateDefaultSubobject<UVRInfoPanel>(TEXT("VrPanel"));
	VrPanel->SetupAttachment(Capsule);
	VrPanel->SetPlacement(UVRInfoPanel::DefaultDistanceCm, 70.0f);
}

void AThrowPawn::BeginPlay()
{
	Super::BeginPlay();

	SetActorRotation(FRotator::ZeroRotator); // 정면(+X) 고정
	HomeLocation = GetActorLocation();

	// HMD 연결 시 컨트롤러 던지기 동작으로 송구. 아니면 스페이스바 충전.
	bVR = UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayEnabled();
	if (bVR)
	{
		UHeadMountedDisplayFunctionLibrary::SetTrackingOrigin(EHMDTrackingOrigin::Stage);
	}

	// 그라운드 기준 임시값 — 첫 시행에서 EnsureFieldAnchor 가 실제 값(HMD 방향)으로 덮어쓴다.
	// (그전에 BaseLocation 이 불려도 월드 원점에 베이스가 찍히지 않게 하는 안전값.)
	FieldAnchor = FVector(HomeLocation.X, HomeLocation.Y, FloorZ());
	FieldYawDeg = GetActorRotation().Yaw;

	// 나가기 제스처 — 송구 와인드업에서 팔이 위로 올라가므로 기본값보다 조인다.
	// 시행이 진행 중인 동안(급구 대기/공 들고 있음/송구 중)에는 Tick 에서 진행을 동결한다.
	ExitGesture.UpThreshold = 0.90f;
	ExitGesture.HoldSec     = 2.0f;

	if (BallInHandMesh)
	{
		BallInHandMesh->SetVisibility(false); // 공을 잡은 뒤(Ready)에만 보인다.
	}

	// 상태 패널은 VR 에서만. PC 는 평면 HUD(AThrowHUD)가 담당한다.
	if (VrPanel)
	{
		VrPanel->BuildPanel();
		if (!bVR)
		{
			VrPanel->HideAll();
		}
		else
		{
			// 액션 모드 — 요소를 눈높이로 모으고, 나가기용 '뒤로' 카드를 상시 띄운다
			// (컨트롤러를 겨눠 잠시 유지 = 나가기. '위로 들기' 제스처와 병행).
			VrPanel->SetStatusCompact();
			VrPanel->ShowBackCard(TEXT("EXIT - aim here & hold"), FColor(255, 190, 90));
		}
	}

	// AI 운동 추천 서비스 (키가 없으면 요청 시 조용히 생략됨).
	FeedbackService = NewObject<UAIFeedbackService>(this);
	FeedbackService->OnFeedbackReady.AddDynamic(this, &AThrowPawn::HandleCoachingReady);

	StartSession();
}

void AThrowPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// 스페이스바는 단계에 따라 역할이 바뀐다: 급구 대기=포구, 송구 준비=충전/발사.
	PlayerInputComponent->BindKey(EKeys::SpaceBar, IE_Pressed,  this, &AThrowPawn::OnSpacePressed);
	PlayerInputComponent->BindKey(EKeys::SpaceBar, IE_Released, this, &AThrowPawn::OnSpaceReleased);
	PlayerInputComponent->BindKey(EKeys::M, IE_Pressed, this, &AThrowPawn::ReturnToModeSelect);

	// HUD 교체 (빙의 후 여기서).
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->ClientSetHUD(AThrowHUD::StaticClass());
	}
}

// ── 베이스 ──

FString AThrowPawn::BaseName(EBaseType Base)
{
	switch (Base)
	{
	case EBaseType::First:  return TEXT("1B");
	case EBaseType::Second: return TEXT("2B");
	case EBaseType::Third:  return TEXT("3B");
	default:                return TEXT("Home");
	}
}

int32 AThrowPawn::BaseIndexOf(EBaseType Base)
{
	switch (Base)
	{
	case EBaseType::First:  return 0;
	case EBaseType::Second: return 1;
	case EBaseType::Third:  return 2;
	default:                return 3;
	}
}

void AThrowPawn::EnsureFieldAnchor()
{
	if (bFieldAnchored)
	{
		return;
	}

	// VR: 머리(카메라)가 있는 자리와 보고 있는 방향. 폰 루트는 트래킹 원점일 뿐이라
	//     루트를 기준으로 삼으면 플레이 공간 한쪽에 선 사람에겐 그라운드가 통째로 어긋난다.
	// PC: 루트가 곧 몸이고 정면은 +X (BeginPlay 에서 고정) — 기존 동작 그대로.
	const bool bUseHead = (bVR && Camera);
	const FVector Src   = bUseHead ? Camera->GetComponentLocation() : GetActorLocation();

	FieldAnchor    = FVector(Src.X, Src.Y, FloorZ());
	FieldYawDeg    = bUseHead ? Camera->GetComponentRotation().Yaw : GetActorRotation().Yaw;
	bFieldAnchored = true;

	UE_LOG(LogMotionBase, Log, TEXT("[Throw] 그라운드 기준 확정: %s / 정면 %.0f°"),
		*FieldAnchor.ToCompactString(), FieldYawDeg);
}

FVector AThrowPawn::BaseLocation(EBaseType Base) const
{
	FVector2D Off = HomePlateOffset;
	switch (Base)
	{
	case EBaseType::First:  Off = FirstBaseOffset;  break;
	case EBaseType::Second: Off = SecondBaseOffset; break;
	case EBaseType::Third:  Off = ThirdBaseOffset;  break;
	default: break;
	}

	// 오프셋은 '내 정면' 기준이다 — 월드 축이 아니라 EnsureFieldAnchor 가 잡은 방향으로 돌린다.
	const FVector Local(Off.X, Off.Y, 0.0f);
	const FVector World = FRotator(0.0f, FieldYawDeg, 0.0f).RotateVector(Local);

	// 베이스는 바닥에 있다. 바닥면 계산은 VR/PC 가 다르므로 FloorZ() 로 통일한다
	// (VR 에서 -88 을 쓰면 베이스 마커가 땅속에 묻혀 안 보였다).
	return FVector(FieldAnchor.X + World.X, FieldAnchor.Y + World.Y, FloorZ());
}

// ── 세션 진행 ──

void AThrowPawn::StartSession()
{
	ThrowIndex = 0;
	SuccessCount = 0;
	bSessionOver = false;
	bWaitingNext = false;
	bHasResult = false;
	IntervalTimer = 0.0f;

	SessionResults.Reset();
	for (int32 i = 0; i < NumBases; ++i) { BaseAttempts[i] = 0; BaseSuccess[i] = 0; }

	LastDrills.Reset();
	CoachingText.Reset();
	bAwaitingCoaching = false;

	// 첫 시행은 한 박자 뒤에 — VR 은 BeginPlay 시점에 HMD 포즈가 없어 그라운드를 깔 방향을
	// 모른다 (FirstTrialDelaySec 주석 참고). 플레이어에게도 준비할 틈이 된다.
	bWaitingNext  = true;
	IntervalTimer = FMath::Max(FirstTrialDelaySec, 0.2f);
}

void AThrowPawn::SpawnNextTrial()
{
	UWorld* World = GetWorld();
	if (!World) { return; }

	// 그라운드 기준(내 자리·내 정면)을 확정한다. 첫 시행에서만 실제로 잡히고 이후엔 그대로 쓴다.
	EnsureFieldAnchor();

	CurrentTrial = FThrowTrial();

	// ① 목표 베이스 지정 — 네 베이스 중 랜덤. 던지기 전에 미리 표시된다.
	CurrentTrial.TargetBase     = static_cast<EBaseType>(FMath::RandRange(0, NumBases - 1));
	CurrentTrial.ThrowOrigin    = FVector(FieldAnchor.X, FieldAnchor.Y, ThrowHandZ()); // 손 높이
	CurrentTrial.TargetLocation = BaseLocation(CurrentTrial.TargetBase);
	CurrentTrial.TargetDistance = FVector::Dist2D(CurrentTrial.TargetLocation, CurrentTrial.ThrowOrigin);
	CurrentTrial.IdealPower     = DistanceToIdealPower(CurrentTrial.TargetDistance);
	CurrentTrial.HitRadius      = HitRadius;

	// ② 급구(feed) — 잡아야 시계가 돈다. **내 정면**에서 가슴 높이로 날아온다.
	//    (동료가 던져 주는 공이다. 이걸 잡는 순간부터 포구→송구 전환 시간이 측정된다.)
	const float G = FMath::Abs(World->GetGravityZ());
	const FRotator FieldFacing(0.0f, FieldYawDeg, 0.0f);
	const FVector  FieldForward = FieldFacing.RotateVector(FVector::ForwardVector);
	const FVector  FieldRight   = FieldFacing.RotateVector(FVector::RightVector);
	const float SideY = FMath::RandRange(-150.0f, 150.0f); // 살짝 좌우로 흔들어 매번 같은 자리로 오지 않게.
	// 높이는 **바닥 기준** — VR(Stage 원점)은 루트가 바닥, PC 는 루트가 몸 중심이라
	// 루트에 그냥 더하면 VR 에서 공이 발밑으로 날아와 글러브에 닿지 않는다.
	CurrentTrial.FeedLaunchLocation =
		FieldAnchor + FieldForward * FeedDistance + FieldRight * SideY + FVector(0, 0, 150.0f);
	CurrentTrial.FeedFlightSec      = FMath::Max(FeedFlightSec, 0.3f);

	const FVector Arrival(FieldAnchor.X, FieldAnchor.Y, CatchHeightZ()); // 가슴 높이
	const FVector ToTarget = Arrival - CurrentTrial.FeedLaunchLocation;
	const FVector Horiz(ToTarget.X, ToTarget.Y, 0.0f);
	const float VHoriz = Horiz.Size() / CurrentTrial.FeedFlightSec;
	const float VZ = (ToTarget.Z / CurrentTrial.FeedFlightSec) + 0.5f * G * CurrentTrial.FeedFlightSec;
	CurrentTrial.FeedVelocity = Horiz.GetSafeNormal() * VHoriz + FVector(0, 0, VZ);

	// 급구 발사.
	TSubclassOf<ACatchBall> Cls = BallClass;
	if (!Cls) { Cls = ACatchBall::StaticClass(); }

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ActiveBall = World->SpawnActor<ACatchBall>(Cls, CurrentTrial.FeedLaunchLocation, FRotator::ZeroRotator, Params);
	if (ActiveBall)
	{
		ActiveBall->SetGroundZ(FloorZ());
		ActiveBall->SetTrailVisible(bVR); // 헤드셋에서 공이 오는 게 보이도록.
		ActiveBall->Launch(CurrentTrial.FeedVelocity);
	}

	Phase = EThrowPhase::Feed;
	CurrentPower = 0.0f;
	bCharging = false;
	bHasResult = false;
	CatchTimeSec = -1.0f;
	bCleanCatch = false;
	PendingTransferSec = -1.0f;
	PendingReleaseSpeedCms = 0.0f;

	if (BallInHandMesh) { BallInHandMesh->SetVisibility(false); }
}

void AThrowPawn::CatchFeed(bool bClean)
{
	if (Phase != EThrowPhase::Feed) { return; }

	UWorld* World = GetWorld();
	CatchTimeSec = World ? World->GetTimeSeconds() : 0.0f;
	bCleanCatch  = bClean;

	if (IsValid(ActiveBall))
	{
		ActiveBall->Destroy();
	}
	ActiveBall = nullptr;

	Phase = EThrowPhase::Ready;
	CurrentPower = 0.0f;
	bCharging = false;
	// 잡자마자 손 움직임이 송구로 오인되지 않게 짧게 잠근다.
	// 길이는 PostCatchThrowLockSec — 전환 시간이 측정 지표라 바닥을 낮게 잡는다(헤더 주석 참고).
	ThrowCooldown = PostCatchThrowLockSec;

	// 손에 든 공은 VR 에서만 보인다 (PC 는 1인칭 손이 없다).
	if (BallInHandMesh) { BallInHandMesh->SetVisibility(bVR); }
}

void AThrowPawn::OnSpacePressed()
{
	if (bSessionOver || bWaitingNext) { return; }

	// 급구 대기 — 타이밍이 맞으면 포구.
	if (Phase == EThrowPhase::Feed)
	{
		if (!IsValid(ActiveBall)) { return; }
		const float Elapsed = ActiveBall->GetElapsedTime();
		if (FMath::Abs(Elapsed - CurrentTrial.FeedFlightSec) <= FeedCatchWindowSec)
		{
			CatchFeed(true);
		}
		return; // 빗나간 타이밍은 무시 — 공이 지나가면 자동으로 fumble 처리된다.
	}

	// 송구 준비 — 파워 충전 시작.
	if (Phase == EThrowPhase::Ready)
	{
		bCharging = true;
		CurrentPower = 0.0f;
	}
}

void AThrowPawn::OnSpaceReleased()
{
	if (!bCharging) { return; }
	bCharging = false;
	ThrowBall(CurrentPower);
}

void AThrowPawn::ThrowBall(float Power)
{
	UWorld* World = GetWorld();
	if (!World || Phase != EThrowPhase::Ready) { return; }

	TSubclassOf<ACatchBall> Cls = BallClass;
	if (!Cls) { Cls = ACatchBall::StaticClass(); }

	const FVector Velocity = PowerToVelocity(Power);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ActiveBall = World->SpawnActor<ACatchBall>(Cls, CurrentTrial.ThrowOrigin, FRotator::ZeroRotator, Params);
	if (!ActiveBall)
	{
		return;
	}
	// 착지면을 폰 바닥에 맞추고, 궤적 선을 켠다 — "공이 어디로 나갔는지" 가 보여야
	// 다음 번에 파워를 조절할 수 있다 (VR 에선 작은 공만으론 안 보인다).
	ActiveBall->SetGroundZ(FloorZ());
	ActiveBall->SetTrailVisible(true);
	ActiveBall->Launch(Velocity);
	LastBallLoc = CurrentTrial.ThrowOrigin;

	// 릴리스 시점에 확정되는 측정값 — 착지 판정 때 결과에 합친다.
	PendingPower           = Power;
	PendingReleaseSpeedCms = Velocity.Size();
	PendingTransferSec     = (CatchTimeSec >= 0.0f) ? (World->GetTimeSeconds() - CatchTimeSec) : -1.0f;

	Phase = EThrowPhase::InFlight;
	if (BallInHandMesh) { BallInHandMesh->SetVisibility(false); }
}

void AThrowPawn::FinishThrow(const FThrowResult& Result)
{
	LastResult = Result;
	bHasResult = true;

	SessionResults.Add(Result);

	// 시도 1건 = 기록 1건 (수비는 성공/실패 + 원시 측정값만 남긴다 — 3축 모델 없음).
	if (UModeManager* MM = GetGameInstance() ? GetGameInstance()->GetSubsystem<UModeManager>() : nullptr)
	{
		TMap<FName, float> Details;
		Details.Add(TEXT("TargetBase"),      static_cast<float>(BaseIndexOf(Result.TargetBase)));
		Details.Add(TEXT("DistanceErrorCm"), Result.DistanceError);
		Details.Add(TEXT("ReleaseKmh"),      Result.ReleaseSpeedKmh);
		Details.Add(TEXT("TransferSec"),     Result.TransferTimeSec);
		MM->RecordResult(UScoringService::ScoreDefenseAttempt(Result.IsSuccess(), Details));
	}

	const int32 Bi = BaseIndexOf(Result.TargetBase);
	++BaseAttempts[Bi];
	if (Result.IsSuccess())
	{
		++BaseSuccess[Bi];
		++SuccessCount;
	}

	if (IsValid(ActiveBall))
	{
		ActiveBall->Destroy();
	}
	ActiveBall = nullptr;

	Phase = EThrowPhase::Done;
	++ThrowIndex;

	if (ThrowIndex >= TotalThrows)
	{
		EndSession();
	}
	else
	{
		bWaitingNext = true;
		IntervalTimer = IntervalBetweenThrows;
	}
}

void AThrowPawn::EndSession()
{
	bSessionOver = true;
	RequestThrowFeedback();
}

// ── 측정 지표 집계 ──

void AThrowPawn::GetBaseStats(EBaseType Base, int32& OutAttempt, int32& OutSuccess) const
{
	const int32 Bi = BaseIndexOf(Base);
	OutAttempt = BaseAttempts[Bi];
	OutSuccess = BaseSuccess[Bi];
}

float AThrowPawn::GetAverageReleaseKmh() const
{
	if (SessionResults.Num() == 0) { return 0.0f; }
	float Sum = 0.0f;
	for (const FThrowResult& R : SessionResults) { Sum += R.ReleaseSpeedKmh; }
	return Sum / SessionResults.Num();
}

float AThrowPawn::GetAverageTransferSec() const
{
	float Sum = 0.0f;
	int32 N = 0;
	for (const FThrowResult& R : SessionResults)
	{
		if (R.bCleanCatch && R.TransferTimeSec >= 0.0f) { Sum += R.TransferTimeSec; ++N; }
	}
	return (N > 0) ? (Sum / N) : -1.0f; // -1 = 정상 포구한 시행이 없음
}

float AThrowPawn::GetLiveTransferTime() const
{
	if (Phase != EThrowPhase::Ready || CatchTimeSec < 0.0f) { return -1.0f; }
	const UWorld* World = GetWorld();
	return World ? (World->GetTimeSeconds() - CatchTimeSec) : -1.0f;
}

FString AThrowPawn::GetLastMetricsLine() const
{
	if (!bHasResult) { return FString(); }

	const FString Transfer = (LastResult.TransferTimeSec >= 0.0f)
		? FString::Printf(TEXT("%.2fs"), LastResult.TransferTimeSec)
		: FString(TEXT("-- (fumble)"));

	return FString::Printf(TEXT("to %s   miss %.0fcm   %.0f km/h   transfer %s"),
		*BaseName(LastResult.TargetBase), LastResult.DistanceError,
		LastResult.ReleaseSpeedKmh, *Transfer);
}

// ── AI 운동 추천 ──

FWeaknessReport AThrowPawn::BuildThrowReport() const
{
	FWeaknessReport R;
	R.Mode = EGameModeId::Defense;
	R.AttemptCount = SessionResults.Num();
	R.bUncalibrated = true; // 구속·전환 기준값은 아직 실측 미보정.

	if (SessionResults.Num() == 0)
	{
		return R; // bValid=false
	}

	const int32 N = SessionResults.Num();
	int32 OnTarget = 0, CleanCatches = 0;
	float SumDist = 0.0f, SumKmh = 0.0f, SumTransfer = 0.0f;
	int32 TransferN = 0;

	for (const FThrowResult& Res : SessionResults)
	{
		if (Res.IsSuccess()) { ++OnTarget; }
		SumDist += Res.DistanceError;
		SumKmh  += Res.ReleaseSpeedKmh;
		if (Res.bCleanCatch)
		{
			++CleanCatches;
			if (Res.TransferTimeSec >= 0.0f) { SumTransfer += Res.TransferTimeSec; ++TransferN; }
		}
	}

	R.ContactCount = OnTarget;
	R.bValid = true;

	const float OnTargetRate = static_cast<float>(OnTarget) / N;
	const float AvgDist      = SumDist / N;                       // cm
	const float AvgKmh       = SumKmh / N;                        // km/h
	const float AvgTransfer  = (TransferN > 0) ? (SumTransfer / TransferN) : -1.0f; // s

	auto AddWeakness = [&R](EWeaknessAxis Axis, float Score, const FString& Evidence)
	{
		FWeakness W;
		W.Axis = Axis;
		W.Score = FMath::Clamp(Score, 0.0f, 1.0f);
		W.Severity = 1.0f - W.Score;
		W.Evidence = Evidence;
		// 문턱은 타격·포구와 같은 모드 공통 상수를 쓴다.
		if (W.Severity >= UWeaknessDetector::MinReportSeverity) { R.Weaknesses.Add(W); }
	};

	// ① 정확도: 도달률이 주(0.6), 빗나간 거리(0.4). 기준 거리는 zone 반경의 4배.
	const float DistScore = FMath::Clamp(1.0f - (AvgDist / FMath::Max(HitRadius * 4.0f, 1.0f)), 0.0f, 1.0f);
	AddWeakness(EWeaknessAxis::ThrowAccuracy, OnTargetRate * 0.6f + DistScore * 0.4f,
		FString::Printf(TEXT("on target %d/%d, average miss %.0f cm (zone radius %.0f cm)"),
			OnTarget, N, AvgDist, HitRadius));

	// ② 구속: 목표 구속 대비 비율.
	AddWeakness(EWeaknessAxis::ArmStrength,
		FMath::Clamp(AvgKmh / FMath::Max(TargetReleaseKmh, 1.0f), 0.0f, 1.0f),
		FString::Printf(TEXT("average release %.0f km/h (target %.0f km/h)"), AvgKmh, TargetReleaseKmh));

	// ③ 전환 시간: 목표 시간 / 실제 시간. 정상 포구가 하나도 없으면 축을 만들지 않는다
	//    (0 을 넣으면 "전환이 완벽하다"로 뒤집혀 읽힌다).
	if (AvgTransfer > 0.0f)
	{
		AddWeakness(EWeaknessAxis::TransferQuick,
			FMath::Clamp(TargetTransferSec / AvgTransfer, 0.0f, 1.0f),
			FString::Printf(TEXT("average catch-to-throw %.2f s (target %.2f s), clean catches %d/%d"),
				AvgTransfer, TargetTransferSec, CleanCatches, N));
	}

	// 베이스별 정확도 — 축이 아니라 노트로. "홈 송구만 다 짧다" 같은 편중을 코칭이 짚을 수 있다.
	{
		FString ByBase;
		const EBaseType Bases[NumBases] =
			{ EBaseType::First, EBaseType::Second, EBaseType::Third, EBaseType::Home };
		for (int32 i = 0; i < NumBases; ++i)
		{
			int32 A = 0, S = 0;
			GetBaseStats(Bases[i], A, S);
			if (A <= 0) { continue; }
			if (!ByBase.IsEmpty()) { ByBase += TEXT(", "); }
			ByBase += FString::Printf(TEXT("%s %d/%d"), *BaseName(Bases[i]), S, A);
		}
		if (!ByBase.IsEmpty())
		{
			R.Notes.Add(FString::Printf(TEXT("Accuracy by target base: %s"), *ByBase));
		}
	}
	if (AvgTransfer > 0.0f)
	{
		R.Notes.Add(FString::Printf(TEXT("Clean catches %d/%d before the throw"), CleanCatches, N));
	}
	else
	{
		R.Notes.Add(TEXT("No clean catch this session - transfer time not measured"));
	}

	R.Weaknesses.Sort([](const FWeakness& A, const FWeakness& B) { return A.Severity > B.Severity; });
	return R;
}

void AThrowPawn::FlushSessionToSave()
{
	UModeManager* MM = GetGameInstance() ? GetGameInstance()->GetSubsystem<UModeManager>() : nullptr;
	if (!MM)
	{
		return;
	}
	MM->FinalizeSession(
		UScoringService::ScoreDefenseSession(SuccessCount, SessionResults.Num()),
		BuildThrowReport());
}

void AThrowPawn::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	FlushSessionToSave();

	if (IsValid(ActiveBall))
	{
		ActiveBall->Destroy();
		ActiveBall = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void AThrowPawn::RequestThrowFeedback()
{
	const FWeaknessReport Report = BuildThrowReport();

	// 과거 "Throw" 세션만 골라 만성 추세를 본다 (종목 필터 없이 보면 포구·백업 축이 섞인다).
	FChronicWeaknessReport Chronic;
	if (UModeManager* MM = GetGameInstance() ? GetGameInstance()->GetSubsystem<UModeManager>() : nullptr)
	{
		Chronic = UWeaknessDetector::AnalyzeTrend(
			MM->GetHistory(), EGameModeId::Defense, 5, UModeManager::GetDefenseDrillIdName(1));
	}
	LastDrills = UDrillCatalog::RecommendWithHistory(Report, Chronic, 3);

	if (FeedbackService && FeedbackService->IsConfigured())
	{
		CoachingText = TEXT("Requesting AI coaching...");
		bAwaitingCoaching = true;
		FeedbackService->RequestThrowCoaching(Report, LastDrills);
	}
	else
	{
		bAwaitingCoaching = false;
		CoachingText = TEXT("AI coaching not configured (Config/Secrets.ini)");
	}

	UE_LOG(LogMotionBase, Log, TEXT("[Throw] 코칭 요청: 명중 %d/%d, 평균 %.0f km/h, 전환 %.2fs, 약점 %d개"),
		SuccessCount, TotalThrows, GetAverageReleaseKmh(), GetAverageTransferSec(), Report.Weaknesses.Num());
}

void AThrowPawn::HandleCoachingReady(bool bSuccess, const FString& Text)
{
	bAwaitingCoaching = false;
	CoachingText = Text;
	UE_LOG(LogMotionBase, Log, TEXT("[Throw] AI 코칭 %s: %s"),
		bSuccess ? TEXT("수신") : TEXT("실패"), *Text);
}

// ── 파워 ↔ 거리 ──

float AThrowPawn::FloorZ() const
{
	// VR: SetTrackingOrigin(Stage) → 바닥이 곧 폰 루트 Z. PC: 루트가 캡슐 중심이라 반높이 아래.
	return bVR ? HomeLocation.Z : (HomeLocation.Z - 88.0f);
}

float AThrowPawn::CatchHeightZ() const
{
	// 급구가 도착해야 할 높이 = 글러브가 닿는 가슴 높이 (바닥 기준 130cm).
	return FloorZ() + 130.0f;
}

float AThrowPawn::ThrowHandZ() const
{
	// 송구 출발 = 손 높이 (바닥 기준 140cm). 어깨보다 조금 아래.
	return FloorZ() + 140.0f;
}

void AThrowPawn::DrawPredictedArc(float Power) const
{
	UWorld* World = GetWorld();
	if (!World || Power <= KINDA_SMALL_NUMBER) { return; }

	// 지금 파워로 던지면 그리는 포물선을 미리 보여준다 —
	// "얼마나 세게 휘둘러야 저기까지 가는지"를 던지기 전에 눈으로 맞출 수 있게.
	const FVector V0 = PowerToVelocity(Power);
	// 출발점은 이번 시행의 송구 원점(=내가 선 자리의 손 높이). 폰 루트를 쓰면 VR 에서
	// 예측선이 내 몸이 아니라 트래킹 원점에서 뻗어 나가 궤적이 어긋나 보인다.
	const FVector P0 = CurrentTrial.ThrowOrigin;
	const float G  = FMath::Abs(World->GetGravityZ());
	const float Ground = FloorZ();

	constexpr int32 Steps = 24;
	constexpr float StepSec = 0.12f;

	FVector Prev = P0;
	for (int32 i = 1; i <= Steps; ++i)
	{
		const float T = i * StepSec;
		const FVector P = P0 + V0 * T - FVector(0, 0, 0.5f * G * T * T);
		if (P.Z <= Ground)
		{
			// 착지 예상 지점에 원을 찍고 끝낸다.
			const FVector Land(P.X, P.Y, Ground + 2.0f);
			DrawDebugLine(World, Prev, Land, FColor(120, 200, 255), false, -1.0f, 0, 1.5f);
			DrawDebugCircle(World, Land, 60.0f, 20, FColor(120, 200, 255), false, -1.0f, 0, 2.0f,
				FVector(1, 0, 0), FVector(0, 1, 0), false);
			return;
		}
		DrawDebugLine(World, Prev, P, FColor(120, 200, 255), false, -1.0f, 0, 1.5f);
		Prev = P;
	}
}

FVector AThrowPawn::PowerToVelocity(float Power) const
{
	Power = FMath::Clamp(Power, 0.0f, 1.0f);

	// 방향: 목표 베이스로 자동 조준 (수평 방향만).
	const FVector Flat = CurrentTrial.TargetLocation - CurrentTrial.ThrowOrigin;
	const FVector Dir = FVector(Flat.X, Flat.Y, 0.0f).GetSafeNormal();

	// 파워 1.0 → MaxThrowRange 까지 가는 45도 발사로 환산.
	//
	// ⚠️ 릴리스(ThrowOrigin=손 높이 140cm)가 착지면(TargetLocation=바닥)보다 높다.
	// 평지 45도 공식 R=v²/g 를 그대로 쓰면, 실제 탄도는 그 높이차만큼 더 오래 낙하하며
	// 수평으로도 더 멀리 나가 목표를 넘긴다 — 그래서 "정답 파워"가 실제 필요한 값보다
	// 과대했다(재발 이력 있는 버그). 높이차 h 를 반영한 45도 사거리 공식을 역산해서 쓴다:
	//   R = u·(u + √(u²+2gh)) / g   (u = 수평·수직 성분, 45도라 v = u√2 )
	//   → v = R·√(g / (R+h))   (h=0 이면 원래 평지 공식 v=√(Rg) 와 정확히 일치)
	const float G = FMath::Abs(GetWorld()->GetGravityZ());
	const float Range = FMath::Max(Power * MaxThrowRange, 1.0f);
	const float HeightDropCm = FMath::Max(0.0f, CurrentTrial.ThrowOrigin.Z - CurrentTrial.TargetLocation.Z);
	const float Speed = Range * FMath::Sqrt(G / (Range + HeightDropCm));

	// 45도: 수평·수직 성분 동일.
	const float Comp = Speed / FMath::Sqrt(2.0f);
	return Dir * Comp + FVector(0, 0, Comp);
}

float AThrowPawn::DistanceToIdealPower(float Distance) const
{
	// PowerToVelocity 의 역: 사거리 = Power * MaxThrowRange 이므로
	// 정답 파워 = 거리 / 최대사거리.
	return FMath::Clamp(Distance / MaxThrowRange, 0.0f, 1.0f);
}

// ── 매 프레임 ──

void AThrowPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (ThrowCooldown > 0.0f)
	{
		ThrowCooldown = FMath::Max(0.0f, ThrowCooldown - DeltaSeconds);
	}

	// VR: 컨트롤러 던지기/포구 동작 인식.
	if (bVR)
	{
		// 패널을 플레이어 정면에 고정 배치(swimming 제거·이질감 제거).
		if (VrPanel && Camera)
		{
			VrPanel->UpdateComfortAnchor(Camera, UVRInfoPanel::DefaultDistanceCm, 70.0f, /*RecenterDeg=*/55.0f);
		}

		// 뒤로가기 — 컨트롤러를 위로 들고 유지하면 모드 선택으로 복귀.
		if (ThrowController)
		{
			// 시행이 도는 동안엔 동결 — 급구를 받으려 손을 들거나 와인드업으로 팔이 올라간
			// 자세를 나가기로 오인하지 않게. 판정이 끝난 뒤(Done) 틈에서만 진행이 쌓인다.
			const bool bGestureAllowed = (Phase == EThrowPhase::Done) || bSessionOver;
			bool bExit = false;
			ExitGesture.Update(ThrowController->GetForwardVector(),
				ThrowController->IsTracked(), bGestureAllowed, DeltaSeconds, bExit);
			if (bExit)
			{
				ReturnToModeSelect();
				return; // 폰이 곧 교체된다 — 이 프레임 종료.
			}

			// 두 번째 출구 — '뒤로' 카드를 컨트롤러로 겨눠 유지하면 나간다.
			// 와인드업/던지기 도중엔 겨눔을 막아(팔 동작으로 오발동 방지) 준다.
			if (VrPanel)
			{
				const bool bAim = ThrowController->IsTracked() && bGestureAllowed;
				if (VrPanel->UpdateBackDwell(ThrowController->GetComponentLocation(),
					ThrowController->GetForwardVector(), bAim, /*DwellSec=*/1.6f, /*AngleDeg=*/8.0f, DeltaSeconds))
				{
					ReturnToModeSelect();
					return;
				}
			}
		}

		TickVRFeedCatch();
		TickVRThrow(DeltaSeconds);
	}

	// 급구가 지나갔는데 못 잡았으면 fumble 로 넘긴다 (전환 시간은 미측정).
	// ⚠️ 시행 대기 중(bWaitingNext)에는 건드리지 않는다 — 초기 Phase 가 Feed 라, 첫 구가
	//    나오기도 전에 "공이 없다 = 놓쳤다"로 판정해 버린다.
	if (Phase == EThrowPhase::Feed && !bWaitingNext && !bSessionOver)
	{
		if (!IsValid(ActiveBall))
		{
			CatchFeed(false); // 공이 사라짐 = 놓침
		}
		else if (ActiveBall->GetElapsedTime() > CurrentTrial.FeedFlightSec + FeedCatchWindowSec)
		{
			CatchFeed(false);
		}
	}

	// 파워 충전 (키보드 모드).
	if (bCharging)
	{
		CurrentPower = FMath::Clamp(CurrentPower + DeltaSeconds / ChargeTime, 0.0f, 1.0f);
	}

	// 송구한 공이 착지했는지 확인 → 판정.
	if (Phase == EThrowPhase::InFlight)
	{
		// 공의 착지면 — 공에도 같은 값을 SetGroundZ 로 넘겼으므로 기준이 일치한다.
		// (예전엔 공은 Z=0, 판정은 발밑을 봐서 폰이 Z=0 이 아니면 서로 어긋났다.)
		const float GroundZ = FloorZ() + 5.0f;

		if (IsValid(ActiveBall))
		{
			LastBallLoc = ActiveBall->GetActorLocation();
		}

		if (!IsValid(ActiveBall) || LastBallLoc.Z <= GroundZ)
		{
			FThrowResult Result = FThrowJudge::Judge(
				LastBallLoc,
				CurrentTrial.TargetLocation,
				CurrentTrial.ThrowOrigin,
				CurrentTrial.HitRadius,
				PendingPower);
			FThrowJudge::FillMotionMetrics(Result, CurrentTrial.TargetBase,
				PendingReleaseSpeedCms, PendingTransferSec, bCleanCatch);
			FinishThrow(Result);
		}
	}

	// 다음 시행 대기.
	if (bWaitingNext && !bSessionOver)
	{
		IntervalTimer -= DeltaSeconds;
		if (IntervalTimer <= 0.0f)
		{
			bWaitingNext = false;
			SpawnNextTrial();
		}
	}

	// 공을 들고 있는 동안 예상 궤적을 보여준다 (지금 파워로 던지면 어디에 떨어지는지).
	// 정답 파워(IdealPower)의 궤적도 함께 그려 "얼마나 더 세게" 를 눈으로 비교하게 한다.
	if (Phase == EThrowPhase::Ready && !bSessionOver)
	{
		DrawPredictedArc(CurrentTrial.IdealPower);   // 목표(연한 파랑)
		if (CurrentPower > 0.01f)
		{
			DrawPredictedArc(CurrentPower);          // 지금 파워
		}
	}

	// ── 베이스 마커 — 네 베이스를 모두 그리고 목표만 강조한다 ──
	// 그라운드 방향이 확정되기 전(첫 시행 대기 중)에는 그리지 않는다 — 임시 방향으로 깔았다가
	// 첫 구에서 통째로 회전하면 "베이스가 순간이동했다"로 보인다.
	if (bFieldAnchored && !bSessionOver && GetWorld())
	{
		const EBaseType Bases[NumBases] =
			{ EBaseType::First, EBaseType::Second, EBaseType::Third, EBaseType::Home };
		for (int32 i = 0; i < NumBases; ++i)
		{
			const bool bTarget = (Bases[i] == CurrentTrial.TargetBase);
			const FVector T = BaseLocation(Bases[i]);
			const FColor Col = bTarget ? FColor(255, 190, 90) : FColor(90, 96, 110);

			// 베이스 판 (마름모 대신 사각 박스로 단순 표시).
			DrawDebugBox(GetWorld(), T + FVector(0, 0, 3.0f), FVector(45.0f, 45.0f, 3.0f),
				FQuat::Identity, Col, false, -1.0f, 0, bTarget ? 3.0f : 1.5f);

			if (bTarget)
			{
				// 받는 사람 + 목표 zone.
				DrawDebugCapsule(GetWorld(), T + FVector(0, 0, 88.0f), 88.0f, 34.0f,
					FQuat::Identity, FColor::Red, false, -1.0f, 0, 3.0f);
				DrawDebugCircle(GetWorld(), T + FVector(0, 0, 2.0f), CurrentTrial.HitRadius, 32,
					FColor::Yellow, false, -1.0f, 0, 3.0f, FVector(1, 0, 0), FVector(0, 1, 0), false);
			}
		}
	}

	// VR 상태 패널 갱신.
	if (bVR)
	{
		RefreshVrPanel();
	}
}

void AThrowPawn::RefreshVrPanel()
{
	if (!VrPanel) { return; }

	// 세션 종료 → AI 운동 추천 오버레이.
	if (bSessionOver)
	{
		VrPanel->SetTitle(
			FString::Printf(TEXT("AI exercise tips    (On-target %d / %d)"), SuccessCount, TotalThrows),
			FColor(150, 210, 255));

		// ⚠️ 컴팩트 상태 패널(SetStatusCompact)은 행이 4줄을 넘으면 푸터·힌트와 겹친다.
		//    요약 1줄 + 코칭 2줄 + 드릴 1개로 압축. 전체 리포트는 데스크톱 결과 화면이 담당.
		constexpr int32 MaxContentRows = 4;
		int32 Row = 0;
		const float AvgT = GetAverageTransferSec();
		VrPanel->SetRow(Row++, FString::Printf(TEXT("avg %.0f km/h    transfer %s"),
			GetAverageReleaseKmh(),
			(AvgT >= 0.0f) ? *FString::Printf(TEXT("%.2fs"), AvgT) : TEXT("--")),
			FColor(150, 200, 255));

		for (const FString& L : ThrowWrap(CoachingText, 30, 2))
		{
			if (Row >= MaxContentRows) { break; }
			VrPanel->SetRow(Row++, L, FColor(228, 233, 244));
		}
		for (const FTrainingDrill& D : LastDrills)
		{
			if (Row >= MaxContentRows) { break; }
			VrPanel->SetRow(Row++, FString::Printf(TEXT("- %s"), *D.Name), FColor(255, 200, 120));
		}
		VrPanel->HideRowsFrom(Row);

		VrPanel->SetFooter(bAwaitingCoaching ? TEXT("Waiting for AI...") : TEXT("Recommended exercises"),
			FColor(150, 156, 168));
		VrPanel->SetHint(TEXT("Raise controller = menu"), FColor(110, 116, 128));
		return;
	}

	// 제목: 진행 + 목표 베이스 (지정된 곳이 항상 눈에 보이게).
	VrPanel->SetTitle(
		FString::Printf(TEXT("Throw  %d / %d      ->  %s      On-target %d"),
			GetThrowNumber(), GetTotalThrows(), *BaseName(CurrentTrial.TargetBase), GetSuccessCount()),
		FColor(228, 233, 244));

	// 행0: 추적 경고 > 단계별 안내 / 파워 게이지 (경고가 최우선).
	if (bControllerLost)
	{
		// 추적이 끊긴 동안에는 던지기가 물리적으로 인식되지 않는다. 원인을 알려주지 않으면
		// 플레이어는 계속 허공에 던지면서 게임이 멈춘 줄 안다.
		VrPanel->SetRow(0, TEXT("Controller not tracked - move it into view"), FColor(255, 120, 120));
	}
	else if (Phase == EThrowPhase::Feed)
	{
		VrPanel->SetRow(0, TEXT("Catch the feed first"), FColor(255, 190, 90));
	}
	else
	{
		const int32 Cells = 10;
		const int32 Filled = FMath::Clamp(FMath::RoundToInt(CurrentPower * Cells), 0, Cells);
		const FString Bar = FString::Printf(TEXT("Power [%s%s] %3.0f%%"),
			*FString::ChrN(Filled, TEXT('=')), *FString::ChrN(Cells - Filled, TEXT('.')),
			CurrentPower * 100.0f);
		VrPanel->SetRow(0, Bar, bCharging ? FColor(255, 190, 90) : FColor(150, 200, 255));
	}

	// 행1: 전환 시계 (잡은 뒤 흐르는 시간) — 늦으면 붉게.
	const float Live = GetLiveTransferTime();
	if (Live >= 0.0f)
	{
		VrPanel->SetRow(1, FString::Printf(TEXT("transfer  %.2fs"), Live),
			(Live > TargetTransferSec) ? FColor(230, 130, 90) : FColor(90, 220, 110));
		VrPanel->HideRowsFrom(2);
	}
	else
	{
		VrPanel->HideRowsFrom(1);
	}

	// 푸터: 직전 결과 + 측정값, 없으면 조준 안내.
	FString Outcome; FLinearColor OColor;
	if (GetLastOutcomeText(Outcome, OColor))
	{
		const FString Metrics = GetLastMetricsLine();
		VrPanel->SetFooter(Metrics.IsEmpty() ? Outcome : (Outcome + TEXT("   ") + Metrics),
			OColor.ToFColor(true));
	}
	else
	{
		VrPanel->SetFooter(TEXT("Auto-aimed at the called base - just match the power (distance)"),
			FColor(150, 156, 168));
	}

	// 힌트: 조작 안내. 컨트롤러를 위로 드는 중이면 나가기 진행바를 보여준다.
	if (ExitGesture.IsHolding())
	{
		VrPanel->SetHint(FString::Printf(TEXT("Raise controller to exit  %s"), *ExitGesture.ProgressBar()),
			FColor(255, 190, 90));
	}
	else
	{
		VrPanel->SetHint(TEXT("Reach the glove to the feed, then fling forward to throw   ·   raise controller = exit"),
			FColor(110, 116, 128));
	}
}

void AThrowPawn::TickVRFeedCatch()
{
	if (Phase != EThrowPhase::Feed || !IsValid(ActiveBall) || !ThrowController)
	{
		return;
	}

	const float Dist = FVector::Dist(ActiveBall->GetActorLocation(), ThrowController->GetComponentLocation());
	if (Dist <= FeedCatchRadius)
	{
		CatchFeed(true);
	}
}

void AThrowPawn::TickVRThrow(float DeltaSeconds)
{
	if (!ThrowController)
	{
		return;
	}

	// ── 추적 가드 ──
	// 추적이 끊기면 컴포넌트 위치가 마지막 값에 고정된다 → 손 속도가 0 으로 잡혀
	// 아무리 던져도 트리거를 못 넘고, 화면엔 아무 일도 안 일어난다("고장난 줄 안다").
	// 그래서 ① 진행 중이던 동작을 버리고 ② 패널에 알릴 플래그를 세운다.
	//
	// ⚠️ bHasPrevControllerLoc 도 반드시 내린다. 추적이 다른 위치에서 복귀하면
	//    그 순간의 좌표 점프가 그대로 거대한 가짜 속도가 되어 의도치 않은 만루 송구가 나간다.
	if (!ThrowController->IsTracked())
	{
		bControllerLost       = true;
		bHasPrevControllerLoc = false;
		bThrowMotionActive    = false;
		ThrowPeakSpeedCms     = 0.0f;
		ThrowMotionSec        = 0.0f;
		return;
	}
	bControllerLost = false;

	// 컨트롤러 속도 (cm/s) = 위치 변화량 / dt.
	const FVector Loc = ThrowController->GetComponentLocation();
	float Speed = 0.0f;
	if (bHasPrevControllerLoc && DeltaSeconds > KINDA_SMALL_NUMBER)
	{
		Speed = FVector::Dist(Loc, PrevControllerLoc) / DeltaSeconds;
	}
	PrevControllerLoc = Loc;
	bHasPrevControllerLoc = true;

	// 손 속도 → 파워(0~1).
	auto ToPower = [this](float S)
	{
		return FMath::Clamp((S - MinThrowSpeedCms) / FMath::Max(MaxThrowSpeedCms - MinThrowSpeedCms, 1.0f),
			0.0f, 1.0f);
	};

	// 공을 들고 있을 때(Ready)만 송구로 인식한다 — 급구를 잡기 전 손 흔들림은 무시.
	const bool bReady = (Phase == EThrowPhase::Ready) && !bSessionOver && !bWaitingNext;
	if (!bReady || ThrowCooldown > 0.0f)
	{
		bThrowMotionActive = false;
		ThrowPeakSpeedCms  = 0.0f;
		ThrowMotionSec     = 0.0f;
		return;
	}

	// ── 던지기 동작 추적 ──
	// 릴리스는 "손이 가장 빨랐던 순간"이다. 트리거를 넘자마자 발사하면 그 지점은 팔을 막
	// 뻗기 시작한 곳이라 파워가 0 에 가까워 공이 안 나간다(예전 동작). 그래서
	//   ① 트리거를 넘으면 동작 시작으로 보고 피크 속도를 기록하다가
	//   ② 피크의 ReleaseDecelRatio 아래로 감속하면 = 손을 놓은 순간 → **피크값**으로 발사한다.
	if (!bThrowMotionActive)
	{
		if (Speed >= ThrowTriggerSpeedCms)
		{
			bThrowMotionActive = true;
			ThrowPeakSpeedCms  = Speed;
			ThrowMotionSec     = 0.0f;
		}
		// 동작 전에는 게이지에 현재 손 속도를 미리보기로 보여준다.
		CurrentPower = ToPower(Speed);
		return;
	}

	ThrowMotionSec += DeltaSeconds;
	ThrowPeakSpeedCms = FMath::Max(ThrowPeakSpeedCms, Speed);

	// 게이지는 지금까지의 피크(=실제 발사될 파워)를 보여준다.
	CurrentPower = ToPower(ThrowPeakSpeedCms);

	const bool bDecelerated = (Speed <= ThrowPeakSpeedCms * ReleaseDecelRatio);
	const bool bTimedOut    = (ThrowMotionSec >= MaxThrowMotionSec);

	if (bDecelerated || bTimedOut)
	{
		const float Power = ToPower(ThrowPeakSpeedCms);
		UE_LOG(LogMotionBase, Log, TEXT("[Throw] 릴리스: peak %.0f cm/s → power %.2f (%s)"),
			ThrowPeakSpeedCms, Power, bDecelerated ? TEXT("감속") : TEXT("시간초과"));

		ThrowBall(Power);
		ThrowCooldown = 0.6f;

		bThrowMotionActive = false;
		ThrowPeakSpeedCms  = 0.0f;
		ThrowMotionSec     = 0.0f;
	}
}

void AThrowPawn::ReturnToModeSelect()
{
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->ClientSetHUD(AModeSelectHUD::StaticClass());
	}
	if (AMotionBaseGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AMotionBaseGameMode>() : nullptr)
	{
		GM->ReturnToModeSelect();
	}
}

bool AThrowPawn::GetLastOutcomeText(FString& OutText, FLinearColor& OutColor) const
{
	if (bSessionOver)
	{
		OutText  = FString::Printf(TEXT("Session over!  On-target %d / %d"), SuccessCount, TotalThrows);
		OutColor = FLinearColor(0.40f, 0.85f, 0.45f, 1.0f);
		return true;
	}
	if (!bHasResult || Phase == EThrowPhase::InFlight)
	{
		return false;
	}

	switch (LastResult.Outcome)
	{
	case EThrowOutcome::Ontarget:
		OutText = TEXT("On target!");        OutColor = FLinearColor(0.40f, 0.85f, 0.45f, 1.0f); return true;
	case EThrowOutcome::Short:
		OutText = TEXT("Short - throw harder"); OutColor = FLinearColor(0.95f, 0.55f, 0.30f, 1.0f); return true;
	case EThrowOutcome::Over:
		OutText = TEXT("Long - ease up");    OutColor = FLinearColor(0.95f, 0.55f, 0.30f, 1.0f); return true;
	default:
		return false;
	}
}
