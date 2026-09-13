#include "Core/Defense/Backup/FielderMarker.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Math/RotationMatrix.h"
#include "UObject/ConstructorHelpers.h"

const FLinearColor AFielderMarker::SelfTint(1.00f, 0.72f, 0.25f, 1.0f);
const FLinearColor AFielderMarker::MateTint(0.93f, 0.95f, 1.00f, 1.0f);

namespace
{
	// ── 뼈대 관절 (cm) ──
	// 좌표계는 액터 로컬: +X 앞(홈 쪽), +Y 오른쪽, +Z 위. 원점 = 발밑 바닥.
	// 야수 준비 자세: 무릎을 굽혀 무게중심을 낮추고, 상체를 홈 쪽으로 살짝 숙이고,
	// 글러브 낀 왼손을 몸 앞으로 낸다. 이 상태의 키는 약 170cm (선 키로는 ~180cm).
	const FVector HipL   (-2.0f, -10.0f,  90.0f);
	const FVector KneeL  ( 5.0f, -15.0f,  52.0f);
	const FVector AnkleL ( 1.0f, -19.0f,  10.0f);
	const FVector HipR   (-2.0f,  10.0f,  90.0f);
	const FVector KneeR  ( 5.0f,  15.0f,  52.0f);
	const FVector AnkleR ( 1.0f,  19.0f,  10.0f);

	const FVector Pelvis ( 0.0f,   0.0f,  93.0f);
	const FVector Chest  ( 5.0f,   0.0f, 137.0f);

	const FVector ShoulderL( 4.0f, -18.0f, 135.0f);
	const FVector ElbowL   (13.0f, -24.0f, 111.0f);
	const FVector HandL    (24.0f, -18.0f,  94.0f);
	const FVector ShoulderR( 4.0f,  18.0f, 135.0f);
	const FVector ElbowR   (13.0f,  24.0f, 111.0f);
	const FVector HandR    (24.0f,  18.0f,  94.0f);

	const FVector NeckBase( 5.0f, 0.0f, 136.0f);
	const FVector NeckTop ( 6.0f, 0.0f, 144.0f);
	const FVector HeadC   ( 6.0f, 0.0f, 156.0f);

	/** 등번호 — 상의 뒤판. 상체가 앞으로 기울어 있어 그만큼 뒤로 밀어 놓는다. */
	const FVector BackNumberLoc(-14.0f, 0.0f, 118.0f);
	constexpr float BackNumberSize = 16.0f;

	/** 머리 위 이름표 — 2.5m 밖에서도 읽히도록 크게. */
	constexpr float LabelZ     = 192.0f;
	constexpr float LabelSize  = 26.0f;
	/** 본인 자리는 몸이 없으니 이름표를 낮게 — 시야를 가리지 않게. */
	constexpr float SelfLabelZ = 45.0f;

	FVector Lerp3(const FVector& A, const FVector& B, float T)
	{
		return A + (B - A) * T;
	}
}

