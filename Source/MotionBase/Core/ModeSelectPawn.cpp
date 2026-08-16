#include "Core/ModeSelectPawn.h"
#include "MotionBase.h"
#include "Core/ModeManager.h"
#include "Core/MotionBaseGameMode.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"

AModeSelectPawn::AModeSelectPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SceneRoot);
	// 타자 시점과 비슷한 높이 — 모드를 고르고 바로 타격으로 넘어가도 시점이 튀지 않는다.
	Camera->SetRelativeLocation(FVector(-400.0f, 0.0f, 170.0f));
	Camera->SetRelativeRotation(FRotator(-10.0f, 0.0f, 0.0f));
}

void AModeSelectPawn::BeginPlay()
{
	Super::BeginPlay();

	MenuModes = UModeManager::GetMenuModes();

	// 수비 세부 종목 4개 (요청: 포구 / 송구 / 풋워크·반응속도 / 백업 위치 판단).
	DefenseDrills = {
		FText::FromString(TEXT("포구")),
		FText::FromString(TEXT("송구")),
		FText::FromString(TEXT("풋워크 / 반응속도")),
		FText::FromString(TEXT("백업 위치 판단"))
	};

	CurrentPage = EMenuPage::TopModes;

	// 커서는 플레이 가능한 모드 위에서 시작한다.
	SelectedIndex = FindFirstImplementedIndex();
}

int32 AModeSelectPawn::FindFirstImplementedIndex() const
{
	for (int32 i = 0; i < MenuModes.Num(); ++i)
	{
		if (UModeManager::IsModeImplemented(MenuModes[i]))
		{
			return i;
		}
	}
	return 0;
}

int32 AModeSelectPawn::GetEntryCount() const
{
	return (CurrentPage == EMenuPage::TopModes) ? MenuModes.Num() : DefenseDrills.Num();
}

FText AModeSelectPawn::GetEntryName(int32 Index) const
{
	if (CurrentPage == EMenuPage::TopModes)
	{
		return MenuModes.IsValidIndex(Index)
			? UModeManager::GetModeDisplayName(MenuModes[Index]) : FText::GetEmpty();
	}
	return DefenseDrills.IsValidIndex(Index) ? DefenseDrills[Index] : FText::GetEmpty();
}

bool AModeSelectPawn::IsEntryAvailable(int32 Index) const
{
	if (CurrentPage == EMenuPage::TopModes)
	{
		return MenuModes.IsValidIndex(Index) && UModeManager::IsModeImplemented(MenuModes[Index]);
	}
	// 세부 종목은 선택 시 수비 훈련(DefensePawn)으로 진입 — 모두 활성.
	return true;
}

FText AModeSelectPawn::GetEntryDescription(int32 Index) const
{
	if (CurrentPage == EMenuPage::TopModes)
	{
		return MenuModes.IsValidIndex(Index)
			? UModeManager::GetModeDescription(MenuModes[Index]) : FText::GetEmpty();
	}

	static const TArray<FText> Descs = {
		FText::FromString(TEXT("타구를 받아내는 포구 동작 훈련")),
		FText::FromString(TEXT("포구 후 정확한 송구 동작 훈련")),
		FText::FromString(TEXT("첫 스텝 풋워크와 반응속도 훈련")),
		FText::FromString(TEXT("상황별 백업 위치 판단 훈련"))
	};
	return Descs.IsValidIndex(Index) ? Descs[Index] : FText::GetEmpty();
}

FText AModeSelectPawn::GetScreenSubtitle() const
{
	if (CurrentPage == EMenuPage::DefenseDrills)
	{
		return FText::FromString(TEXT("수비 훈련 - 세부 종목 선택    (Backspace: 뒤로)"));
	}
	return FText::FromString(TEXT("SporTrack : Baseball    비착용형 XR 야구 훈련"));
}

void AModeSelectPawn::SetPage(EMenuPage NewPage)
{
	CurrentPage = NewPage;
	NoticeText.Reset();
	NoticeTimer   = 0.0f;
	SelectedIndex = (NewPage == EMenuPage::TopModes) ? FindFirstImplementedIndex() : 0;
}

