#include "CoreMinimal.h"

#if WITH_EDITOR
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

// Explicit editor repair, never a runtime fallback or automatic asset rewrite.
static FAutoConsoleCommand RepairMinecraftSkeleton(
	TEXT("Totoris.RepairMinecraftSkeleton"), TEXT("Restore and save the Minecraft model's missing shared skeleton."),
	FConsoleCommandDelegate::CreateLambda([]
	{
		const FString Folder = TEXT("/Game/Totoris/Objects/Minecraft/");
		USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, *(Folder + TEXT("MC_CustomSkin_Model")));
		if (!Mesh || Mesh->GetRefSkeleton().GetNum() == 0) return;
		USkeleton* Skeleton = Mesh->GetSkeleton();
		if (!Skeleton)
		{
			Skeleton = LoadObject<USkeleton>(nullptr, *(Folder + TEXT("MC_CustomSkin_Skeleton")));
			if (!Skeleton)
			{
				UPackage* Package = CreatePackage(*(Folder + TEXT("MC_CustomSkin_Skeleton")));
				Skeleton = NewObject<USkeleton>(Package, TEXT("MC_CustomSkin_Skeleton"), RF_Public | RF_Standalone);
				if (!Skeleton->MergeAllBonesToBoneTree(Mesh)) return;
			}
			Mesh->Modify();
			Mesh->SetSkeleton(Skeleton);
		}
		TArray<UObject*> Assets = { Skeleton, Mesh };
		for (const TCHAR* Name : { TEXT("MC_CrouchPose"), TEXT("MC_LookDown"), TEXT("MC_PlaceBlock"), TEXT("MC_Walk") })
		{
			UAnimSequence* Clip = LoadObject<UAnimSequence>(nullptr, *(Folder + Name));
			if (!Clip || (Clip->GetSkeleton() && Clip->GetSkeleton() != Skeleton))
			{
				UE_LOG(LogTemp, Error, TEXT("Minecraft repair refused missing/incompatible clip %s"), Name);
				return;
			}
			Clip->Modify();
			Clip->SetSkeleton(Skeleton);
			Clip->PostEditChange();
			Assets.Add(Clip);
		}
		for (UObject* Asset : Assets)
		{
			Asset->MarkPackageDirty();
			FSavePackageArgs Args;
			Args.TopLevelFlags = RF_Public | RF_Standalone;
			const FString File = FPackageName::LongPackageNameToFilename(Asset->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
			const bool bSaved = UPackage::SavePackage(Asset->GetOutermost(), Asset, *File, Args);
			UE_LOG(LogTemp, Display, TEXT("Minecraft skeleton repair: %s saved=%d"), *Asset->GetName(), bSaved);
		}
	}));
#endif
