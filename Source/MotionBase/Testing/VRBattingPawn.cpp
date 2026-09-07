#include "Testing/VRBattingPawn.h"
#include "MotionBase.h"
#include "Actors/Bat.h"
#include "Actors/PitchingZone.h"
#include "Analysis/HitModel.h"
#include "Analysis/WeaknessDetector.h"
#include "AI/DrillCatalog.h"
#include "AI/AIFeedbackService.h"
#include "Core/ModeManager.h"
#include "Core/MotionBaseGameMode.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "Components/SceneComponent.h"
#include "Components/TextRenderComponent.h"
#include "UI/VRInfoPanel.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HeadMountedDisplayFunctionLibrary.h"

namespace
{
	// TextRender 는 자동 줄바꿈이 없다 → 글자수로 하드 랩(한글은 한 글자=한 글리프라 안전).
	// MaxLines 를 넘으면 마지막 줄에 '…' 로 잘렸음을 표시한다.
	TArray<FString> WrapForPanel(const FString& In, int32 MaxCharsPerLine, int32 MaxLines)
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

AVRBattingPawn::AVRBattingPawn()
{
	PrimaryActorTick.bCanEverTick = true;
	// 빙의는 AMotionBaseGameMode 가 넘긴다 (모드 선택 폰과 Player0 다툼 방지).

	VROrigin = CreateDefaultSubobject<USceneComponent>(TEXT("VROrigin"));
	SetRootComponent(VROrigin);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(VROrigin);
	// bLockToHmd 는 기본 true — 카메라가 HMD 를 따라간다.

	// 타격 결과 3D 텍스트 — 카메라 앞 위쪽에 잠깐 띄운다 (헤드셋 안에서 보이게).
	ResultText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("ResultText"));
	ResultText->SetupAttachment(Camera);
	ResultText->SetRelativeLocation(FVector(300.0f, 0.0f, 70.0f)); // 앞 3m, 위로 약간
	ResultText->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f)); // 카메라를 향하게
	ResultText->SetHorizontalAlignment(EHTA_Center);
	ResultText->SetVerticalAlignment(EVRTA_TextCenter);
	ResultText->SetWorldSize(40.0f);
	ResultText->SetVisibility(false);

	// 상태·세션 패널 — 트래킹 원점(VROrigin)에 월드 고정. 결과 토스트(ResultText)와 달리
	// 머리를 따라오지 않아 고개를 돌려도 제자리에 있는다.
	VrPanel = CreateDefaultSubobject<UVRInfoPanel>(TEXT("VrPanel"));
	VrPanel->SetupAttachment(VROrigin);
	VrPanel->SetPlacement(UVRInfoPanel::DefaultDistanceCm, UVRInfoPanel::DefaultHeightCm);
}

