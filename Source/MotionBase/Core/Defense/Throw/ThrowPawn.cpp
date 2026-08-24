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
	if (BallInHandMesh)
	{
		BallInHandMesh->SetVisibility(bVR); // 손에 든 공은 VR 에서만.
	}

	// 상태 패널은 VR 에서만. PC 는 평면 HUD(AThrowHUD)가 담당한다.
	if (VrPanel)
	{
		VrPanel->BuildPanel();
		if (!bVR) { VrPanel->HideAll(); }
	}

	StartSession();
}

void AThrowPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// 스페이스바: 누름=충전 시작, 뗌=발사.
	PlayerInputComponent->BindKey(EKeys::SpaceBar, IE_Pressed,  this, &AThrowPawn::OnChargeStart);
	PlayerInputComponent->BindKey(EKeys::SpaceBar, IE_Released, this, &AThrowPawn::OnChargeRelease);
	PlayerInputComponent->BindKey(EKeys::M, IE_Pressed, this, &AThrowPawn::ReturnToModeSelect);

	// HUD 교체 (빙의 후 여기서).
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		// 주: ThrowHUD 는 다음 단계. 지금은 임시로 기본 HUD 유지.
		PC->ClientSetHUD(AThrowHUD::StaticClass());
	}
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

	SpawnNextTarget();
}

void AThrowPawn::SpawnNextTarget()
{
	// 타겟을 정면(+X) 랜덤 거리·좌우로 배치.
	const float Dist = FMath::RandRange(MinTargetDistance, MaxTargetDistance);
	const float SideY = FMath::RandRange(-TargetSideSpread, TargetSideSpread);

	CurrentTrial = FThrowTrial();
	CurrentTrial.ThrowOrigin    = HomeLocation + FVector(0, 0, 60.0f); // 손 높이
	CurrentTrial.TargetLocation = HomeLocation + FVector(Dist, SideY, -88.0f); // 바닥
	CurrentTrial.TargetDistance = FVector::Dist2D(CurrentTrial.TargetLocation, CurrentTrial.ThrowOrigin);
	CurrentTrial.IdealPower     = DistanceToIdealPower(CurrentTrial.TargetDistance);
	CurrentTrial.HitRadius      = HitRadius;

	CurrentPower = 0.0f;
	bCharging = false;
	bBallInFlight = false;
	bHasResult = false;
}

void AThrowPawn::OnChargeStart()
{
	// 공이 날아가는 중이거나 세션 끝이면 무시.
	if (bBallInFlight || bSessionOver || bWaitingNext)
	{
		return;
	}
	bCharging = true;
	CurrentPower = 0.0f;
}

void AThrowPawn::OnChargeRelease()
{
	if (!bCharging)
	{
		return;
	}
	bCharging = false;
	ThrowBall(CurrentPower);
}

void AThrowPawn::ThrowBall(float Power)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TSubclassOf<ACatchBall> Cls = BallClass;
	if (!Cls)
	{
		Cls = ACatchBall::StaticClass();
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ActiveBall = World->SpawnActor<ACatchBall>(Cls, CurrentTrial.ThrowOrigin, FRotator::ZeroRotator, Params);
	if (ActiveBall)
	{
		ActiveBall->Launch(PowerToVelocity(Power));
		bBallInFlight = true;
		LastResult.UsedPower = Power;
	}
}

void AThrowPawn::FinishThrow(const FThrowResult& Result)
{
	LastResult = Result;
	bHasResult = true;
	bBallInFlight = false;

	if (Result.IsSuccess())
	{
		++SuccessCount;
	}

	if (ActiveBall)
	{
		ActiveBall->Destroy();
		ActiveBall = nullptr;
	}

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
}

// ── 파워 ↔ 거리 ──

