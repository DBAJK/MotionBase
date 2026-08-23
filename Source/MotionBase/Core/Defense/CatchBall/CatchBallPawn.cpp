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
#include "GameFramework/PlayerController.h"

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
	if (GloveMesh)
	{
		GloveMesh->SetVisibility(bVR); // 글러브는 VR 에서만 보인다.
	}

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

		StatusLine = FString::Printf(TEXT("%d / %d 구 — 잡을 준비!"), PitchIndex + 1, TotalPitches);
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
	case ECatchOutcome::Success: OutcomeText = TEXT("포구 성공!"); break;
	case ECatchOutcome::Miss:    OutcomeText = TEXT("헛손질"); break;
	default:                     OutcomeText = TEXT("놓침"); break;
	}
	StatusLine = FString::Printf(TEXT("%s  (거리 %.0fcm, 타이밍 %+.2fs)"),
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
	StatusLine = FString::Printf(TEXT("훈련 종료!  성공 %d / %d    (M: 모드 선택으로)"),
		SuccessCount, TotalPitches);
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
	case ECatchBallType::GroundBall: Flight = 1.2f; Trial.CatchRadius = 160.0f; break; // 낮고 빠름, 관대
	case ECatchBallType::FlyBall:    Flight = 2.4f; Trial.CatchRadius = 140.0f; break; // 높이 뜨고 김
	case ECatchBallType::LineDrive:  Flight = 1.0f; Trial.CatchRadius = 110.0f; break; // 빠르고 빡셈
	default: break;
	}

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
		TickVRCatch();
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
		OutText  = FString::Printf(TEXT("훈련 종료!  성공 %d / %d"), SuccessCount, TotalPitches);
		OutColor = FLinearColor(0.40f, 0.85f, 0.45f, 1.0f);
		return true;
	}

	switch (LastResult.Outcome)
	{
	case ECatchOutcome::Success:
		OutText = TEXT("포구 성공!");  OutColor = FLinearColor(0.40f, 0.85f, 0.45f, 1.0f); return true;
	case ECatchOutcome::Miss:
		OutText = TEXT("헛손질");      OutColor = FLinearColor(0.95f, 0.55f, 0.30f, 1.0f); return true;
	case ECatchOutcome::Dropped:
		OutText = TEXT("놓침");        OutColor = FLinearColor(0.90f, 0.35f, 0.35f, 1.0f); return true;
	default:
		return false;
	}
}

void ACatchBallPawn::SelectGround() { SessionType = ECatchBallType::GroundBall; }
void ACatchBallPawn::SelectFly()    { SessionType = ECatchBallType::FlyBall; }
void ACatchBallPawn::SelectLine()   { SessionType = ECatchBallType::LineDrive; }
void ACatchBallPawn::SelectRandom() { SessionType = ECatchBallType::Mixed; }