void AVRBattingPawn::BeginPlay()
{
	Super::BeginPlay();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 룸스케일 기준(바닥) — 서 있는 타자의 실제 키가 반영되도록.
	UHeadMountedDisplayFunctionLibrary::SetTrackingOrigin(EHMDTrackingOrigin::Stage);

	// PostContactDelaySec 은 EditAnywhere 라 인스턴스/블루프린트에서 실수로 창(ContactTimeWindowSec)
	// 보다 짧게 덮어쓸 수 있다 — 그러면 늦은 스윙 표본이 버퍼에 쌓이기 전에 분석해버려 전부
	// TAKE로 오분류된다(재발 이력 있는 버그). 컴파일 타임에 못 잡으니 여기서 강제로 맞춘다.
	if (PostContactDelaySec < USwingAnalyzer::ContactTimeWindowSec)
	{
		UE_LOG(LogMotionBase, Warning,
			TEXT("[VRBatting] PostContactDelaySec(%.2f)이 ContactTimeWindowSec(%.2f)보다 짧습니다 — 늦은 스윙이 TAKE로 오분류됩니다. 자동으로 올립니다."),
			PostContactDelaySec, USwingAnalyzer::ContactTimeWindowSec);
		PostContactDelaySec = USwingAnalyzer::ContactTimeWindowSec;
	}

	// 나가기 제스처: 타격 준비 자세(배트를 세움)와 겹치므로 가장 빡빡하게 잡는다.
	// 수직에서 ±18° 이내로 3초 — 스탠스에서 배트를 이 정도로 곧게 세워 3초를 버티긴 어렵다.
	ExitGesture.UpThreshold = 0.95f;
	ExitGesture.HoldSec     = 3.0f;

	// VR 상태 패널 자식 텍스트 생성 (한 번). 액션 모드라 요소를 눈높이로 모으고,
	// 나가기용 '뒤로' 카드를 상시 띄운다 (배트를 겨눠 잠시 유지 = 나가기).
	if (VrPanel)
	{
		VrPanel->BuildPanel();
		VrPanel->SetStatusCompact();
		VrPanel->ShowBackCard(TEXT("EXIT - aim bat here & hold"), FColor(255, 190, 90));
	}

	// 세션 파라미터 (모드 선택에서 고른 난이도·타석).
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UModeManager* MM = GI->GetSubsystem<UModeManager>())
		{
			SessionDifficulty = MM->GetActiveDifficulty();
			SessionStance = MM->GetActiveStance();
		}
	}

	// Tick 이 매 프레임 읽는 코칭 요청 트리거 키를 여기서 한 번만 만든다
	// (SessionStance 가 세션 내내 안 바뀌므로 손도 안 바뀐다).
	{
		const TCHAR* Side = (SessionStance == EBattingStance::Left) ? TEXT("Left") : TEXT("Right");
		TriggerGenericKey = FKey(*FString::Printf(TEXT("MotionController_%s_Trigger"), Side));
		TriggerViveKey    = FKey(*FString::Printf(TEXT("Vive_%s_Trigger"), Side));
	}

	// 난이도 → 타구 판정 관대도 (점수 산식은 건드리지 않는다 — FScoringConfig 주석 참고).
	ScoringConfig.ApplyDifficulty(SessionDifficulty);

	// 배트 생성 — Vive 입력 소스로. provider 는 BeginPlay 에서 만들어지므로
	// SpawnActorDeferred → SetInputSource → FinishSpawning 순서를 지킨다.
	FActorSpawnParameters Params;
	Params.Owner = this;
	Bat = World->SpawnActorDeferred<ABat>(ABat::StaticClass(), GetActorTransform(), this);
	if (Bat)
	{
		Bat->SetInputSource(EInputSource::ViveController);
		// 배트를 쥔 손: 우타=오른손, 좌타=왼손 (윗손 기준).
		Bat->SetHandMotionSource(SessionStance == EBattingStance::Left ? FName(TEXT("Left")) : FName(TEXT("Right")));
		Bat->FinishSpawning(GetActorTransform());
		// 트래킹 원점에 붙여 컨트롤러가 이 폰 기준으로 추적되게 한다.
		Bat->AttachToComponent(VROrigin, FAttachmentTransformRules::KeepRelativeTransform);
		// (OnSwingCompleted 델리게이트는 쓰지 않는다 — 분석 시점을 폰이 직접 제어한다.)
	}

	// 투수 생성.
	PitchingZone = World->SpawnActor<APitchingZone>(APitchingZone::StaticClass(),
		GetActorLocation(), GetActorRotation(), Params);
	if (PitchingZone)
	{
		const float Distance = PitchingZone->GetReleaseToPlateCm();
		const FVector Origin = GetActorLocation();
		const FVector Fwd    = GetActorForwardVector();
		const FVector Right  = GetActorRightVector();

		// 컨택 지점(공이 도착할 곳)의 좌우/앞 위치 — 높이는 지면 기준으로 두고,
		// 실제 도착 높이는 PitchingZone 의 PlateHeight 로 준다(마운드는 지면에서 수평 조준).
		const FVector ContactXY = Origin + Fwd * ContactForwardCm + Right * ContactSideCm; // Z = 지면
		// 마운드는 컨택 지점에서 정면으로 Distance(18.44m) 뒤(투수 쪽), 지면 높이.
		const FVector MoundLocation = ContactXY + Fwd * Distance;
		PitchingZone->SetActorLocation(MoundLocation);
		// 마운드 Forward 가 컨택 지점을 수평으로 향하게 (양쪽 모두 지면 Z).
		PitchingZone->SetActorRotation((ContactXY - MoundLocation).Rotation());
		// 공이 몸이 아니라 가슴 높이로 도착하도록.
		PitchingZone->SetPlateHeightCm(ContactHeightCm);
		PitchingZone->ApplyDifficulty(SessionDifficulty);

		// 동적 난이도: 과거 기록(이 모드 평균 총점)이 좋을수록 더 어렵게 시작한다.
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UModeManager* MM = GI->GetSubsystem<UModeManager>())
			{
				const FModeStats St = MM->GetModeStats(EGameModeId::Batting);
				if (St.SessionCount > 0)
				{
					PitchingZone->SeedDynamicLevel(St.AverageTotal / 100.0f);
				}
			}
		}

		PitchingZone->OnPitchThrown.AddDynamic(this, &AVRBattingPawn::HandlePitchThrown);
		PitchingZone->OnPitchArrived.AddDynamic(this, &AVRBattingPawn::HandlePitchArrived);
	}

	// AI 운동 추천 서비스 (키가 없으면 요청 시 조용히 생략됨).
	FeedbackService = NewObject<UAIFeedbackService>(this);
	FeedbackService->OnFeedbackReady.AddDynamic(this, &AVRBattingPawn::HandleCoachingReady);
}

