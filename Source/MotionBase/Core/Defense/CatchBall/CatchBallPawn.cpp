#include "Core/Defense/CatchBall/CatchBallPawn.h"
#include "Core/Defense/CatchBall/CatchBall.h"
#include "Core/Defense/CatchBall/CatchBallJudge.h"
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
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "Core/Defense/CatchBall/CatchBallHUD.h"
#include "UI/ModeSelectHUD.h"
#include "UI/VRInfoPanel.h"
#include "AI/DrillCatalog.h"
#include "AI/AIFeedbackService.h"
#include "Analysis/WeaknessDetector.h"
#include "Scoring/ScoringService.h"
#include "Core/ModeManager.h"
#include "GameFramework/PlayerController.h"

namespace
{
	// 타구 유형 한글 이름 (UMETA DisplayName 은 에디터 전용이라 런타임엔 직접 반환).
	FString CatchTypeName(ECatchBallType Type)
	{
		switch (Type)
		{
		case ECatchBallType::GroundBall: return TEXT("Grounder");
		case ECatchBallType::FlyBall:    return TEXT("Fly ball");
		case ECatchBallType::LineDrive:  return TEXT("Line drive");
		default:                         return TEXT("Mixed");
		}
	}

	// TextRender 는 자동 줄바꿈이 없다 → 글자수로 하드 랩(한글 한 글자=한 글리프라 안전).
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

ACatchBallPawn::ACatchBallPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	Capsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	Capsule->InitCapsuleSize(34.0f, 88.0f);
	SetRootComponent(Capsule);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Capsule);
	Camera->SetRelativeLocation(FVector(0.0f, 0.0f, 70.0f)); // 눈높이
	Camera->bUsePawnControlRotation = false; // 시점 고정 (좌우 이동만)

	// VR 글러브 = 오른손 컨트롤러 + 손 위치 구체.
	GloveController = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("GloveController"));
	GloveController->SetupAttachment(Capsule);
	GloveController->MotionSource = FName(TEXT("Right"));

	GloveMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GloveMesh"));
	GloveMesh->SetupAttachment(GloveController);
	GloveMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sph(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (Sph.Succeeded())
	{
		GloveMesh->SetStaticMesh(Sph.Object);
		GloveMesh->SetRelativeScale3D(FVector(0.22f)); // 지름 22cm 글러브
	}

	// VR 상태 패널 — 캡슐 루트에 월드 고정(정면 +X, Y=0, 눈높이쯤). 헤드락 아님.
	VrPanel = CreateDefaultSubobject<UVRInfoPanel>(TEXT("VrPanel"));
	VrPanel->SetupAttachment(Capsule);
	VrPanel->SetPlacement(UVRInfoPanel::DefaultDistanceCm, 70.0f); // 캡슐 중심 기준 눈높이
}

void ACatchBallPawn::BeginPlay()
{
	Super::BeginPlay();

	// 시점을 정면(+X)으로 고정한다. 이후 모든 발사·낙구지점을 이 정면 기준으로 만든다.
	SetActorRotation(FRotator::ZeroRotator);
	HomeLocation = GetActorLocation();

	// HMD 연결 시 글러브(컨트롤러) 근접 포구 모드. 아니면 키보드.
	bVR = UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayEnabled();
	if (bVR)
	{
		UHeadMountedDisplayFunctionLibrary::SetTrackingOrigin(EHMDTrackingOrigin::Stage);
	}

	// 나가기 제스처를 이 종목에 맞게 조인다.
	// 뜬공을 기다리는 자세 = 글러브를 위로 들고 대기 = 기본값(37°/1.5s)과 정확히 겹친다.
	// 거의 수직(±23°)으로 2.5초를 요구하고, 공이 날아오는 동안에는 Tick 에서 아예 끈다.
	ExitGesture.UpThreshold = 0.92f;
	ExitGesture.HoldSec     = 2.5f;
	if (GloveMesh)
	{
		GloveMesh->SetVisibility(bVR); // 글러브는 VR 에서만 보인다.
	}

	// 상태 패널은 VR 에서만. PC 는 평면 HUD(ACatchBallHUD)가 담당한다.
	if (VrPanel)
	{
		VrPanel->BuildPanel();
		if (!bVR) { VrPanel->HideAll(); }
	}

	// AI 운동 추천 서비스 (키가 없으면 요청 시 조용히 생략됨).
	FeedbackService = NewObject<UAIFeedbackService>(this);
	FeedbackService->OnFeedbackReady.AddDynamic(this, &ACatchBallPawn::HandleCoachingReady);

	StartSession();
}

void ACatchBallPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// 축 입력 — M 과 같은 BindKey 방식(눌림/뗌 → 플래그). BindAxisKey 는 이 프로젝트에서 안 먹음.
	PlayerInputComponent->BindKey(EKeys::D, IE_Pressed,  this, &ACatchBallPawn::OnRightPressed);
	PlayerInputComponent->BindKey(EKeys::D, IE_Released, this, &ACatchBallPawn::OnRightReleased);
	PlayerInputComponent->BindKey(EKeys::A, IE_Pressed,  this, &ACatchBallPawn::OnLeftPressed);
	PlayerInputComponent->BindKey(EKeys::A, IE_Released, this, &ACatchBallPawn::OnLeftReleased);
	PlayerInputComponent->BindKey(EKeys::W, IE_Pressed,  this, &ACatchBallPawn::OnFwdPressed);
	PlayerInputComponent->BindKey(EKeys::W, IE_Released, this, &ACatchBallPawn::OnFwdReleased);
	PlayerInputComponent->BindKey(EKeys::S, IE_Pressed,  this, &ACatchBallPawn::OnBackPressed);
	PlayerInputComponent->BindKey(EKeys::S, IE_Released, this, &ACatchBallPawn::OnBackReleased);

	PlayerInputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &ACatchBallPawn::OnCatchPressed);
	PlayerInputComponent->BindKey(EKeys::M, IE_Pressed, this, &ACatchBallPawn::ReturnToModeSelect);

	// 타구 유형 선택 — 숫자 1~4. 누른 순간부터 다음 공에 반영된다.
	PlayerInputComponent->BindKey(EKeys::One,   IE_Pressed, this, &ACatchBallPawn::SelectGround);
	PlayerInputComponent->BindKey(EKeys::Two,   IE_Pressed, this, &ACatchBallPawn::SelectFly);
	PlayerInputComponent->BindKey(EKeys::Three, IE_Pressed, this, &ACatchBallPawn::SelectLine);
	PlayerInputComponent->BindKey(EKeys::Four,  IE_Pressed, this, &ACatchBallPawn::SelectRandom);

	// 공 속도 조절 — [ 느리게 / ] 빠르게. 다음 구부터 반영된다.
	PlayerInputComponent->BindKey(EKeys::LeftBracket,  IE_Pressed, this, &ACatchBallPawn::SpeedDown);
	PlayerInputComponent->BindKey(EKeys::RightBracket, IE_Pressed, this, &ACatchBallPawn::SpeedUp);

	// HUD 교체는 빙의 완료 후(여기)에 한다 — BeginPlay 시점엔 아직 Controller 가 없다.
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->ClientSetHUD(ACatchBallHUD::StaticClass());
		UE_LOG(LogMotionBase, Log, TEXT("CatchBall: HUD 교체 완료"));
	}
	else
	{
		UE_LOG(LogMotionBase, Warning, TEXT("CatchBall: HUD 교체 실패 - Controller 없음"));
	}
}

// ── 이동 ──

// ── 이동: Tick 의 플래그 처리로 대체됨 (BindKey 방식) ──

// ── 세션 진행 ──

void ACatchBallPawn::StartSession()
{
	PitchIndex = 0;
	SuccessCount = 0;
	bSessionOver = false;
	bWaitingNext = false;
	IntervalTimer = 0.0f;

	// 타구 타입별 집계 초기화 (측정 지표 ②).
	for (int32 i = 0; i < NumBallTypes; ++i)
	{
		TypeAttempts[i] = 0;
		TypeSuccess[i]  = 0;
	}

	// AI 운동 추천 세션 상태 초기화.
	SessionResults.Reset();
	LastDrills.Reset();
	CoachingText.Reset();
	bAwaitingCoaching = false;

	SpawnNextPitch();
}

