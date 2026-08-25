#include "Core/Defense/CatchBall/CatchBall.h"
#include "MotionBase.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"

ACatchBall::ACatchBall()
{
	PrimaryActorTick.bCanEverTick = true;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);

	// 엔진 기본 구 메시 — 별도 에셋 없이 바로 보인다. (나중에 야구공 메시로 교체)
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		Mesh->SetStaticMesh(SphereMesh.Object);
	}

	// 야구공 지름 ≈ 7.3cm. 기본 Sphere 는 지름 100cm → 0.073 스케일.
	Mesh->SetWorldScale3D(FVector(0.073f));
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetSimulatePhysics(false);

	// 궤적은 ProjectileMovement 가 굴린다.
	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->UpdatedComponent = Mesh;
	Movement->bAutoActivate = false;     // Launch() 전까지 대기
	Movement->ProjectileGravityScale = 1.0f;
	Movement->bShouldBounce = false;     // 지금은 튕김 없음 (원하면 true 로)
	Movement->bRotationFollowsVelocity = false;
}

void ACatchBall::BeginPlay()
{
	Super::BeginPlay();
}

void ACatchBall::Launch(const FVector& InVelocity)
{
	ElapsedTime = 0.0f;
	bInFlight   = true;
	bLanded     = false;
	LandedTimer = 0.0f;
	bHasPrevTrailLoc = false;

	// ProjectileMovement 에 초기 속도를 실어 활성화.
	Movement->Velocity = InVelocity;
	Movement->Activate();

	UE_LOG(LogMotionBase, Log, TEXT("CatchBall: 발사 v=(%.0f, %.0f, %.0f)"),
		InVelocity.X, InVelocity.Y, InVelocity.Z);
}

void ACatchBall::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bInFlight)
	{
		return;
	}

	// 궤적 선 — 공이 지나간 자리를 짧게 남긴다. 지름 7cm 공은 헤드셋에서 눈에 잘 띄지
	// 않아, 선이 없으면 "공이 안 온다/어디로 갔는지 모르겠다"가 된다.
	if (bDrawTrail)
	{
		const FVector Now = GetActorLocation();
		if (bHasPrevTrailLoc)
		{
			if (UWorld* W = GetWorld())
			{
				// LifeTime 0.6초 — 꼬리가 짧게 남았다 사라져 화면이 지저분해지지 않는다.
				DrawDebugLine(W, PrevTrailLoc, Now, FColor(255, 240, 150), false, 0.6f, 0, 2.5f);
			}
		}
		PrevTrailLoc = Now;
		bHasPrevTrailLoc = true;
	}

	if (!bLanded)
	{
		ElapsedTime += DeltaSeconds;

		// 바닥에 닿으면 착지 처리 (물리는 계속 떨어뜨리므로 여기서 멈춘다).
		if (GetActorLocation().Z <= GroundZ)
		{
			FVector Pos = GetActorLocation();
			Pos.Z = GroundZ;
			SetActorLocation(Pos);

			Movement->Deactivate();
			Movement->Velocity = FVector::ZeroVector;
			bLanded = true;
		}
	}
	else
	{
		LandedTimer += DeltaSeconds;
		if (LandedTimer >= DestroyDelayAfterLanding)
		{
			bInFlight = false;
			Destroy();
		}
	}
}