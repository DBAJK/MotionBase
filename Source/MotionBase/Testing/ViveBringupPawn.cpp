#include "Testing/ViveBringupPawn.h"
#include "MotionBase.h"
#include "Actors/Bat.h"
#include "Analysis/SwingAnalyzer.h"
#include "Core/MotionBaseGameMode.h"
#include "Data/SwingSample.h"
#include "Input/MotionInputProvider.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HeadMountedDisplayFunctionLibrary.h"

AViveBringupPawn::AViveBringupPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SceneRoot);
	Camera->SetRelativeLocation(FVector(-300.0f, 0.0f, 170.0f));
	Camera->SetRelativeRotation(FRotator(-10.0f, 0.0f, 0.0f));
}

void AViveBringupPawn::BeginPlay()
{
	Super::BeginPlay();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// InputSource 는 ABat::BeginPlay 에서 읽히므로, 스폰을 지연시켜 먼저 지정한다.
	const FTransform SpawnTM = GetActorTransform();
	Bat = World->SpawnActorDeferred<ABat>(ABat::StaticClass(), SpawnTM, this, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

	if (!Bat)
	{
		UE_LOG(LogMotionBase, Warning, TEXT("Bringup: ABat 스폰 실패"));
		return;
	}

	Bat->SetInputSource(EInputSource::ViveController);
	Bat->FinishSpawning(SpawnTM);

	UE_LOG(LogMotionBase, Log, TEXT("Bringup: ABat 스폰 완료 (source=%d, hand=%s)"),
		static_cast<int32>(Bat->GetInputSource()), *Bat->GetHandMotionSource().ToString());
}

void AViveBringupPawn::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Bat)
	{
		Bat->Destroy();
		Bat = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void AViveBringupPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindKey(EKeys::H, IE_Pressed, this, &AViveBringupPawn::ToggleHand);
	PlayerInputComponent->BindKey(EKeys::R, IE_Pressed, this, &AViveBringupPawn::ResetPeak);
	PlayerInputComponent->BindKey(EKeys::M, IE_Pressed, this, &AViveBringupPawn::ReturnToModeSelect);
}

void AViveBringupPawn::ToggleHand()
{
	if (!Bat)
	{
		return;
	}

	// OpenXR 의 MotionSource 이름. ABat 기본값은 "Right".
	const FName Current = Bat->GetHandMotionSource();
	const FName Next = (Current == FName(TEXT("Right"))) ? FName(TEXT("Left")) : FName(TEXT("Right"));
	Bat->SetHandMotionSource(Next);

	ResetPeak();
}

void AViveBringupPawn::ResetPeak()
{
	SessionPeakMps = 0.0f;
	MaxTipDriftCm = 0.0f;
	bHasFirstTip = false;
}

void AViveBringupPawn::ReturnToModeSelect()
{
	if (AMotionBaseGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AMotionBaseGameMode>() : nullptr)
	{
		GM->ReturnToModeSelect();
	}
}

FString AViveBringupPawn::BuildDiagnosis(bool bHmdConnected, bool bHmdEnabled,
	bool bProviderOk, bool bTracking, int32 SampleCount) const
{
	// 사슬의 앞쪽부터 끊긴 지점을 찾는다. 앞이 끊겼는데 뒤를 보면 헛수고다.
	// ⚠️ 조각마다 TEXT() 로 감쌀 것 — TEXT("a") "b" 는 wide/narrow 혼합 연결이 된다.
	if (!bHmdConnected)
	{
		return TEXT("XR 장치 미연결 → SteamVR 실행 중인지, 설정>개발자에서 ")
			TEXT("'SteamVR 을 OpenXR 런타임으로 설정' 했는지 확인. 여기부터 뚫려야 나머지가 의미 있다.");
	}

	if (!bProviderOk)
	{
		return TEXT("ABat 의 provider 가 Vive 가 아니다 → ABat::BeginPlay 의 InputSource 분기 확인 ")
			TEXT("(Mock 으로 폴백됐을 가능성).");
	}

	if (!bTracking)
	{
		return TEXT("장치는 보이는데 컨트롤러가 추적되지 않음 → 컨트롤러 전원/페어링, ")
			TEXT("베이스 스테이션 시야 확인. [H] 로 좌/우 손 전환 (MotionSource 불일치일 수 있음).");
	}

	if (SampleCount <= 1)
	{
		return TEXT("추적은 되는데 샘플이 쌓이지 않음 → provider Tick 이 안 돌고 있다. ")
			TEXT("ABat 의 Tick 활성화와 Initialize() 성공 여부 확인.");
	}

	if (MaxTipDriftCm < 1.0f)
	{
		return TEXT("샘플은 쌓이는데 BatTip 이 제자리다 → 컨트롤러를 크게 흔들어 볼 것. ")
			TEXT("그래도 고정이면 트래킹 포즈가 갱신되지 않는 것.");
	}

	if (!bHmdEnabled)
	{
		return TEXT("정상 동작 중 (HMD 비활성) → 비착용형 구동에 해당한다. ")
			TEXT("BatTip 미분값이 나오므로 실측 캘리브레이션으로 넘어갈 수 있다.");
	}

	return TEXT("정상 동작 중 → 사슬이 끝까지 살아 있다. 다음: 트리거 입력 배선, ")
		TEXT("그리고 실측 캘리브레이션 (σt, d_max, v_min, v_target).");
}

void AViveBringupPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!GEngine)
	{
		return;
	}

	// ── XR 계층 ──
	const bool bHmdConnected = UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayConnected();
	const bool bHmdEnabled = UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayEnabled();
	const FName DeviceName = UHeadMountedDisplayFunctionLibrary::GetHMDDeviceName();

	// ── ABat / provider 계층 ──
	UMotionInputProvider* Provider = Bat ? Bat->GetInputProvider() : nullptr;
	const bool bProviderOk = Provider && Provider->GetSourceType() == EInputSource::ViveController;
	const bool bTracking = Bat && Bat->IsTracking();

	// ── 샘플 / 계산 계층 ──
	// 속도는 직접 구하지 않고 USwingAnalyzer 를 통과시킨다 — 계산 계층도 같이 검증된다.
	TArray<FSwingSample> Samples;
	int32 SampleCount = 0;
	float RingPeakMps = 0.0f;
	float LatestMps = 0.0f;
	float ElapsedSec = 0.0f;

	if (Provider)
	{
		ElapsedSec = Provider->GetElapsedSeconds();

		if (Provider->GetBatTipHistory(Samples))
		{
			SampleCount = Samples.Num();

			int32 PeakIdx = INDEX_NONE;
			RingPeakMps = USwingAnalyzer::FindPeakSpeed(Samples, PeakIdx);

			if (SampleCount >= 2)
			{
				LatestMps = USwingAnalyzer::ComputeSpeedMps(Samples[SampleCount - 2], Samples[SampleCount - 1]);
			}
		}
	}

	SessionPeakMps = FMath::Max(SessionPeakMps, RingPeakMps);

	// BatTip 이 실제로 움직였는지 — 값이 나온다고 트래킹이 살아 있는 건 아니다.
	const FVector TipLoc = Bat ? Bat->GetBatTipWorldLocation() : FVector::ZeroVector;
	if (!bHasFirstTip)
	{
		FirstTipLocation = TipLoc;
		bHasFirstTip = true;
	}
	MaxTipDriftCm = FMath::Max(MaxTipDriftCm, static_cast<float>(FVector::Dist(FirstTipLocation, TipLoc)));

	// ── 출력 ──
	const auto Mark = [](bool b) { return b ? TEXT("O") : TEXT("X"); };

	GEngine->AddOnScreenDebugMessage(20, 2.0f, FColor::White,
		TEXT("=== Vive 브링업 진단 ===   [H] 좌/우손   [R] 리셋   [M] 모드 선택"));

	GEngine->AddOnScreenDebugMessage(21, 2.0f, bHmdConnected ? FColor::Green : FColor::Red,
		FString::Printf(TEXT("1) XR 장치   연결 %s | 활성 %s | 이름 %s"),
			Mark(bHmdConnected), Mark(bHmdEnabled),
			DeviceName.IsNone() ? TEXT("(없음)") : *DeviceName.ToString()));

	GEngine->AddOnScreenDebugMessage(22, 2.0f, bProviderOk ? FColor::Green : FColor::Red,
		FString::Printf(TEXT("2) ABat      스폰 %s | provider %s | 손 %s"),
			Mark(Bat != nullptr),
			Provider ? (bProviderOk ? TEXT("Vive") : TEXT("Mock(폴백)")) : TEXT("없음"),
			Bat ? *Bat->GetHandMotionSource().ToString() : TEXT("-")));

	GEngine->AddOnScreenDebugMessage(23, 2.0f, bTracking ? FColor::Green : FColor::Red,
		FString::Printf(TEXT("3) 트래킹    IsTracking %s | 경과 %.1f초 | 샘플 %d개"),
			Mark(bTracking), ElapsedSec, SampleCount));

	GEngine->AddOnScreenDebugMessage(24, 2.0f, MaxTipDriftCm >= 1.0f ? FColor::Green : FColor::Yellow,
		FString::Printf(TEXT("4) BatTip    (%.1f, %.1f, %.1f) cm | 최대 이동 %.1f cm"),
			TipLoc.X, TipLoc.Y, TipLoc.Z, MaxTipDriftCm));

	GEngine->AddOnScreenDebugMessage(25, 2.0f, FColor::Cyan,
		FString::Printf(TEXT("5) 속도      현재 %.2f m/s | 링버퍼 피크 %.2f m/s | 세션 최고 %.2f m/s"),
			LatestMps, RingPeakMps, SessionPeakMps));

	GEngine->AddOnScreenDebugMessage(26, 2.0f, FColor::Orange,
		FString::Printf(TEXT("→ %s"),
			*BuildDiagnosis(bHmdConnected, bHmdEnabled, bProviderOk, bTracking, SampleCount)));
}
