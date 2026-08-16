#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "CoverHUD.generated.h"

/**
 * 커버 훈련 전용 HUD (C++ Canvas).
 *
 * 빙의된 폰이 ACoverPawn 일 때만 그린다. 상단에 목표 베이스·남은 시간 바·
 * 진행/성공, 중앙에 판정 결과를 표시한다.
 */
UCLASS()
class MOTIONBASE_API ACoverHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
	void DrawPanel(float X, float Y, float W, float H, const FLinearColor& Fill, const FLinearColor& Border);
	void DrawLabel(const FString& Text, float X, float Y, const FLinearColor& Color, float Scale);
	void DrawCentered(const FString& Text, float CenterX, float Y, const FLinearColor& Color, float Scale);
};