AFielderMarker::AFielderMarker()
{
	// 이름표 방향은 소유 폰이 돌려 준다 (FaceLabelTowards) — 마커가 각자 틱하지 않는다.
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMatFinder(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	UStaticMesh* const Cyl = CylinderFinder.Succeeded() ? CylinderFinder.Object : nullptr;
	UStaticMesh* const Sph = SphereFinder.Succeeded() ? SphereFinder.Object : nullptr;
	ShapeMaterial = ShapeMatFinder.Succeeded() ? ShapeMatFinder.Object : nullptr;

	// ── 다리 ──
	// 니커즈(무릎까지 오는 바지) + 스타킹 — 야구 유니폼에서 제일 알아보기 쉬운 특징.
	MakeBone(TEXT("ThighL"), Cyl, EFielderPart::Pants, 17.0f, HipL, KneeL);
	MakeBone(TEXT("ThighR"), Cyl, EFielderPart::Pants, 17.0f, HipR, KneeR);
	// 바지단 — 무릎 바로 위에서 한 번 굵어졌다 끝난다.
	MakeBone(TEXT("CuffL"), Cyl, EFielderPart::Pants, 19.0f, Lerp3(HipL, KneeL, 0.84f), KneeL);
	MakeBone(TEXT("CuffR"), Cyl, EFielderPart::Pants, 19.0f, Lerp3(HipR, KneeR, 0.84f), KneeR);
	MakeBone(TEXT("SockL"), Cyl, EFielderPart::Accent, 12.0f, KneeL, AnkleL);
	MakeBone(TEXT("SockR"), Cyl, EFielderPart::Accent, 12.0f, KneeR, AnkleR);
	MakeBlob(TEXT("KneeL"), Sph, EFielderPart::Pants, FVector(8.0f), KneeL);
	MakeBlob(TEXT("KneeR"), Sph, EFielderPart::Pants, FVector(8.0f), KneeR);
	// 스파이크 — 발목보다 앞으로 나와야 발처럼 보인다.
	MakeBlob(TEXT("ShoeL"), Sph, EFielderPart::Shoe, FVector(11.0f, 6.0f, 5.0f), AnkleL + FVector(5.0f, 0.0f, -4.0f));
	MakeBlob(TEXT("ShoeR"), Sph, EFielderPart::Shoe, FVector(11.0f, 6.0f, 5.0f), AnkleR + FVector(5.0f, 0.0f, -4.0f));

	// ── 몸통 ──
	// 골반 가로대 — 없으면 허벅지 둘이 따로 노는 것처럼 보인다.
	MakeBone(TEXT("Hips"), Cyl, EFielderPart::Pants, 26.0f,
		FVector(0.0f, -10.0f, 90.0f), FVector(0.0f, 10.0f, 90.0f));
	// 원기둥은 테이퍼가 안 되니 허리(가늘게)/가슴(굵게) 두 단으로 나눠 체형을 낸다.
	MakeBone(TEXT("TorsoWaist"), Cyl, EFielderPart::Jersey, 28.0f, Pelvis, Lerp3(Pelvis, Chest, 0.45f));
	MakeBone(TEXT("TorsoChest"), Cyl, EFielderPart::Jersey, 31.0f, Lerp3(Pelvis, Chest, 0.42f), Chest);
	MakeBone(TEXT("Belt"), Cyl, EFielderPart::Accent, 32.0f, Pelvis, Lerp3(Pelvis, Chest, 0.09f));
	MakeBone(TEXT("Shoulders"), Cyl, EFielderPart::Jersey, 19.0f, ShoulderL, ShoulderR);

	// ── 팔 ──
	// 맨팔을 먼저 깔고 그 위에 반팔 소매를 덮는다 (소매가 더 굵어 자연히 감싼다).
	MakeBone(TEXT("UpperArmL"), Cyl, EFielderPart::Skin, 11.0f, ShoulderL, ElbowL);
	MakeBone(TEXT("UpperArmR"), Cyl, EFielderPart::Skin, 11.0f, ShoulderR, ElbowR);
	MakeBone(TEXT("SleeveL"), Cyl, EFielderPart::Jersey, 15.0f, ShoulderL, Lerp3(ShoulderL, ElbowL, 0.62f));
	MakeBone(TEXT("SleeveR"), Cyl, EFielderPart::Jersey, 15.0f, ShoulderR, Lerp3(ShoulderR, ElbowR, 0.62f));
	MakeBone(TEXT("ForearmL"), Cyl, EFielderPart::Skin, 10.0f, ElbowL, HandL);
	MakeBone(TEXT("ForearmR"), Cyl, EFielderPart::Skin, 10.0f, ElbowR, HandR);
	// 관절 구 — 이음매가 각져 보이지 않게 덮는다.
	MakeBlob(TEXT("ShoulderBallL"), Sph, EFielderPart::Jersey, FVector(9.5f), ShoulderL);
	MakeBlob(TEXT("ShoulderBallR"), Sph, EFielderPart::Jersey, FVector(9.5f), ShoulderR);
	MakeBlob(TEXT("ElbowBallL"), Sph, EFielderPart::Skin, FVector(5.5f), ElbowL);
	MakeBlob(TEXT("ElbowBallR"), Sph, EFielderPart::Skin, FVector(5.5f), ElbowR);
	// 글러브는 왼손, 오른손은 맨손.
	MakeBlob(TEXT("Glove"), Sph, EFielderPart::Glove, FVector(11.0f, 6.5f, 12.0f), HandL + FVector(3.0f, -3.0f, -1.0f));
	MakeBlob(TEXT("HandR"), Sph, EFielderPart::Skin, FVector(6.0f, 5.0f, 6.0f), HandR);

	// ── 머리 ──
	MakeBone(TEXT("Neck"), Cyl, EFielderPart::Skin, 11.0f, NeckBase, NeckTop);
	MakeBlob(TEXT("Head"), Sph, EFielderPart::Skin, FVector(12.5f, 12.0f, 13.5f), HeadC);
	// 모자 — 납작한 크라운 + 앞(+X)으로 나온 얇은 챙.
	MakeBlob(TEXT("CapCrown"), Sph, EFielderPart::Accent, FVector(12.8f, 12.8f, 8.0f), HeadC + FVector(0.0f, 0.0f, 6.0f));
	MakeBlob(TEXT("CapBrim"), Sph, EFielderPart::Accent, FVector(10.0f, 11.5f, 1.4f), HeadC + FVector(13.0f, 0.0f, 3.5f));

	// 등번호 — TextRender 는 컴포넌트 +X 가 읽히는 면이라(UVRInfoPanel 참고)
	// yaw 180 을 줘야 뒤(-X)에서 읽힌다.
	BackNumber = CreateDefaultSubobject<UTextRenderComponent>(TEXT("BackNumber"));
	BackNumber->SetupAttachment(Root);
	BackNumber->SetHorizontalAlignment(EHTA_Center);
	BackNumber->SetVerticalAlignment(EVRTA_TextCenter);
	BackNumber->SetWorldSize(BackNumberSize);
	BackNumber->SetRelativeLocation(BackNumberLoc);
	BackNumber->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));

	// 머리 위 이름표 — 매 프레임 플레이어 쪽으로 돌아간다.
	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(Root);
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetVerticalAlignment(EVRTA_TextCenter);
	Label->SetWorldSize(LabelSize);
	Label->SetRelativeLocation(FVector(0.0f, 0.0f, LabelZ));
}

