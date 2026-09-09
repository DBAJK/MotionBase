#include "Core/Defense/Backup/BackupPawn.h"
#include "Core/Defense/Backup/BackupPlaybook.h"
#include "Core/Defense/Backup/BackupJudge.h"
#include "Core/Defense/Backup/BackupHUD.h"
#include "Core/Defense/Backup/BackupGameState.h"
#include "Core/Defense/CatchBall/CatchBall.h"
#include "Core/Defense/Backup/FielderMarker.h"
#include "Core/MotionBaseGameMode.h"
#include "Core/ModeManager.h"
#include "MotionBase.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "MotionControllerComponent.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/PlayerController.h"
#include "UI/ModeSelectHUD.h"
#include "UI/VRInfoPanel.h"
#include "AI/DrillCatalog.h"
#include "AI/AIFeedbackService.h"
#include "Analysis/WeaknessDetector.h"
#include "Scoring/ScoringService.h"

namespace
{
	// ── LLM 프롬프트용 영문 라벨 ──
	// UEnum::GetValueAsString 을 쓰지 않는 이유: "EBackupRole::CutoffRelay" 같은 식별자가
	// 그대로 나가면 코치 문장에 코드 이름이 섞인다. 야구 용어로 읽히는 문구를 따로 둔다.
	// (시스템 프롬프트가 설명하는 네 가지 job 과 표현을 맞춰 놓았다.)
	FString RoleLabel(EBackupRole Role)
	{
		switch (Role)
		{
		case EBackupRole::BackUpBase:    return TEXT("back up a base");
		case EBackupRole::CoverBase:     return TEXT("cover a base");
		case EBackupRole::CutoffRelay:   return TEXT("cut off / relay the throw");
		case EBackupRole::BackUpFielder: return TEXT("back up a fielder");
		case EBackupRole::Hold:
		default:                         return TEXT("hold your spot");
		}
	}

	FString OutcomeLabel(EBackupOutcome Outcome)
	{
		switch (Outcome)
		{
		case EBackupOutcome::Covered:    return TEXT("CORRECT");
		case EBackupOutcome::TooSlow:    return TEXT("right direction but arrived too late");
		case EBackupOutcome::WrongZone:  return TEXT("went to the wrong place");
		case EBackupOutcome::NoStart:    return TEXT("never moved");
		case EBackupOutcome::FalseStart: return TEXT("moved when the right answer was to stay put");
		default:                         return TEXT("unknown");
		}
	}

	// TextRender 는 자동 줄바꿈이 없다 → 글자수로 하드 랩 (다른 폰들과 동일 패턴).
	TArray<FString> WrapBackupPanel(const FString& In, int32 MaxCharsPerLine, int32 MaxLines)
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

	// ── 코스메틱 타구 방향/깊이 근사 (판정과 무관 — 실제 판정은 FBaseballField 의 정확한
	// 존 기하를 쓴다. 이건 순전히 "타구가 대략 그쪽으로 날아가는 걸 보여주기" 위한 것). ──
	float ApproxZoneBearingDeg(EBattedBallZone Zone)
	{
		switch (Zone)
		{
		case EBattedBallZone::InfieldLeft:  return -20.0f;
		case EBattedBallZone::InfieldRight: return 20.0f;
		case EBattedBallZone::BuntFirst:    return 15.0f;
		case EBattedBallZone::BuntThird:    return -15.0f;
		case EBattedBallZone::LeftLine:     return -44.0f;
		case EBattedBallZone::LeftField:    return -30.0f;
		case EBattedBallZone::LeftCenter:   return -11.0f;
		case EBattedBallZone::RightCenter:  return 11.0f;
		case EBattedBallZone::RightField:   return 30.0f;
		case EBattedBallZone::RightLine:    return 44.0f;
		case EBattedBallZone::Backstop:     return 0.0f;
		case EBattedBallZone::InfieldMiddle:
		case EBattedBallZone::Center:
		default:                            return 0.0f;
		}
	}

	float ApproxZoneDepthCm(EBattedBallZone Zone, const FBaseballField& Field)
	{
		switch (Zone)
		{
		case EBattedBallZone::InfieldLeft:
		case EBattedBallZone::InfieldMiddle:
		case EBattedBallZone::InfieldRight:
		case EBattedBallZone::BuntFirst:
		case EBattedBallZone::BuntThird:
			return 2200.0f;
		case EBattedBallZone::Backstop:
			return 300.0f;
		default:
			return Field.OutfieldDepthCm * 0.75f; // 외야 계열은 전부 외야 깊이 근처로.
		}
	}
}

ABackupPawn::ABackupPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	Capsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	Capsule->InitCapsuleSize(34.0f, 88.0f);
	SetRootComponent(Capsule);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Capsule);
	Camera->SetRelativeLocation(FVector(0.0f, 0.0f, 70.0f));
	Camera->bUsePawnControlRotation = false;

	MoveController = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("MoveController"));
	MoveController->SetupAttachment(Capsule);
	MoveController->MotionSource = FName(TEXT("Right"));

	VrPanel = CreateDefaultSubobject<UVRInfoPanel>(TEXT("VrPanel"));
	VrPanel->SetupAttachment(Capsule);
	VrPanel->SetPlacement(UVRInfoPanel::DefaultDistanceCm, 70.0f);
}

void ABackupPawn::BeginPlay()
{
	Super::BeginPlay();

	HomeLocation = GetActorLocation();

	bVR = UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayEnabled();
	if (bVR)
	{
		UHeadMountedDisplayFunctionLibrary::SetTrackingOrigin(EHMDTrackingOrigin::Stage);
	}

	// 나가기 제스처 — 이동 게이트(트리거)를 쥔 채 팔을 크게 휘두를 수 있어 기본값보다 조인다.
	// 시행이 진행 중인 동안(Live)에는 Tick 에서 진행을 동결한다.
	ExitGesture.UpThreshold = LiveExitUpThreshold;
	ExitGesture.HoldSec     = LiveExitHoldSec;

	if (VrPanel)
	{
		VrPanel->BuildPanel();
		if (!bVR)
		{
			VrPanel->HideAll();
		}
		else
		{
			VrPanel->SetStatusCompact();
			VrPanel->ShowBackCard(TEXT("EXIT - aim here & hold"), FColor(255, 190, 90));
		}
	}

	// 포지션은 메뉴에서 ModeManager 에 지정해 둔 값을 읽는다 — ActiveDifficulty/ActiveStance 와
	// 같은 패턴(폰 스폰 시 커스텀 파라미터를 못 넘기는 구조라 세션 상태를 경유한다).
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UModeManager* MM = GI->GetSubsystem<UModeManager>())
		{
			Position = MM->GetActiveFieldPosition();
		}
	}

	PlayTable = UBackupPlaybook::BuildPlayTable();
	RuleTable = UBackupPlaybook::BuildRuleTable();
	RefillBags();

	// 동료 마커는 Position 이 확정된 뒤에 세운다 (본인 자리를 알아야 이름표만 남길 수 있다).
	SpawnFielderMarkers();

	// 첫 배치 — VR 은 HMD 포즈가 아직 정확하지 않을 수 있지만, 없는 것보단 낫다.
	// 실제 배치는 SpawnNextTrial 이 매 시행(첫 시행 포함) 다시 잡는다.
	SnapToFieldingSpot();

	FeedbackService = NewObject<UAIFeedbackService>(this);
	FeedbackService->OnFeedbackReady.AddDynamic(this, &ABackupPawn::HandleCoachingReady);
	FeedbackService->OnPlayExplanationReady.AddDynamic(this, &ABackupPawn::HandlePlayExplanationReady);

	StartSession();
}

void ABackupPawn::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	FlushSessionToSave();

	// 등록을 풀지 않으면 협동에서 "전원 보고"가 영영 안 차서 세션이 멈춘다.
	if (ABackupGameState* GS = GetOwningGameState())
	{
		GS->UnregisterPawn(this);
	}

	if (IsValid(ActiveBall))
	{
		ActiveBall->Destroy();
		ActiveBall = nullptr;
	}

	DestroyFielderMarkers();

	Super::EndPlay(EndPlayReason);
}

// ── 공 속도 조절 (측정이 아니라 연출 손잡이) ──

void ABackupPawn::SpeedDown()
{
	BallSpeedScale = FMath::Clamp(BallSpeedScale - BallSpeedStep, 0.4f, 2.0f);
}

void ABackupPawn::SpeedUp()
{
	BallSpeedScale = FMath::Clamp(BallSpeedScale + BallSpeedStep, 0.4f, 2.0f);
}

void ABackupPawn::SpawnFlavorBall()
{
	// 도루·포일은 타구 자체가 없다 — 공을 띄우지 않는다.
	if (CurrentTrial.Play.BallKind == EBattedBallKind::Steal || CurrentTrial.Play.BallKind == EBattedBallKind::WildPitch)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float BearingDeg = ApproxZoneBearingDeg(CurrentTrial.Play.BallZone);
	const float DepthCm = ApproxZoneDepthCm(CurrentTrial.Play.BallZone, Field);
	const FVector Home = Field.GetBaseLocation(EBaseType::Home);
	const FVector TargetXY = Home + FRotator(0.0f, BearingDeg, 0.0f).RotateVector(FVector(DepthCm, 0.0f, 0.0f));
	FVector Target(TargetXY.X, TargetXY.Y, Field.GroundZ);

	// 정답이 커버/백업인 시행 = "이 공은 남이 처리한다" — 그런 공을 내 발밑에 떨어뜨리면
	// 눈으로 보는 상황과 정답이 어긋난다. 타구를 실제 처리 야수 쪽으로 밀어낸다.
	// (Hold 시행은 그대로 둔다 — 그쪽은 내 쪽으로 오는 게 맞는 신호다.)
	if (!CurrentTrial.bIsHoldTrial)
	{
		Target = ClearBallFromMySpot(Target);
	}

	// 발사 지점 = 홈 플레이트 임팩트 높이 근방(타자가 방금 친 자리).
	const FVector Launch = Home + FVector(0.0f, 0.0f, 120.0f);
	const float Flight = FMath::Max(BallFlightSec / FMath::Max(BallSpeedScale, 0.1f), 0.3f);
	const float G = FMath::Abs(World->GetGravityZ());

	const FVector ToTarget = Target - Launch;
	const FVector Horiz(ToTarget.X, ToTarget.Y, 0.0f);
	const float VHoriz = Horiz.Size() / Flight;
	const float VZ = (ToTarget.Z / Flight) + 0.5f * G * Flight;
	const FVector Velocity = Horiz.GetSafeNormal() * VHoriz + FVector(0.0f, 0.0f, VZ);

	UClass* const Cls = BallClass ? BallClass.Get() : ACatchBall::StaticClass();
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ActiveBall = World->SpawnActor<ACatchBall>(Cls, Launch, FRotator::ZeroRotator, Params);
	if (ActiveBall)
	{
		ActiveBall->SetGroundZ(Field.GroundZ);
		ActiveBall->SetTrailVisible(true);
		ActiveBall->Launch(Velocity);
	}
}