void ACatchBallPawn::SpawnNextPitch()
{
	CurrentTrial = BuildTrial(SessionType);

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TSubclassOf<ACatchBall> Cls = CatchBallClass;
	if (!Cls)
	{
		Cls = ACatchBall::StaticClass();
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ActiveBall = World->SpawnActor<ACatchBall>(Cls, CurrentTrial.LaunchLocation, FRotator::ZeroRotator, Params);
	if (ActiveBall)
	{
		ActiveBall->Launch(CurrentTrial.LaunchVelocity);
		bPitchActive = true;

		StatusLine = FString::Printf(TEXT("%d / %d   Get ready!"), PitchIndex + 1, TotalPitches);
	}
}

void ACatchBallPawn::OnCatchPressed()
{
	if (!bPitchActive || !ActiveBall)
	{
		return;
	}

	const FCatchResult Result = FCatchBallJudge::JudgePress(
		ActiveBall->GetActorLocation(),
		GetActorLocation(),
		CurrentTrial.CatchRadius,
		ActiveBall->GetElapsedTime(),
		CurrentTrial.TimeToLanding,
		TimingTolerance);

	FinishPitch(Result);
}

void ACatchBallPawn::FinishPitch(const FCatchResult& Result)
{
	LastResult = Result;
	bPitchActive = false;

	SessionResults.Add(Result); // AI 약점 리포트 입력으로 누적.

	// 시도 1건 = 기록 1건. 세션 종료 시 FinalizeSession 이 한 판으로 묶어 저장한다.
	// (수비는 3축 채점 모델이 없어 성공/실패만 남기고, 원시 측정값은 Details 로 붙인다.)
	if (UModeManager* MM = GetGameInstance() ? GetGameInstance()->GetSubsystem<UModeManager>() : nullptr)
	{
		TMap<FName, float> Details;
		Details.Add(TEXT("BallType"),      static_cast<float>(TypeIndexOf(CurrentTrial.ResolvedType)));
		Details.Add(TEXT("DistanceErrorCm"), Result.DistanceError);
		Details.Add(TEXT("TimingErrorSec"),  Result.TimingError);
		Details.Add(TEXT("BallSpeedScale"),  BallSpeedScale);
		MM->RecordResult(UScoringService::ScoreDefenseAttempt(Result.IsSuccess(), Details));
	}

	// 타구 타입별 집계 — 이번 구가 실제로 어떤 유형이었는지(Mixed 확정 결과) 기준.
	if (const int32 Ti = TypeIndexOf(CurrentTrial.ResolvedType); Ti != INDEX_NONE)
	{
		++TypeAttempts[Ti];
		if (Result.IsSuccess()) { ++TypeSuccess[Ti]; }
	}

	if (Result.IsSuccess())
	{
		++SuccessCount;
	}

	if (ActiveBall)
	{
		ActiveBall->Destroy();
		ActiveBall = nullptr;
	}

	// 결과 문구.
	FString OutcomeText;
	switch (Result.Outcome)
	{
	case ECatchOutcome::Success: OutcomeText = TEXT("Caught!"); break;
	case ECatchOutcome::Miss:    OutcomeText = TEXT("Whiff"); break;
	default:                     OutcomeText = TEXT("Dropped"); break;
	}
	StatusLine = FString::Printf(TEXT("%s  (dist %.0fcm, timing %+.2fs)"),
		*OutcomeText, Result.DistanceError, Result.TimingError);

	++PitchIndex;

	// 다음 공 or 종료.
	if (PitchIndex >= TotalPitches)
	{
		EndSession();
	}
	else
	{
		bWaitingNext = true;
		IntervalTimer = IntervalBetweenPitches;
	}
}

void ACatchBallPawn::EndSession()
{
	bSessionOver = true;
	StatusLine = FString::Printf(TEXT("Session over!  Caught %d / %d    (M: back to menu)"),
		SuccessCount, TotalPitches);

	// 세션이 끝나면 포구 성적으로 약점을 판별해 AI 운동 추천을 요청한다.
	RequestCatchFeedback();
}

FWeaknessReport ACatchBallPawn::BuildCatchReport() const
{
	FWeaknessReport R;
	R.Mode = EGameModeId::Defense;
	R.AttemptCount = SessionResults.Num();
	R.bUncalibrated = true; // 포구 기준값은 아직 실측 미보정.

	if (SessionResults.Num() == 0)
	{
		return R; // bValid=false
	}

	int32 Catches = 0, Drops = 0, Misses = 0;
	float SumAbsTiming = 0.0f;   // s
	float SumDist = 0.0f; int32 DistN = 0; // cm, 놓침 제외
	for (const FCatchResult& Res : SessionResults)
	{
		switch (Res.Outcome)
		{
		case ECatchOutcome::Success: ++Catches; break;
		case ECatchOutcome::Dropped: ++Drops;   break;
		default:                     ++Misses;  break;
		}
		SumAbsTiming += FMath::Abs(Res.TimingError);
		if (Res.Outcome != ECatchOutcome::Dropped)
		{
			SumDist += Res.DistanceError; ++DistN; // 실제 뻗어본 시도만 거리 의미.
		}
	}

	const int32 N = SessionResults.Num();
	R.ContactCount = Catches;
	R.bValid = true;

	const float AvgAbsTiming = SumAbsTiming / N;                 // s
	const float DropRate = static_cast<float>(Drops) / N;
	const float MissRate = static_cast<float>(Misses + Drops) / N;
	const float AvgDist = (DistN > 0) ? (SumDist / DistN) : 0.0f; // cm

	// 문턱을 넘는 축만 약점으로 담는다. 문턱은 타격과 같은 값을 쓴다 —
	// 모드마다 다르면 같은 수행도가 모드에 따라 약점이 됐다 안 됐다 한다.
	auto AddWeakness = [&R](EWeaknessAxis Axis, float Score, const FString& Evidence)
	{
		FWeakness W;
		W.Axis = Axis;
		W.Score = FMath::Clamp(Score, 0.0f, 1.0f);
		W.Severity = 1.0f - W.Score;
		W.Evidence = Evidence;
		if (W.Severity >= UWeaknessDetector::MinReportSeverity) { R.Weaknesses.Add(W); }
	};

	// 반응속도: 타이밍 오차(±0.35s 창) + 놓침 페널티.
	const float ReactionScore = FMath::Clamp(1.0f - (AvgAbsTiming / 0.35f), 0.0f, 1.0f) * (1.0f - 0.5f * DropRate);
	AddWeakness(EWeaknessAxis::CatchReaction, ReactionScore,
		FString::Printf(TEXT("avg timing error %.0f ms, drops %d/%d"), AvgAbsTiming * 1000.0f, Drops, N));

	// 상체 유연성: 포구 순간 글러브-공 거리(못 닿음). 기준 150cm.
	const float FlexScore = FMath::Clamp(1.0f - (AvgDist / 150.0f), 0.0f, 1.0f);
	AddWeakness(EWeaknessAxis::UpperBodyFlex, FlexScore,
		FString::Printf(TEXT("avg glove-to-ball distance at catch %.0f cm"), AvgDist));

	// 발 스피드: 위치 선점 실패(실패율). 이동이 컨트롤러라 비중을 낮춰(×0.6) 반영.
	const float FootScore = FMath::Clamp(1.0f - MissRate * 0.6f, 0.0f, 1.0f);
	AddWeakness(EWeaknessAxis::FootSpeed, FootScore,
		FString::Printf(TEXT("caught %d/%d - room to get into position"), Catches, N));

	// 타구 타입별 성공률(측정 지표 ②) — 축이 아니라 노트로 실어 코칭 문장에 숫자가 남게 한다.
	// "전체 6/10"보다 "뜬공만 1/3"이 훨씬 실행 가능한 조언으로 이어진다.
	{
		FString Breakdown;
		const ECatchBallType Types[NumBallTypes] =
			{ ECatchBallType::GroundBall, ECatchBallType::FlyBall, ECatchBallType::LineDrive };
		const TCHAR* Names[NumBallTypes] = { TEXT("grounder"), TEXT("fly ball"), TEXT("line drive") };

		for (int32 i = 0; i < NumBallTypes; ++i)
		{
			int32 A = 0, S = 0;
			GetTypeStats(Types[i], A, S);
			if (A <= 0) { continue; } // 안 나온 유형은 적지 않는다 (LLM 이 0%로 오해하지 않도록).
			if (!Breakdown.IsEmpty()) { Breakdown += TEXT(", "); }
			Breakdown += FString::Printf(TEXT("%s %d/%d (%.0f%%)"),
				Names[i], S, A, 100.0f * S / A);
		}
		if (!Breakdown.IsEmpty())
		{
			R.Notes.Add(FString::Printf(TEXT("Catch rate by ball type: %s"), *Breakdown));
		}
		R.Notes.Add(FString::Printf(TEXT("Ball speed setting: x%.1f"), BallSpeedScale));
	}

	// 심각도 내림차순 (가장 시급한 약점이 앞으로).
	R.Weaknesses.Sort([](const FWeakness& A, const FWeakness& B) { return A.Severity > B.Severity; });
	return R;
}

void ACatchBallPawn::FlushSessionToSave()
{
	UModeManager* MM = GetGameInstance() ? GetGameInstance()->GetSubsystem<UModeManager>() : nullptr;
	if (!MM)
	{
		return;
	}
	// 시도가 0건이면 ModeManager 가 빈 세션으로 스스로 무시하므로 무조건 불러도 안전하다.
	MM->FinalizeSession(
		UScoringService::ScoreDefenseSession(SuccessCount, SessionResults.Num()),
		BuildCatchReport());
}

void ACatchBallPawn::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 모드 복귀·앱 종료로 폰이 사라지기 전에 세션을 확정 저장한다.
	// 다음 진입의 SetActiveMode 가 누적을 비우므로 여기서 flush 하지 않으면 기록이 사라진다.
	FlushSessionToSave();

	if (IsValid(ActiveBall))
	{
		ActiveBall->Destroy();
		ActiveBall = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void ACatchBallPawn::RequestCatchFeedback()
{
	const FWeaknessReport Report = BuildCatchReport();

	// 과거 같은 종목("Catch") 이력만 골라 만성 약점을 본다 — 종목 필터를 빼면
	// 송구·백업 세션의 축이 섞여 엉뚱한 추세가 나온다.
	FChronicWeaknessReport Chronic;
	if (UModeManager* MM = GetGameInstance() ? GetGameInstance()->GetSubsystem<UModeManager>() : nullptr)
	{
		Chronic = UWeaknessDetector::AnalyzeTrend(
			MM->GetHistory(), EGameModeId::Defense, 5, UModeManager::GetDefenseDrillIdName(0));
	}
	LastDrills = UDrillCatalog::RecommendWithHistory(Report, Chronic, 3);

	if (FeedbackService && FeedbackService->IsConfigured())
	{
		CoachingText = TEXT("Requesting AI coaching...");
		bAwaitingCoaching = true;
		FeedbackService->RequestCatchCoaching(Report, LastDrills);
	}
	else
	{
		bAwaitingCoaching = false;
		CoachingText = TEXT("AI coaching not configured (Config/Secrets.ini)");
	}

	UE_LOG(LogMotionBase, Log, TEXT("[CatchBall] 코칭 요청: 성공 %d/%d, 약점 %d개, 드릴 %d개"),
		SuccessCount, TotalPitches, Report.Weaknesses.Num(), LastDrills.Num());
}

void ACatchBallPawn::HandleCoachingReady(bool bSuccess, const FString& Text)
{
	bAwaitingCoaching = false;
	CoachingText = Text; // 성공=코칭 문장, 실패=사유. 둘 다 그대로 보여준다.
	UE_LOG(LogMotionBase, Log, TEXT("[CatchBall] AI 코칭 %s: %s"),
		bSuccess ? TEXT("수신") : TEXT("실패"), *Text);
}

// ── 유형 → 발사 파라미터 ──

ECatchBallType ACatchBallPawn::ResolveType(ECatchBallType Type) const
{
	if (Type != ECatchBallType::Mixed)
	{
		return Type;
	}
	// 셋 중 랜덤.
	const int32 Pick = FMath::RandRange(0, 2);
	switch (Pick)
	{
	case 0:  return ECatchBallType::GroundBall;
	case 1:  return ECatchBallType::FlyBall;
	default: return ECatchBallType::LineDrive;
	}
}

FCatchTrial ACatchBallPawn::BuildTrial(ECatchBallType Type) const
{
	FCatchTrial Trial;
	Trial.ResolvedType = ResolveType(Type);

	const float G = FMath::Abs(GetWorld()->GetGravityZ()); // 보통 980

	// 발사 지점: 홈 기준 정면(+X) 먼 곳, 위쪽. (항상 플레이어 시야 정면에서 출발)
	Trial.LaunchLocation = HomeLocation + FVector(PitchDistance, 0.0f, PitchHeight);

	// 도착 지점: 홈 근처에서 좌우(Y)로만 랜덤. 높이는 플레이어 몸 중심(=포구 위치).
	const float TargetY = HomeLocation.Y + FMath::RandRange(-SideSpread, SideSpread);
	const FVector Arrival(HomeLocation.X, TargetY, HomeLocation.Z);

	// 유형별 체공시간·캐치 반경. (시작값 — 플레이하며 조절)
	float Flight = 1.6f;
	switch (Trial.ResolvedType)
	{
	case ECatchBallType::GroundBall: Flight = GroundBallFlightSec; Trial.CatchRadius = GroundBallCatchRadius; break; // 낮고 빠름, 그나마 관대
	case ECatchBallType::FlyBall:    Flight = FlyBallFlightSec;    Trial.CatchRadius = FlyBallCatchRadius;    break; // 높이 뜨고 김
	case ECatchBallType::LineDrive:  Flight = LineDriveFlightSec;  Trial.CatchRadius = LineDriveCatchRadius;  break; // 빠르고 빡셈
	default: break;
	}

	// 공 속도 조절: 체공시간을 배율로 나눈다 (배율↑ = 체공↓ = 공이 빨라짐).
	// 발사 속도는 아래 포물선 역산이 이 체공시간에서 자동으로 따라온다.
	Flight = FMath::Max(Flight / FMath::Max(BallSpeedScale, 0.1f), 0.2f);

	// 포물선 역산: Flight 초 뒤 정확히 Arrival 에 도달하는 초기 속도.
	const FVector ToTarget = Arrival - Trial.LaunchLocation;
	const FVector Horiz(ToTarget.X, ToTarget.Y, 0.0f);
	const float VHoriz = Horiz.Size() / Flight;
	const float VZ = (ToTarget.Z / Flight) + 0.5f * G * Flight;

	Trial.LaunchVelocity = Horiz.GetSafeNormal() * VHoriz + FVector(0, 0, VZ);
	Trial.TimeToLanding  = Flight;

	// 바닥 마커는 도착 지점 바로 아래(발밑)에 그린다 — "여기 서라" 표시.
	Trial.PredictedLanding = FVector(Arrival.X, Arrival.Y, HomeLocation.Z - 88.0f);

	return Trial;
}

// ── 매 프레임: 마커·상태 표시 ──

void ACatchBallPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// 이동 (키보드 모드만 — VR 에선 실제 몸으로 움직인다).
	if (!bVR)
	{
		FVector Move = FVector::ZeroVector;
		if (bMoveFwd)   Move.X += 1.0f;
		if (bMoveBack)  Move.X -= 1.0f;
		if (bMoveRight) Move.Y += 1.0f;
		if (bMoveLeft)  Move.Y -= 1.0f;

		if (!Move.IsNearlyZero())
		{
			Move = Move.GetSafeNormal() * MoveSpeed * DeltaSeconds;
			AddActorWorldOffset(Move, false);
		}
	}

	// VR: 글러브(컨트롤러)를 공에 가져가면 자동 포구.
	if (bVR)
	{
		// 뒤로가기 — 글러브(컨트롤러)를 위로 들고 유지하면 모드 선택으로 복귀.
		if (GloveController)
		{
			// 공이 날아오는 동안(bPitchActive)에는 진행을 동결한다 — 뜬공을 잡으려고
			// 글러브를 들고 기다리는 자세가 나가기로 오인되면 세션이 통째로 날아간다.
			// (리셋이 아니라 동결이라, 투구 사이 틈에 계속 들고 있으면 정상적으로 나갈 수 있다.)
			bool bExit = false;
			ExitGesture.Update(GloveController->GetForwardVector(),
				GloveController->IsTracked(), /*bAllowed=*/!bPitchActive, DeltaSeconds, bExit);
			if (bExit)
			{
				ReturnToModeSelect();
				return; // 폰이 곧 교체된다 — 이 프레임 종료.
			}
		}

		// 컨트롤러 썸스틱/트랙패드로 위치 이동 (걷기 대체) → 글러브를 공에 맞춰 포구.
		TickVRLocomotion(DeltaSeconds);

		TickVRCatch();
		RefreshVrPanel();
	}

	// 다음 공 대기.
	if (bWaitingNext && !bSessionOver)
	{
		IntervalTimer -= DeltaSeconds;
		if (IntervalTimer <= 0.0f)
		{
			bWaitingNext = false;
			SpawnNextPitch();
		}
	}

	// 낙구지점 마커 (공이 날아가는 동안).
	if (bPitchActive)
	{
		DrawDebugCircle(GetWorld(), CurrentTrial.PredictedLanding + FVector(0, 0, 2.0f),
			CurrentTrial.CatchRadius, 32, FColor::Yellow, false, -1.0f, 0, 3.0f,
			FVector(1, 0, 0), FVector(0, 1, 0), false);

		// 내 캐치 반경 표시 — VR 은 글러브 위치, 키보드는 발밑 기준.
		const FVector CatchCenter = (bVR && GloveController)
			? GloveController->GetComponentLocation()
			: GetActorLocation() - FVector(0, 0, 86.0f);
		DrawDebugCircle(GetWorld(), CatchCenter,
			CurrentTrial.CatchRadius, 32, FColor::Cyan, false, -1.0f, 0, 2.0f,
			FVector(1, 0, 0), FVector(0, 1, 0), false);
	}
}

void ACatchBallPawn::TickVRCatch()
{
	if (!bPitchActive || !ActiveBall || !GloveController)
	{
		return;
	}

	const float Elapsed = ActiveBall->GetElapsedTime();

	// 타이밍 창에 들어와야 판정 시작 (너무 이른 포구 방지).
	if (Elapsed < CurrentTrial.TimeToLanding - TimingTolerance)
	{
		return;
	}

	const FCatchResult Result = FCatchBallJudge::JudgePress(
		ActiveBall->GetActorLocation(),
		GloveController->GetComponentLocation(),
		CurrentTrial.CatchRadius,
		Elapsed,
		CurrentTrial.TimeToLanding,
		TimingTolerance);

	// 글러브가 닿아 성공이거나, 타이밍 창을 지났으면 이번 구 종료.
	if (Result.IsSuccess() || Elapsed > CurrentTrial.TimeToLanding + TimingTolerance)
	{
		FinishPitch(Result);
	}
}

void ACatchBallPawn::TickVRLocomotion(float DeltaSeconds)
{
	// 룸스케일로 실제 걸어서 낙구지점(최대 ±5m)까지 가는 건 무리다.
	// 컨트롤러 썸스틱/트랙패드로 폰(몸+글러브)을 옮겨 위치를 잡고, 글러브로 공을 맞춰 포구한다.
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	// 글러브가 붙은 손(기본 오른손) 기준으로 축 키를 고른다.
	const FName Hand = GloveController ? GloveController->MotionSource : FName(TEXT("Right"));
	const bool bLeft = (Hand == FName(TEXT("Left")));
	const TCHAR* Side = bLeft ? TEXT("Left") : TEXT("Right");

	// Vive 컨트롤러 가운데 '원형 트랙패드'로 이동한다. 문제는 UE 가 이 패드를 두 이름 중
	// 하나로 잡는다는 것:
	//   · MotionController_<Side>_Thumbstick_X/Y  (OpenXR 제네릭)
	//   · Vive_<Side>_Trackpad_X/Y                (Vive 전용)
	// 실기에서 어느 쪽으로 매핑되든 동작하도록 둘 다 읽어 절댓값이 큰 쪽을 쓴다.
	// (미등록 키면 FKey 가 무효라 GetInputAnalogKeyState 가 0 을 돌려주므로 안전.)
	auto ReadAxis = [PC, Side](const TCHAR* Generic, const TCHAR* ViveName) -> float
	{
		const float A = PC->GetInputAnalogKeyState(FKey(*FString::Printf(TEXT("MotionController_%s_%s"), Side, Generic)));
		const float B = PC->GetInputAnalogKeyState(FKey(*FString::Printf(TEXT("Vive_%s_%s"), Side, ViveName)));
		return (FMath::Abs(B) > FMath::Abs(A)) ? B : A;
	};

	float AxisX = ReadAxis(TEXT("Thumbstick_X"), TEXT("Trackpad_X")); // 좌우(+우)
	float AxisY = ReadAxis(TEXT("Thumbstick_Y"), TEXT("Trackpad_Y")); // 앞뒤(+앞)

	// 데드존 (손떨림/드리프트 무시).
	if (FMath::Abs(AxisX) < StickDeadzone) { AxisX = 0.0f; }
	if (FMath::Abs(AxisY) < StickDeadzone) { AxisY = 0.0f; }

	if (AxisX == 0.0f && AxisY == 0.0f)
	{
		return;
	}

	// 시점은 정면(+X) 고정이므로 월드축으로 바로 이동. (X=앞뒤, Y=좌우)
	FVector Move(AxisY, AxisX, 0.0f);
	if (Move.SizeSquared() > 1.0f)
	{
		Move.Normalize(); // 대각선 가속 방지.
	}
	AddActorWorldOffset(Move * VRMoveSpeed * DeltaSeconds, false);
}

void ACatchBallPawn::ReturnToModeSelect()
{
	// 모드 선택 HUD 로 되돌린다.
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->ClientSetHUD(AModeSelectHUD::StaticClass());
	}

	if (AMotionBaseGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AMotionBaseGameMode>() : nullptr)
	{
		GM->ReturnToModeSelect();
	}
}

