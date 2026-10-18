#pragma once
#include "CoreMinimal.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"

// Track event cues, including short one-shots, so abandoning a run stops them.
namespace TotorisEventAudio
{
    inline TMap<TWeakObjectPtr<const UObject>, TArray<TWeakObjectPtr<UAudioComponent>>> Voices;
    inline UAudioComponent* Track(const UObject* Owner, UAudioComponent* Voice)
    {
        for (auto It = Voices.CreateIterator(); It; ++It)
            if (!It.Key().IsValid()) It.RemoveCurrent();
        auto& List = Voices.FindOrAdd(Owner);
        List.RemoveAll([](const auto& Item) { return !Item.IsValid() || !Item->IsPlaying(); });
        if (Voice) List.Add(Voice);
        return Voice;
    }
    inline void Stop(const UObject* Owner)
    {
        if (auto* List = Voices.Find(Owner))
            for (auto& Voice : *List) if (Voice.IsValid()) Voice->Stop();
        Voices.Remove(Owner);
    }
    inline UAudioComponent* PlaySound2D(const UObject* Owner, USoundBase* Sound, float Volume = 1.f)
    { return Track(Owner, UGameplayStatics::SpawnSound2D(Owner, Sound, Volume)); }
    inline UAudioComponent* SpawnSoundAtLocation(const UObject* Owner, USoundBase* Sound,
        FVector Location, FRotator Rotation = FRotator::ZeroRotator, float Volume = 1.f)
    { return Track(Owner, UGameplayStatics::SpawnSoundAtLocation(Owner, Sound, Location, Rotation, Volume)); }
    inline void PlaySoundAtLocation(const UObject* Owner, USoundBase* Sound, FVector Location, float Volume = 1.f)
    { SpawnSoundAtLocation(Owner, Sound, Location, FRotator::ZeroRotator, Volume); }
}
