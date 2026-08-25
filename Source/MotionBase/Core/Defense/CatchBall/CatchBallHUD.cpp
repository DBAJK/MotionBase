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
		case ECatchBallType::GroundBall: return TEXT("Grounder");
		case ECatchBallType::FlyBall:    return TEXT("Fly ball");
		case ECatchBallType::LineDrive:  return TEXT("Line drive");
		default:                         return TEXT("Random");
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
	const FString Progress = FString::Printf(TEXT("%d / %d"),
		Pawn->GetPitchNumber(), Pawn->GetTotalPitches());
	DrawLabel(Progress, PanelX + 20.0f * S, PanelY + 16.0f * S, TextMain, 1.1f * S);

	const FString SuccessStr = FString::Printf(TEXT("Caught  %d"), Pawn->GetSuccessCount());
	DrawLabel(SuccessStr, PanelX + PanelW - 150.0f * S, PanelY + 16.0f * S, Good, 1.1f * S);

	// 타구 타입별 성공률 (측정 지표 ②) + 공 속도 배율 — 진행 줄 가운데.
	{
		FString ByType;
		const ECatchBallType StatTypes[3] =
			{ ECatchBallType::GroundBall, ECatchBallType::FlyBall, ECatchBallType::LineDrive };
		const TCHAR* Short[3] = { TEXT("GB"), TEXT("FB"), TEXT("LD") };
		for (int32 i = 0; i < 3; ++i)
		{
			int32 A = 0, Su = 0;
			Pawn->GetTypeStats(StatTypes[i], A, Su);
			if (A <= 0) { continue; }
			if (!ByType.IsEmpty()) { ByType += TEXT("  "); }
			ByType += FString::Printf(TEXT("%s %d/%d"), Short[i], Su, A);
		}
		const FString Line = ByType.IsEmpty()
			? FString::Printf(TEXT("speed x%.1f  ([ / ])"), Pawn->GetBallSpeedScale())
			: FString::Printf(TEXT("%s      speed x%.1f"), *ByType, Pawn->GetBallSpeedScale());
		DrawCentered(Line, PanelX + PanelW * 0.5f, PanelY + 18.0f * S, TextDim, 0.8f * S);
	}

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
	DrawCentered(TEXT("WASD move   Space catch   1-4 type   [ / ] ball speed   M exit"),
		W * 0.5f, PanelY + PanelH + 12.0f * S, TextDim, 0.8f * S);

	// ── 마지막 판정 결과 (있으면 중앙에 크게) ──
	FString ResultLine;
	FLinearColor ResultColor;
	if (Pawn->GetLastOutcomeText(ResultLine, ResultColor))
	{
		DrawCentered(ResultLine, W * 0.5f, Canvas->SizeY * 0.42f, ResultColor, 1.6f * S);
	}

	// ── 세션 종료 시 AI 운동 추천 (코칭 문장 + 추천 드릴) ──
	// ⚠️ 코칭·드릴은 한글이라 Korean 글리프가 있는 폰트에서만 제대로 보인다.
	//    (엔진 기본 MediumFont 는 한글이 없어 네모로 나올 수 있음 — 앱 전역 폰트 이슈.)
	const FString& Coaching = Pawn->GetCoachingText();
	if (!Coaching.IsEmpty())
	{
		float PY = Canvas->SizeY * 0.52f;
		DrawCentered(TEXT("AI exercise tips"), W * 0.5f, PY, FLinearColor(0.6f, 0.82f, 1.0f, 1.0f), 1.1f * S);
		PY += 34.0f * S;

		// Coaching sentences — rough wrap by character count.
		constexpr int32 MaxChars = 60;
		int32 i = 0;
		while (i < Coaching.Len())
		{
			DrawCentered(Coaching.Mid(i, MaxChars), W * 0.5f, PY, TextMain, 0.85f * S);
			PY += 26.0f * S;
			i += MaxChars;
		}

		// Recommended drills.
		for (const FTrainingDrill& D : Pawn->GetRecommendedDrills())
		{
			DrawCentered(FString::Printf(TEXT("- %s : %s"), *D.Name, *D.FocusCue),
				W * 0.5f, PY, FLinearColor(1.0f, 0.78f, 0.47f, 1.0f), 0.8f * S);
			PY += 24.0f * S;
		}
	}
}