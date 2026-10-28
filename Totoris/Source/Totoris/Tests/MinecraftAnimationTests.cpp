#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMinecraftCompressedAnimationTest,
	"Totoris.Minecraft.CompressedAnimation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::EngineFilter)

bool FMinecraftCompressedAnimationTest::RunTest(const FString& Parameters)
{
	struct FCase { const TCHAR* Clip; const TCHAR* Bone; float Time; };
	const FCase Cases[] = {
		{TEXT("MC_CrouchPose"), TEXT("Bone_001"), 0.f},
		{TEXT("MC_LookDown"), TEXT("Bone_002"), .3f},
		{TEXT("MC_PlaceBlock"), TEXT("Bone_010"), .25f},
		{TEXT("MC_Walk"), TEXT("Bone_006"), .25f},
		{TEXT("MC_Walk"), TEXT("Bone_008"), .25f}
	};
	for (const FCase& Case : Cases)
	{
		const FString Path = FString::Printf(TEXT("/Game/Totoris/Objects/Minecraft/%s.%s"), Case.Clip, Case.Clip);
		UAnimSequence* Clip = LoadObject<UAnimSequence>(nullptr, *Path);
		if (!TestNotNull(Path, Clip) || !TestNotNull(TEXT("Skeleton"), Clip->GetSkeleton())) continue;
#if WITH_EDITOR
		Clip->CacheDerivedDataForCurrentPlatform();
		Clip->WaitOnExistingCompression();
#endif
		const FReferenceSkeleton& Ref = Clip->GetSkeleton()->GetReferenceSkeleton();
		const int32 Index = Ref.FindBoneIndex(Case.Bone);
		if (!TestTrue(TEXT("Animated bone exists"), Index != INDEX_NONE)) continue;
		FAnimExtractContext Context(static_cast<double>(Case.Time));
#if WITH_EDITOR
		Context.bEnforceCompressedDataSampling = true;
#endif
		FTransform Pose = Ref.GetRefBonePose()[Index];
		Clip->GetBoneTransform(Pose, FSkeletonPoseBoneIndex(Index), Context, false);
		TestTrue(FString::Printf(TEXT("%s %s compressed rotation is animated"), Case.Clip, Case.Bone),
			Pose.GetRotation().AngularDistance(Ref.GetRefBonePose()[Index].GetRotation()) > .01f);
#if WITH_EDITOR
		FTransform Raw;
		Clip->GetBoneTransform(Raw, FSkeletonPoseBoneIndex(Index), FAnimExtractContext(static_cast<double>(Case.Time)), true);
		TestTrue(TEXT("Compressed rotation matches authored track"), Pose.GetRotation().AngularDistance(Raw.GetRotation()) < .01f);
#endif
	}
	return true;
}
#endif