bool ACatchBallPawn::GetLastOutcomeText(FString& OutText, FLinearColor& OutColor) const
{
	// 공이 날아가는 중엔 결과를 숨긴다 (직전 결과 잔상 방지).
	if (bPitchActive)
	{
		return false;
	}

	if (bSessionOver)
	{
		OutText  = FString::Printf(TEXT("Session over!  Caught %d / %d"), SuccessCount, TotalPitches);
		OutColor = FLinearColor(0.40f, 0.85f, 0.45f, 1.0f);
		return true;
	}

	switch (LastResult.Outcome)
	{
	case ECatchOutcome::Success:
		OutText = TEXT("Caught!");   OutColor = FLinearColor(0.40f, 0.85f, 0.45f, 1.0f); return true;
	case ECatchOutcome::Miss:
		OutText = TEXT("Whiff");     OutColor = FLinearColor(0.95f, 0.55f, 0.30f, 1.0f); return true;
	case ECatchOutcome::Dropped:
		OutText = TEXT("Dropped");   OutColor = FLinearColor(0.90f, 0.35f, 0.35f, 1.0f); return true;
	default:
		return false;
	}
}

void ACatchBallPawn::SelectGround() { SessionType = ECatchBallType::GroundBall; }
void ACatchBallPawn::SelectFly()    { SessionType = ECatchBallType::FlyBall; }
void ACatchBallPawn::SelectLine()   { SessionType = ECatchBallType::LineDrive; }
void ACatchBallPawn::SelectRandom() { SessionType = ECatchBallType::Mixed; }

