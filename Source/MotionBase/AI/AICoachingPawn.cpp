#include "AI/AICoachingPawn.h"
#include "MotionBase.h"
#include "AI/AIFeedbackService.h"
#include "AI/DrillCatalog.h"
#include "Analysis/WeaknessDetector.h"
#include "Data/SessionResult.h"
#include "Core/ModeManager.h"
#include "Core/MotionBaseGameMode.h"
#include "UI/VRInfoPanel.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "Components/SceneComponent.h"
#include "MotionControllerComponent.h"
#include "GameFramework/PlayerController.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "Engine/World.h"

namespace
{
	// TextRender 는 자동 줄바꿈이 없다 → 글자 수로 접되, 가능하면 공백에서 끊는다
	// (글자 수로만 자르면 한글 어절이 중간에서 갈려 읽기 나쁘다).
	// (파일 고유 이름 — 유니티 빌드에서 다른 파일의 동명 헬퍼와 충돌하지 않게.)
	TArray<FString> WrapAiPanel(const FString& In, int32 MaxCharsPerLine)
	{
		TArray<FString> Lines;
		FString Rest = In.TrimStartAndEnd();
		while (!Rest.IsEmpty())
		{
			if (Rest.Len() <= MaxCharsPerLine)
			{
				Lines.Add(Rest);
				break;
			}

			// 한 줄 한도 안에서 가장 뒤쪽 공백을 찾는다. 줄이 너무 짧아지면(절반 미만) 그냥 한도에서 자른다.
			int32 Cut = INDEX_NONE;
			for (int32 i = MaxCharsPerLine; i > MaxCharsPerLine / 2; --i)
			{
				if (FChar::IsWhitespace(Rest[i]))
				{
					Cut = i;
					break;
				}
			}
			if (Cut == INDEX_NONE)
			{
				Cut = MaxCharsPerLine;
			}

			Lines.Add(Rest.Left(Cut).TrimEnd());
			Rest = Rest.Mid(Cut).TrimStart();
		}
		return Lines;
	}

	const TCHAR* ModeLabel(EGameModeId Mode, FName Drill)
	{
		switch (Mode)
		{
		case EGameModeId::Batting: return TEXT("타격");
		case EGameModeId::Defense:
			// ⚠️ 리터럴로 비교하지 않는다 — 저장되는 종목 ID 는 UModeManager 가 정본이고,
			//    실제로 index 2 가 "Backup" → "BackupMove" 로 바뀌었을 때 여기가 함께
			//    안 고쳐져서 백업 세션이 조용히 "Fielding" 으로 떨어진 적이 있다.
			if (Drill == UModeManager::GetDefenseDrillIdName(0)) { return TEXT("수비 - 포구"); }
			if (Drill == UModeManager::GetDefenseDrillIdName(1)) { return TEXT("수비 - 송구"); }
			if (Drill == UModeManager::GetDefenseDrillIdName(2)) { return TEXT("수비 - 백업"); }
			return TEXT("수비");
		default: return TEXT("훈련");
		}
	}
}

AAICoachingPawn::AAICoachingPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	VROrigin = CreateDefaultSubobject<USceneComponent>(TEXT("VROrigin"));
	SetRootComponent(VROrigin);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(VROrigin);

	PointerController = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("PointerController"));
	PointerController->SetupAttachment(VROrigin);
	PointerController->MotionSource = FName(TEXT("Right"));

	VrPanel = CreateDefaultSubobject<UVRInfoPanel>(TEXT("VrPanel"));
	VrPanel->SetupAttachment(VROrigin);
	VrPanel->SetPlacement(UVRInfoPanel::DefaultDistanceCm, UVRInfoPanel::DefaultHeightCm);
}

