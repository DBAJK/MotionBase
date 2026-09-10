#include "Core/Defense/Backup/FielderCrowd.h"
#include "Core/Defense/Backup/FielderMarker.h"
#include "MotionBase.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"

AFielderCrowd::AFielderCrowd()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

void AFielderCrowd::Clear()
{
	for (TPair<uint32, TObjectPtr<UInstancedStaticMeshComponent>>& Pair : Batches)
	{
		if (UInstancedStaticMeshComponent* Ism = Pair.Value)
		{
			Ism->ClearInstances();
		}
	}
}

UInstancedStaticMeshComponent* AFielderCrowd::GetOrCreateBatch(UStaticMesh* Mesh, uint8 Group,
	const FLinearColor& Color, UMaterialInterface* BaseMaterial)
{
	if (!Mesh)
	{
		return nullptr;
	}

	const uint32 Key = HashCombine(GetTypeHash(Mesh), static_cast<uint32>(Group));
	if (TObjectPtr<UInstancedStaticMeshComponent>* Found = Batches.Find(Key))
	{
		return *Found;
	}

	UInstancedStaticMeshComponent* Ism = NewObject<UInstancedStaticMeshComponent>(this);
	if (!Ism)
	{
		return nullptr;
	}

	Ism->SetupAttachment(SceneRoot);
	Ism->SetStaticMesh(Mesh);

	// ⚠️ 충돌 끔 — 원본 마커와 같은 이유다. 백업 존으로 뛰어가는 플레이어가 동료에게
	//    막히면 훈련이 성립하지 않고, 코스메틱 타구도 그냥 통과해야 한다.
	Ism->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Ism->SetCastShadow(false); // 그림자까지 6명분 그릴 이유가 없다 (여기가 부하의 원인이었다).

	// 런타임에 인스턴스를 넣으므로 Movable. Static 으로 두면 등록 이후 인스턴스 추가에
	// 대해 엔진이 경고를 낸다.
	Ism->SetMobility(EComponentMobility::Movable);

	if (UMaterialInterface* Base = BaseMaterial ? BaseMaterial : Mesh->GetMaterial(0))
	{
		if (UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(Base, this))
		{
			// BasicShapeMaterial 의 색 파라미터 이름은 "Color" (마커와 동일).
			Mid->SetVectorParameterValue(TEXT("Color"), Color);
			Ism->SetMaterial(0, Mid);
		}
	}

	Ism->RegisterComponent();
	Batches.Add(Key, Ism);
	return Ism;
}

void AFielderCrowd::Rebuild(const TArray<TObjectPtr<AFielderMarker>>& Markers)
{
	Clear();

	int32 InstanceCount = 0;
	TArray<FFielderPartInstance> Parts;

	for (const TObjectPtr<AFielderMarker>& Marker : Markers)
	{
		if (!IsValid(Marker) || !Marker->IsBodyVisible())
		{
			continue; // 본인 자리는 애초에 몸이 없다 (이름표만 남긴다).
		}

		Parts.Reset();
		Marker->CollectBodyParts(Parts);

		for (const FFielderPartInstance& Part : Parts)
		{
			UInstancedStaticMeshComponent* Ism =
				GetOrCreateBatch(Part.Mesh, Part.Group, Part.Color, Marker->GetShapeMaterial());
			if (!Ism)
			{
				continue;
			}

			// ⚠️ 이 액터는 원점에 아이덴티티로 스폰된다(ABackupPawn 이 그렇게 만든다).
			//    그래서 로컬 = 월드이고, 부위의 월드 트랜스폼을 그대로 넣어도 맞는다.
			//    액터를 옮기게 되면 여기서 역변환을 해야 한다.
			Ism->AddInstance(Part.WorldTransform);
			++InstanceCount;
		}

		// 인스턴스가 대신 그리므로 원본 부위 컴포넌트는 끈다 — 안 끄면 두 번 그려지고
		// 드로우콜을 줄이려는 목적 자체가 사라진다.
		Marker->SetBodyVisible(false);
	}

	UE_LOG(LogMotionBase, Log,
		TEXT("[FielderCrowd] 동료 몸 인스턴싱 — 인스턴스 %d개를 배치 %d개로 그린다 (이전: 컴포넌트 %d개)."),
		InstanceCount, Batches.Num(), InstanceCount);
}
