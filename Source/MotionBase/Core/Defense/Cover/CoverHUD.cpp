#include "Core/Defense/Cover/CoverHUD.h"
#include "Core/Defense/Cover/CoverPawn.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"

namespace
{
	const FLinearColor CvPanelBg   (0.05f, 0.06f, 0.08f, 0.82f);
	const FLinearColor CvPanelLine (1.00f, 0.62f, 0.20f, 0.90f);
	const FLinearColor CvTextMain  (0.92f, 0.94f, 0.97f, 1.00f);
	const FLinearColor CvTextDim   (0.55f, 0.60f, 0.66f, 1.00f);
	const FLinearColor CvGood      (0.40f, 0.85f, 0.45f, 1.00f);
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

	// ── 상단 패널 (진행/성공 + 상황 + 역할) ──
	const float PanelW = 940.0f * S;
	const float PanelH = 120.0f * S;
	const float PanelX = (W - PanelW) * 0.5f;
	const float PanelY = 28.0f * S;
	DrawPanel(PanelX, PanelY, PanelW, PanelH, CvPanelBg, CvPanelLine);

	const FString Progress = FString::Printf(TEXT("%d / %d 문제      성공 %d"),
		Pawn->GetTrialNumber(), Pawn->GetTotalTrials(), Pawn->GetSuccessCount());
	DrawLabel(Progress, PanelX + 20.0f * S, PanelY + 12.0f * S, CvTextDim, 0.85f * S);

	DrawCentered(Pawn->GetSituationText(), W * 0.5f, PanelY + 42.0f * S, CvTextMain, 1.15f * S);
	const FString RoleLine = FString::Printf(TEXT("당신은 [%s] — 어디를 백업?"), *Pawn->GetRoleText());
	DrawCentered(RoleLine, W * 0.5f, PanelY + 78.0f * S, CvGood, 1.0f * S);

	// ── 보기 4개 ──
	const int32 N = Pawn->GetOptionCount();
	const int32 Sel = Pawn->GetSelectedIndex();
	const int32 Correct = Pawn->GetRevealCorrectIndex();
	const bool  bAnswered = Pawn->IsAnswered();

	const float OptY0 = PanelY + PanelH + 44.0f * S;
	const float OptStep = 58.0f * S;
	for (int32 i = 0; i < N; ++i)
	{
		FLinearColor Col = CvTextMain;
		if (bAnswered)
		{
			Col = (i == Correct) ? CvGood : CvTextDim;
		}
		else if (i == Sel)
		{
			Col = FLinearColor(1.0f, 0.75f, 0.35f, 1.0f); // 커서 = 앰버
		}

		FString Line = FString::Printf(TEXT("%s %d. %s"),
			(!bAnswered && i == Sel) ? TEXT("▶") : TEXT("   "),
			i + 1, *Pawn->GetOptionText(i));
		if (bAnswered && i == Correct) { Line += TEXT("   (정답)"); }
		DrawCentered(Line, W * 0.5f, OptY0 + i * OptStep, Col, 1.2f * S);
	}

	// ── 결과 + 해설 ──
	FString Outcome;
	FLinearColor OColor;
	if (Pawn->GetLastOutcomeText(Outcome, OColor))
	{
		DrawCentered(Outcome, W * 0.5f, OptY0 + N * OptStep + 24.0f * S, OColor, 1.5f * S);
		if (bAnswered)
		{
			DrawCentered(Pawn->GetExplainText(), W * 0.5f, OptY0 + N * OptStep + 64.0f * S,
				CvTextDim, 0.85f * S);
		}
	}

	// ── 조작 안내 ──
	DrawCentered(TEXT("숫자키 1~4 선택 (VR: 컨트롤러로 겨누고 유지)    ·    M 나가기"),
		W * 0.5f, H - 40.0f * S, CvTextDim, 0.78f * S);
}