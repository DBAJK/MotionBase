#include "Core/Defense/Throw/ThrowHUD.h"
#include "Core/Defense/Throw/ThrowPawn.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"

namespace
{
	const FLinearColor TwPanelBg   (0.05f, 0.06f, 0.08f, 0.82f);
	const FLinearColor TwPanelLine (1.00f, 0.62f, 0.20f, 0.90f);
	const FLinearColor TwTextMain  (0.92f, 0.94f, 0.97f, 1.00f);
	const FLinearColor TwTextDim   (0.55f, 0.60f, 0.66f, 1.00f);
	const FLinearColor TwGood      (0.40f, 0.85f, 0.45f, 1.00f);

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
	const float PanelW = 560.0f * S;
	const float PanelH = 92.0f * S;
	const float PanelX = (W - PanelW) * 0.5f;
	const float PanelY = 28.0f * S;
	DrawPanel(PanelX, PanelY, PanelW, PanelH, TwPanelBg, TwPanelLine);

	const FString Progress = FString::Printf(TEXT("%d / %d"),
		Pawn->GetThrowNumber(), Pawn->GetTotalThrows());
	DrawLabel(Progress, PanelX + 20.0f * S, PanelY + 14.0f * S, TwTextMain, 1.1f * S);

	const FString SuccessStr = FString::Printf(TEXT("On-target  %d"), Pawn->GetSuccessCount());
	DrawLabel(SuccessStr, PanelX + PanelW - 140.0f * S, PanelY + 14.0f * S, TwGood, 1.1f * S);

	// ── 지정된 목표 베이스 (측정 지표 ①의 전제 — 어디로 던지는지가 항상 보여야 한다) ──
	const FString CallLine = FString::Printf(TEXT("THROW TO  %s"),
		*AThrowPawn::BaseName(Pawn->GetTargetBase()));
	DrawCentered(CallLine, PanelX + PanelW * 0.5f, PanelY + 42.0f * S,
		FLinearColor(1.0f, 0.75f, 0.35f, 1.0f), 1.3f * S);

	// ── 단계 안내 + 전환 시계 (측정 지표 ③) ──
	{
		FString StageLine;
		FLinearColor StageColor = TwTextDim;
		switch (Pawn->GetPhase())
		{
		case EThrowPhase::Feed:
			StageLine = TEXT("Catch the feed  (Space at the right time)");
			StageColor = FLinearColor(1.0f, 0.75f, 0.35f, 1.0f);
			break;
		case EThrowPhase::Ready:
		{
			const float Live = Pawn->GetLiveTransferTime();
			StageLine = (Live >= 0.0f)
				? FString::Printf(TEXT("Ball in hand - transfer %.2fs"), Live)
				: FString(TEXT("Ball in hand"));
			StageColor = TwGood;
			break;
		}
		case EThrowPhase::InFlight:
			StageLine = TEXT("Ball away...");
			break;
		default:
			StageLine = Pawn->GetLastMetricsLine();
			break;
		}
		if (!StageLine.IsEmpty())
		{
			DrawCentered(StageLine, PanelX + PanelW * 0.5f, PanelY + 68.0f * S, StageColor, 0.82f * S);
		}
	}

	// ── 세션 누적 측정값 (베이스별 정확도 · 평균 구속 · 평균 전환) ──
	{
		FString ByBase;
		const EBaseType Bases[4] = { EBaseType::First, EBaseType::Second, EBaseType::Third, EBaseType::Home };
		for (int32 i = 0; i < 4; ++i)
		{
			int32 A = 0, Su = 0;
			Pawn->GetBaseStats(Bases[i], A, Su);
			if (A <= 0) { continue; }
			if (!ByBase.IsEmpty()) { ByBase += TEXT("  "); }
			ByBase += FString::Printf(TEXT("%s %d/%d"), *AThrowPawn::BaseName(Bases[i]), Su, A);
		}

		const float AvgT = Pawn->GetAverageTransferSec();
		FString Summary = FString::Printf(TEXT("avg %.0f km/h"), Pawn->GetAverageReleaseKmh());
		if (AvgT >= 0.0f) { Summary += FString::Printf(TEXT("   transfer %.2fs"), AvgT); }
		if (!ByBase.IsEmpty()) { Summary = ByBase + TEXT("      ") + Summary; }

		DrawCentered(Summary, W * 0.5f, PanelY + PanelH + 10.0f * S, TwTextDim, 0.78f * S);
	}

	// ── 하단 파워 게이지 ──
	const float GaugeW = 560.0f * S;
	const float GaugeH = 32.0f * S;
	const float GaugeX = (W - GaugeW) * 0.5f;
	const float GaugeY = H - 90.0f * S;

	// 게이지 배경
	DrawPanel(GaugeX, GaugeY, GaugeW, GaugeH, GaugeBg, TwPanelLine);

	// 현재 파워 채움
	const float Power = FMath::Clamp(Pawn->GetCurrentPower(), 0.0f, 1.0f);
	DrawRect(GaugeFill, GaugeX + 2.0f, GaugeY + 2.0f, (GaugeW - 4.0f) * Power, GaugeH - 4.0f);

	// 정답 파워 표시선 (초록 세로선) — "여기서 떼라".
	// 목표 베이스마다 거리가 달라 정답 파워도 매번 바뀐다 → 매 시행 다시 그린다.
	{
		const float Ideal = FMath::Clamp(Pawn->GetIdealPower(), 0.0f, 1.0f);
		const float MarkX = GaugeX + 2.0f + (GaugeW - 4.0f) * Ideal;
		DrawRect(IdealMark, MarkX - 1.5f, GaugeY - 4.0f, 3.0f, GaugeH + 8.0f);
	}

	// 게이지 라벨
	DrawCentered(TEXT("Hold Space to charge power, release to throw"),
		W * 0.5f, GaugeY - 26.0f * S, TwTextDim, 0.8f * S);

	const FString PowerPct = FString::Printf(TEXT("Power %d%%"), FMath::RoundToInt(Power * 100.0f));
	DrawCentered(PowerPct, W * 0.5f, GaugeY + GaugeH + 6.0f * S,
		Pawn->IsCharging() ? GaugeFill : TwTextDim, 0.85f * S);

	// ── 조작 안내 ──
	DrawCentered(TEXT("Space: catch the feed, then hold/release to throw    -    M to exit"),
		W * 0.5f, H - 40.0f * S, TwTextDim, 0.75f * S);

	// ── 판정 결과 (중앙) ──
	FString ResultLine;
	FLinearColor ResultColor;
	if (Pawn->GetLastOutcomeText(ResultLine, ResultColor))
	{
		DrawCentered(ResultLine, W * 0.5f, H * 0.4f, ResultColor, 1.6f * S);

		const FString Metrics = Pawn->GetLastMetricsLine();
		if (!Metrics.IsEmpty())
		{
			DrawCentered(Metrics, W * 0.5f, H * 0.4f + 40.0f * S, TwTextDim, 0.9f * S);
		}
	}

	// ── 세션 종료 시 AI 운동 추천 ──
	const FString& Coaching = Pawn->GetCoachingText();
	if (!Coaching.IsEmpty())
	{
		float PY = H * 0.52f;
		DrawCentered(TEXT("AI exercise tips"), W * 0.5f, PY, FLinearColor(0.6f, 0.82f, 1.0f, 1.0f), 1.1f * S);
		PY += 34.0f * S;

		constexpr int32 MaxChars = 60;
		int32 i = 0;
		while (i < Coaching.Len())
		{
			DrawCentered(Coaching.Mid(i, MaxChars), W * 0.5f, PY, TwTextMain, 0.85f * S);
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
}