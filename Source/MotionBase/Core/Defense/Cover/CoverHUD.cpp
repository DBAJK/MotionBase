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
	const float PanelH = 134.0f * S;
	const float PanelX = (W - PanelW) * 0.5f;
	const float PanelY = 28.0f * S;
	DrawPanel(PanelX, PanelY, PanelW, PanelH, CvPanelBg, CvPanelLine);

	const FString Progress = FString::Printf(TEXT("Q %d / %d      Correct %d"),
		Pawn->GetTrialNumber(), Pawn->GetTotalTrials(), Pawn->GetSuccessCount());
	DrawLabel(Progress, PanelX + 20.0f * S, PanelY + 12.0f * S, CvTextDim, 0.85f * S);

	// 판단 시간(측정 지표 ②)을 흐르는 채로 보여준다 — 목표 시간을 넘기면 주황.
	{
		const float Live = Pawn->GetLiveDecisionSec();
		const float Shown = (Live >= 0.0f) ? Live : Pawn->GetLastDecisionSec();
		if (Shown >= 0.0f)
		{
			DrawLabel(FString::Printf(TEXT("decide  %.1fs"), Shown),
				PanelX + PanelW - 150.0f * S, PanelY + 12.0f * S,
				(Shown > 3.0f) ? FLinearColor(0.95f, 0.55f, 0.30f, 1.0f) : CvTextDim, 0.85f * S);
		}
	}

	// 1 케이스 = 타구 방향·종류 + 주자 상황 + 내 포지션. 세 줄을 모두 보여줘야 판단이 성립한다.
	DrawCentered(Pawn->GetSituationText(), W * 0.5f, PanelY + 38.0f * S, CvTextMain, 1.15f * S);
	DrawCentered(FString::Printf(TEXT("Runners: %s"), *Pawn->GetRunnersText()),
		W * 0.5f, PanelY + 66.0f * S, CvTextDim, 0.9f * S);
	const FString RoleLine = FString::Printf(TEXT("You: [%s]  -  what is your job?"), *Pawn->GetRoleText());
	DrawCentered(RoleLine, W * 0.5f, PanelY + 92.0f * S, CvGood, 1.0f * S);

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
		if (bAnswered && i == Correct) { Line += TEXT("   (correct)"); }
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

	// ── 세션 종료 시 AI 판단 코칭 + 추천 훈련 ──
	const FString& Coaching = Pawn->GetCoachingText();
	if (!Coaching.IsEmpty())
	{
		float PY = H * 0.60f;
		const float AvgD = Pawn->GetAverageDecisionSec();
		if (AvgD >= 0.0f)
		{
			DrawCentered(FString::Printf(TEXT("average decision time  %.1fs"), AvgD),
				W * 0.5f, PY, CvTextDim, 0.85f * S);
			PY += 28.0f * S;
		}

		DrawCentered(TEXT("AI judgment tips"), W * 0.5f, PY, FLinearColor(0.6f, 0.82f, 1.0f, 1.0f), 1.1f * S);
		PY += 34.0f * S;

		constexpr int32 MaxChars = 60;
		int32 i = 0;
		while (i < Coaching.Len())
		{
			DrawCentered(Coaching.Mid(i, MaxChars), W * 0.5f, PY, CvTextMain, 0.85f * S);
			PY += 26.0f * S;
			i += MaxChars;
		}

		for (const FTrainingDrill& D : Pawn->GetRecommendedDrills())
		{
			DrawCentered(FString::Printf(TEXT("- %s : %s"), *D.Name, *D.FocusCue),
				W * 0.5f, PY, FLinearColor(1.0f, 0.78f, 0.47f, 1.0f), 0.8f * S);
			PY += 24.0f * S;
		}
	}

	// ── 조작 안내 ──
	DrawCentered(TEXT("Press 1-4 to answer (VR: aim & hold)    -    M to exit"),
		W * 0.5f, H - 40.0f * S, CvTextDim, 0.78f * S);
}