FVector ABackupPawn::ClearBallFromMySpot(const FVector& Target) const
{
	const FVector MySpot = Field.GetFieldingSpot(Position);
	if (BallClearanceFromMeCm <= 0.0f || FVector::Dist2D(Target, MySpot) >= BallClearanceFromMeCm)
	{
		return Target; // 이미 충분히 떨어져 있다 — 저작한 타구 방향을 그대로 존중한다.
	}

	// 밀어낼 방향은 실제로 공을 처리하는 야수 쪽이 1순위다 — 그래야 타구가 "저 사람에게"
	// 가는 것으로 읽혀, 내가 왜 베이스로 가야 하는지가 화면만 보고도 납득된다.
	FVector Dir = FVector::ZeroVector;
	const FBackupPlay& Play = CurrentTrial.Play;
	if (Play.bHasThrowFrom && Play.ThrowFrom != Position)
	{
		Dir = (Field.GetFieldingSpot(Play.ThrowFrom) - MySpot).GetSafeNormal2D();
	}
	if (Dir.IsNearlyZero())
	{
		Dir = (Target - MySpot).GetSafeNormal2D(); // 처리 야수가 없으면 원래 방향으로 더 밀어낸다.
	}
	if (Dir.IsNearlyZero())
	{
		// 타구가 내 자리와 정확히 겹치는 극단적 경우 — 홈 반대쪽(더 깊은 쪽)으로 보낸다.
		Dir = (MySpot - Field.GetBaseLocation(EBaseType::Home)).GetSafeNormal2D();
	}

	const FVector Pushed = Field.ClampToFairTerritory(MySpot + Dir * BallClearanceFromMeCm);
	return FVector(Pushed.X, Pushed.Y, Field.GroundZ);
}

void ABackupPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindKey(EKeys::W, IE_Pressed,  this, &ABackupPawn::OnFwdPressed);
	PlayerInputComponent->BindKey(EKeys::W, IE_Released, this, &ABackupPawn::OnFwdReleased);
	PlayerInputComponent->BindKey(EKeys::S, IE_Pressed,  this, &ABackupPawn::OnBackPressed);
	PlayerInputComponent->BindKey(EKeys::S, IE_Released, this, &ABackupPawn::OnBackReleased);
	PlayerInputComponent->BindKey(EKeys::A, IE_Pressed,  this, &ABackupPawn::OnLeftPressed);
	PlayerInputComponent->BindKey(EKeys::A, IE_Released, this, &ABackupPawn::OnLeftReleased);
	PlayerInputComponent->BindKey(EKeys::D, IE_Pressed,  this, &ABackupPawn::OnRightPressed);
	PlayerInputComponent->BindKey(EKeys::D, IE_Released, this, &ABackupPawn::OnRightReleased);

	// 시야 회전 — Q/E 와 ←/→ 둘 다 받는다 (손 습관이 갈리는 자리라 양쪽 다 열어 둔다).
	// ⚠️ BindAxisKey 는 이 프로젝트에서 안 먹는다(DefaultInput.ini 에 AxisMappings 가 없음)
	//    — 그래서 WASD 와 같은 눌림/뗌 플래그 방식으로 맞춘다. 마우스 룩은 축 바인딩을
	//    거치지 않고 TickPCLook 에서 GetInputMouseDelta 로 직접 읽는다.
	PlayerInputComponent->BindKey(EKeys::Q, IE_Pressed,  this, &ABackupPawn::OnTurnLeftPressed);
	PlayerInputComponent->BindKey(EKeys::Q, IE_Released, this, &ABackupPawn::OnTurnLeftReleased);
	PlayerInputComponent->BindKey(EKeys::E, IE_Pressed,  this, &ABackupPawn::OnTurnRightPressed);
	PlayerInputComponent->BindKey(EKeys::E, IE_Released, this, &ABackupPawn::OnTurnRightReleased);
	PlayerInputComponent->BindKey(EKeys::Left,  IE_Pressed,  this, &ABackupPawn::OnTurnLeftPressed);
	PlayerInputComponent->BindKey(EKeys::Left,  IE_Released, this, &ABackupPawn::OnTurnLeftReleased);
	PlayerInputComponent->BindKey(EKeys::Right, IE_Pressed,  this, &ABackupPawn::OnTurnRightPressed);
	PlayerInputComponent->BindKey(EKeys::Right, IE_Released, this, &ABackupPawn::OnTurnRightReleased);

	PlayerInputComponent->BindKey(EKeys::M, IE_Pressed, this, &ABackupPawn::ReturnToModeSelect);
	// [R] 다시 하기 — 헤드셋 밖(데스크톱)에서도 세션을 이어 돌릴 수 있게. VR 은 종료 화면의 카드로.
	PlayerInputComponent->BindKey(EKeys::R, IE_Pressed, this, &ABackupPawn::RestartSession);

	// 타구 속도 조절 — [ 느리게 / ] 빠르게. 다음 시행부터 반영된다 (사용자가 요청한
	// "타구가 날아오는 걸 느리게 볼 수 있게"를 만족시키는 손잡이 — CatchBallPawn 과 동일 패턴).
	PlayerInputComponent->BindKey(EKeys::LeftBracket,  IE_Pressed, this, &ABackupPawn::SpeedDown);
	PlayerInputComponent->BindKey(EKeys::RightBracket, IE_Pressed, this, &ABackupPawn::SpeedUp);

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->ClientSetHUD(ABackupHUD::StaticClass());
	}
}

// ── 배치 ──

void ABackupPawn::SnapToFieldingSpot()
{
	const FVector Spot = Field.GetFieldingSpot(Position);
	const FVector Home = Field.GetBaseLocation(EBaseType::Home);
	const float DesiredYaw = (Home - Spot).Rotation().Yaw; // 항상 홈 플레이트를 본다.
	const float GroundW = FloorZ();

	if (bVR && Camera)
	{
		// 폰 루트는 트래킹 원점일 뿐이라, 머리(카메라)가 실제로 수비 위치에 서고 홈을
		// 보도록 루트를 역산해서 놓는다. 회전 먼저, 위치는 회전된 오프셋에 의존하니 나중에.
		const FRotator CamRel = Camera->GetRelativeRotation();
		const FVector CamRelLoc = Camera->GetRelativeLocation();
		const FRotator RootRot(0.0f, FRotator::NormalizeAxis(DesiredYaw - CamRel.Yaw), 0.0f);
		SetActorRotation(RootRot);
		const FVector CamOffsetWS = RootRot.RotateVector(FVector(CamRelLoc.X, CamRelLoc.Y, 0.0f));
		SetActorLocation(FVector(Spot.X, Spot.Y, GroundW) - CamOffsetWS);
	}
	else
	{
		SetActorRotation(FRotator(0.0f, DesiredYaw, 0.0f));
		SetActorLocation(FVector(Spot.X, Spot.Y, GroundW));

		// 매 시행은 타자를 보는 준비 자세에서 시작한다 — 둘러보다 만 상하각을 그대로
		// 들고 가면 하늘이나 발밑을 본 채로 큐가 뜬다. (좌우는 위 SetActorRotation 이 처리)
		if (Camera)
		{
			Camera->SetRelativeRotation(FRotator::ZeroRotator);
		}
	}
}

float ABackupPawn::FloorZ() const
{
	// VR: SetTrackingOrigin(Stage) → 바닥이 곧 폰 루트 Z. PC: 루트가 캡슐 중심이라 반높이 아래.
	return bVR ? Field.GroundZ : (Field.GroundZ + 88.0f);
}

FVector ABackupPawn::GetPlayerXY() const
{
	// VR: 머리(카메라)의 XY = 플레이어가 실제로 서 있는 자리 (폰 루트는 트래킹 원점일 뿐).
	// PC: 루트가 곧 몸.
	const FVector Src = (bVR && Camera) ? Camera->GetComponentLocation() : GetActorLocation();
	return FVector(Src.X, Src.Y, FloorZ());
}

// ── 세션 진행 ──

