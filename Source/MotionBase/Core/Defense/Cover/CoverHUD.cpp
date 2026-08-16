#include "Core/Defense/Cover/CoverHUD.h"
#include "Core/Defense/Cover/CoverPawn.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"

namespace
{
	const FLinearColor PanelBg   (0.05f, 0.06f, 0.08f, 0.82f);
	const FLinearColor PanelLine (1.00f, 0.62f, 0.20f, 0.90f);
	const FLinearColor TextMain  (0.92f, 0.94f, 0.97f, 1.00f);
	const FLinearColor TextDim   (0.55f, 0.60f, 0.66f, 1.00f);
	const FLinearColor Good      (0.40f, 0.85f, 0.45f, 1.00f);

	const FLinearColor BarBg     (0.12f, 0.13f, 0.16f, 0.92f);
	const FLinearColor BarFill   (1.00f, 0.62f, 0.20f, 0.95f); // 남은 시간
	const FLinearColor BarLow    (0.90f, 0.35f, 0.35f, 0.95f); // 시간 얼마 안 남음
}

void ACoverHUD::DrawPanel(float X, float Y, float W, float H, const FLinearColor& Fill, const FLinearColor& Border)
{
	DrawRect(Fill, X, Y, W, H);
	const float T = 2.0f;
	DrawRect(Border, X, Y, W, T);
	DrawRect(Border, X, Y + H - T, W, T);
	DrawRect(Border, X, Y, T, H);
	DrawRect(Border, X + W - T, Y, T, H);
}

void ACoverHUD::DrawLabel(const FString& Text, float X, float Y, const FLinearColor& Color, float Scale)
{
	DrawText(Text, Color, X, Y, GEngine->GetMediumFont(), Scale);
}

void ACoverHUD::DrawCentered(const FString& Text, float CenterX, float Y, const FLinearColor& Color, float Scale)
{
	float TW = 0.0f, TH = 0.0f;
	GetTextSize(Text, TW, TH, GEngine->GetMediumFont(), Scale);
	DrawText(Text, Color, CenterX - TW * 0.5f, Y, GEngine->GetMediumFont(), Scale);
}

void ACoverHUD::DrawHUD()
{
	Super::DrawHUD();

	ACoverPawn* Pawn = Cast<ACoverPawn>(GetOwningPawn());
	if (!Pawn || !Canvas)
	{
		return;
	}

	const float W = Canvas->SizeX;
	const float H = Canvas->SizeY;
	const float S = FMath::Clamp(W / 1920.0f, 0.7f, 1.4f);

	// ── 상단 패널 (목표 베이스 + 진행/성공) ──
	const float PanelW = 520.0f * S;
	const float PanelH = 96.0f * S;
	const float PanelX = (W - PanelW) * 0.5f;
	const float PanelY = 28.0f * S;
	DrawPanel(PanelX, PanelY, PanelW, PanelH, PanelBg, PanelLine);

	// 목표 베이스 (크게)
	const FString Target = FString::Printf(TEXT("%s 커버!"), *Pawn->GetTargetBaseName());
	DrawCentered(Target, W * 0.5f, PanelY + 12.0f * S, TextMain, 1.5f * S);

	// 진행 / 성공 (작게, 좌우)
	const FString Progress = FString::Printf(TEXT("%d / %d"),
		Pawn->GetTrialNumber(), Pawn->GetTotalTrials());
	DrawLabel(Progress, PanelX + 18.0f * S, PanelY + 14.0f * S, TextDim, 0.85f * S);

	const FString SuccessStr = FString::Printf(TEXT("성공 %d"), Pawn->GetSuccessCount());
	DrawLabel(SuccessStr, PanelX + PanelW - 110.0f * S, PanelY + 14.0f * S, Good, 0.85f * S);

	// ── 남은 시간 바 (패널 하단) ──
	const float BarW = PanelW - 36.0f * S;
	const float BarH = 18.0f * S;
	const float BarX = PanelX + 18.0f * S;
	const float BarY = PanelY + PanelH - BarH - 12.0f * S;

	const float TimeLeft  = FMath::Max(Pawn->GetTimeLeft(), 0.0f);
	const float TimeLimit = 3.0f; // 표시용 기준 (Pawn 기본 TimeLimit 과 맞춤)
	const float Ratio = FMath::Clamp(TimeLeft / TimeLimit, 0.0f, 1.0f);

	DrawRect(BarBg, BarX, BarY, BarW, BarH);
	DrawRect(Ratio < 0.34f ? BarLow : BarFill, BarX, BarY, BarW * Ratio, BarH);

	// ── 조작 안내 ──
	DrawCentered(TEXT("WASD 이동    정답 베이스로 제한 시간 안에!    M 나가기"),
		W * 0.5f, PanelY + PanelH + 12.0f * S, TextDim, 0.78f * S);

	// ── 판정 결과 (중앙) ──
	FString ResultLine;
	FLinearColor ResultColor;
	if (Pawn->GetLastOutcomeText(ResultLine, ResultColor))
	{
		DrawCentered(ResultLine, W * 0.5f, H * 0.4f, ResultColor, 1.7f * S);
	}
}