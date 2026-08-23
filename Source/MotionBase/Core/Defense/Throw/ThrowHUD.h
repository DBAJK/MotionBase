#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "ThrowHUD.generated.h"

/**
 * 송구 훈련 전용 HUD (C++ Canvas).
 *
 * 빙의된 폰이 AThrowPawn 일 때만 그린다. 상단에 진행/성공, 하단에 파워 게이지
 * (정답 파워 표시선 포함), 중앙에 판정 결과를 네모 박스로 표시한다.
 */
UCLASS()
class MOTIONBASE_API AThrowHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
	void DrawPanel(float X, float Y, float W, float H, const FLinearColor& Fill, const FLinearColor& Border);
	void DrawLabel(const FString& Text, float X, float Y, const FLinearColor& Color, float Scale);
	void DrawCentered(const FString& Text, float CenterX, float Y, const FLinearColor& Color, float Scale);
};