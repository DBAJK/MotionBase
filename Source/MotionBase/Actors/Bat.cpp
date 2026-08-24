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

	// 눈에 보이는 배트 — 그립(가는 손잡이) + 배럴(굵은 타격면) + 노브 로 실제 배트처럼 구성.
	// 배트는 그립(x=0)에서 +X 방향으로 뻗는다. 총 길이 ≈ 84cm.
	// 실린더 기본 메시: 로컬 Z축 길이 100cm·지름 100cm → rot(90,0,0) 으로 +X 로 눕히고,
	//   Z스케일=길이(m), XY스케일=지름(m) 으로 잡는다.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cyl(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sph(TEXT("/Engine/BasicShapes/Sphere.Sphere"));

	// 손잡이(그립): 가는 원기둥, 길이 46cm·지름 3.4cm, 중심 x=23.
	BatMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BatMesh"));
	BatMesh->SetupAttachment(MotionController);
	BatMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (Cyl.Succeeded())
	{
		BatMesh->SetStaticMesh(Cyl.Object);
		BatMesh->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));
		BatMesh->SetRelativeLocation(FVector(23.0f, 0.0f, 0.0f));
		BatMesh->SetRelativeScale3D(FVector(0.034f, 0.034f, 0.46f));
	}

	// 배럴(타격면): 굵은 원기둥, 길이 38cm·지름 6.6cm, 중심 x=65 (그립 끝~배트 끝).
	BarrelMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BarrelMesh"));
	BarrelMesh->SetupAttachment(MotionController);
	BarrelMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (Cyl.Succeeded())
	{
		BarrelMesh->SetStaticMesh(Cyl.Object);
		BarrelMesh->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));
		BarrelMesh->SetRelativeLocation(FVector(65.0f, 0.0f, 0.0f));
		BarrelMesh->SetRelativeScale3D(FVector(0.066f, 0.066f, 0.38f));
	}

	// 노브(그립 끝): 작은 구, 지름 5cm, x=0.
	KnobMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("KnobMesh"));
	KnobMesh->SetupAttachment(MotionController);
	KnobMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (Sph.Succeeded())
	{
		KnobMesh->SetStaticMesh(Sph.Object);
		KnobMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));
		KnobMesh->SetRelativeScale3D(FVector(0.05f, 0.05f, 0.05f));
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

FVector ABat::GetAimForwardVector() const
{
	return MotionController ? MotionController->GetForwardVector() : FVector::ForwardVector;
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