// ── 공 속도 조절 (측정이 아니라 난이도 손잡이) ──

void ACatchBallPawn::SpeedDown()
{
	BallSpeedScale = FMath::Clamp(BallSpeedScale - BallSpeedStep, 0.5f, 2.0f);
	StatusLine = FString::Printf(TEXT("Ball speed x%.1f  (applies from the next pitch)"), BallSpeedScale);
}

void ACatchBallPawn::SpeedUp()
{
	BallSpeedScale = FMath::Clamp(BallSpeedScale + BallSpeedStep, 0.5f, 2.0f);
	StatusLine = FString::Printf(TEXT("Ball speed x%.1f  (applies from the next pitch)"), BallSpeedScale);
}

// ── 타구 타입별 집계 ──

int32 ACatchBallPawn::TypeIndexOf(ECatchBallType Type)
{
	switch (Type)
	{
	case ECatchBallType::GroundBall: return 0;
	case ECatchBallType::FlyBall:    return 1;
	case ECatchBallType::LineDrive:  return 2;
	default:                         return INDEX_NONE; // Mixed 는 확정 유형이 아니다.
	}
}

void ACatchBallPawn::GetTypeStats(ECatchBallType Type, int32& OutAttempt, int32& OutSuccess) const
{
	const int32 Ti = TypeIndexOf(Type);
	OutAttempt = (Ti != INDEX_NONE) ? TypeAttempts[Ti] : 0;
	OutSuccess = (Ti != INDEX_NONE) ? TypeSuccess[Ti]  : 0;
}