UStaticMeshComponent* AFielderMarker::AddPart(const TCHAR* Name, UStaticMesh* Mesh, EFielderPart Group)
{
	UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(FName(Name));
	if (!Part)
	{
		return nullptr;
	}

	Part->SetupAttachment(Root);
	if (Mesh)
	{
		Part->SetStaticMesh(Mesh);
	}

	// ⚠️ 충돌 끔 — 백업 존으로 뛰어가는 플레이어가 동료에게 막히면 훈련이 성립하지 않는다.
	//    코스메틱 타구(ACatchBall)도 그냥 통과해야 한다.
	Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Part->SetSimulatePhysics(false);

	BodyParts.Add(Part);
	PartGroups.Add(static_cast<uint8>(Group));
	return Part;
}

void AFielderMarker::MakeBone(const TCHAR* Name, UStaticMesh* Mesh, EFielderPart Group,
	float DiameterCm, const FVector& A, const FVector& B)
{
	UStaticMeshComponent* Part = AddPart(Name, Mesh, Group);
	if (!Part)
	{
		return;
	}

	const FVector Delta = B - A;
	const float Length = Delta.Size();
	if (Length <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	// 기본 원기둥은 지름·높이 100cm, 피벗은 한가운데, 길이 방향은 로컬 +Z.
	// 회전자를 손으로 적지 않고 MakeFromZ 로 방향에서 직접 뽑는다 — 부호를 눈으로
	// 확인할 수 없는 상태에서 각도를 추측하면 관절이 꺾인 것처럼 보이기 쉽다.
	Part->SetRelativeRotation(FRotationMatrix::MakeFromZ(Delta / Length).Rotator());
	Part->SetRelativeScale3D(FVector(DiameterCm, DiameterCm, Length) / 100.0f);
	Part->SetRelativeLocation((A + B) * 0.5f);
}

void AFielderMarker::MakeBlob(const TCHAR* Name, UStaticMesh* Mesh, EFielderPart Group,
	const FVector& RadiiCm, const FVector& CenterCm)
{
	if (UStaticMeshComponent* Part = AddPart(Name, Mesh, Group))
	{
		// 기본 구는 지름 100cm — 반지름을 지름으로 바꿔 스케일을 만든다.
		Part->SetRelativeScale3D(RadiiCm * 2.0f / 100.0f);
		Part->SetRelativeLocation(CenterCm);
	}
}

FLinearColor AFielderMarker::ColorFor(EFielderPart Group) const
{
	switch (Group)
	{
	case EFielderPart::Jersey: return JerseyColor;
	case EFielderPart::Accent: return AccentColor;
	case EFielderPart::Pants:  return PantsColor;
	case EFielderPart::Skin:   return SkinColor;
	case EFielderPart::Glove:  return GloveColor;
	case EFielderPart::Shoe:   return ShoeColor;
	default:                   return JerseyColor;
	}
}

void AFielderMarker::BeginPlay()
{
	Super::BeginPlay();

	// 색 그룹마다 MID 를 하나만 만들어 여러 부위가 공유한다 — 부위가 30개 가까이라
	// 부위마다 MID 를 만들면 마커 하나에 30개, 필드 전체로는 180개가 된다.
	TMap<uint8, UMaterialInstanceDynamic*> MidByGroup;

	for (int32 i = 0; i < BodyParts.Num(); ++i)
	{
		UStaticMeshComponent* Part = BodyParts[i];
		if (!Part)
		{
			continue;
		}

		const uint8 Group = PartGroups.IsValidIndex(i) ? PartGroups[i] : 0;
		UMaterialInstanceDynamic** Found = MidByGroup.Find(Group);
		if (!Found)
		{
			UMaterialInterface* Base = ShapeMaterial ? ShapeMaterial.Get() : Part->GetMaterial(0);
			UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(Base, this);
			if (Mid)
			{
				// BasicShapeMaterial 의 색 파라미터 이름은 "Color" (엔진 에셋에서 확인).
				Mid->SetVectorParameterValue(TEXT("Color"), ColorFor(static_cast<EFielderPart>(Group)));
			}
			Found = &MidByGroup.Add(Group, Mid);
		}

		if (*Found)
		{
			Part->SetMaterial(0, *Found);
		}
	}

	if (BackNumber)
	{
		// 크림색 저지 위에 팀 컬러 번호.
		BackNumber->SetTextRenderColor(AccentColor.ToFColor(true));
	}
}

void AFielderMarker::CollectBodyParts(TArray<FFielderPartInstance>& Out) const
{
	Out.Reserve(Out.Num() + BodyParts.Num());

	for (int32 i = 0; i < BodyParts.Num(); ++i)
	{
		const UStaticMeshComponent* Part = BodyParts[i];
		if (!Part || !Part->GetStaticMesh())
		{
			continue;
		}

		FFielderPartInstance Inst;
		Inst.Mesh  = Part->GetStaticMesh();
		Inst.Group = PartGroups.IsValidIndex(i) ? PartGroups[i] : 0;
		Inst.Color = ColorFor(static_cast<EFielderPart>(Inst.Group));

		// 부위의 상대 트랜스폼은 생성자에서 이미 잡혀 있고, 스폰 시점에 컴포넌트가 등록되므로
		// 월드 트랜스폼(= 마커 배치 × 부위 상대)이 그대로 유효하다. 여기서 다시 곱하지 않는다.
		Inst.WorldTransform = Part->GetComponentTransform();

		Out.Add(Inst);
	}
}

void AFielderMarker::SetBodyVisible(bool bVisible)
{
	bBodyVisible = bVisible;
	for (UStaticMeshComponent* Part : BodyParts)
	{
		if (Part)
		{
			Part->SetVisibility(bVisible);
		}
	}

	if (BackNumber)
	{
		BackNumber->SetVisibility(bVisible);
	}
}

void AFielderMarker::Configure(EFieldPosition InPosition, const FString& InLabel, int32 InBackNumber, bool bIsSelfSpot)
{
	Position = InPosition;

	// 본인 자리엔 사람을 세우지 않는다 — 플레이어가 바로 그 자리에 서 있어서,
	// VR 이면 자기 머릿속에 몸이 박힌 채로 시야를 가린다. 돌아올 자리 표시로
	// 이름표만 낮게 남긴다.
	SetBodyVisible(!bIsSelfSpot);

	if (BackNumber && !bIsSelfSpot)
	{
		BackNumber->SetVisibility(InBackNumber > 0);
		BackNumber->SetText(FText::AsNumber(InBackNumber));
	}

	if (Label)
	{
		Label->SetText(FText::FromString(InLabel));
		Label->SetTextRenderColor((bIsSelfSpot ? SelfTint : MateTint).ToFColor(true));
		Label->SetRelativeLocation(FVector(0.0f, 0.0f, bIsSelfSpot ? SelfLabelZ : LabelZ));
	}
}

void AFielderMarker::FaceLabelTowards(const FVector& ViewerLocation)
{
	if (!Label)
	{
		return;
	}

	// TextRender 는 컴포넌트 +X 쪽이 읽히는 면이다 (UVRInfoPanel 이 yaw 180 을 주는 이유).
	// 뷰어 쪽으로 +X 를 돌리되 yaw 만 쓴다 — pitch 를 주면 글자가 기울어 읽기 나빠진다.
	FVector ToViewer = ViewerLocation - Label->GetComponentLocation();
	ToViewer.Z = 0.0f;
	if (ToViewer.IsNearlyZero())
	{
		return;
	}

	const float DesiredYaw = ToViewer.Rotation().Yaw;

	// ⚠️ 변화가 미미하면 건드리지 않는다. SetWorldRotation 은 렌더 상태를 더티로 만들어
	//    TextRender 지오메트리를 다시 올리는데, 이걸 마커 6개에 **매 프레임** 하고 있었다.
	//
	//    "움직이는 동안엔 어차피 계속 변하니 소용없지 않나"가 아니다 — 마커는 10~60m 밖에
	//    있어서 각속도가 작다. 30m 지점 마커를 7m/s 로 지나가도 프레임당 약 0.08° 라,
	//    1.5° 문턱이면 이동 중에도 갱신 횟수가 한 자릿수 분의 일로 줄어든다.
	//    글자 크기·거리를 감안하면 1.5° 틀어진 이름표는 눈에 띄지 않는다.
	if (bLabelYawApplied
		&& FMath::Abs(FRotator::NormalizeAxis(DesiredYaw - LastLabelYaw)) < LabelYawEpsilonDeg)
	{
		return;
	}

	LastLabelYaw = DesiredYaw;
	bLabelYawApplied = true;
	Label->SetWorldRotation(FRotator(0.0f, DesiredYaw, 0.0f));
}
