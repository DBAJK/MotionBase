#include "Core/Defense/CatchBall/CatchBallHUD.h"
#include "Core/Defense/CatchBall/CatchBallPawn.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"

namespace
{
	// 색 팔레트 (AModeSelectHUD 톤에 맞춤)
	const FLinearColor PanelBg   (0.05f, 0.06f, 0.08f, 0.82f);
	const FLinearColor PanelLine (1.00f, 0.62f, 0.20f, 0.90f); // 앰버 테두리
	const FLinearColor ChipIdle  (0.14f, 0.16f, 0.20f, 0.90f);
	const FLinearColor ChipOn    (1.00f, 0.62f, 0.20f, 0.95f); // 선택된 유형
	const FLinearColor TextMain  (0.92f, 0.94f, 0.97f, 1.00f);
	const FLinearColor TextDim   (0.55f, 0.60f, 0.66f, 1.00f);
	const FLinearColor TextOnChip(0.05f, 0.06f, 0.08f, 1.00f);
	const FLinearColor Good      (0.40f, 0.85f, 0.45f, 1.00f);

	FString TypeLabel(ECatchBallType T)
	{
		switch (T)
		{
		case ECatchBallType::GroundBall: return TEXT("땅볼");
		case ECatchBallType::FlyBall:    return TEXT("뜬공");
		case ECatchBallType::LineDrive:  return TEXT("라인드라이브");
		default:                         return TEXT("랜덤");
		}
	}
}

void ACatchBallHUD::DrawPanel(float X, float Y, float W, float H, const FLinearColor& Fill, const FLinearColor& Border)
{
	DrawRect(Fill, X, Y, W, H);
	// 테두리 (얇은 사각 4변)
	const float T = 2.0f;
	DrawRect(Border, X, Y, W, T);
	DrawRect(Border, X, Y + H - T, W, T);
	DrawRect(Border, X, Y, T, H);
	DrawRect(Border, X + W - T, Y, T, H);
}

float ACatchBallHUD::DrawLabel(const FString& Text, float X, float Y, const FLinearColor& Color, float Scale)
{
	float TW = 0.0f, TH = 0.0f;
	GetTextSize(Text, TW, TH, GEngine->GetMediumFont(), Scale);
	DrawText(Text, Color, X, Y, GEngine->GetMediumFont(), Scale);
	return TW;
}

void ACatchBallHUD::DrawCentered(const FString& Text, float CenterX, float Y, const FLinearColor& Color, float Scale)
{
	float TW = 0.0f, TH = 0.0f;
	GetTextSize(Text, TW, TH, GEngine->GetMediumFont(), Scale);
	DrawText(Text, Color, CenterX - TW * 0.5f, Y, GEngine->GetMediumFont(), Scale);
}

void ACatchBallHUD::DrawHUD()
{
	Super::DrawHUD();

	// 이 HUD 는 포구 폰일 때만 그린다.
	ACatchBallPawn* Pawn = Cast<ACatchBallPawn>(GetOwningPawn());
	if (!Pawn || !Canvas)
	{
		return;
	}

	const float W = Canvas->SizeX;
	const float S = FMath::Clamp(W / 1920.0f, 0.7f, 1.4f); // 해상도 스케일

	// ── 상단 메인 패널 ──
	const float PanelW = 720.0f * S;
	const float PanelH = 132.0f * S;
	const float PanelX = (W - PanelW) * 0.5f;   // 상단 가운데
	const float PanelY = 28.0f * S;

	DrawPanel(PanelX, PanelY, PanelW, PanelH, PanelBg, PanelLine);

	// 진행/성공 (패널 상단 줄)
	const FString Progress = FString::Printf(TEXT("%d / %d 구"),
		Pawn->GetPitchNumber(), Pawn->GetTotalPitches());
	DrawLabel(Progress, PanelX + 20.0f * S, PanelY + 16.0f * S, TextMain, 1.1f * S);

	const FString SuccessStr = FString::Printf(TEXT("성공  %d"), Pawn->GetSuccessCount());
	DrawLabel(SuccessStr, PanelX + PanelW - 150.0f * S, PanelY + 16.0f * S, Good, 1.1f * S);

	// ── 유형 선택 칩 4개 (패널 하단 줄) ──
	const ECatchBallType Types[4] = {
		ECatchBallType::GroundBall, ECatchBallType::FlyBall,
		ECatchBallType::LineDrive,  ECatchBallType::Mixed
	};
	const ECatchBallType Cur = Pawn->GetSessionType();

	const float ChipsY = PanelY + 62.0f * S;
	const float ChipH  = 46.0f * S;
	const float Gap    = 10.0f * S;
	const float AreaX  = PanelX + 20.0f * S;
	const float AreaW  = PanelW - 40.0f * S;
	const float ChipW  = (AreaW - Gap * 3.0f) / 4.0f;

	for (int32 i = 0; i < 4; ++i)
	{
		const bool bOn = (Types[i] == Cur);
		const float CX = AreaX + i * (ChipW + Gap);

		DrawPanel(CX, ChipsY, ChipW, ChipH, bOn ? ChipOn : ChipIdle,
			bOn ? ChipOn : FLinearColor(0.3f, 0.33f, 0.38f, 0.8f));

		// 숫자키 + 이름
		const FString Label = FString::Printf(TEXT("%d  %s"), i + 1, *TypeLabel(Types[i]));
		DrawCentered(Label, CX + ChipW * 0.5f, ChipsY + 12.0f * S,
			bOn ? TextOnChip : TextMain, 0.85f * S);
	}

	// ── 하단 조작 안내 ──
	DrawCentered(TEXT("WASD 이동    Space 포구    1~4 유형 선택    M 나가기"),
		W * 0.5f, PanelY + PanelH + 12.0f * S, TextDim, 0.8f * S);

	// ── 마지막 판정 결과 (있으면 중앙에 크게) ──
	FString ResultLine;
	FLinearColor ResultColor;
	if (Pawn->GetLastOutcomeText(ResultLine, ResultColor))
	{
		DrawCentered(ResultLine, W * 0.5f, Canvas->SizeY * 0.42f, ResultColor, 1.6f * S);
	}
}