void AVRBattingPawn::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 모드 복귀([M]·배트 들기)·앱 종료로 폰이 사라지기 전에 진행 중이던 세션을 저장한다.
	// 다음 모드 진입 시 SetActiveMode 가 누적을 비우므로, 여기서 flush 하지 않으면 기록이 사라진다.
	FlushSessionToSave();

	// 진동을 반드시 끄고 나간다 — 안 끄면 모드 선택 화면에서도 컨트롤러가 계속 울린다.
	ContactHaptic.Stop(Cast<APlayerController>(GetController()));

	if (PitchingZone)
	{
		PitchingZone->Destroy();
		PitchingZone = nullptr;
	}
	if (Bat)
	{
		Bat->Destroy();
		Bat = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void AVRBattingPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// 개발용 키보드 보조 (헤드셋만 있을 땐 안 쓰이지만 PC 확인에 유용).
	PlayerInputComponent->BindKey(EKeys::M, IE_Pressed, this, &AVRBattingPawn::ReturnToModeSelect);
	PlayerInputComponent->BindKey(EKeys::R, IE_Pressed, this, &AVRBattingPawn::ResetSession);
}

void AVRBattingPawn::HandlePitchThrown(EPitchType PitchType, FVector InPlateLocation, float InArrivalWorldTime)
{
	CurrentPitchType = PitchType;
	CurrentPlate = InPlateLocation;
	CurrentArrivalWorldTime = InArrivalWorldTime;
	bPitchActive = true;
	bAnalyzedThisPitch = false;

	if (Bat)
	{
		Bat->BeginSwingCapture(InPlateLocation, InArrivalWorldTime);
	}
}

void AVRBattingPawn::HandlePitchArrived(FVector /*InPlateLocation*/)
{
	// 실제 분석은 Tick 에서 도달 + PostContactDelay 시점에 한다 (늦은 컨택까지 담기게).
}

