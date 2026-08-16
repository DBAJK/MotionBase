#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "DefensePawn.generated.h"

class UCameraComponent;
class USceneComponent;

/**
 * 수비 훈련 폰 (뼈대).
 *
 * 지금은 진입 확인용 — 화면에 안내만 띄우고 M 으로 모드 선택에 복귀한다.
 * LiDAR 전신 추적(FBodyPoseSample) → 분석 → 채점은 이후 단계에서 붙인다.
 */
UCLASS()
class MOTIONBASE_API ADefensePawn : public APawn
{
	GENERATED_BODY()

public:
	ADefensePawn();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	UPROPERTY(VisibleAnywhere, Category = "Defense")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Defense")
	TObjectPtr<UCameraComponent> Camera;

	/** 시작 화면(모드 선택)으로 복귀. */
	void ReturnToModeSelect();
};