void AModeSelectPawn::GoBack()
{
	if (CurrentPage == EMenuPage::DefenseDrills)
	{
		SetPage(EMenuPage::TopModes);
		const int32 Idx = MenuModes.IndexOfByKey(EGameModeId::Defense);
		if (Idx != INDEX_NONE)
		{
			SelectedIndex = Idx; // 방금 나온 수비 카드에 커서를 놓는다.
		}
	}
}

void AModeSelectPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Enhanced Input 에셋 없이 키 직접 바인딩 (ASwingTestPawn 과 동일한 방식).
	PlayerInputComponent->BindKey(EKeys::Up, IE_Pressed, this, &AModeSelectPawn::SelectPrev);
	PlayerInputComponent->BindKey(EKeys::W, IE_Pressed, this, &AModeSelectPawn::SelectPrev);
	PlayerInputComponent->BindKey(EKeys::Down, IE_Pressed, this, &AModeSelectPawn::SelectNext);
	PlayerInputComponent->BindKey(EKeys::S, IE_Pressed, this, &AModeSelectPawn::SelectNext);

	PlayerInputComponent->BindKey(EKeys::Enter, IE_Pressed, this, &AModeSelectPawn::Confirm);
	PlayerInputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &AModeSelectPawn::Confirm);

	// 수비 세부 종목 화면 → 상위 목록 복귀.
	PlayerInputComponent->BindKey(EKeys::BackSpace, IE_Pressed, this, &AModeSelectPawn::GoBack);
}

void AModeSelectPawn::MoveSelection(int32 Delta)
{
	const int32 Count = GetEntryCount();
	if (Count == 0)
	{
		return;
	}

	// 위/아래로 순환. 미구현 모드도 커서는 올라간다 — 목록에서 감추면
	// 전체 구성이 안 보여서 심사·시연 때 설명이 어렵다.
	SelectedIndex = (SelectedIndex + Delta + Count) % Count;

	NoticeText.Reset();
	NoticeTimer = 0.0f;
}

void AModeSelectPawn::SelectPrev()
{
	MoveSelection(-1);
}

void AModeSelectPawn::SelectNext()
{
	MoveSelection(1);
}

void AModeSelectPawn::Confirm()
{
	AMotionBaseGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AMotionBaseGameMode>() : nullptr;

	// ── 수비 세부 종목 페이지: 고르면 수비 훈련으로 진입 ──
	// ── 수비 세부 종목 페이지: 고른 종목의 훈련 폰으로 진입 ──
	if (CurrentPage == EMenuPage::DefenseDrills)
	{
		if (GM)
		{
			// SelectedIndex: 0=포구, 1=송구, 2=풋워크/반응속도, 3=백업
			if (!GM->StartDefenseDrill(SelectedIndex))
			{
				// 아직 준비 중인 종목 — 안내만.
				NoticeText = FString::Printf(TEXT("%s — 아직 준비 중인 종목입니다."),
					*GetEntryName(SelectedIndex).ToString());
				NoticeTimer = NoticeDurationSec;
			}
		}
		return;
	}

	// ── 최상위 모드 페이지 ──
	const EGameModeId Mode = MenuModes.IsValidIndex(SelectedIndex)
		? MenuModes[SelectedIndex] : EGameModeId::Batting;

	// 수비를 고르면 세부 종목 선택 화면으로 전환 (실제 진입은 그 화면에서).
	if (Mode == EGameModeId::Defense)
	{
		SetPage(EMenuPage::DefenseDrills);
		return;
	}

	if (!UModeManager::IsModeImplemented(Mode))
	{
		NoticeText = FString::Printf(TEXT("%s — 아직 준비 중인 모드입니다."),
			*UModeManager::GetModeDisplayName(Mode).ToString());
		NoticeTimer = NoticeDurationSec;
		return;
	}

	if (!GM)
	{
		UE_LOG(LogMotionBase, Warning, TEXT("ModeSelect: AMotionBaseGameMode 를 찾지 못해 모드를 시작할 수 없습니다."));
		return;
	}

	GM->StartMode(Mode);
}

void AModeSelectPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (NoticeTimer > 0.0f)
	{
		NoticeTimer -= DeltaSeconds;
		if (NoticeTimer <= 0.0f)
		{
			NoticeText.Reset();
		}
	}
}
