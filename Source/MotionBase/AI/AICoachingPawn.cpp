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
	// TextRender 는 자동 줄바꿈이 없다 → 글자수로 하드 랩(한글 한 글자=한 글리프라 안전).
	// (파일 고유 이름 — 유니티 빌드에서 다른 파일의 동명 헬퍼와 충돌하지 않게.)
	TArray<FString> WrapAiPanel(const FString& In, int32 MaxCharsPerLine, int32 MaxLines)
	{
		TArray<FString> Lines;
		int32 i = 0;
		const int32 Len = In.Len();
		while (i < Len && Lines.Num() < MaxLines)
		{
			Lines.Add(In.Mid(i, MaxCharsPerLine));
			i += MaxCharsPerLine;
		}
		if (i < Len && Lines.Num() > 0)
		{
			Lines.Last().Append(TEXT(" …"));
		}
		return Lines;
	}

	// 3D 텍스트는 한글 폰트가 없을 수 있어 구조 라벨은 영어로.
	const TCHAR* ModeEn(EGameModeId Mode, FName Drill)
	{
		switch (Mode)
		{
		case EGameModeId::Batting: return TEXT("Batting");
		case EGameModeId::Defense:
			// ⚠️ 리터럴로 비교하지 않는다 — 저장되는 종목 ID 는 UModeManager 가 정본이고,
			//    실제로 index 2 가 "Backup" → "BackupMove" 로 바뀌었을 때 여기가 함께
			//    안 고쳐져서 백업 세션이 조용히 "Fielding" 으로 떨어진 적이 있다.
			if (Drill == UModeManager::GetDefenseDrillIdName(0)) { return TEXT("Fielding - Catch"); }
			if (Drill == UModeManager::GetDefenseDrillIdName(1)) { return TEXT("Fielding - Throw"); }
			if (Drill == UModeManager::GetDefenseDrillIdName(2)) { return TEXT("Fielding - Backup"); }
			return TEXT("Fielding");
		default: return TEXT("Training");
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
		VrPanel->ShowBackCard(TEXT("EXIT - aim here & hold"), FColor(255, 190, 90));
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
		CoachingText = TEXT("No save data available.");
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
		CoachingText = TEXT("No records yet. Play a mode first.");
		return;
	}

	bHasData    = true;
	FocusReport = Focus->Report;
	FocusMode   = Focus->Mode;
	FocusDrill  = Focus->DrillId;

	// 만성 추세 — 같은 모드/세부종목만 모아 분석(축이 섞이지 않게).
	FocusChronic = UWeaknessDetector::AnalyzeTrend(History, FocusMode, /*Window=*/5, FocusDrill);

	// 결정론적 추천 (네트워크 불필요 — 항상 나온다).
	Drills = UDrillCatalog::RecommendWithHistory(FocusReport, FocusChronic, 3);

	// AI 코칭 문장 (키가 있으면). 도메인별로 코치 역할이 다르다.
	if (FeedbackService && FeedbackService->IsConfigured())
	{
		CoachingText = TEXT("Requesting AI coaching...");
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
		CoachingText = TEXT("AI text off - showing recommended drills:");
	}

	UE_LOG(LogMotionBase, Log, TEXT("[AICoaching] focus=%s drill=%s 약점 %d개, 드릴 %d개"),
		*UModeManager::GetModeDisplayName(FocusMode).ToString(), *FocusDrill.ToString(),
		FocusReport.Weaknesses.Num(), Drills.Num());
}

void AAICoachingPawn::HandleCoachingReady(bool bSuccess, const FString& Text)
{
	bAwaitingCoaching = false;
	CoachingText = Text; // 성공=코칭 문장, 실패=사유. 둘 다 그대로 보여준다.
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

	RefreshPanel();
}

void AAICoachingPawn::RefreshPanel()
{
	if (!VrPanel) { return; }

	VrPanel->SetTitle(FString::Printf(TEXT("AI Coaching   [%s]"), ModeEn(FocusMode, FocusDrill)),
		FColor(150, 210, 255));

	int32 Row = 0;

	// ⚠️ 컴팩트 상태 패널은 행이 4줄을 넘으면 푸터·힌트와 겹친다(SetStatusCompact 기준).
	//    그래서 코칭 2줄 + 드릴 2개로 압축한다 — 자세한 리포트는 데스크톱 결과 화면이 담당.
	constexpr int32 MaxContentRows = 4;

	if (!bHasData)
	{
		// 기록 없음 — 안내만.
		for (const FString& L : WrapAiPanel(CoachingText, 30, 2))
		{
			if (Row >= MaxContentRows) { break; }
			VrPanel->SetRow(Row++, L, FColor(228, 233, 244));
		}
		VrPanel->HideRowsFrom(Row);
		VrPanel->SetFooter(TEXT("Play a mode to get recommendations"), FColor(150, 156, 168));
		VrPanel->SetHint(TEXT("raise controller = menu  ·  aim card & hold = exit  ·  [M]"),
			FColor(150, 160, 175));
		return;
	}

	// 코칭 문장 2줄 (성공 시 한글 — KRFont 필요, 없으면 데스크톱 로그로 확인).
	for (const FString& L : WrapAiPanel(CoachingText, 30, 2))
	{
		if (Row >= MaxContentRows) { break; }
		VrPanel->SetRow(Row++, L, FColor(228, 233, 244));
	}

	// 추천 드릴 (남은 줄 안에서 최대 2개).
	for (const FTrainingDrill& D : Drills)
	{
		if (Row >= MaxContentRows) { break; }
		VrPanel->SetRow(Row++, D.CompactLabel(), FColor(255, 200, 120));
	}
	VrPanel->HideRowsFrom(Row);

	VrPanel->SetFooter(bAwaitingCoaching ? TEXT("Waiting for AI...") : TEXT("Recommended exercises"),
		FColor(150, 200, 255));
	VrPanel->SetHint(TEXT("raise controller = menu  ·  aim card & hold = exit  ·  [M]"),
		FColor(150, 160, 175));
}
