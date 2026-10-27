#pragma once

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/GameUserSettings.h"
#include "Misc/ConfigCacheIni.h"
#include "Widgets/SWindow.h"

// Keep normal-window geometry separate from fullscreen resolution.
namespace TotorisWindowPlacement
{
    inline double NextCaptureTime = 0.;
    inline constexpr const TCHAR* Section = TEXT("Totoris.WindowPlacement");

    inline TSharedPtr<SWindow> GetWindow(const UWorld* World)
    {
        if (!World || World->WorldType == EWorldType::PIE || !GEngine || !GEngine->GameViewport)
            return nullptr;
        const auto Window = GEngine->GameViewport->GetWindow();
        return Window.IsValid() && Window->GetNativeWindow().IsValid() ? Window : nullptr;
    }

    inline void Capture(const UWorld* World, bool bForce = false)
    {
        const double Now = FPlatformTime::Seconds();
        if (!bForce && Now < NextCaptureTime) return;
        NextCaptureTime = Now + 1.;
        const auto Window = GetWindow(World);
        if (!Window.IsValid() || Window->GetWindowMode() != EWindowMode::Windowed ||
            Window->IsWindowMinimized() || Window->IsWindowMaximized() || !GConfig) return;
        const FVector2D Size = Window->GetClientSizeInScreen();
        const FVector2D Position = Window->GetPositionInScreen();
        if (Size.X < 320 || Size.Y < 180) return;
        const TCHAR* Keys[] = { TEXT("Width"), TEXT("Height"), TEXT("X"), TEXT("Y") };
        const int32 Values[] = { FMath::RoundToInt(Size.X), FMath::RoundToInt(Size.Y),
            FMath::RoundToInt(Position.X), FMath::RoundToInt(Position.Y) };
        bool bChanged = false;
        for (int32 Index = 0; Index < 4; ++Index)
        {
            int32 Previous = 0;
            if (!GConfig->GetInt(Section, Keys[Index], Previous, GGameUserSettingsIni) || Previous != Values[Index])
            {
                GConfig->SetInt(Section, Keys[Index], Values[Index], GGameUserSettingsIni);
                bChanged = true;
            }
        }
        if (bChanged) GConfig->Flush(false, GGameUserSettingsIni);
    }

    inline bool Read(FIntPoint& Size, FIntPoint& Position)
    {
        if (!GConfig || !FSlateApplication::IsInitialized() ||
            !GConfig->GetInt(Section, TEXT("Width"), Size.X, GGameUserSettingsIni) ||
            !GConfig->GetInt(Section, TEXT("Height"), Size.Y, GGameUserSettingsIni) ||
            !GConfig->GetInt(Section, TEXT("X"), Position.X, GGameUserSettingsIni) ||
            !GConfig->GetInt(Section, TEXT("Y"), Position.Y, GGameUserSettingsIni) ||
            Size.X < 320 || Size.Y < 180) return false;
        // GetWorkArea chooses the nearest remaining monitor after a display is removed.
        const FSlateRect Area = FSlateApplication::Get().GetWorkArea(FSlateRect(
            Position.X, Position.Y, float(Position.X) + Size.X, float(Position.Y) + Size.Y));
        if (Area.Right - Area.Left < 352 || Area.Bottom - Area.Top < 244) return false;
        Size.X = FMath::Min(Size.X, int32(Area.Right - Area.Left) - 32);
        Size.Y = FMath::Min(Size.Y, int32(Area.Bottom - Area.Top) - 64);
        Position.X = FMath::Clamp(Position.X, int32(Area.Left), int32(Area.Right) - Size.X - 16);
        Position.Y = FMath::Clamp(Position.Y, int32(Area.Top), int32(Area.Bottom) - Size.Y - 48);
        return true;
    }
}