FVector AThrowPawn::PowerToVelocity(float Power) const
{
	Power = FMath::Clamp(Power, 0.0f, 1.0f);

	// 방향: 타겟으로 자동 조준 (수평 방향만).
	const FVector Flat = CurrentTrial.TargetLocation - CurrentTrial.ThrowOrigin;
	const FVector Dir = FVector(Flat.X, Flat.Y, 0.0f).GetSafeNormal();

	// 파워 1.0 → MaxThrowRange 까지 가는 45도 발사로 환산.
	// 45도 사거리 R = v^2/g  →  v = sqrt(R*g). 파워로 사거리를 스케일.
	const float G = FMath::Abs(GetWorld()->GetGravityZ());
	const float Range = FMath::Max(Power * MaxThrowRange, 1.0f);
	const float Speed = FMath::Sqrt(Range * G);

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

	// VR: 컨트롤러 던지기 동작 인식 (파워는 손 속도로).
	if (bVR)
	{
		TickVRThrow(DeltaSeconds);
	}

	// 파워 충전 (키보드 모드).
	if (bCharging)
	{
		CurrentPower = FMath::Clamp(CurrentPower + DeltaSeconds / ChargeTime, 0.0f, 1.0f);
	}

	// 공이 착지했는지 확인 → 판정.
	if (bBallInFlight && ActiveBall)
	{
		if (ActiveBall->GetActorLocation().Z <= HomeLocation.Z - 88.0f + 5.0f)
		{
			const FThrowResult Result = FThrowJudge::Judge(
				ActiveBall->GetActorLocation(),
				CurrentTrial.TargetLocation,
				CurrentTrial.ThrowOrigin,
				CurrentTrial.HitRadius,
				LastResult.UsedPower);
			FinishThrow(Result);
		}
	}

	// 다음 타겟 대기.
	if (bWaitingNext && !bSessionOver)
	{
		IntervalTimer -= DeltaSeconds;
		if (IntervalTimer <= 0.0f)
		{
			bWaitingNext = false;
			SpawnNextTarget();
		}
	}

	// 타겟 마커 (사람 자리) — 캡슐 형태로 표시.
	if (!bSessionOver)
	{
		const FVector T = CurrentTrial.TargetLocation;
		DrawDebugCapsule(GetWorld(), T + FVector(0, 0, 88.0f), 88.0f, 34.0f,
			FQuat::Identity, FColor::Red, false, -1.0f, 0, 3.0f);
		DrawDebugCircle(GetWorld(), T + FVector(0, 0, 2.0f), CurrentTrial.HitRadius, 32,
			FColor::Yellow, false, -1.0f, 0, 3.0f, FVector(1, 0, 0), FVector(0, 1, 0), false);
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

	// 제목: 진행 + 성공 수.
	VrPanel->SetTitle(
		FString::Printf(TEXT("송구  %d / %d 구      성공 %d"),
			GetThrowNumber(), GetTotalThrows(), GetSuccessCount()),
		FColor(228, 233, 244));

	// 행0: 파워 게이지 (ASCII 바 — 폰트 글리프 걱정 없음).
	const int32 Cells = 10;
	const int32 Filled = FMath::Clamp(FMath::RoundToInt(CurrentPower * Cells), 0, Cells);
	const FString Bar = FString::Printf(TEXT("파워 [%s%s] %3.0f%%"),
		*FString::ChrN(Filled, TEXT('=')), *FString::ChrN(Cells - Filled, TEXT('.')),
		CurrentPower * 100.0f);
	VrPanel->SetRow(0, Bar, bCharging ? FColor(255, 190, 90) : FColor(150, 200, 255));
	VrPanel->HideRowsFrom(1);

	// 푸터: 직전 결과(색 포함), 없으면 조준 안내.
	FString Outcome; FLinearColor OColor;
	if (GetLastOutcomeText(Outcome, OColor))
	{
		VrPanel->SetFooter(Outcome, OColor.ToFColor(true));
	}
	else
	{
		VrPanel->SetFooter(TEXT("정면 타겟으로 자동 조준 — 파워(거리)만 맞추세요"), FColor(150, 156, 168));
	}

	// 힌트: 조작 안내.
	VrPanel->SetHint(TEXT("컨트롤러를 앞으로 던지면 송구 (손 속도 = 파워)   ·   M: 나가기"),
		FColor(110, 116, 128));
}

void AThrowPawn::TickVRThrow(float DeltaSeconds)
{
	if (ThrowCooldown > 0.0f)
	{
		ThrowCooldown = FMath::Max(0.0f, ThrowCooldown - DeltaSeconds);
	}
	if (!ThrowController)
	{
		return;
	}

	// 컨트롤러 속도 (cm/s) = 위치 변화량 / dt.
	const FVector Loc = ThrowController->GetComponentLocation();
	float Speed = 0.0f;
	if (bHasPrevControllerLoc && DeltaSeconds > KINDA_SMALL_NUMBER)
	{
		Speed = FVector::Dist(Loc, PrevControllerLoc) / DeltaSeconds;
	}
	PrevControllerLoc = Loc;
	bHasPrevControllerLoc = true;

	// 손 속도 → 파워(0~1). 게이지에 실시간으로 보여준다.
	const float SpeedPower = FMath::Clamp(
		(Speed - MinThrowSpeedCms) / FMath::Max(MaxThrowSpeedCms - MinThrowSpeedCms, 1.0f),
		0.0f, 1.0f);

	const bool bReady = !bBallInFlight && !bSessionOver && !bWaitingNext;
	if (bReady)
	{
		CurrentPower = SpeedPower;
	}

	// 던지는 동작(속도 임계 초과) 인식 → 그 파워로 송구.
	if (bReady && ThrowCooldown <= 0.0f && Speed >= ThrowTriggerSpeedCms)
	{
		ThrowBall(SpeedPower);
		ThrowCooldown = 0.6f;
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
		OutText  = FString::Printf(TEXT("훈련 종료!  성공 %d / %d"), SuccessCount, TotalThrows);
		OutColor = FLinearColor(0.40f, 0.85f, 0.45f, 1.0f);
		return true;
	}
	if (!bHasResult || bBallInFlight)
	{
		return false;
	}

	switch (LastResult.Outcome)
	{
	case EThrowOutcome::Ontarget:
		OutText = TEXT("명중!");            OutColor = FLinearColor(0.40f, 0.85f, 0.45f, 1.0f); return true;
	case EThrowOutcome::Short:
		OutText = TEXT("짧음 — 더 세게");   OutColor = FLinearColor(0.95f, 0.55f, 0.30f, 1.0f); return true;
	case EThrowOutcome::Over:
		OutText = TEXT("넘김 — 살살");      OutColor = FLinearColor(0.95f, 0.55f, 0.30f, 1.0f); return true;
	default:
		return false;
	}
}