void AAICoachingPawn::BeginPlay()
{
	Super::BeginPlay();

	UHeadMountedDisplayFunctionLibrary::SetTrackingOrigin(EHMDTrackingOrigin::Stage);

	// 겨눔 화면이라 컨트롤러가 위를 향할 일이 드물다 → 나가기 제스처는 기본에 가깝게.
	ExitGesture.UpThreshold = 0.85f;
	ExitGesture.HoldSec     = 1.5f;

	if (VrPanel)
	{
		VrPanel->BuildPanel();
		VrPanel->SetStatusCompact();
		VrPanel->ShowBackCard(TEXT("나가기 - 여기를 겨눈 채 유지"), FColor(255, 190, 90));
	}

	// AI 코칭 서비스 (키가 없으면 결정론적 추천만 보여준다).
	FeedbackService = NewObject<UAIFeedbackService>(this);
	FeedbackService->OnFeedbackReady.AddDynamic(this, &AAICoachingPawn::HandleCoachingReady);

	BuildRecommendation();
	RefreshPanel();
}

void AAICoachingPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	// 개발용 키보드 보조 — 헤드셋만 있을 땐 안 쓰이지만 PC 확인에 유용.
	PlayerInputComponent->BindKey(EKeys::M, IE_Pressed, this, &AAICoachingPawn::ReturnToModeSelect);
}

void AAICoachingPawn::BuildRecommendation()
{
	UModeManager* MM = GetGameInstance() ? GetGameInstance()->GetSubsystem<UModeManager>() : nullptr;
	if (!MM)
	{
		bHasData = false;
		CoachingText = TEXT("저장된 기록이 없습니다.");
		return;
	}

	// 가장 최근의 '리포트 있는' 세션을 focus 로 잡는다.
	const TArray<FSessionResult>& History = MM->GetHistory();
	const FSessionResult* Focus = nullptr;
	for (int32 i = History.Num() - 1; i >= 0; --i)
	{
		if (History[i].Report.bValid)
		{
			Focus = &History[i];
			break;
		}
	}

	if (!Focus)
	{
		bHasData = false;
		CoachingText = TEXT("아직 기록이 없습니다. 먼저 한 모드를 플레이해보세요.");
		return;
	}

	bHasData        = true;
	FocusReport     = Focus->Report;
	FocusMode       = Focus->Mode;
	FocusDrill      = Focus->DrillId;
	FocusStartedAt  = Focus->StartedAt;
	FocusScore      = Focus->Average.TotalScore;
	FocusDifficulty = Focus->DifficultyLevel;
	FocusAttempts   = Focus->AttemptCount;

	// 만성 추세 — 같은 모드/세부종목만 모아 분석(축이 섞이지 않게).
	FocusChronic = UWeaknessDetector::AnalyzeTrend(History, FocusMode, /*Window=*/5, FocusDrill);

	// 결정론적 추천 (네트워크 불필요 — 항상 나온다).
	Drills = UDrillCatalog::RecommendWithHistory(FocusReport, FocusChronic, 3);

	// AI 코칭 문장 (키가 있으면). 도메인별로 코치 역할이 다르다.
	if (FeedbackService && FeedbackService->IsConfigured())
	{
		CoachingText = TEXT("AI 코칭 요청 중...");
		bAwaitingCoaching = true;

		if (FocusMode == EGameModeId::Defense)
		{
			// 수비도 만성 추세를 함께 넘긴다 — 바로 위에서 이미 분석해 둔 값이고(FocusChronic),
			// 리뷰 화면은 애초에 "이력을 읽는 화면"이라 추세를 빼면 존재 이유가 반쯤 사라진다.
			// ⚠️ 종목 ID 는 UModeManager 가 정본이다. 리터럴 "Backup" 과 비교하던 코드가
			//    ID 가 "BackupMove" 로 바뀐 뒤에도 남아 있어서, **백업 리뷰가 포구 코치
			//    프롬프트로 떨어지고 있었다** — 판단 훈련에 체력 처방을 하는 조합이라
			//    Backup 프롬프트가 존재하는 이유 자체를 무력화한다.
			if (FocusDrill == UModeManager::GetDefenseDrillIdName(1))      { FeedbackService->RequestThrowCoaching(FocusReport, Drills, FocusChronic); }
			else if (FocusDrill == UModeManager::GetDefenseDrillIdName(2)) { FeedbackService->RequestBackupCoaching(FocusReport, Drills, FocusChronic); }
			else                                                           { FeedbackService->RequestCatchCoaching(FocusReport, Drills, FocusChronic); }
		}
		else
		{
			FeedbackService->RequestSwingCoaching(FocusReport, Drills, FocusChronic);
		}
	}
	else
	{
		bAwaitingCoaching = false;
		CoachingText = TEXT("AI 코칭이 설정되지 않아 추천 운동만 표시합니다.");
	}

	UE_LOG(LogMotionBase, Log, TEXT("[AICoaching] focus=%s drill=%s 약점 %d개, 드릴 %d개"),
		*UModeManager::GetModeDisplayName(FocusMode).ToString(), *FocusDrill.ToString(),
		FocusReport.Weaknesses.Num(), Drills.Num());
}