void ABackupPawn::StartSession()
{
	TrialIndex = 0;
	SuccessCount = 0;
	bSessionOver = false;
	bWaitingNext = false;

	SessionResults.Reset();
	LastDrills.Reset();
	CoachingText.Reset();
	bAwaitingCoaching = false;

	Phase = EBackupPhase::Done; // 첫 시행 전까지는 "진행 중"이 아니다.

	// ⚠️ 첫 시행 예약은 여기서 하지 않는다 — 진행권은 GameState 에 있다.
	//    (첫 시행을 한 박자 늦추는 이유는 그대로다: VR 은 BeginPlay 시점에 HMD 포즈가
	//     아직 안 들어와 배치를 정확히 못 잡고, 플레이어에게도 준비할 틈이 필요하다.
	//     그 지연값 FirstTrialDelaySec 은 등록 때 GameState 에 넘긴다.)
	bWaitingNext = true;

	// GameState 에 등록 — 첫 등록 폰이 세션을 개시한다. 이미 진행 중인 세션에 뒤늦게
	// 합류하면(협동) 현재 시행부터 같이 뛴다.
	ABackupGameState* GS = GetBackupGameState();

	// ⚠️ **혼자 하는 경우가 절대 깨지면 안 된다.** GameState 를 못 찾으면(다른 GameMode 를
	//    쓰는 맵, World Settings 오버라이드, 스폰 실패 등) 예전처럼 이 폰이 직접 시행을
	//    진행한다. 협동 기능은 "여러 대가 실제로 붙었을 때만" 얹히는 것이지,
	//    그게 없으면 드릴이 안 돌아가는 구조가 되어선 안 된다.
	bLocalSessionFallback = (GS == nullptr);

	if (GS)
	{
		GS->RegisterPawn(this, TotalTrials, FirstTrialDelaySec, IntervalBetweenTrials);

		// ⚠️ **등록 뒤에** 읽어야 한다 — 첫 등록은 GameState 의 시리얼을 0 으로 되돌리므로,
		//    등록 전 값을 잡아두면 기준이 어긋난다(드릴에 재진입할 때 실제로 발생).
		//    진행 중인 세션에 뒤늦게 합류한 경우엔 현재 시행을 건너뛰고 다음 시행부터
		//    참여하게 되는데, 이미 절반쯤 지난 시행에 끼어드는 것보다 이쪽이 맞다.
		LastSeenTrialSerial = GS->GetTrialSerial();
	}
	else
	{
		UE_LOG(LogMotionBase, Warning,
			TEXT("[Backup] BackupGameState 가 없다 — 폰이 직접 진행하는 단독 모드로 돈다(협동 불가)."));
		IntervalTimer = FMath::Max(FirstTrialDelaySec, 0.2f);
	}
}

void ABackupPawn::RefillBags()
{
	EligibleBag.Reset();
	AllBag.Reset();
	for (int32 i = 0; i < PlayTable.Num(); ++i)
	{
		AllBag.Add(i);
		if (UBackupPlaybook::IsEligibleForPosition(RuleTable, PlayTable[i], Position))
		{
			EligibleBag.Add(i);
		}
	}
}

int32 ABackupPawn::DrawSoloPlayIndex()
{
	if (PlayTable.Num() == 0)
	{
		return INDEX_NONE;
	}

	// HoldTrialFraction 확률로 전체 주머니(=Hold 로 풀릴 수도 있는 플레이 포함)에서 뽑는다.
	// "정답이 절대 제자리가 아니다"라는 편법을 막으려면 이게 꼭 필요하다 (설계 노트 참고).
	const bool bWantHold = (FMath::FRand() < HoldTrialFraction);

	if (EligibleBag.Num() == 0 || AllBag.Num() == 0)
	{
		RefillBags();
	}

	TArray<int32>& Bag = bWantHold ? AllBag : EligibleBag;
	if (Bag.Num() == 0)
	{
		return 0; // 저작 데이터가 비정상적으로 빈 극단적 경우의 안전망.
	}

	const int32 Pick = FMath::RandRange(0, Bag.Num() - 1);
	const int32 PlayIdx = Bag[Pick];
	Bag.RemoveAtSwap(Pick);
	return PlayIdx;
}

ABackupGameState* ABackupPawn::GetBackupGameState() const
{
	return GetWorld() ? GetWorld()->GetGameState<ABackupGameState>() : nullptr;
}

ABackupGameState* ABackupPawn::GetOwningGameState() const
{
	// 단독 모드로 시작했으면 GameState 가 지금 존재하더라도 이 폰의 진행 주체가 아니다
	// (등록된 적이 없으므로 보고해봤자 아무도 받지 않는다).
	return bLocalSessionFallback ? nullptr : GetBackupGameState();
}

void ABackupPawn::TryAttachToLateGameState()
{
	if (!bLocalSessionFallback)
	{
		return;
	}

	ABackupGameState* GS = GetBackupGameState();
	if (!GS)
	{
		return; // 아직도 없다 — 단독 모드 계속.
	}

	// 뒤늦게 붙는다. 진행 중이던 시행은 버리고 GameState 의 시행 흐름을 따른다 —
	// 어차피 단독 진행분은 다른 참가자와 상황이 다르므로 이어 붙일 수 없다.
	UE_LOG(LogMotionBase, Warning,
		TEXT("[Backup] GameState 가 뒤늦게 나타났다 — 단독 모드에서 GameState 진행으로 전환한다."));

	bLocalSessionFallback = false;
	bWaitingNext = true;
	GS->RegisterPawn(this, TotalTrials, FirstTrialDelaySec, IntervalBetweenTrials);
	LastSeenTrialSerial = GS->GetTrialSerial();
}

void ABackupPawn::SyncFromGameState()
{
	// 진행 주체 판정은 한 곳에서만 — 단독 모드면 여기 오면 안 되고, 와도 아무것도 안 한다.
	ABackupGameState* GS = GetOwningGameState();
	if (!GS)
	{
		return;
	}

	// 표시용 값은 GameState 를 정본으로 삼는다 — HUD/패널 코드는 그대로 두고 여기서만 채운다.
	TotalTrials  = GS->GetTotalTrials();
	TrialIndex   = GS->GetTrialIndex();
	bWaitingNext = GS->IsWaitingNext();

	// 새 시행 신호. 같은 플레이가 연달아 나와도 시리얼이 바뀌므로 놓치지 않는다.
	const int32 Serial = GS->GetTrialSerial();
	if (Serial != LastSeenTrialSerial)
	{
		LastSeenTrialSerial = Serial;
		const int32 PlayIndex = GS->GetCurrentPlayIndex();
		if (PlayTable.IsValidIndex(PlayIndex))
		{
			// 이전 시행이 아직 Live 인데 새 시행이 오는 경우 = 서버의 하드 타임아웃이
			// 나를 기다리다 지쳐 넘어간 것(헤드셋을 벗었거나 멈춘 상황). 그 시행은 결과
			// 없이 버려진다 — 여기서 FinishTrial 을 부르면 진행 중인 서버 전이와 얽힌다.
			if (Phase == EBackupPhase::Live)
			{
				UE_LOG(LogMotionBase, Warning,
					TEXT("[Backup] 이전 시행이 끝나기 전에 다음 시행이 시작됐다 — 결과 없이 넘어간다."));
			}
			SpawnNextTrial(PlayIndex);
		}
	}

	// 세션 종료는 한 번만 처리한다 (EndSession 이 저장·AI 요청을 건다).
	if (GS->IsSessionOver() && !bSessionOver)
	{
		EndSession();
	}
}

void ABackupPawn::SpawnNextTrial(int32 PlayIndex)
{
	// 실내 드리프트가 시행마다 누적된다 — 매번 다시 스냅한다 (설계 노트).
	SnapToFieldingSpot();

	if (IsValid(ActiveBall))
	{
		ActiveBall->Destroy();
	}
	ActiveBall = nullptr;

	// 플레이는 GameState 가 정한 **인덱스**로 온다. 정답은 여기서 **내 포지션 기준으로**
	// 로컬 계산한다 — 같은 타구라도 사람마다 정답이 다르므로 복제할 값이 아니다.
	const FBackupPlay Play = PlayTable.IsValidIndex(PlayIndex) ? PlayTable[PlayIndex] : FBackupPlay();
	CurrentTrial = UBackupPlaybook::BuildTrial(Field, RuleTable, Position, Play);

	// 해설 미리 받기 — 정답은 지금 확정됐고, 플레이어가 뛰는 동안 응답이 도착한다.
	// (표시는 판정 후에만 — IsAnswerRevealed 기준이라 정답이 미리 새지 않는다.)
	PrepareExplanationForCurrentTrial();
	SpawnFlavorBall(); // 코스메틱 — 판정(큐 시점부터 이미 시작됨)에는 영향 없음.

	Phase = EBackupPhase::Live;
	CueTimeSec = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	CueXY = GetPlayerXY();

	// 선출발 방지 — 큐 시점에 이미 게이트가 눌려 있으면, 한 번 뗄 때까지 이동을 무효화한다.
	// 저하 상태에선 "스틱을 밀고 있음"이 게이트이므로, 래치도 같은 정의를 써야 한다 —
	// IsGateCurrentlyHeld() 를 쓰면 저하 후엔 항상 false 가 되어 선출발이 뚫린다.
	bGateLatchedAtCue = IsMoveIntentHeld();
	bGateEverReleased = !bGateLatchedAtCue;

	GateElapsedSec = 0.0f;
	DisplacedCm = 0.0f;
	AccumulatedPathCm = 0.0f;
	bHeadingCommitted = false;
	bDirectionCorrect = false;
	bAmbiguousDirection = false;
	bFalseStart = false;
	PendingDecisionTimeSec = -1.0f;
	TrialSamples.Reset();

	UE_LOG(LogMotionBase, Log, TEXT("[Backup] #%d %s @ %s : %s / %s → %s"),
		TrialIndex + 1, *UBackupPlaybook::PositionName(Position), *Play.PlayId.ToString(),
		*Play.Situation, *Play.RunnerText,
		CurrentTrial.bIsHoldTrial ? TEXT("Hold") : *CurrentTrial.CorrectZone.Explain);
}

