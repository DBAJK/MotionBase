using UnrealBuildTool;
using System.Collections.Generic;

public class MotionBaseTarget : TargetRules
{
	public MotionBaseTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		// UE 5.8 기본값 (Editor 타깃과 일치).
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("MotionBase");
	}
}