void AAICoachingPawn::HandleCoachingReady(bool bSuccess, const FString& Text)
{
	bAwaitingCoaching = false;
	CoachingText = Text; // 성공=코칭 문장, 실패=사유. 둘 다 그대로 보여준다.

	// 새 문장은 첫 페이지부터 읽게 한다.
	PageIndex = 0;
	PageTimer = 0.0f;

	UE_LOG(LogMotionBase, Log, TEXT("[AICoaching] AI %s: %s"),
		bSuccess ? TEXT("수신") : TEXT("실패"), *Text);
}

void AAICoachingPawn::ReturnToModeSelect()
{
	if (AMotionBaseGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AMotionBaseGameMode>() : nullptr)
	{
		GM->ReturnToModeSelect();
	}
}

void AAICoachingPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// 패널을 플레이어 정면에 고정 배치(swimming 제거·이질감 제거).
	if (VrPanel && Camera)
	{
		VrPanel->UpdateComfortAnchor(Camera, UVRInfoPanel::DefaultDistanceCm,
			UVRInfoPanel::DefaultHeightCm, /*RecenterDeg=*/55.0f);
	}

	// 나가기 — 컨트롤러를 위로 들거나('제스처') '뒤로' 카드를 겨눠 유지(드웰).
	if (PointerController)
	{
		bool bExit = false;
		ExitGesture.Update(PointerController->GetForwardVector(), PointerController->IsTracked(),
			/*bAllowed=*/true, DeltaSeconds, bExit);
		if (bExit)
		{
			ReturnToModeSelect();
			return;
		}

		if (VrPanel && VrPanel->UpdateBackDwell(PointerController->GetComponentLocation(),
			PointerController->GetForwardVector(), PointerController->IsTracked(),
			DwellTimeSec, DwellAngleDeg, DeltaSeconds))
		{
			ReturnToModeSelect();
			return;
		}
	}

	// 코칭 문장·추천 운동 페이지 넘김.
	PageTimer += DeltaSeconds;
	if (PageTimer >= PageIntervalSec)
	{
		PageTimer = 0.0f;
		++PageIndex;
	}

	RefreshPanel();
}