void ABackupPawn::FinishTrial(EBackupOutcome Outcome)
{
	Phase = EBackupPhase::Done;

	const float Elapsed = (CueTimeSec >= 0.0f && GetWorld()) ? (GetWorld()->GetTimeSeconds() - CueTimeSec) : 0.0f;

	FBackupResult Result;
	Result.Outcome = Outcome;
	Result.bDirectionCorrect = bDirectionCorrect;
	Result.bAmbiguousDirection = bAmbiguousDirection;
	Result.DecisionTimeSec = bHeadingCommitted ? PendingDecisionTimeSec : -1.0f;
	Result.ArrivalTimeSec = Elapsed;
	Result.bKeyScenario = CurrentTrial.CorrectZone.bKeyScenario;
	Result.Role = CurrentTrial.CorrectZone.Role;
	Result.Situation = CurrentTrial.Play.Situation;
	Result.bHoldTrial = CurrentTrial.bIsHoldTrial;

	const float StraightLine = FVector::Dist2D(CueXY, CurrentTrial.CorrectZone.RepresentativePoint());
	Result.PathEfficiency = FBackupJudge::PathEfficiency(StraightLine, AccumulatedPathCm);

	LastResult = Result;
	bHasResult = true;
	SessionResults.Add(Result);

	if (Result.IsSuccess())
	{
		++SuccessCount;
	}

	// 시도 1건 = 기록 1건 (수비는 성공/실패 + 원시 측정값만 남긴다 — 3축 모델 없음).
	if (UModeManager* MM = GetGameInstance() ? GetGameInstance()->GetSubsystem<UModeManager>() : nullptr)
	{
		TMap<FName, float> Details;
		Details.Add(TEXT("Position"), static_cast<float>(static_cast<uint8>(Position)));
		Details.Add(TEXT("DecisionSec"), Result.DecisionTimeSec);
		Details.Add(TEXT("PathEfficiency"), Result.PathEfficiency);
		Details.Add(TEXT("KeyScenario"), Result.bKeyScenario ? 1.0f : 0.0f);
		Details.Add(TEXT("HoldTrial"), CurrentTrial.bIsHoldTrial ? 1.0f : 0.0f);
		MM->RecordResult(UScoringService::ScoreDefenseAttempt(Result.IsSuccess(), Details));
	}

	UE_LOG(LogMotionBase, Log, TEXT("[Backup] 판정: %s  판단 %.2fs  경로효율 %.0f%%"),
		*UEnum::GetValueAsString(Outcome), Result.DecisionTimeSec, Result.PathEfficiency * 100.0f);

	// 시행 진행권은 GameState 에 있다 — 내 시행이 끝났다고 보고만 한다.
	// 협동이면 전원이 보고해야 다음으로 넘어가고(먼저 끝낸 사람은 나머지를 지켜본다),
	// 혼자면 등록 폰이 1개뿐이라 즉시 다음 시행이 예약된다(= 기존 동작 그대로).
	// TrialIndex 증가·세션 종료 판단은 전부 GameState 쪽으로 옮겼다 — 여기서 같이
	// 올리면 두 개의 진행권이 생겨 협동에서 시행 번호가 어긋난다.
	if (ABackupGameState* GS = GetOwningGameState())
	{
		GS->ReportTrialFinished(this);
	}
	else
	{
		// GameState 가 없는 비정상 상황의 안전망 — 예전 로컬 진행 방식으로 계속 돈다.
		++TrialIndex;
		if (TrialIndex >= TotalTrials)
		{
			EndSession();
		}
		else
		{
			bWaitingNext = true;
			IntervalTimer = IntervalBetweenTrials;
		}
	}
}

void ABackupPawn::EndSession()
{
	bSessionOver = true;

	// ── 여기서부터는 '나가는 길'을 최대한 열어 준다 ──
	// 플레이 중 임계는 이 종목의 자연 동작(글러브 들기·와인드업 등)과 겹치지 않으려고
	// 조여 둔 값이다. 세션이 끝나면 오발동시킬 동작이 없으므로 그대로 두면 어렵기만 하다.
	ExitGesture.UpThreshold = 0.80f; // 수직에서 ±37°
	ExitGesture.HoldSec     = 1.2f;
	ExitGesture.HeldSec     = 0.0f;
	EndMenu.Reset();
	if (VrPanel)
	{
		VrPanel->RequestRecenter(); // 결과·선택 카드를 지금 보는 정면에 다시 잡는다.
	}
	RequestBackupFeedback();
}

// ── AI 판단 코칭 ──

FWeaknessReport ABackupPawn::BuildBackupReport() const
{
	FWeaknessReport R;
	R.Mode = EGameModeId::Defense;
	R.AttemptCount = SessionResults.Num();
	R.bUncalibrated = true; // 판단 시간·경로효율 기준값은 아직 실측 미보정.

	if (SessionResults.Num() == 0)
	{
		return R; // bValid=false
	}

	const int32 N = SessionResults.Num();
	int32 Correct = 0, KeyN = 0, KeyCorrect = 0;
	float SumDecision = 0.0f; int32 DecisionN = 0;
	float SumCorrectDecision = 0.0f; int32 CorrectDecisionN = 0;
	float SumPathEff = 0.0f; int32 PathEffN = 0;

	for (const FBackupResult& Res : SessionResults)
	{
		if (Res.DecisionTimeSec >= 0.0f) { SumDecision += Res.DecisionTimeSec; ++DecisionN; }
		if (Res.IsSuccess())
		{
			++Correct;
			if (Res.DecisionTimeSec >= 0.0f) { SumCorrectDecision += Res.DecisionTimeSec; ++CorrectDecisionN; }
		}
		if (Res.PathEfficiency >= 0.0f) { SumPathEff += Res.PathEfficiency; ++PathEffN; }
		if (Res.bKeyScenario) { ++KeyN; if (Res.IsSuccess()) { ++KeyCorrect; } }
	}

	R.ContactCount = Correct;
	R.bValid = true;

	const float CorrectRate = static_cast<float>(Correct) / N;

	// 문턱은 타격·포구·송구와 같은 모드 공통 상수를 쓴다.
	auto AddWeakness = [&R](EWeaknessAxis Axis, float Score, const FString& Evidence)
	{
		FWeakness W;
		W.Axis = Axis;
		W.Score = FMath::Clamp(Score, 0.0f, 1.0f);
		W.Severity = 1.0f - W.Score;
		W.Evidence = Evidence;
		if (W.Severity >= UWeaknessDetector::MinReportSeverity) { R.Weaknesses.Add(W); }
	};

	// ① 백업 판단: 정답률 그대로.
	AddWeakness(EWeaknessAxis::BackupJudgment, CorrectRate,
		FString::Printf(TEXT("correct %d/%d backup calls"), Correct, N));

	// ② 판단 속도: **맞힌 시행만** 기준으로 본다 — 빨리 틀리는 사람이 만점 받는 걸 막는다
	// (퀴즈 시절부터 이어지는 규칙, CoverPawn 의 동일 로직 참고).
	if (CorrectDecisionN > 0)
	{
		const float AvgCorrectDecision = SumCorrectDecision / CorrectDecisionN;
		AddWeakness(EWeaknessAxis::DecisionSpeed,
			FMath::Clamp(Field.TargetDecisionSec / FMath::Max(AvgCorrectDecision, 0.01f), 0.0f, 1.0f),
			FString::Printf(TEXT("average %.2f s to start moving on correct calls (target %.2f s)"),
				AvgCorrectDecision, Field.TargetDecisionSec));
	}

	// ③ 경로 효율: 직선거리/실제이동거리. 이동 속도(MoveSpeedCms)와 무관한 진짜 "헤맴" 지표.
	// FootSpeed 를 재사용하지 않는다 — 다리를 안 쓰는데 LLM 에 "사다리 스텝"이 전달되면
	// Backup 시스템 프롬프트의 컨디셔닝 금지 규칙과 정면 충돌한다.
	if (PathEffN > 0)
	{
		const float AvgPathEff = SumPathEff / PathEffN;
		AddWeakness(EWeaknessAxis::RouteEfficiency, AvgPathEff,
			FString::Printf(TEXT("average route efficiency %.0f%% (straight-line distance / distance actually covered)"),
				AvgPathEff * 100.0f));
	}

	if (DecisionN > 0)
	{
		R.Notes.Add(FString::Printf(TEXT("Average decision time over all cases: %.2f s"), SumDecision / DecisionN));
	}
	if (KeyN > 0)
	{
		R.Notes.Add(FString::Printf(
			TEXT("Key scenarios (wide CF coverage, 2B covering first when 1B is pulled off the bag): %d/%d"),
			KeyCorrect, KeyN));
	}
	R.Notes.Add(FString::Printf(TEXT("Position: %s"), *UBackupPlaybook::PositionName(Position)));

	// ── 역할별 분해 ──
	// 정답률 하나로는 편중이 안 보인다. "백업은 되는데 중계(cutoff)를 못 선다"는 정답률이
	// 같아도 완전히 다른 처방이라, 역할을 코칭이 볼 수 있는 축으로 따로 내보낸다.
	{
		struct FRoleTally { int32 Correct = 0; int32 Total = 0; };
		TMap<EBackupRole, FRoleTally> ByRole;
		for (const FBackupResult& Res : SessionResults)
		{
			FRoleTally& T = ByRole.FindOrAdd(Res.Role);
			++T.Total;
			if (Res.IsSuccess()) { ++T.Correct; }
		}

		FString Breakdown;
		for (const TPair<EBackupRole, FRoleTally>& Pair : ByRole)
		{
			if (!Breakdown.IsEmpty()) { Breakdown += TEXT(", "); }
			Breakdown += FString::Printf(TEXT("%s %d/%d"),
				*RoleLabel(Pair.Key), Pair.Value.Correct, Pair.Value.Total);
		}
		if (!Breakdown.IsEmpty())
		{
			R.Notes.Add(FString::Printf(TEXT("Correct by job type: %s"), *Breakdown));
		}
	}

	// ── 시행별 상세 ──
	// 지금까지는 집계 숫자만 나가서 코칭이 "6개 중 4개 맞았다" 수준을 못 벗어났다.
	// **어떤 상황에서 무엇을 틀렸는지**가 있어야 조언이 쓸모 있어진다 —
	// 시스템 프롬프트는 이미 "which cases were missed"를 인용하라고 요구하고 있었는데,
	// 정작 그 데이터가 안 실려 있었다.
	for (int32 i = 0; i < SessionResults.Num(); ++i)
	{
		const FBackupResult& Res = SessionResults[i];

		FString Line = FString::Printf(TEXT("Case %d: \"%s\" - job: %s - %s"),
			i + 1,
			Res.Situation.IsEmpty() ? TEXT("(situation not recorded)") : *Res.Situation,
			*RoleLabel(Res.Role),
			*OutcomeLabel(Res.Outcome));

		if (Res.DecisionTimeSec >= 0.0f)
		{
			Line += FString::Printf(TEXT(", started moving after %.1fs"), Res.DecisionTimeSec);
		}
		if (Res.PathEfficiency >= 0.0f && Res.PathEfficiency < 0.85f && !Res.bHoldTrial)
		{
			// 낮을 때만 싣는다 — 잘 간 경로까지 전부 실으면 프롬프트가 잡음으로 덮인다.
			Line += FString::Printf(TEXT(", wandered (route efficiency %.0f%%)"), Res.PathEfficiency * 100.0f);
		}
		R.Notes.Add(Line);
	}

	R.Weaknesses.Sort([](const FWeakness& A, const FWeakness& B) { return A.Severity > B.Severity; });
	return R;
}

