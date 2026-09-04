#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Core/Defense/Backup/BackupTypes.h"
#include "BackupHUD.generated.h"

class ABackupPawn;
struct FBaseballField;

/**
 * 백업 위치 판단 전용 HUD (C++ Canvas).
 *
 * 빙의된 폰이 ABackupPawn 일 때만 그린다. 상단에 진행·정답 존 후보 안내, 중앙에
 * **탑다운 미니맵**(다이아몬드·수비 위치·정답 존·내 위치)을 그린다 — 헤드셋 없이
 * PC 에서 드릴 전체를 플레이·검증할 수 있는 핵심 도구다 (DrawDebug 는 3D 월드에만
 * 보이므로, 위에서 내려다본 요약은 이 HUD 가 유일하게 제공한다).
 */
UCLASS()
class MOTIONBASE_API ABackupHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
	void DrawPanel(float X, float Y, float W, float H, const FLinearColor& Fill, const FLinearColor& Border);
	void DrawLabel(const FString& Text, float X, float Y, const FLinearColor& Color, float Scale);
	void DrawCentered(const FString& Text, float CenterX, float Y, const FLinearColor& Color, float Scale);

	/** 월드 XY(cm) → 미니맵 화면 좌표. 홈이 아래쪽, 2루/외야가 위쪽으로 오게 뒤집는다. */
	FVector2D WorldToMap(const FVector2D& WorldXY, const FVector2D& MapCenter, float MapRadiusPx, float WorldRadiusCm) const;

	/** 미니맵 본체 — 다이아몬드·수비 위치·정답 존·후보 존·내 위치. */
	void DrawMinimap(ABackupPawn* Pawn, float CenterX, float CenterY, float RadiusPx);
};
