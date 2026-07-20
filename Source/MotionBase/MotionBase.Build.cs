using UnrealBuildTool;

public class MotionBase : ModuleRules
{
	public MotionBase(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// 카테고리별 하위 폴더(Data/ Analysis/ Actors/ ...)를 모듈 루트 기준
		// 경로로 #include 하므로 모듈 루트를 인클루드 경로에 추가.
		PublicIncludePaths.Add(ModuleDirectory);

		// 공개 의존성: 게임 로직 전반에서 쓰는 코어 모듈
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",   // Vive 컨트롤러 입력 매핑
			"HeadMountedDisplay", // OpenXR / MotionController
			"HTTP",            // (후반) 생성형 AI API 호출
			"Json",            // 결과 직렬화 / API 페이로드
			"JsonUtilities"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		// 계산 계층은 UE 렌더/액터에 비의존한 순수 로직으로 유지 (헤드셋 없이 단위 테스트 가능)
	}
}