void ABackupPawn::RestartSession()
{
	// 끝난 판을 먼저 확정 저장한다 — 안 하면 StartSession 이 누적을 비워 기록이 사라진다.
	FlushSessionToSave();

	// 종료 화면에서 풀어 뒀던 나가기 조건을 플레이용으로 다시 조이고, 카드를 내린다.
	ExitGesture.UpThreshold = LiveExitUpThreshold;
	ExitGesture.HoldSec     = LiveExitHoldSec;
	ExitGesture.HeldSec     = 0.0f;
	EndMenu.Reset();
	if (VrPanel && bVR)
	{
		VrPanel->ShowBackCard(TEXT("EXIT - aim here & hold"), FColor(255, 190, 90));
		VrPanel->RequestRecenter();
	}

	StartSession();

	// 진행권이 GameState 에 있으므로 재시작도 거기서 걸어야 한다 — 협동에선 한 사람이
	// PLAY AGAIN 을 누르면 판 전체가 새로 시작된다(의도된 동작).
	if (ABackupGameState* GS = GetOwningGameState())
	{
		GS->RequestRestart();
		LastSeenTrialSerial = GS->GetTrialSerial();
	}
}

void ABackupPawn::FlushSessionToSave()
{
	UModeManager* MM = GetGameInstance() ? GetGameInstance()->GetSubsystem<UModeManager>() : nullptr;
	if (!MM)
	{
		return;
	}
	MM->FinalizeSession(
		UScoringService::ScoreDefenseSession(SuccessCount, SessionResults.Num()),
		BuildBackupReport());
}

void ABackupPawn::RequestBackupFeedback()
{
	const FWeaknessReport Report = BuildBackupReport();

	// 과거 "BackupMove" 세션만 골라 만성 추세를 본다. 판단 훈련의 추세에 포구 반응속도가
	// 섞이면 "판단이 만성 약점"인지 "몸이 느린 건지"를 구분할 수 없게 된다.
	FChronicWeaknessReport Chronic;
	if (UModeManager* MM = GetGameInstance() ? GetGameInstance()->GetSubsystem<UModeManager>() : nullptr)
	{
		Chronic = UWeaknessDetector::AnalyzeTrend(
			MM->GetHistory(), EGameModeId::Defense, 5, UModeManager::GetDefenseDrillIdName(2));
	}
	LastDrills = UDrillCatalog::RecommendWithHistory(Report, Chronic, 3);

	if (FeedbackService && FeedbackService->IsConfigured())
	{
		CoachingText = TEXT("Requesting AI coaching...");
		bAwaitingCoaching = true;
		FeedbackService->RequestBackupCoaching(Report, LastDrills, Chronic);
	}
	else
	{
		bAwaitingCoaching = false;
		CoachingText = TEXT("AI coaching not configured (Config/Secrets.ini)");
	}

	UE_LOG(LogMotionBase, Log, TEXT("[Backup] 코칭 요청: 정답 %d/%d, 약점 %d개"),
		SuccessCount, TotalTrials, Report.Weaknesses.Num());
}

void ABackupPawn::PrepareExplanationForCurrentTrial()
{
	CurrentAIExplain.Reset();
	CurrentExplainKey.Reset();

	// Hold 시행은 "아무 일도 없다"가 정답이라 확장 해설이 오히려 군더더기다.
	if (CurrentTrial.bIsHoldTrial || CurrentTrial.Play.PlayId.IsNone())
	{
		return;
	}

	// 같은 (플레이 × 포지션)이면 상황이 같으므로 해설도 같다 — 캐시 키가 성립한다.
	CurrentExplainKey = FString::Printf(TEXT("%s|%s"),
		*CurrentTrial.Play.PlayId.ToString(), *UBackupPlaybook::PositionName(Position));

	UModeManager* MM = GetGameInstance() ? GetGameInstance()->GetSubsystem<UModeManager>() : nullptr;
	if (MM && MM->TryGetPlayExplanation(CurrentExplainKey, CurrentAIExplain))
	{
		return; // 이미 받아둔 해설 — 호출 없음(부스 반복 시연에서 여기로 대부분 수렴한다).
	}

	if (!FeedbackService || !FeedbackService->IsConfigured())
	{
		return; // 키 없음 — 저작 해설로 간다.
	}

	FBackupExplainRequest Req;
	Req.CacheKey        = CurrentExplainKey;
	Req.PositionName    = UBackupPlaybook::PositionName(Position);
	Req.Situation       = CurrentTrial.Play.Situation;
	Req.RunnerText      = CurrentTrial.Play.RunnerText;
	Req.JobText         = RoleLabel(CurrentTrial.CorrectZone.Role);
	Req.AuthoredExplain = CurrentTrial.CorrectZone.Explain;

	FeedbackService->RequestPlayExplanation(Req);
}

void ABackupPawn::HandlePlayExplanationReady(bool bSuccess, const FString& CacheKey, const FString& Explanation)
{
	if (!bSuccess || Explanation.IsEmpty())
	{
		return; // 저작 해설이 이미 화면에 있다 — 아무것도 안 해도 된다.
	}

	// 캐시는 키와 무관하게 채운다. 늦게 온 응답이라도 다음 세션에서는 즉시 쓸 수 있다.
	if (UModeManager* MM = GetGameInstance() ? GetGameInstance()->GetSubsystem<UModeManager>() : nullptr)
	{
		MM->CachePlayExplanation(CacheKey, Explanation);
	}

	// ⚠️ 표시는 **지금 그 시행일 때만.** 응답이 늦어 다음 시행으로 넘어갔으면 키가 달라지고,
	//    그대로 붙이면 지금 화면의 상황과 다른 해설이 정답 설명인 척 나간다.
	if (CacheKey == CurrentExplainKey)
	{
		CurrentAIExplain = Explanation;
	}
}

void ABackupPawn::HandleCoachingReady(bool bSuccess, const FString& Text)
{
	bAwaitingCoaching = false;
	CoachingText = Text;
	UE_LOG(LogMotionBase, Log, TEXT("[Backup] AI 코칭 %s: %s"),
		bSuccess ? TEXT("수신") : TEXT("실패"), *Text);
}

// ── 측정 지표 접근자 ──

float ABackupPawn::GetLiveDecisionSec() const
{
	if (Phase != EBackupPhase::Live || bHeadingCommitted || CueTimeSec < 0.0f)
	{
		return -1.0f;
	}
	const UWorld* World = GetWorld();
	return World ? (World->GetTimeSeconds() - CueTimeSec) : -1.0f;
}

float ABackupPawn::GetAverageDecisionSec() const
{
	float Sum = 0.0f; int32 N = 0;
	for (const FBackupResult& R : SessionResults)
	{
		if (R.DecisionTimeSec >= 0.0f) { Sum += R.DecisionTimeSec; ++N; }
	}
	return (N > 0) ? (Sum / N) : -1.0f;
}

float ABackupPawn::GetAveragePathEfficiency() const
{
	float Sum = 0.0f; int32 N = 0;
	for (const FBackupResult& R : SessionResults)
	{
		if (R.PathEfficiency >= 0.0f) { Sum += R.PathEfficiency; ++N; }
	}
	return (N > 0) ? (Sum / N) : -1.0f;
}

bool ABackupPawn::GetLastOutcomeText(FString& OutText, FLinearColor& OutColor) const
{
	if (Phase == EBackupPhase::Live)
	{
		return false; // 진행 중엔 직전 결과 잔상을 숨긴다.
	}
	if (bSessionOver)
	{
		OutText = FString::Printf(TEXT("Session over!  %d / %d correct"), SuccessCount, TotalTrials);
		OutColor = FLinearColor(0.40f, 0.85f, 0.45f, 1.0f);
		return true;
	}
	if (!bHasResult)
	{
		return false;
	}

	switch (LastResult.Outcome)
	{
	case EBackupOutcome::Covered:
		OutText = TEXT("Covered!");     OutColor = FLinearColor(0.40f, 0.85f, 0.45f, 1.0f); return true;
	case EBackupOutcome::TooSlow:
		OutText = TEXT("Too slow");     OutColor = FLinearColor(0.95f, 0.55f, 0.30f, 1.0f); return true;
	case EBackupOutcome::WrongZone:
		OutText = TEXT("Wrong spot");   OutColor = FLinearColor(0.90f, 0.35f, 0.35f, 1.0f); return true;
	case EBackupOutcome::NoStart:
		OutText = TEXT("No reaction");  OutColor = FLinearColor(0.90f, 0.35f, 0.35f, 1.0f); return true;
	case EBackupOutcome::FalseStart:
		OutText = TEXT("False start");  OutColor = FLinearColor(0.90f, 0.35f, 0.35f, 1.0f); return true;
	default:
		return false;
	}
}