float ACatchBallPawn::GetTypeSuccessRate(ECatchBallType Type) const
{
	int32 A = 0, S = 0;
	GetTypeStats(Type, A, S);
	return (A > 0) ? (static_cast<float>(S) / A) : -1.0f; // -1 = 시행 없음
}

void ACatchBallPawn::RefreshVrPanel()
{
	if (!VrPanel) { return; }

	// 세션 종료 → AI 운동 추천 오버레이 (코칭 문장 + 추천 드릴).
	// 한글 코칭은 KRFont 가 있으면 렌더된다. 없으면 데스크톱 로그로 확인.
	if (bSessionOver)
	{
		VrPanel->SetTitle(
			FString::Printf(TEXT("AI exercise tips    (Caught %d / %d)"), SuccessCount, TotalPitches),
			FColor(150, 210, 255));

		int32 Row = 0;

		// 타구 타입별 성공률 — AI 문장보다 먼저, 근거 숫자를 눈으로 확인할 수 있게.
		{
			FString Line;
			const ECatchBallType Types[NumBallTypes] =
				{ ECatchBallType::GroundBall, ECatchBallType::FlyBall, ECatchBallType::LineDrive };
			const TCHAR* Short[NumBallTypes] = { TEXT("GB"), TEXT("FB"), TEXT("LD") };
			for (int32 i = 0; i < NumBallTypes; ++i)
			{
				int32 A = 0, S = 0;
				GetTypeStats(Types[i], A, S);
				if (A <= 0) { continue; }
				if (!Line.IsEmpty()) { Line += TEXT("   "); }
				Line += FString::Printf(TEXT("%s %d/%d"), Short[i], S, A);
			}
			if (!Line.IsEmpty() && Row < UVRInfoPanel::MaxRows)
			{
				VrPanel->SetRow(Row++, Line, FColor(150, 200, 255));
			}
		}

		for (const FString& L : WrapForPanel(CoachingText, 30, 3))
		{
			if (Row >= UVRInfoPanel::MaxRows) { break; }
			VrPanel->SetRow(Row++, L, FColor(228, 233, 244));
		}
		for (const FTrainingDrill& D : LastDrills)
		{
			if (Row >= UVRInfoPanel::MaxRows) { break; }
			VrPanel->SetRow(Row++, FString::Printf(TEXT("- %s"), *D.Name), FColor(255, 200, 120));
		}
		VrPanel->HideRowsFrom(Row);

		VrPanel->SetFooter(bAwaitingCoaching ? TEXT("Waiting for AI...") : TEXT("Recommended exercises"),
			FColor(150, 156, 168));
		VrPanel->SetHint(TEXT("Raise glove = menu"), FColor(110, 116, 128));
		return;
	}

	// 제목: 진행 + 성공 수.
	VrPanel->SetTitle(
		FString::Printf(TEXT("Catch  %d / %d      Caught %d"),
			GetPitchNumber(), GetTotalPitches(), GetSuccessCount()),
		FColor(228, 233, 244));

	// 행0: 이번 세션 타구 유형 + 공 속도 배율.
	VrPanel->SetRow(0, FString::Printf(TEXT("Type: %s      Ball speed x%.1f"),
		*CatchTypeName(SessionType), BallSpeedScale), FColor(150, 200, 255));
	VrPanel->HideRowsFrom(1);

	// 푸터: 직전 결과(색 포함), 없으면 진행 상태 문구.
	FString Outcome; FLinearColor OColor;
	if (GetLastOutcomeText(Outcome, OColor))
	{
		VrPanel->SetFooter(Outcome, OColor.ToFColor(true));
	}
	else
	{
		VrPanel->SetFooter(StatusLine, FColor(150, 156, 168));
	}

	// 힌트: 조작 안내. 글러브를 위로 드는 중이면 나가기 진행바를 보여준다.
	if (ExitGesture.IsHolding())
	{
		VrPanel->SetHint(FString::Printf(TEXT("Raise glove to exit  %s"), *ExitGesture.ProgressBar()),
			FColor(255, 190, 90));
	}
	else
	{
		VrPanel->SetHint(TEXT("Stick/trackpad = move   ·   reach the glove to the ball to catch   ·   raise glove = exit"),
			FColor(110, 116, 128));
	}
}