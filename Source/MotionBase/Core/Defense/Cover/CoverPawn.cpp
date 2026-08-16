#include "Core/Defense/Cover/CoverPawn.h"
#include "Core/Defense/Cover/CoverJudge.h"
#include "Core/MotionBaseGameMode.h"
#include "MotionBase.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/PlayerController.h"
#include "Core/Defense/Cover/CoverHUD.h"
#include "UI/ModeSelectHUD.h"

ACoverPawn::ACoverPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	Capsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	Capsule->InitCapsuleSize(34.0f, 88.0f);
	SetRootComponent(Capsule);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Capsule);
	Camera->SetRelativeLocation(FVector(0.0f, 0.0f, 70.0f));
	Camera->bUsePawnControlRotation = false;
}

void ACoverPawn::BeginPlay()
{
	Super::BeginPlay();

	SetActorRotation(FRotator::ZeroRotator);
	HomeLocation = GetActorLocation();

	SetupBaseLocations();
	StartSession();
}

void ACoverPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindKey(EKeys::D, IE_Pressed,  this, &ACoverPawn::OnRightPressed);
	PlayerInputComponent->BindKey(EKeys::D, IE_Released, this, &ACoverPawn::OnRightReleased);
	PlayerInputComponent->BindKey(EKeys::A, IE_Pressed,  this, &ACoverPawn::OnLeftPressed);
	PlayerInputComponent->BindKey(EKeys::A, IE_Released, this, &ACoverPawn::OnLeftReleased);
	PlayerInputComponent->BindKey(EKeys::W, IE_Pressed,  this, &ACoverPawn::OnFwdPressed);
	PlayerInputComponent->BindKey(EKeys::W, IE_Released, this, &ACoverPawn::OnFwdReleased);
	PlayerInputComponent->BindKey(EKeys::S, IE_Pressed,  this, &ACoverPawn::OnBackPressed);
	PlayerInputComponent->BindKey(EKeys::S, IE_Released, this, &ACoverPawn::OnBackReleased);
	PlayerInputComponent->BindKey(EKeys::M, IE_Pressed, this, &ACoverPawn::ReturnToModeSelect);

	// HUD 교체 (빙의 후). CoverHUD 는 다음 단계 — 지금은 주석.
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->ClientSetHUD(ACoverHUD::StaticClass());
	}
}

// ── 베이스 배치 ──

void ACoverPawn::SetupBaseLocations()
{
	// 홈 기준 다이아몬드. 플레이어 정면 +X, 우측 +Y.
	//   2루 = 앞,  1루 = 우앞,  3루 = 좌앞,  홈 = 발밑
	const float D = BaseDistance;
	BaseLocations[(int32)EBaseType::First]  = HomeLocation + FVector(D * 0.7f,  D * 0.7f, 0);
	BaseLocations[(int32)EBaseType::Second] = HomeLocation + FVector(D,         0.0f,     0);
	BaseLocations[(int32)EBaseType::Third]  = HomeLocation + FVector(D * 0.7f, -D * 0.7f, 0);
	BaseLocations[(int32)EBaseType::Home]   = HomeLocation + FVector(0.0f,      0.0f,     0);
}

FVector ACoverPawn::GetBaseLocation(EBaseType Base) const
{
	return BaseLocations[(int32)Base];
}

// ── 세션 ──

void ACoverPawn::StartSession()
{
	TrialIndex = 0;
	SuccessCount = 0;
	bSessionOver = false;
	bWaitingNext = false;
	bHasResult = false;
	IntervalTimer = 0.0f;

	SpawnNextTrial();
}

void ACoverPawn::SpawnNextTrial()
{
	// 정답 베이스 랜덤 (넷 중 하나).
	const EBaseType Target = (EBaseType)FMath::RandRange(0, 3);

	CurrentTrial = FCoverTrial();
	CurrentTrial.TargetBase     = Target;
	CurrentTrial.TargetLocation = GetBaseLocation(Target);
	CurrentTrial.TimeLimit      = TimeLimit;
	CurrentTrial.CoverRadius    = CoverRadius;

	TimeLeft = TimeLimit;
	bTrialActive = true;
	bHasResult = false;
}