void AVRBattingPawn::AnalyzeSwingNow()
{
	bAnalyzedThisPitch = true;
	bPitchActive = false;

	if (!Bat)
	{
		return;
	}

	LastMetrics = Bat->EndSwingCaptureAndAnalyze();
	LastHit = UHitModel::Simulate(LastMetrics, ScoringConfig);
	LastSwingScore = UScoringService::ScoreSwing(LastMetrics, ScoringConfig);

	// ── ① 지켜본 공 (스윙 동작 없음) — 시도가 아니다 ──
	// 세션 통계에도, 동적 난이도에도 넣지 않는다. 안 휘두른 것은 실력 신호가 아니라
	// 선구(選球)이거나 그냥 서 있었던 것이라, 어느 쪽으로도 해석하면 안 된다.
	if (!LastMetrics.bSwingDetected)
	{
		++TakeCount;
		LastCall = TEXT("Take");
		ShowResultText(TEXT("TAKE"), FLinearColor(0.7f, 0.7f, 0.75f));
		return;
	}

	// ── ② 여기부터는 실제로 휘두른 스윙 (컨택 / 헛스윙 모두) = 시도 1건 ──
	// 헛스윙도 반드시 여기 들어와야 한다. 빠지면 컨택률 분모가 사라져
	// UWeaknessDetector 가 컨택률 약점을 영원히 못 잡는다.
	SessionHistory.Add(LastMetrics);
	SessionScore = UScoringService::ScoreSession(SessionHistory, ScoringConfig);
	++SwingCount;
	bHasResult = true;

	// 시도 1건 = 기록 1건. 세션 종료 시 FinalizeSession 이 이것들을 한 판으로 저장한다.
	if (UModeManager* MM = GetGameInstance() ? GetGameInstance()->GetSubsystem<UModeManager>() : nullptr)
	{
		MM->RecordResult(LastSwingScore);
	}

	// 동적 난이도는 **휘두른 스윙에서만** 양방향으로 움직인다.
	// (컨택 여부를 그대로 넘기므로 헛스윙이면 난이도가 내려간다 — 예전엔 컨택만 도달해
	//  난이도가 올라가기만 했다.)
	if (PitchingZone)
	{
		PitchingZone->RegisterSwingOutcome(LastMetrics.bContacted, LastSwingScore.TotalScore / 100.0f);
	}

	// ── ③ 헛스윙 — 집계는 했으니 연출만 하고 끝 ──
	if (!LastMetrics.bContacted)
	{
		++WhiffCount;
		LastCall = TEXT("Whiff");
		ShowResultText(TEXT("MISS"), FLinearColor(0.85f, 0.55f, 0.35f));

		UE_LOG(LogMotionBase, Log, TEXT("[VRBatting] #%d Whiff (closest %.1f cm) peak=%.1f m/s"),
			SwingCount, LastMetrics.ContactDistanceCm, LastMetrics.PeakSpeedMps);
		return;
	}

	// ── ④ 컨택 ──
	++ContactCount;

	// 손에 임팩트를 돌려준다. 세기는 타구 속도에 비례하므로 빗맞으면 약하게 울린다
	// (= 얼마나 잘 맞았는지가 화면을 보기 전에 손으로 먼저 온다).
	PlayContactHaptic(LastHit.ExitVelocityMps);

	// 비거리 집계 (결과 화면용) — 컨택한 타구만.
	MaxCarryDistanceM = FMath::Max(MaxCarryDistanceM, LastHit.CarryDistanceM);
	SumCarryDistanceM += LastHit.CarryDistanceM;

	if (LastHit.Class == EHitClass::HomeRun) { ++HomeRunCount; }
	else if (LastHit.Class == EHitClass::Hit) { ++HitCount; }

	// 헤드셋 안 3D 결과 표시 + 푸터용 라벨 (영문 — 폰트 의존 없음).
	switch (LastHit.Class)
	{
	case EHitClass::HomeRun: LastCall = TEXT("Home run"); ShowResultText(TEXT("HOME RUN!"), FLinearColor(1.0f, 0.85f, 0.15f)); break;
	case EHitClass::Hit:     LastCall = TEXT("Hit");      ShowResultText(TEXT("HIT!"),      FLinearColor(0.35f, 0.9f, 0.4f));  break;
	case EHitClass::Foul:    LastCall = TEXT("Foul");     ShowResultText(TEXT("FOUL"),      FLinearColor(0.75f, 0.75f, 0.8f)); break;
	default:                 LastCall = TEXT("Out");      ShowResultText(TEXT("OUT"),       FLinearColor(1.0f, 0.55f, 0.2f));  break;
	}

	// 타구 연출 — 좌타는 당겨치는 좌우각을 반전.
	if (PitchingZone)
	{
		const float SpraySign = (SessionStance == EBattingStance::Left) ? -1.0f : 1.0f;
		const FVector LocalDir = FRotator(LastHit.LaunchAngleDeg, LastHit.SprayAngleDeg * SpraySign, 0.0f).Vector();
		const FVector HitDir = GetActorTransform().TransformVectorNoScale(LocalDir).GetSafeNormal();
		PitchingZone->LaunchHitBall(HitDir, LastHit.ExitVelocityMps);
		// (동적 난이도는 위 ② 에서 컨택/헛스윙 공통으로 이미 반영했다.)
	}

	UE_LOG(LogMotionBase, Log, TEXT("[VRBatting] #%d %s EV=%.1f m/s peak=%.1f total=%.1f"),
		SwingCount, *LastCall, LastHit.ExitVelocityMps, LastMetrics.PeakSpeedMps, LastSwingScore.TotalScore);
}

void AVRBattingPawn::ReturnToModeSelect()
{
	if (AMotionBaseGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AMotionBaseGameMode>() : nullptr)
	{
		GM->ReturnToModeSelect();
	}
}

void AVRBattingPawn::PlayContactHaptic(float ExitVelocityMps)
{
	if (!bContactHaptics)
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	// 배트를 쥔 손 = 스탠스가 정하는 손 (BeginPlay 에서 ABat::SetHandMotionSource 와 같은 기준).
	const EControllerHand BatHand = (SessionStance == EBattingStance::Left)
		? EControllerHand::Left
		: EControllerHand::Right;

	// 타구 속도 → 세기. 0 m/s = 최소, HapticFullExitVelocityMps 이상 = 최대.
	const float Alpha = FMath::Clamp(
		ExitVelocityMps / FMath::Max(HapticFullExitVelocityMps, KINDA_SMALL_NUMBER), 0.0f, 1.0f);
	const float Amplitude = FMath::Lerp(HapticMinAmplitude, HapticMaxAmplitude, Alpha);

	ContactHaptic.Play(PC, BatHand, HapticFrequency, Amplitude, HapticDurationSec);
}

