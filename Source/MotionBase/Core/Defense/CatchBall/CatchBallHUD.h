#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "CatchBallHUD.generated.h"

/**
 * 포구 훈련 전용 HUD (C++ Canvas).
 *
 * 빙의된 폰이 ACatchBallPawn 일 때만 그린다. 상단에 유형 선택기·진행/성공
 * 상태를 네모 박스로 표시한다. AModeSelectHUD 와 같은 방식(디버그 텍스트 X).
 */
UCLASS()
class MOTIONBASE_API ACatchBallHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
	/** 채운 사각형 + 테두리. */
	void DrawPanel(float X, float Y, float W, float H, const FLinearColor& Fill, const FLinearColor& Border);

	/** 좌상단 기준 텍스트. 반환값은 그린 텍스트의 너비(px). */
	float DrawLabel(const FString& Text, float X, float Y, const FLinearColor& Color, float Scale);

	/** 텍스트를 특정 폭 안에서 가운데 정렬로 그림. */
	void DrawCentered(const FString& Text, float CenterX, float Y, const FLinearColor& Color, float Scale);
};