void ACoverPawn::FinishTrial(const FCoverResult& Result)
{
	LastResult = Result;
	bHasResult = true;
	bTrialActive = false;

	if (Result.IsSuccess())
	{
		++SuccessCount;
	}

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

void ACoverPawn::EndSession()
{
	bSessionOver = true;
}

// ── 이동: Tick 의 플래그 처리로 대체됨 (BindKey 방식) ──

// ── 매 프레임 ──

void ACoverPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// 이동 (눌림 플래그만큼 매 프레임 이동).
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

	// 카운트다운 + 도착 체크.
	if (bTrialActive)
	{
		TimeLeft -= DeltaSeconds;

		// 정답 베이스 반경 안에 들어오면 즉시 성공.
		if (FCoverJudge::IsOnBase(GetActorLocation(), CurrentTrial.TargetLocation, CurrentTrial.CoverRadius))
		{
			const float Taken = CurrentTrial.TimeLimit - FMath::Max(TimeLeft, 0.0f);
			FinishTrial(FCoverJudge::JudgeArrival(
				GetActorLocation(), CurrentTrial.TargetLocation, CurrentTrial.CoverRadius, Taken));
		}
		// 시간 초과.
		else if (TimeLeft <= 0.0f)
		{
			FinishTrial(FCoverJudge::JudgeTimeout(
				GetActorLocation(), CurrentTrial.TargetLocation, CurrentTrial.CoverRadius, CurrentTrial.TimeLimit));
		}
	}

	// 다음 회 대기.
	if (bWaitingNext && !bSessionOver)
	{
		IntervalTimer -= DeltaSeconds;
		if (IntervalTimer <= 0.0f)
		{
			bWaitingNext = false;
			SpawnNextTrial();
		}
	}

	// ── 베이스 마커 그리기 ──
	if (!bSessionOver)
	{
		for (int32 i = 0; i < 4; ++i)
		{
			const bool bTarget = bTrialActive && ((int32)CurrentTrial.TargetBase == i);
			const FColor Col = bTarget ? FColor::Yellow : FColor(60, 60, 70);

			// 베이스 판(사각형 대용 원) + 정답이면 굵게.
			DrawDebugCircle(GetWorld(), BaseLocations[i] + FVector(0, 0, 2.0f),
				CoverRadius, 32, Col, false, -1.0f, 0, bTarget ? 5.0f : 2.0f,
				FVector(1, 0, 0), FVector(0, 1, 0), false);

			// 정답 베이스는 기둥으로 한 번 더 강조.
			if (bTarget)
			{
				DrawDebugLine(GetWorld(), BaseLocations[i],
					BaseLocations[i] + FVector(0, 0, 300.0f), FColor::Yellow, false, -1.0f, 0, 4.0f);
			}
		}
	}
}

void ACoverPawn::ReturnToModeSelect()
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

// ── HUD 접근자 ──

FString ACoverPawn::GetTargetBaseName() const
{
	switch (CurrentTrial.TargetBase)
	{
	case EBaseType::First:  return TEXT("1루");
	case EBaseType::Second: return TEXT("2루");
	case EBaseType::Third:  return TEXT("3루");
	default:                return TEXT("홈");
	}
}

bool ACoverPawn::GetLastOutcomeText(FString& OutText, FLinearColor& OutColor) const
{
	if (bSessionOver)
	{
		OutText  = FString::Printf(TEXT("훈련 종료!  성공 %d / %d"), SuccessCount, TotalTrials);
		OutColor = FLinearColor(0.40f, 0.85f, 0.45f, 1.0f);
		return true;
	}
	if (!bHasResult || bTrialActive)
	{
		return false;
	}

	switch (LastResult.Outcome)
	{
	case ECoverOutcome::Covered:
		OutText = TEXT("커버 성공!");        OutColor = FLinearColor(0.40f, 0.85f, 0.45f, 1.0f); return true;
	case ECoverOutcome::TooSlow:
		OutText = TEXT("시간 초과 — 더 빨리"); OutColor = FLinearColor(0.95f, 0.55f, 0.30f, 1.0f); return true;
	case ECoverOutcome::WrongBase:
		OutText = TEXT("베이스 잘못!");        OutColor = FLinearColor(0.90f, 0.35f, 0.35f, 1.0f); return true;
	default:
		return false;
	}
}