// ── 매 프레임 ──

bool ABackupPawn::ReadVRStickAxes(float& OutX, float& OutY) const
{
	OutX = 0.0f;
	OutY = 0.0f;
	if (!bVR || !MoveController)
	{
		return false;
	}

	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return false;
	}

	const FName Hand = MoveController->MotionSource;
	const TCHAR* Side = (Hand == FName(TEXT("Left"))) ? TEXT("Left") : TEXT("Right");

	// Vive 트랙패드는 두 이름 중 하나로 잡힌다 — 둘 다 읽어 절댓값이 큰 쪽을 쓴다
	// (다른 VR 폰들과 동일 패턴).
	auto ReadAxis = [PC, Side](const TCHAR* Generic, const TCHAR* ViveName) -> float
	{
		const float A = PC->GetInputAnalogKeyState(FKey(*FString::Printf(TEXT("MotionController_%s_%s"), Side, Generic)));
		const float B = PC->GetInputAnalogKeyState(FKey(*FString::Printf(TEXT("Vive_%s_%s"), Side, ViveName)));
		return (FMath::Abs(B) > FMath::Abs(A)) ? B : A;
	};

	OutX = ReadAxis(TEXT("Thumbstick_X"), TEXT("Trackpad_X"));
	OutY = ReadAxis(TEXT("Thumbstick_Y"), TEXT("Trackpad_Y"));
	return true;
}

bool ABackupPawn::IsMoveIntentHeld() const
{
	if (!bVR || bGateRequired)
	{
		return IsGateCurrentlyHeld();
	}

	// 저하 상태: 스틱을 밀고 있는 것 자체가 이동 의도다.
	// 문턱은 TickTrial 의 게이트 성립 조건과 **같은 값**이어야 한다 — 다르면 큐 래치와
	// 실제 이동 판정이 어긋나 선출발 방지가 반 프레임씩 새는 상태가 된다.
	float X = 0.0f, Y = 0.0f;
	if (!ReadVRStickAxes(X, Y))
	{
		return false;
	}
	return FMath::Max(FMath::Abs(X), FMath::Abs(Y)) >= FMath::Max(StickDeadzone, FirstFrameAxisFloor);
}

bool ABackupPawn::IsGateCurrentlyHeld() const
{
	if (!bVR)
	{
		return bMoveFwd || bMoveBack || bMoveLeft || bMoveRight;
	}

	if (const APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (MoveController)
		{
			const FName Hand = MoveController->MotionSource;
			const TCHAR* Side = (Hand == FName(TEXT("Left"))) ? TEXT("Left") : TEXT("Right");
			const float Trig = PC->GetInputAnalogKeyState(
				FKey(*FString::Printf(TEXT("MotionController_%s_Trigger"), Side)));
			return MoveController->IsTracked() && (Trig >= GateTriggerThreshold);
		}
	}
	return false;
}

void ABackupPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// 시야 회전은 단계와 무관하게 항상 허용 — 시행 사이(결과 확인 중)에도 둘러볼 수 있어야
	// 다음 시행의 배치를 눈에 익힌다.
	TickPCLook(DeltaSeconds);

	if (bVR)
	{
		// 패널을 플레이어 정면에 고정 배치(swimming 제거).
		if (VrPanel && Camera)
		{
			VrPanel->UpdateComfortAnchor(Camera, UVRInfoPanel::DefaultDistanceCm, 70.0f, /*RecenterDeg=*/55.0f);
		}

		if (MoveController)
		{
			// 시행이 진행 중(Live)일 땐 나가기 제스처를 동결한다 — 백업 이동 중 팔이
			// 위로 향할 수 있는데(스틱 조작 자세) 그게 나가기로 오인되면 세션이 날아간다.
			const bool bGestureAllowed = (Phase != EBackupPhase::Live);
			// 세션 종료 화면 — 패널 하단 카드를 겨눠 '다시 하기 / 메뉴로'를 고른다.
			// 제스처보다 먼저 본다: 명시적으로 고른 선택이 우연한 자세보다 우선한다.
			if (bSessionOver && VrPanel)
			{
				const int32 Chosen = EndMenu.Update(VrPanel, EndCardFirstRow, /*CardCount=*/2,
					MoveController->GetComponentLocation(), MoveController->GetForwardVector(),
					MoveController->IsTracked(), DeltaSeconds);
				if (Chosen == 0)
				{
					RestartSession();
					return; // 이번 프레임의 나머지 판정은 이전 세션 기준이라 건너뛴다.
				}
				if (Chosen == 1)
				{
					ReturnToModeSelect();
					return; // 폰이 곧 교체된다 — 이 프레임 종료.
				}
			}

			bool bExit = false;
			ExitGesture.Update(MoveController->GetForwardVector(),
				MoveController->IsTracked(), bGestureAllowed, DeltaSeconds, bExit);
			if (bExit)
			{
				ReturnToModeSelect();
				return; // 폰이 곧 교체된다 — 이 프레임 종료.
			}

			if (VrPanel)
			{
				const bool bAim = MoveController->IsTracked() && bGestureAllowed;
				if (VrPanel->UpdateBackDwell(MoveController->GetComponentLocation(),
					MoveController->GetForwardVector(), bAim, /*DwellSec=*/1.6f, /*AngleDeg=*/8.0f, DeltaSeconds))
				{
					ReturnToModeSelect();
					return;
				}
			}
		}
	}

	if (Phase == EBackupPhase::Live && !bSessionOver)
	{
		TickTrial(DeltaSeconds);
	}

	// 시행 교체·세션 종료는 GameState 가 정한다 — 여기선 그 변화를 따라간다.
	// (두 곳에서 세면 협동에서 시행이 어긋난다.)
	if (bLocalSessionFallback)
	{
		// GameState 가 뒤늦게 생겼는지 매 프레임 확인한다 — 생겼으면 그쪽으로 넘긴다.
		// (넘어가면 이 프레임부터 아래 로컬 카운트다운 대신 GameState 흐름을 따른다.)
		TryAttachToLateGameState();
	}

	if (bLocalSessionFallback)
	{
		// GameState 가 없는 단독 모드 — 예전 그대로 폰이 직접 센다.
		if (bWaitingNext && !bSessionOver)
		{
			IntervalTimer -= DeltaSeconds;
			if (IntervalTimer <= 0.0f)
			{
				bWaitingNext = false;
				SpawnNextTrial(DrawSoloPlayIndex());
			}
		}
	}
	else
	{
		SyncFromGameState();
	}

	// 정지 프레임(rest frame) — VR 멀미 완화. 폰 루트 기준 고정된 링 + 기둥 2개.
	// 눈 기준 정지된 참조물이 있으면 vection 멀미가 크게 줄어든다 (설계 노트).
	if (bVR && GetWorld())
	{
		const FVector Base = GetActorLocation();
		DrawDebugCircle(GetWorld(), Base + FVector(0, 0, 2.0f), 80.0f, 24, FColor(90, 90, 100),
			false, -1.0f, 0, 1.5f, FVector(1, 0, 0), FVector(0, 1, 0), false);
		DrawDebugLine(GetWorld(), Base + FVector(60, 0, 0), Base + FVector(60, 0, 140), FColor(90, 90, 100), false, -1.0f, 0, 1.2f);
		DrawDebugLine(GetWorld(), Base + FVector(-60, 0, 0), Base + FVector(-60, 0, 140), FColor(90, 90, 100), false, -1.0f, 0, 1.2f);
	}

	// 진행 중엔 중립색 후보만, 판정 뒤엔 정답 강조 — DrawZones 안에서 갈린다.
	// (예전엔 Live 에서만 그렸는데, 그래서 정작 복기용 정답 공개가 없었다.)
	if (CurrentTrial.CandidateZones.Num() > 0)
	{
		DrawZones();
	}

	UpdateFielderLabels();

	if (bVR)
	{
		RefreshVrPanel();
	}
}

void ABackupPawn::TryCommitHeading()
{
	if (!FBackupJudge::ShouldCommitHeading(DisplacedCm, GateElapsedSec, HeadingCommitDistanceCm, HeadingCommitTimeSec))
	{
		return;
	}

	const FVector NewPos = GetPlayerXY();
	const FVector2D HeadingDir = FVector2D(NewPos.X - CueXY.X, NewPos.Y - CueXY.Y).GetSafeNormal();

	TArray<FVector2D> CandidateDirs;
	CandidateDirs.Reserve(CurrentTrial.CandidateZones.Num());
	for (const FBackupZone& Z : CurrentTrial.CandidateZones)
	{
		const FVector Rep = Z.RepresentativePoint();
		CandidateDirs.Add(FVector2D(Rep.X - CueXY.X, Rep.Y - CueXY.Y).GetSafeNormal());
	}

	float MarginDeg = 0.0f;
	const int32 NearestIdx = FBackupJudge::NearestCandidate(HeadingDir, CandidateDirs, MarginDeg);

	bHeadingCommitted = true;
	bAmbiguousDirection = (MarginDeg < DirectionMarginDeg);
	bDirectionCorrect = (NearestIdx != INDEX_NONE) && (NearestIdx == CurrentTrial.CorrectCandidateIndex);

	// 측정 지표: 판단(이동 시작)까지 걸린 시간 = 큐부터 지금까지의 벽시계 시간.
	// VR 게이트 입력은 PC 즉시 키 입력보다 구조적으로 늦으므로 그만큼 뺀다 —
	// 안 빼면 VR 성적이 PC 대비 구조적으로 나빠져 두 입력을 비교할 수 없다.
	const float Elapsed = (CueTimeSec >= 0.0f && GetWorld()) ? (GetWorld()->GetTimeSeconds() - CueTimeSec) : 0.0f;
	const float Bias = bVR ? VRInputLatencyBiasSec : 0.0f;
	PendingDecisionTimeSec = FMath::Max(Elapsed - Bias, 0.0f);
}