void AVRBattingPawn::ShowResultText(const FString& Text, const FLinearColor& Color)
{
	if (!ResultText)
	{
		return;
	}
	ResultText->SetText(FText::FromString(Text));
	ResultText->SetTextRenderColor(Color.ToFColor(true));
	ResultText->SetVisibility(true);
	ResultTimer = 1.8f; // 이 시간 동안 표시 후 Tick 에서 숨긴다.
}

void AVRBattingPawn::ResetSession()
{
	// 리셋 = 지금 세션 종료 + 새 세션 시작. 버리기 전에 저장한다 (SwingTestPawn 과 동일).
	FlushSessionToSave();

	SessionHistory.Reset();
	SessionScore = FScoreResult();
	LastSwingScore = FScoreResult();
	LastMetrics = FSwingMetrics();
	LastHit = FBattedBallResult();
	LastCall.Reset();
	SwingCount = ContactCount = HomeRunCount = HitCount = WhiffCount = TakeCount = 0;
	MaxCarryDistanceM = SumCarryDistanceM = 0.0f;
	LastReport = FWeaknessReport();
	LastChronic = FChronicWeaknessReport();
	LastDrills.Reset();
	bHasResult = false;
}

bool AVRBattingPawn::GetSessionSummary(FSessionSummary& OutSummary) const
{
	// 결과 화면은 트리거로 리포트를 요청한 동안만 뜬다 — 타격은 끝이 정해진 종목이 아니라
	// (연속 투구) "세션 종료" 시점이 따로 없기 때문. 요청 = 지금까지의 판을 보겠다는 뜻.
	if (CoachingShowTimer <= 0.0f || SessionHistory.Num() == 0)
	{
		return false;
	}

	OutSummary = FSessionSummary();
	OutSummary.Mode = EGameModeId::Batting;
	OutSummary.Difficulty = SessionDifficulty;
	OutSummary.Score = SessionScore;

	OutSummary.SwingCount   = SwingCount;
	OutSummary.ContactCount = ContactCount;
	OutSummary.HomeRunCount = HomeRunCount;
	OutSummary.HitCount     = HitCount;
	// 삼진·볼넷은 이 폰이 볼카운트를 돌리지 않아 집계하지 않는다 (0 유지).

	OutSummary.MaxCarryDistanceM = MaxCarryDistanceM;
	OutSummary.AvgCarryDistanceM = (ContactCount > 0) ? (SumCarryDistanceM / ContactCount) : 0.0f;

	OutSummary.Report  = LastReport;
	OutSummary.Chronic = LastChronic;
	OutSummary.Drills  = LastDrills;
	OutSummary.CoachingText = CoachingText;
	OutSummary.bAwaitingCoaching = bAwaitingCoaching;

	// 신체역학은 VR 경로에 카메라 입력이 없어 비워 둔다 (BodyMechanics.bValid=false).
	OutSummary.bMockBodyMechanics = false;

	// 과거 기록 비교 — 이번 세션은 아직 저장 전이라 GetModeStats 에 섞이지 않는다.
	if (const UGameInstance* GI = GetGameInstance())
	{
		if (const UModeManager* MM = GI->GetSubsystem<UModeManager>())
		{
			OutSummary.PriorStats = MM->GetModeStats(EGameModeId::Batting);
			OutSummary.BestTotalScore = MM->GetBestTotalScore(EGameModeId::Batting);
			OutSummary.bNewRecord = (SessionScore.bValid)
				&& (OutSummary.BestTotalScore < 0.0f || SessionScore.TotalScore > OutSummary.BestTotalScore);
		}
	}

	return true;
}

FWeaknessReport AVRBattingPawn::BuildSessionReport() const
{
	// VR 경로엔 카메라(MediaPipe) 포즈 입력이 없어 신체역학 축은 비어 있다.
	// (SwingTestPawn 은 Mock 포즈로 AppendBodyMechanicsWeaknesses 까지 붙인다.)
	return UWeaknessDetector::DetectSwing(SessionHistory, ScoringConfig);
}

