#include "Testing/VRBattingPawn.h"
#include "MotionBase.h"
#include "Actors/Bat.h"
#include "Actors/PitchingZone.h"
#include "Analysis/HitModel.h"
#include "Core/ModeManager.h"
#include "Core/MotionBaseGameMode.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "Components/SceneComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HeadMountedDisplayFunctionLibrary.h"

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

	// 세션 파라미터 (모드 선택에서 고른 난이도·타석).
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UModeManager* MM = GI->GetSubsystem<UModeManager>())
		{
			SessionDifficulty = MM->GetActiveDifficulty();
			SessionStance = MM->GetActiveStance();
		}
	}

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
}

void AVRBattingPawn::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
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

	if (!LastMetrics.bContacted)
	{
		// 스윙 동작이 감지되지 않음/헛스윙.
		++MissedPitchCount;
		LastCall = TEXT("지켜봄 (스윙 없음)");
		ShowResultText(TEXT("MISS"), FLinearColor(0.7f, 0.7f, 0.75f));
		return;
	}

	// 실제 스윙 → 집계·연출.
	SessionHistory.Add(LastMetrics);
	SessionScore = UScoringService::ScoreSession(SessionHistory, ScoringConfig);
	++SwingCount;
	++ContactCount;
	bHasResult = true;

	if (LastHit.Class == EHitClass::HomeRun) { ++HomeRunCount; }
	else if (LastHit.Class == EHitClass::Hit) { ++HitCount; }

	LastCall = UHitModel::GetClassDisplayName(LastHit.Class).ToString();

	// 헤드셋 안 3D 결과 표시 (영문/기호 — 폰트 의존 없음).
	switch (LastHit.Class)
	{
	case EHitClass::HomeRun: ShowResultText(TEXT("HOME RUN!"), FLinearColor(1.0f, 0.85f, 0.15f)); break;
	case EHitClass::Hit:     ShowResultText(TEXT("HIT!"),      FLinearColor(0.35f, 0.9f, 0.4f));  break;
	case EHitClass::Foul:    ShowResultText(TEXT("FOUL"),      FLinearColor(0.75f, 0.75f, 0.8f)); break;
	default:                 ShowResultText(TEXT("OUT"),       FLinearColor(1.0f, 0.55f, 0.2f));  break;
	}

	// 타구 연출 — 좌타는 당겨치는 좌우각을 반전.
	if (PitchingZone)
	{
		const float SpraySign = (SessionStance == EBattingStance::Left) ? -1.0f : 1.0f;
		const FVector LocalDir = FRotator(LastHit.LaunchAngleDeg, LastHit.SprayAngleDeg * SpraySign, 0.0f).Vector();
		const FVector HitDir = GetActorTransform().TransformVectorNoScale(LocalDir).GetSafeNormal();
		PitchingZone->LaunchHitBall(HitDir, LastHit.ExitVelocityMps);

		// 동적 난이도 반영 — 이번 스윙 성적으로 다음 투구의 구속·변화구를 조정한다.
		// (여기는 컨택한 스윙만 도달한다. 지켜본 공은 위에서 조기 반환.)
		PitchingZone->RegisterSwingOutcome(true, LastSwingScore.TotalScore / 100.0f);
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
	SessionHistory.Reset();
	SessionScore = FScoreResult();
	LastSwingScore = FScoreResult();
	LastMetrics = FSwingMetrics();
	LastHit = FBattedBallResult();
	LastCall.Reset();
	SwingCount = ContactCount = HomeRunCount = HitCount = MissedPitchCount = 0;
	bHasResult = false;
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

	if (!GEngine)
	{
		return;
	}

	// 추적 상태 — 베이스 스테이션이 없으면 여기서 바로 경고.
	const bool bTracking = Bat && Bat->IsTracking();
	if (!bTracking)
	{
		GEngine->AddOnScreenDebugMessage(20, 2.0f, FColor::Red,
			TEXT("⚠ 컨트롤러 추적 안됨 — SteamVR/베이스 스테이션을 확인하세요 (위치 추적 없이는 스윙 측정 불가)"));
	}

	GEngine->AddOnScreenDebugMessage(1, 2.0f, FColor::White,
		FString::Printf(TEXT("=== VR 타격 [%s · %s] ===   추적:%s   [M] 모드선택  [R] 리셋"),
			*UModeManager::GetDifficultyDisplayName(SessionDifficulty).ToString(),
			*UModeManager::GetStanceDisplayName(SessionStance).ToString(),
			bTracking ? TEXT("정상") : TEXT("없음")));

	if (PitchingZone)
	{
		GEngine->AddOnScreenDebugMessage(5, 2.0f, FColor(255, 180, 90),
			FString::Printf(TEXT("동적 난이도: %.0f%%  (잘 치면 상승 · 놓치면 하강)"),
				PitchingZone->GetDynamicLevel() * 100.0f));
	}

	if (PitchingZone && PitchingZone->IsPitchInFlight())
	{
		const float Remain = PitchingZone->GetArrivalWorldTime() - (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f);
		GEngine->AddOnScreenDebugMessage(2, 2.0f, FColor::Yellow,
			FString::Printf(TEXT("%s 투구 중 — 도달까지 %.2f초"),
				CurrentPitchType == EPitchType::Breaking ? TEXT("변화구") : TEXT("직구"),
				FMath::Max(0.0f, Remain)));
	}

	if (bHasResult)
	{
		GEngine->AddOnScreenDebugMessage(3, 2.0f, FColor::Green,
			FString::Printf(TEXT("최근: %s | 배트속도 %.1f m/s | 타구 %.1f m/s · %.0f m | 총점 %.1f"),
				*LastCall, LastMetrics.ContactSpeedMps, LastHit.ExitVelocityMps, LastHit.CarryDistanceM,
				LastSwingScore.TotalScore));
		GEngine->AddOnScreenDebugMessage(4, 2.0f, FColor::Orange,
			FString::Printf(TEXT("세션: 스윙 %d · 홈런 %d · 안타 %d · 지켜봄 %d | 평균 총점 %.1f"),
				SwingCount, HomeRunCount, HitCount, MissedPitchCount, SessionScore.TotalScore));
	}
	else if (!LastCall.IsEmpty())
	{
		GEngine->AddOnScreenDebugMessage(3, 2.0f, FColor::Silver, FString::Printf(TEXT("최근: %s"), *LastCall));
	}
}