void ABackupPawn::TickTrial(float DeltaSeconds)
{
	// ── 게이트 + 축 입력 ──
	bool bGateHeld = false;
	FVector2D MoveDir = FVector2D::ZeroVector; // 이미 월드 XY 축으로 정렬된 단위(이하) 벡터.

	if (bVR)
	{
		// 축을 게이트보다 **먼저**, 게이트 성립과 무관하게 읽는다 — 자동 저하 판정이
		// "트리거는 안 잡히는데 스틱은 움직이고 있다"를 관측해야 하기 때문.
		float AxisX = 0.0f, AxisY = 0.0f;
		const bool bHasAxis = ReadVRStickAxes(AxisX, AxisY);
		const float RawMag = FMath::Max(FMath::Abs(AxisX), FMath::Abs(AxisY));

		const bool bGatePressed = IsGateCurrentlyHeld();
		if (bGatePressed)
		{
			bGateEverObserved = true;
		}

		// 자동 저하 — 트리거가 한 번도 안 잡혔는데 스틱만 GateProbeSec 이상 들어오면
		// 게이트 요구를 내린다. (트리거 키가 이 런타임에서 미등록일 때의 탈출구.)
		if (bGateRequired && !bGateEverObserved && bHasAxis && RawMag >= StickDeadzone)
		{
			AxisWithoutGateSec += DeltaSeconds;
			if (AxisWithoutGateSec >= GateProbeSec)
			{
				bGateRequired = false;
				UE_LOG(LogMotionBase, Warning,
					TEXT("[Backup] 이동 게이트(MotionController_*_Trigger) 미검출 %.1fs — 스틱 단독 이동으로 저하."),
					AxisWithoutGateSec);
			}
		}

		// 저하 상태에선 "스틱을 밀고 있다"가 곧 게이트다. 아래 로직(선출발 래치·Hold 시행
		// 오출발·게이트 누적 시간)은 전부 이 플래그를 "이동 의도가 있는가"로 읽으므로
		// 정의만 바꿔주면 의미가 그대로 보존된다.
		//
		// ⚠️ 단, 저하되면 "버튼을 눌러야만 이동"이라는 드리프트 방어가 통째로 사라진다.
		//    트랙패드를 스치기만 해도 Hold 시행이 FalseStart 로 깎일 수 있으므로, 게이트
		//    성립 문턱만은 데드존이 아니라 더 높은 FirstFrameAxisFloor 를 쓴다 —
		//    방향 계산용 데드존(아래)보다 엄격해야 스침과 의도를 가를 수 있다.
		const float DegradedGateFloor = FMath::Max(StickDeadzone, FirstFrameAxisFloor);
		bGateHeld = bGateRequired ? bGatePressed : (RawMag >= DegradedGateFloor);

		if (bGateHeld && bHasAxis)
		{
			// 게이트를 막 쥔 첫 프레임은 트랙패드 최초 접촉 노이즈가 있을 수 있어
			// 더 높은 문턱을 쓴다 (그 뒤로는 일반 데드존).
			const float Floor = (GateElapsedSec <= KINDA_SMALL_NUMBER) ? FirstFrameAxisFloor : StickDeadzone;
			if (FMath::Abs(AxisX) < Floor) { AxisX = 0.0f; }
			if (FMath::Abs(AxisY) < Floor) { AxisY = 0.0f; }

			FVector2D Local(AxisY, AxisX); // 앞뒤=X, 좌우=Y (다른 VR 폰들과 동일 축 관례).
			if (Local.SizeSquared() > 1.0f) { Local.Normalize(); }

			// 머리가 보는 방향 기준 → 월드 축으로 변환.
			if (Camera && !Local.IsNearlyZero())
			{
				const float HeadYaw = Camera->GetComponentRotation().Yaw;
				const FVector Rotated = FRotator(0.0f, HeadYaw, 0.0f).RotateVector(FVector(Local.X, Local.Y, 0.0f));
				MoveDir = FVector2D(Rotated.X, Rotated.Y);
			}
		}
	}
	else
	{
		bGateHeld = IsGateCurrentlyHeld();
		if (bGateHeld)
		{
			FVector Local = FVector::ZeroVector;
			if (bMoveFwd)   Local.X += 1.0f;
			if (bMoveBack)  Local.X -= 1.0f;
			if (bMoveRight) Local.Y += 1.0f;
			if (bMoveLeft)  Local.Y -= 1.0f;
			if (!Local.IsNearlyZero())
			{
				Local = Local.GetSafeNormal();
				// ⚠️ 포지션마다 폰이 다른 방향(항상 홈을 보도록, SnapToFieldingSpot)으로 회전돼
				// 있다 — CatchBallPawn 처럼 월드 축을 그대로 쓰면 예를 들어 우익수 자리에서
				// 'W' 를 눌러도 "내가 보는 방향"이 아니라 엉뚱한 세계 축으로 움직인다.
				// 폰의 현재 회전(=바라보는 방향) 기준으로 돌려 월드 축으로 변환한다.
				const float FacingYaw = GetActorRotation().Yaw;
				const FVector Rotated = FRotator(0.0f, FacingYaw, 0.0f).RotateVector(Local);
				MoveDir = FVector2D(Rotated.X, Rotated.Y);
			}
		}
	}

	// ── 선출발 방지: 큐 시점에 이미 게이트가 눌려 있었으면, 한 번 뗄 때까지 무효 ──
	if (bGateLatchedAtCue)
	{
		if (!bGateHeld)
		{
			bGateLatchedAtCue = false;
			bGateEverReleased = true;
		}
		bGateHeld = false;
		MoveDir = FVector2D::ZeroVector;
	}

	// ── Hold 시행: 게이트가 한 번이라도 걸리면 즉시 성급한 출발 ──
	if (CurrentTrial.bIsHoldTrial)
	{
		if (bGateHeld && !MoveDir.IsNearlyZero())
		{
			FinishTrial(EBackupOutcome::FalseStart);
			return;
		}
	}
	else if (bGateHeld)
	{
		GateElapsedSec += DeltaSeconds;
	}

	// ── 이동 적용 ──
	if (bGateHeld && !MoveDir.IsNearlyZero())
	{
		const float SpeedCms = bVR ? Field.MoveSpeedCms : PCMoveSpeedCms;
		const FVector2D DeltaXY = MoveDir * SpeedCms * DeltaSeconds;
		AddActorWorldOffset(FVector(DeltaXY.X, DeltaXY.Y, 0.0f), false);
		AccumulatedPathCm += DeltaXY.Size();
	}

	const FVector NewPos = GetPlayerXY();
	DisplacedCm = FVector::Dist2D(CueXY, NewPos);

	// 위치 샘플 기록 — Data/BodyPose.h(FBodyPoseSample)의 첫 소비자.
	const float Elapsed = (CueTimeSec >= 0.0f && GetWorld()) ? (GetWorld()->GetTimeSeconds() - CueTimeSec) : 0.0f;
	FBodyPoseSample Sample;
	Sample.TimeSec = Elapsed;
	Sample.RootPosition = NewPos;
	TrialSamples.Add(Sample);

	// ── 1단계: 방향 커밋 시도 (Hold 시행은 방향 개념이 없다) ──
	if (!bHeadingCommitted && !CurrentTrial.bIsHoldTrial)
	{
		TryCommitHeading();
	}

	// ── 2단계: 도착/유지 판정 ──
	const float EffectiveTimeLimit = CurrentTrial.bIsHoldTrial ? HoldWindowSec : CurrentTrial.TimeLimitSec;

	if (!CurrentTrial.bIsHoldTrial && CurrentTrial.CorrectZone.Contains(NewPos))
	{
		FinishTrial(EBackupOutcome::Covered);
		return;
	}

	if (Elapsed >= EffectiveTimeLimit)
	{
		if (CurrentTrial.bIsHoldTrial)
		{
			FinishTrial(EBackupOutcome::Covered); // 안 움직이고 버텼다 = 정답.
		}
		else if (DisplacedCm < 20.0f)
		{
			FinishTrial(EBackupOutcome::NoStart); // 사실상 안 움직였다.
		}
		else
		{
			FinishTrial((bHeadingCommitted && bDirectionCorrect) ? EBackupOutcome::TooSlow : EBackupOutcome::WrongZone);
		}
	}
}

void ABackupPawn::DrawZones() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 판정 전엔 어느 게 정답인지 드러내지 않는다 — 후보 전부 같은 색·같은 두께.
	// 판정 후에만 정답을 초록으로 띄워 복기시킨다 (IsAnswerRevealed).
	const bool bReveal = IsAnswerRevealed();
	constexpr uint8 Neutral[3] = { 150, 156, 170 };

	for (int32 i = 0; i < CurrentTrial.CandidateZones.Num(); ++i)
	{
		const FBackupZone& Z = CurrentTrial.CandidateZones[i];
		const bool bCorrect = (i == CurrentTrial.CorrectCandidateIndex);

		FColor Col = FColor(Neutral[0], Neutral[1], Neutral[2]);
		float Thickness = 1.6f;
		if (bReveal)
		{
			Col = bCorrect ? FColor(90, 220, 110) : FColor(90, 96, 110);
			Thickness = bCorrect ? 3.0f : 1.2f;
		}

		if (Z.Role == EBackupRole::CutoffRelay)
		{
			DrawDebugLine(World, Z.SegmentA + FVector(0, 0, 2), Z.SegmentB + FVector(0, 0, 2), Col, false, -1.0f, 0, Thickness);
		}
		else
		{
			DrawDebugCircle(World, Z.Center + FVector(0, 0, 2), Z.RadiusCm, 32, Col, false, -1.0f, 0, Thickness,
				FVector(1, 0, 0), FVector(0, 1, 0), false);
		}
	}
}

