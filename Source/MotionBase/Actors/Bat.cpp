#include "Actors/Bat.h"
#include "MotionBase.h"
#include "Analysis/SwingAnalyzer.h"
#include "Input/MotionInputProvider.h"
#include "Input/ViveMotionInputProvider.h"
#include "MotionControllerComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

ABat::ABat()
{
	PrimaryActorTick.bCanEverTick = true;

	MotionController = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("MotionController"));
	SetRootComponent(MotionController);
	// 기본은 오른손. Enhanced Input / 플레이어 설정에서 교체 가능.
	MotionController->MotionSource = FName(TEXT("Right"));

	BatTip = CreateDefaultSubobject<USceneComponent>(TEXT("BatTip"));
	BatTip->SetupAttachment(MotionController);
	// 배트 길이 오프셋 (cm). 실제 배트/그립에 맞춰 조정.
	BatTip->SetRelativeLocation(FVector(80.0f, 0.0f, 0.0f));

	// 눈에 보이는 배트 (그립 → BatTip 방향의 가는 원기둥). VR 에서 손에 배트가 보인다.
	BatMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BatMesh"));
	BatMesh->SetupAttachment(MotionController);
	BatMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cyl(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (Cyl.Succeeded())
	{
		BatMesh->SetStaticMesh(Cyl.Object);
		// 실린더(로컬 Z축 100cm)를 +X 로 눕혀 길이 80cm·지름 ~6cm 배트로.
		BatMesh->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));
		BatMesh->SetRelativeLocation(FVector(40.0f, 0.0f, 0.0f));
		BatMesh->SetRelativeScale3D(FVector(0.06f, 0.06f, 0.8f));
	}
}

void ABat::BeginPlay()
{
	Super::BeginPlay();

	switch (InputSource)
	{
	case EInputSource::ViveController:
	{
		UViveMotionInputProvider* Vive = NewObject<UViveMotionInputProvider>(this);
		Vive->SetTrackedComponents(BatTip, MotionController);
		InputProvider = Vive;
		break;
	}
	default:
		// Mock / LiDAR 미구현 → PC 테스트용 Mock 으로 폴백.
		InputProvider = NewObject<UMockMotionInputProvider>(this);
		break;
	}

	if (InputProvider)
	{
		InputProvider->SetHistoryCapacity(RingBufferSize);
		const bool bOk = InputProvider->Initialize();
		UE_LOG(LogMotionBase, Log, TEXT("ABat: provider=%d 초기화 %s"),
			static_cast<int32>(InputProvider->GetSourceType()), bOk ? TEXT("성공") : TEXT("실패"));
	}

	// 액터 틱이 MotionController 컴포넌트 틱 뒤에 오도록 강제한다.
	// 안 그러면 provider 가 한 프레임 뒤처진 포즈를 읽어 속도(v_tip)가 부정확해진다.
	if (MotionController)
	{
		AddTickPrerequisiteComponent(MotionController);
	}
}

void ABat::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (InputProvider)
	{
		InputProvider->Shutdown();
	}
	Super::EndPlay(EndPlayReason);
}

bool ABat::IsTracking() const
{
	return InputProvider && InputProvider->IsTracking();
}

void ABat::SetHandMotionSource(FName NewSource)
{
	if (MotionController)
	{
		MotionController->MotionSource = NewSource;
		UE_LOG(LogMotionBase, Log, TEXT("ABat: MotionSource → %s"), *NewSource.ToString());
	}
}

FName ABat::GetHandMotionSource() const
{
	return MotionController ? MotionController->MotionSource : NAME_None;
}

FVector ABat::GetBatTipWorldLocation() const
{
	return BatTip ? BatTip->GetComponentLocation() : FVector::ZeroVector;
}

void ABat::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (InputProvider)
	{
		InputProvider->Tick(DeltaSeconds);
	}
}

void ABat::BeginSwingCapture(const FVector& InBallLocation, double InIdealContactWorldTime)
{
	if (!InputProvider)
	{
		return;
	}

	bCapturing = true;
	BallLocation = InBallLocation;

	// 샘플은 provider 시간축(Initialize 이후 경과초)으로 찍힌다.
	// 월드 시간으로 들어온 컨택 예정 시각을 같은 축으로 옮겨둔다.
	const double WorldNow = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	IdealContactProviderTime = InputProvider->GetElapsedSeconds() + (InIdealContactWorldTime - WorldNow);
}

FSwingMetrics ABat::EndSwingCaptureAndAnalyze()
{
	bCapturing = false;

	FSwingMetrics Metrics;

	TArray<FSwingSample> Samples;
	if (!InputProvider || !InputProvider->GetBatTipHistory(Samples))
	{
		UE_LOG(LogMotionBase, Verbose, TEXT("ABat: 궤적 샘플 없음 — 헛스윙 처리"));
		OnSwingCompleted.Broadcast(Metrics);
		return Metrics;
	}

	Metrics = USwingAnalyzer::AnalyzeSwing(Samples, BallLocation, IdealContactProviderTime);

	UE_LOG(LogMotionBase, Log, TEXT("Swing: contacted=%d speed=%.1f m/s dist=%.1f cm"),
		Metrics.bContacted, Metrics.ContactSpeedMps, Metrics.ContactDistanceCm);

	OnSwingCompleted.Broadcast(Metrics);
	return Metrics;
}