void AVRBattingPawn::FlushSessionToSave()
{
	UModeManager* MM = GetGameInstance() ? GetGameInstance()->GetSubsystem<UModeManager>() : nullptr;
	if (!MM)
	{
		return;
	}

	// 세션 집계 점수 + 약점 리포트를 함께 확정 저장한다. 리포트는 저장 시점의 SessionHistory 로
	// 새로 계산한다 — 트리거(코칭 요청)를 한 번도 안 눌렀어도 만성 약점 추적이 이어지도록.
	MM->FinalizeSession(SessionScore, BuildSessionReport());
}

void AVRBattingPawn::RequestCoaching()
{
	if (bAwaitingCoaching)
	{
		return; // 이미 대기 중 — 중복 호출/과금 방지.
	}
	if (SessionHistory.Num() == 0)
	{
		CoachingText = TEXT("Take a few swings first.");
		return;
	}

	// 1) 결정론적 약점 판별 + 드릴 추천 (네트워크 불필요 — 항상 나온다).
	//    저장(FlushSessionToSave)과 같은 함수를 쓴다 — 화면에 보인 리포트와 저장된 리포트가
	//    달라지면 다음 세션의 만성 약점 계산이 화면과 어긋난다.
	LastReport = BuildSessionReport();

	// 과거 저장 이력에서 만성 약점·추세를 반영 (SwingTestPawn 과 동일).
	LastChronic = FChronicWeaknessReport();
	if (UModeManager* MM = GetGameInstance() ? GetGameInstance()->GetSubsystem<UModeManager>() : nullptr)
	{
		LastChronic = UWeaknessDetector::AnalyzeTrend(MM->GetHistory(), EGameModeId::Batting, 5, NAME_None);
	}
	LastDrills = UDrillCatalog::RecommendWithHistory(LastReport, LastChronic, 3);

	// 2) AI 코칭 요청 (키가 있으면 표현 문장을 얹는다).
	CoachingShowTimer = 14.0f; // 요청 순간부터 패널에 코칭 오버레이를 띄운다.
	if (FeedbackService && FeedbackService->IsConfigured())
	{
		CoachingText = TEXT("Requesting AI coaching...");
		bAwaitingCoaching = true;
		FeedbackService->RequestSwingCoaching(LastReport, LastDrills, LastChronic);
	}
	else
	{
		bAwaitingCoaching = false;
		CoachingText = TEXT("AI coaching not configured (Config/Secrets.ini)");
	}

	UE_LOG(LogMotionBase, Log, TEXT("[VRBatting] 코칭 요청: 약점 %d개, 드릴 %d개"),
		LastReport.Weaknesses.Num(), LastDrills.Num());
}

void AVRBattingPawn::HandleCoachingReady(bool bSuccess, const FString& Text)
{
	bAwaitingCoaching = false;
	CoachingText = Text; // 성공=코칭 문장, 실패=사유. 둘 다 그대로 보여준다.
	CoachingShowTimer = FMath::Max(CoachingShowTimer, 14.0f); // 응답 도착 시점부터 충분히 읽을 시간.
	UE_LOG(LogMotionBase, Log, TEXT("[VRBatting] AI 코칭 %s: %s"),
		bSuccess ? TEXT("수신") : TEXT("실패"), *Text);
}

void AVRBattingPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// 도달 후 딜레이가 지나면 스윙 분석.
	if (bPitchActive && !bAnalyzedThisPitch)
	{
		const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
		if (Now >= CurrentArrivalWorldTime + PostContactDelaySec)
		{
			AnalyzeSwingNow();
		}
	}

	// 결과 텍스트 표시 시간 카운트다운.
	if (ResultTimer > 0.0f)
	{
		ResultTimer -= DeltaSeconds;
		if (ResultTimer <= 0.0f && ResultText)
		{
			ResultText->SetVisibility(false);
		}
	}

	// 컨택 진동 지속시간 카운트다운 — SetHapticsByValue 는 꺼줄 때까지 계속 울린다.
	ContactHaptic.Update(Cast<APlayerController>(GetController()), DeltaSeconds);

	// 컨트롤러 트리거(아래 검지 버튼)를 당기면 지금까지의 스윙으로 AI 운동 추천을 요청한다.
	// (스윙은 배트 궤적으로 자동 판정되므로 트리거는 비어 있다 — 코칭 버튼으로 재활용.)
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		// 키 자체는 BeginPlay 에서 캐싱해 뒀다(손이 세션 내내 안 바뀜).
		const float Generic = PC->GetInputAnalogKeyState(TriggerGenericKey);
		const float Vive    = PC->GetInputAnalogKeyState(TriggerViveKey);
		const bool bHeld = FMath::Max(Generic, Vive) >= TriggerPressThreshold;
		if (bHeld && !bTriggerHeldPrev)
		{
			RequestCoaching();
		}
		bTriggerHeldPrev = bHeld;
	}

	// 패널 배치 — 정면 고정(swimming 제거) + **반대 타석 쪽으로 비켜 놓기**.
	//
	// 타격은 정면에서 공이 날아오는 종목이라, 패널을 정면에 두면 투구 궤적을 그대로 가린다.
	// 그래서 타자가 서지 않은 **반대 타석 위**로 옮긴다:
	//   우타는 3루 쪽 타석(-Y)에 서므로 패널은 1루 쪽(+Y),  좌타는 그 반대.
	// 고개만 살짝 돌리면 읽히고, 스윙 시야(정면)는 비어 있다.
	// (겨눔 판정이 카드 위치를 쓰므로 UpdateBackDwell 보다 먼저 자리를 잡는다.)
	if (VrPanel && Camera)
	{
		const float SideYaw = (SessionStance == EBattingStance::Left)
			? -PanelSideYawDeg   // 좌타(1루 쪽 타석) → 패널은 3루 쪽
			: +PanelSideYawDeg;  // 우타(3루 쪽 타석) → 패널은 1루 쪽

		VrPanel->UpdateComfortAnchor(Camera, UVRInfoPanel::DefaultDistanceCm,
			UVRInfoPanel::DefaultHeightCm, /*RecenterDeg=*/55.0f, SideYaw);
	}

	// VR 뒤로가기 — 배트를 위(천장)로 들고 유지하면 모드 선택으로 복귀.
	// ⚠️ 타격은 이 제스처의 오발동 위험이 가장 크다 — **타자의 준비 자세가 배트를 거의
	//    수직으로 세운 채 다음 투구를 기다리는 것**이라 기본 임계(37°/1.5s)와 그대로 겹친다.
	//    그래서 ① 임계를 수직 ±18° 로 좁히고 ② 공이 날아오는 동안엔 진행을 동결한다.
	//    (동결이라 투구 사이 틈에 배트를 세워 두면 정상적으로 나갈 수 있다.)
	if (Bat)
	{
		bool bExit = false;
		ExitGesture.Update(Bat->GetAimForwardVector(), Bat->IsTracking(),
			/*bAllowed=*/!bPitchActive, DeltaSeconds, bExit);
		if (bExit)
		{
			ReturnToModeSelect();
			return; // 폰이 곧 교체된다 — 이 프레임 종료.
		}

		// 두 번째 출구 — '뒤로' 카드를 배트로 겨눠 유지하면 나간다 (제스처와 병행).
		// 공이 날아오는 동안엔 스윙 궤적이 카드를 스쳐 오발동하지 않게 겨눔을 막는다.
		if (VrPanel)
		{
			const bool bAim = Bat->IsTracking() && !bPitchActive;
			if (VrPanel->UpdateBackDwell(Bat->GetBatTipWorldLocation(), Bat->GetAimForwardVector(),
				bAim, /*DwellSec=*/1.6f, /*AngleDeg=*/8.0f, DeltaSeconds))
			{
				ReturnToModeSelect();
				return;
			}
		}
	}

	if (CoachingShowTimer > 0.0f)
	{
		CoachingShowTimer -= DeltaSeconds;
	}

	// 상태·세션 정보는 월드 고정 3D 패널로 (헤드셋 안에서 보이게).
	RefreshVrPanel();
}

