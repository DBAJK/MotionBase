#include "Core/Defense/Throw/ThrowHUD.h"
#include "Core/Defense/Throw/ThrowPawn.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"

namespace
{
	const FLinearColor PanelBg   (0.05f, 0.06f, 0.08f, 0.82f);
	const FLinearColor PanelLine (1.00f, 0.62f, 0.20f, 0.90f);
	const FLinearColor TextMain  (0.92f, 0.94f, 0.97f, 1.00f);
	const FLinearColor TextDim   (0.55f, 0.60f, 0.66f, 1.00f);
	const FLinearColor Good      (0.40f, 0.85f, 0.45f, 1.00f);

	const FLinearColor GaugeBg   (0.12f, 0.13f, 0.16f, 0.92f);
	const FLinearColor GaugeFill (1.00f, 0.62f, 0.20f, 0.95f); // 현재 파워 (앰버)
	const FLinearColor IdealMark (0.40f, 0.85f, 0.45f, 1.00f); // 정답 파워 표시선 (초록)
}

void AThrowHUD::DrawPanel(float X, float Y, float W, float H, const FLinearColor& Fill, const FLinearColor& Border)
{
	DrawRect(Fill, X, Y, W, H);
	const float T = 2.0f;
	DrawRect(Border, X, Y, W, T);
	DrawRect(Border, X, Y + H - T, W, T);
	DrawRect(Border, X, Y, T, H);
	DrawRect(Border, X + W - T, Y, T, H);
}

void AThrowHUD::DrawLabel(const FString& Text, float X, float Y, const FLinearColor& Color, float Scale)
{
	DrawText(Text, Color, X, Y, GEngine->GetMediumFont(), Scale);
}

void AThrowHUD::DrawCentered(const FString& Text, float CenterX, float Y, const FLinearColor& Color, float Scale)
{
	float TW = 0.0f, TH = 0.0f;
	GetTextSize(Text, TW, TH, GEngine->GetMediumFont(), Scale);
	DrawText(Text, Color, CenterX - TW * 0.5f, Y, GEngine->GetMediumFont(), Scale);
}

void AThrowHUD::DrawHUD()
{
	Super::DrawHUD();

	AThrowPawn* Pawn = Cast<AThrowPawn>(GetOwningPawn());
	if (!Pawn || !Canvas)
	{
		return;
	}

	const float W = Canvas->SizeX;
	const float H = Canvas->SizeY;
	const float S = FMath::Clamp(W / 1920.0f, 0.7f, 1.4f);

	// ── 상단 진행/성공 패널 ──
	const float PanelW = 420.0f * S;
	const float PanelH = 56.0f * S;
	const float PanelX = (W - PanelW) * 0.5f;
	const float PanelY = 28.0f * S;
	DrawPanel(PanelX, PanelY, PanelW, PanelH, PanelBg, PanelLine);

	const FString Progress = FString::Printf(TEXT("%d / %d 구"),
		Pawn->GetThrowNumber(), Pawn->GetTotalThrows());
	DrawLabel(Progress, PanelX + 20.0f * S, PanelY + 14.0f * S, TextMain, 1.1f * S);

	const FString SuccessStr = FString::Printf(TEXT("성공  %d"), Pawn->GetSuccessCount());
	DrawLabel(SuccessStr, PanelX + PanelW - 140.0f * S, PanelY + 14.0f * S, Good, 1.1f * S);

	// ── 하단 파워 게이지 ──
	const float GaugeW = 560.0f * S;
	const float GaugeH = 32.0f * S;
	const float GaugeX = (W - GaugeW) * 0.5f;
	const float GaugeY = H - 90.0f * S;

	// 게이지 배경
	DrawPanel(GaugeX, GaugeY, GaugeW, GaugeH, GaugeBg, PanelLine);

	// 현재 파워 채움
	const float Power = FMath::Clamp(Pawn->GetCurrentPower(), 0.0f, 1.0f);
	DrawRect(GaugeFill, GaugeX + 2.0f, GaugeY + 2.0f, (GaugeW - 4.0f) * Power, GaugeH - 4.0f);

	// 정답 파워 표시선 (초록 세로선) — "여기서 떼라"
	// 주: 정답 파워는 폰 내부값이라, 게이지 위에 표식만 그린다. Pawn 에 getter 가
	//     없으므로 여기선 현재 파워만 보여주고, 정답선은 아래 라벨로 안내한다.
	// (정답선을 그리려면 Pawn 에 GetIdealPower() 를 추가하면 된다 — 아래 참고)

	// 게이지 라벨
	DrawCentered(TEXT("Space 를 눌러 파워 충전 → 떼면 송구"),
		W * 0.5f, GaugeY - 26.0f * S, TextDim, 0.8f * S);

	const FString PowerPct = FString::Printf(TEXT("파워 %d%%"), FMath::RoundToInt(Power * 100.0f));
	DrawCentered(PowerPct, W * 0.5f, GaugeY + GaugeH + 6.0f * S,
		Pawn->IsCharging() ? GaugeFill : TextDim, 0.85f * S);

	// ── 조작 안내 ──
	DrawCentered(TEXT("M 나가기"), W * 0.5f, H - 40.0f * S, TextDim, 0.75f * S);

	// ── 판정 결과 (중앙) ──
	FString ResultLine;
	FLinearColor ResultColor;
	if (Pawn->GetLastOutcomeText(ResultLine, ResultColor))
	{
		DrawCentered(ResultLine, W * 0.5f, H * 0.4f, ResultColor, 1.6f * S);
	}
}