#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CatchBall.generated.h"

class UStaticMeshComponent;
class UProjectileMovementComponent;

/**
 * 포구 훈련용 공 액터.
 *
 * 언리얼 ProjectileMovementComponent 로 궤적을 굴린다 (중력·속도 자동).
 * 낙구지점/도달시간 같은 "예측값"은 물리엔진이 알려주지 않으므로,
 * 발사 파라미터로부터 CatchBallPawn 이 따로 계산한다 (포물선 공식).
 *
 * 공은 판정/집계를 하지 않는다 — 날아가고, 다 지나가면 스스로 사라진다까지만.
 */
UCLASS()
class MOTIONBASE_API ACatchBall : public AActor
{
	GENERATED_BODY()

public:
	ACatchBall();

	virtual void Tick(float DeltaSeconds) override;

	/**
	 * 공을 발사한다.
	 * @param InVelocity  초기 속도 벡터 (cm/s). 방향·속력을 함께 담는다.
	 */
	void Launch(const FVector& InVelocity);

	/** 발사 후 경과 시간 (초). 타이밍 판정에 쓰인다. */
	float GetElapsedTime() const { return ElapsedTime; }

	/** 아직 날아가는 중인지. */
	bool IsInFlight() const { return bInFlight; }

	/**
	 * 착지면 높이를 소유 폰의 바닥에 맞춘다. **Launch 전에 부를 것.**
	 *
	 * ⚠️ 기본값 0(월드 원점)을 그대로 쓰면 안 되는 이유: 폰이 Z=0 이 아닌 곳에 스폰되면
	 *    (모드 전환은 직전 폰의 트랜스폼을 물려받는다) 공이 실제 바닥에 닿기도 전에
	 *    "착지"로 판정돼 공중에서 멈췄다가 사라지거나, 반대로 바닥을 뚫고 계속 떨어진다.
	 */
	void SetGroundZ(float InGroundZ) { GroundZ = InGroundZ; }

	/**
	 * 날아가는 동안 궤적 선을 남긴다 (헤드셋에서 공이 어디로 가는지 보이게).
	 * 작은 공(지름 7cm)은 VR 에서 눈에 잘 안 띄어, 선이 없으면 "안 날아온다"고 느낀다.
	 */
	void SetTrailVisible(bool bVisible) { bDrawTrail = bVisible; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "CatchBall")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, Category = "CatchBall")
	TObjectPtr<UProjectileMovementComponent> Movement;

private:
	float ElapsedTime = 0.0f;
	bool  bInFlight   = false;

	/** 바닥(Z) 이하로 내려가면 착지로 보고 정리한다. */
	UPROPERTY(EditAnywhere, Category = "CatchBall")
	float GroundZ = 0.0f;

	/** 착지 후 이 시간(초) 뒤 자동 소멸. */
	UPROPERTY(EditAnywhere, Category = "CatchBall")
	float DestroyDelayAfterLanding = 0.5f;

	float LandedTimer = 0.0f;
	bool  bLanded     = false;

	/** 궤적 선을 그릴지 (SetTrailVisible). VR 가독성용. */
	bool  bDrawTrail  = false;

	/** 직전 프레임 위치 — 궤적 선분을 잇는 기준. */
	FVector PrevTrailLoc = FVector::ZeroVector;
	bool    bHasPrevTrailLoc = false;
};