void AVRBattingPawn::RefreshVrPanel()
{
	if (!VrPanel)
	{
		return;
	}

	// AI 운동 추천 오버레이 — 트리거로 요청한 뒤 잠시 코칭+드릴을 패널에 띄운다.
	// (한글 코칭은 KRFont 가 있으면 렌더된다. 없으면 데스크톱 로그로 확인.)
	if (CoachingShowTimer > 0.0f)
	{
		// 헤드셋 안 결과 요약 — 데스크톱 미러는 ISessionResultView 로 전체 패널을 그리지만,
		// 헤드셋에서는 3D 텍스트라 줄 수가 한정돼 핵심 숫자만 압축해 보여준다.
		VrPanel->SetTitle(
			FString::Printf(TEXT("Session result    score %.1f"), SessionScore.TotalScore),
			FColor(150, 210, 255));

		// ⚠️ 컴팩트 상태 패널(SetStatusCompact)은 행이 4줄을 넘으면 푸터·힌트와 겹친다.
		//    핵심만 압축: 요약 1줄 + 코칭 2줄 + 드릴 1개. 전체 리포트는 데스크톱 결과 화면이 담당.
		constexpr int32 MaxContentRows = 4;

		int32 Row = 0;
		if (Row < MaxContentRows)
		{
			const float ContactRate = (SwingCount > 0)
				? (100.0f * ContactCount / SwingCount) : 0.0f;
			VrPanel->SetRow(Row++,
				FString::Printf(TEXT("swings %d  contact %.0f%%  HR %d  hits %d  takes %d"),
					SwingCount, ContactRate, HomeRunCount, HitCount, TakeCount),
				FColor(150, 200, 255));
		}

		for (const FString& L : WrapForPanel(CoachingText, 30, 2))
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
		VrPanel->SetHint(TEXT("trigger = request again · raise bat = exit · [M/R]"),
			FColor(110, 116, 128));
		return;
	}

	const bool bTracking = Bat && Bat->IsTracking();

	// 3D 텍스트는 한글 폰트가 없어 영어로 표기.
	auto DiffEn = [](EDifficultyLevel D) -> const TCHAR*
	{
		switch (D)
		{
		case EDifficultyLevel::Beginner: return TEXT("Beginner");
		case EDifficultyLevel::Pro:      return TEXT("Pro");
		default:                         return TEXT("Amateur");
		}
	};
	const TCHAR* StanceEn = (SessionStance == EBattingStance::Left) ? TEXT("Lefty") : TEXT("Righty");

	// 제목: 난이도·타석. 추적 끊기면 붉게.
	VrPanel->SetTitle(
		FString::Printf(TEXT("VR Batting   [%s / %s]"), DiffEn(SessionDifficulty), StanceEn),
		bTracking ? FColor(228, 233, 244) : FColor(235, 90, 90));

	int32 Row = 0;

	// 추적 경고 / 동적 난이도.
	if (!bTracking)
	{
		VrPanel->SetRow(Row++, TEXT("! Controller not tracked - check SteamVR / base stations"),
			FColor(235, 90, 90));
	}
	else if (PitchingZone)
	{
		VrPanel->SetRow(Row++,
			FString::Printf(TEXT("Dynamic difficulty %.0f%%  (up on good hits, down on misses)"),
				PitchingZone->GetDynamicLevel() * 100.0f),
			FColor(255, 180, 90));
	}

	// 투구 중 정보.
	if (PitchingZone && PitchingZone->IsPitchInFlight())
	{
		const float Remain = PitchingZone->GetArrivalWorldTime() - (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f);
		VrPanel->SetRow(Row++,
			FString::Printf(TEXT("%s incoming - %.2fs to plate"),
				CurrentPitchType == EPitchType::Breaking ? TEXT("Breaking ball") : TEXT("Fastball"),
				FMath::Max(0.0f, Remain)),
			FColor(240, 220, 90));
	}
	VrPanel->HideRowsFrom(Row);

	// 푸터: 직전 타격 결과 상세.
	if (bHasResult)
	{
		VrPanel->SetFooter(
			FString::Printf(TEXT("Last: %s | bat %.1f m/s | ball %.1f m/s / %.0f m | score %.1f"),
				*LastCall, LastMetrics.ContactSpeedMps, LastHit.ExitVelocityMps, LastHit.CarryDistanceM,
				LastSwingScore.TotalScore),
			FColor(120, 220, 130));
	}
	else if (!LastCall.IsEmpty())
	{
		VrPanel->SetFooter(FString::Printf(TEXT("Last: %s"), *LastCall), FColor(180, 184, 192));
	}
	else
	{
		VrPanel->SetFooter(TEXT("Watch the ball and swing on time"), FColor(150, 156, 168));
	}

	// 힌트: 세션 집계 + 조작. 배트를 위로 드는 중이면 나가기 진행바를 크게 보여준다.
	if (ExitGesture.IsHolding())
	{
		VrPanel->SetHint(
			FString::Printf(TEXT("Raise bat to exit  %s"), *ExitGesture.ProgressBar()),
			FColor(255, 190, 90));
	}
	else
	{
		VrPanel->SetHint(
			FString::Printf(TEXT("Session: swings %d (contact %d · whiff %d) · HR %d · hits %d · takes %d | avg %.1f    ·    trigger = AI coaching · raise bat = exit   [M/R]"),
				SwingCount, ContactCount, WhiffCount, HomeRunCount, HitCount, TakeCount, SessionScore.TotalScore),
			FColor(110, 116, 128));
	}
}