// ── PC 시야 조작 ──

void ABackupPawn::TickPCLook(float DeltaSeconds)
{
	if (bVR)
	{
		return; // VR 은 고개를 돌리면 된다.
	}

	// 좌우 — 폰을 돌린다. WASD 가 폰 기준이라 시야와 이동 축이 함께 돌아간다.
	float YawDelta = 0.0f;
	if (bTurnLeft)  { YawDelta -= PCTurnSpeedDegPerSec * DeltaSeconds; }
	if (bTurnRight) { YawDelta += PCTurnSpeedDegPerSec * DeltaSeconds; }

	// 상하 — 카메라만. 마우스는 축 바인딩 없이 컨트롤러에서 직접 읽는다.
	float PitchDelta = 0.0f;
	if (PCMouseLookSensitivity > 0.0f)
	{
		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			float MouseX = 0.0f, MouseY = 0.0f;
			PC->GetInputMouseDelta(MouseX, MouseY);
			YawDelta += MouseX * PCMouseLookSensitivity;
			// UE 관례상 마우스 Y 는 위로 밀면 +. 위를 보려면 pitch 가 + 여야 하므로 그대로 쓴다.
			PitchDelta += (bPCInvertMouseY ? -MouseY : MouseY) * PCMouseLookSensitivity;
		}
	}

	if (!FMath::IsNearlyZero(YawDelta))
	{
		AddActorWorldRotation(FRotator(0.0f, YawDelta, 0.0f));
	}

	if (Camera && !FMath::IsNearlyZero(PitchDelta))
	{
		const float NewPitch = FMath::Clamp(
			Camera->GetRelativeRotation().Pitch + PitchDelta, -PCMaxPitchDeg, PCMaxPitchDeg);
		Camera->SetRelativeRotation(FRotator(NewPitch, 0.0f, 0.0f));
	}
}

// ── 동료 수비수 3D 마커 ──

void ABackupPawn::SpawnFielderMarkers()
{
	DestroyFielderMarkers();

	UWorld* World = GetWorld();
	if (!World || !bShowFielderMarkers)
	{
		return;
	}

	UClass* const MarkerCls = FielderMarkerClass ? FielderMarkerClass.Get() : AFielderMarker::StaticClass();

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.Owner = this;

	for (EFieldPosition Pos : UBackupPlaybook::AllPositions())
	{
		const FVector Spot = Field.GetFieldingSpot(Pos);
		const FVector Home = Field.GetBaseLocation(EBaseType::Home);
		// 동료도 홈(타자) 쪽을 보고 서 있게 — 이름표는 매 프레임 따로 돌린다.
		const FRotator Facing(0.0f, (Home - Spot).Rotation().Yaw, 0.0f);

		AFielderMarker* Marker = World->SpawnActor<AFielderMarker>(
			MarkerCls, FVector(Spot.X, Spot.Y, Field.GroundZ), Facing, Params);
		if (!Marker)
		{
			continue;
		}

		Marker->Configure(Pos, UBackupPlaybook::PositionName(Pos),
			UBackupPlaybook::PositionNumber(Pos), Pos == Position);
		FielderMarkers.Add(Marker);
	}
}

void ABackupPawn::DestroyFielderMarkers()
{
	for (TObjectPtr<AFielderMarker>& Marker : FielderMarkers)
	{
		if (IsValid(Marker))
		{
			Marker->Destroy();
		}
	}
	FielderMarkers.Reset();
}

void ABackupPawn::UpdateFielderLabels()
{
	if (FielderMarkers.Num() == 0)
	{
		return;
	}

	// 눈높이 기준으로 돌려야 이름표가 정면으로 보인다 (GetPlayerXY 는 바닥 Z 라 그대로 쓰면 안 됨).
	const FVector Eye = (bVR && Camera) ? Camera->GetComponentLocation() : GetActorLocation();

	for (const TObjectPtr<AFielderMarker>& Marker : FielderMarkers)
	{
		if (IsValid(Marker))
		{
			Marker->FaceLabelTowards(Eye);
		}
	}
}

// ── VR 상태 패널 ──

void ABackupPawn::RefreshVrPanel()
{
	if (!VrPanel) { return; }

	if (bSessionOver)
	{
		VrPanel->SetTitle(
			FString::Printf(TEXT("AI judgment tips    (Correct %d / %d)"), SuccessCount, TotalTrials),
			FColor(150, 210, 255));

		// ⚠️ 컴팩트 상태 패널(SetStatusCompact)은 행이 4줄을 넘으면 푸터·힌트와 겹친다.
		//    종료 화면은 마지막 두 줄을 선택 카드에 내주므로 내용이 한 줄 줄어든다.
		const int32 MaxContentRows = EndCardFirstRow;
		int32 Row = 0;

		const float AvgD = GetAverageDecisionSec();
		const float AvgP = GetAveragePathEfficiency();
		if (Row < MaxContentRows && (AvgD >= 0.0f || AvgP >= 0.0f))
		{
			VrPanel->SetRow(Row++, FString::Printf(TEXT("avg decision %.2fs   route %.0f%%"),
				FMath::Max(AvgD, 0.0f), FMath::Max(AvgP, 0.0f) * 100.0f), FColor(150, 200, 255));
		}

		for (const FString& L : WrapBackupPanel(CoachingText, 30, 2))
		{
			if (Row >= MaxContentRows) { break; }
			VrPanel->SetRow(Row++, L, FColor(228, 233, 244));
		}
		for (const FTrainingDrill& D : LastDrills)
		{
			if (Row >= MaxContentRows) { break; }
			VrPanel->SetRow(Row++, D.CompactLabel(), FColor(255, 200, 120));
		}
		VrPanel->HideRowsFrom(Row);

		// 세션 종료 화면 — 패널 하단을 선택 카드 두 장으로 바꾼다.
		// '뒤로' 카드는 내린다: 카드와 각도가 거의 겹쳐 오선택을 만들고, 같은 일을
		// BACK TO MENU 카드가 더 잘 보이는 자리에서 대신한다.
		VrPanel->SetRow(EndCardFirstRow,     EndMenu.Label(0, TEXT("PLAY AGAIN")),   EndMenu.Color(0));
		VrPanel->SetRow(EndCardFirstRow + 1, EndMenu.Label(1, TEXT("BACK TO MENU")), EndMenu.Color(1));
		VrPanel->HideFooter();
		VrPanel->HideBackCard();
		VrPanel->SetHint(TEXT("aim the controller at a card and hold"), FColor(110, 116, 128));
		return;
	}

	VrPanel->SetTitle(
		FString::Printf(TEXT("%s   %d / %d   Correct %d"),
			*UBackupPlaybook::PositionName(Position), GetTrialNumber(), TotalTrials, SuccessCount),
		FColor(228, 233, 244));

	VrPanel->SetRow(0, CurrentTrial.Play.Situation, FColor(150, 200, 255));

	FString Outcome; FLinearColor OColor;
	const bool bHasOutcome = GetLastOutcomeText(Outcome, OColor);

	const float Live = GetLiveDecisionSec();
	if (Live >= 0.0f)
	{
		// 진행 중: 상황(0) · 주자(1) · 판단 시간(2).
		VrPanel->SetRow(1, CurrentTrial.Play.RunnerText, FColor(150, 156, 168));
		VrPanel->SetRow(2, FString::Printf(TEXT("deciding...  %.1fs"), Live),
			(Live > Field.TargetDecisionSec) ? FColor(230, 130, 90) : FColor(90, 220, 110));
		VrPanel->HideRowsFrom(3);
	}
	else if (bHasOutcome)
	{
		// ⚠️ 해설을 푸터에 붙이지 않는다. 푸터는 한 줄이라 AI 확장 해설(1~2문장)이 잘린다.
		//    판정이 끝나면 주자·판단시간 줄은 역할이 끝났으므로(플레이 중 내내 보고 있었다)
		//    행 1~3 을 통째로 해설에 내준다. 컴팩트 패널은 4행(0~3)까지만 안전하다.
		const TArray<FString> Lines = WrapBackupPanel(GetLastExplainText(), /*MaxCharsPerLine=*/34, /*MaxLines=*/3);
		int32 Row = 1;
		for (const FString& L : Lines)
		{
			VrPanel->SetRow(Row++, L, FColor(190, 200, 215));
		}
		VrPanel->HideRowsFrom(Row);
	}
	else
	{
		VrPanel->SetRow(1, CurrentTrial.Play.RunnerText, FColor(150, 156, 168));
		VrPanel->HideRowsFrom(2);
	}

	if (bHasOutcome)
	{
		// 푸터엔 판정 결과만 — 짧고 색으로 구분되는 값이라 한 줄에 맞는다.
		VrPanel->SetFooter(Outcome, OColor.ToFColor(true));
	}
	else
	{
		// 저하됐으면 조용히 넘어가지 않는다 — 조작법이 바뀌었다는 사실을 그 자리에서 알린다.
		if (IsGateDegraded())
		{
			VrPanel->SetFooter(TEXT("Trigger not detected - stick only. Push the stick to move."),
				FColor(255, 190, 90));
		}
		else
		{
			VrPanel->SetFooter(TEXT("Hold the move button and go to your backup spot"), FColor(150, 156, 168));
		}
	}

	if (ExitGesture.IsHolding())
	{
		VrPanel->SetHint(FString::Printf(TEXT("Raise controller to exit  %s"), *ExitGesture.ProgressBar()),
			FColor(255, 190, 90));
	}
	else
	{
		VrPanel->SetHint(TEXT("Hold the trigger and push the stick to move to your backup zone"),
			FColor(110, 116, 128));
	}
}

void ABackupPawn::ReturnToModeSelect()
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
