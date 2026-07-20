using UnrealBuildTool;
using System.Collections.Generic;

public class MotionBaseTarget : TargetRules
{
	public MotionBaseTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		// 설치형 엔진과 공유 빌드환경에서 경고레벨 충돌 방지 (UBT 권고).
		bOverrideBuildEnvironment = true;
		ExtraModuleNames.Add("MotionBase");
	}
}