void AAICoachingPawn::RefreshPanel()
{
	if (!VrPanel) { return; }

	VrPanel->SetTitle(FString::Printf(TEXT("AI 코칭   [%s]"), ModeLabel(FocusMode, FocusDrill)),
		FColor(150, 210, 255));

	// ⚠️ 컴팩트 상태 패널은 행이 4줄을 넘으면 푸터·힌트와 겹친다(SetStatusCompact 기준).
	//    그래서 "기준 세션 1줄 + 코칭 2줄 + 추천 운동 1줄"로 두고, 코칭·운동은 PageIntervalSec
	//    마다 다음 페이지로 넘긴다 — 자세한 리포트는 데스크톱 결과 화면이 담당.
	constexpr int32 MaxContentRows    = 4;
	constexpr int32 CharsPerLine      = 30;
	constexpr int32 CoachLinesPerPage = 2;
	const TCHAR* Hint = TEXT("컨트롤러 들기 = 메뉴  ·  카드 겨눈 채 유지 = 나가기  ·  [M]");

	int32 Row = 0;

	if (!bHasData)
	{
		// 기록 없음 — 안내만.
		for (const FString& L : WrapAiPanel(CoachingText, CharsPerLine))
		{
			if (Row >= MaxContentRows) { break; }
			VrPanel->SetRow(Row++, L, FColor(228, 233, 244));
		}
		VrPanel->HideRowsFrom(Row);
		VrPanel->SetFooter(TEXT("추천을 받으려면 먼저 한 모드를 플레이하세요"), FColor(150, 156, 168));
		VrPanel->SetHint(Hint, FColor(150, 160, 175));
		return;
	}

	// 기준 세션 — 어떤 기록을 보고 한 코칭인지. 이게 없으면 새로 플레이해도 화면이 바뀌었는지 알 수 없다.
	const EDifficultyLevel Level = static_cast<EDifficultyLevel>(
		FMath::Clamp(FocusDifficulty, 0, static_cast<int32>(EDifficultyLevel::Pro)));
	VrPanel->SetRow(Row++, FString::Printf(TEXT("%s  ·  %s  ·  %.0f점  ·  %d회"),
		*FocusStartedAt.ToString(TEXT("%m/%d %H:%M")),
		*UModeManager::GetDifficultyDisplayName(Level).ToString(),
		FocusScore, FocusAttempts), FColor(150, 200, 255));

	// 코칭 문장 — 페이지 단위로 2줄씩. 짧은 페이지도 빈 줄로 채워 추천 운동 줄 위치가 흔들리지 않게 한다.
	const TArray<FString> CoachLines = WrapAiPanel(CoachingText, CharsPerLine);
	const int32 CoachPages = FMath::Max(1, FMath::DivideAndRoundUp(CoachLines.Num(), CoachLinesPerPage));
	const int32 CoachPage  = PageIndex % CoachPages;
	for (int32 i = 0; i < CoachLinesPerPage; ++i)
	{
		const int32 LineIdx = CoachPage * CoachLinesPerPage + i;
		VrPanel->SetRow(Row++, CoachLines.IsValidIndex(LineIdx) ? CoachLines[LineIdx] : FString(),
			FColor(228, 233, 244));
	}

	// 추천 운동 — 한 번에 하나씩 돌려 보여준다.
	const int32 DrillIdx = (Drills.Num() > 0) ? (PageIndex % Drills.Num()) : INDEX_NONE;
	if (Drills.IsValidIndex(DrillIdx) && Row < MaxContentRows)
	{
		VrPanel->SetRow(Row++, Drills[DrillIdx].CompactLabel(CharsPerLine), FColor(255, 200, 120));
	}
	VrPanel->HideRowsFrom(Row);

	// 푸터 — 페이지 위치를 같이 보여줘야 "넘어가는 중"임을 안다.
	FString Footer;
	if (bAwaitingCoaching)
	{
		Footer = TEXT("AI 응답 대기 중...");
	}
	else
	{
		Footer = FString::Printf(TEXT("코칭 %d/%d"), CoachPage + 1, CoachPages);
		if (Drills.IsValidIndex(DrillIdx))
		{
			Footer += FString::Printf(TEXT("   ·   추천 운동 %d/%d"), DrillIdx + 1, Drills.Num());
		}
	}
	VrPanel->SetFooter(Footer, FColor(150, 200, 255));
	VrPanel->SetHint(Hint, FColor(150, 160, 175));
}
