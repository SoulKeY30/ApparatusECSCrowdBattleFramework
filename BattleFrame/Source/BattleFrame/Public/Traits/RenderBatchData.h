#pragma once

#include "CoreMinimal.h"

// Niagara Systems
#include "NiagaraSystem.h" 
#include "NiagaraComponent.h"

#include "RenderBatchData.generated.h"


USTRUCT(BlueprintType, Category = "TraitRenderer")
struct BATTLEFRAME_API FRenderBatchData
{
    GENERATED_BODY()

    private:

    mutable std::atomic<bool> LockFlag{ false };

    public:

    void Lock() const
    {
        while (LockFlag.exchange(true, std::memory_order_acquire));
    }

    void Unlock() const
    {
        LockFlag.store(false, std::memory_order_release);
    }

    // Renderer
    UNiagaraComponent* SpawnedNiagaraSystem = nullptr;

    // Offset
    FVector OffsetLocation = FVector::ZeroVector;
    FRotator OffsetRotation = FRotator::ZeroRotator;
    FVector Scale = { 1.0f, 1.0f, 1.0f };

    // Pooling
    TArray<FTransform> Transforms;
    FBitMask ValidTransforms;
    TArray<int32> FreeTransforms;

    // Transform
    TArray<FVector> LocationArray;
    TArray<FQuat> OrientationArray;
    TArray<FVector> ScaleArray;

    // Anim To Texture
    TArray<FVector4> Anim_Index0_Index1_PauseTime0_PauseTime1_Array; // Dynamic params 0
    TArray<FVector4> Anim_TimeStamp0_TimeStamp1_PlayRate0_Playrate1_Array; // Dynamic params 1
    TArray<float> Anim_Lerp_Array; // lerp progress

    // Material FX
    TArray<FVector4> Mat_HitGlow_Freeze_Burn_Dissolve_Array; // Dynamic params 2

    // HealthBar
    TArray<FVector> HealthBar_Opacity_CurrentRatio_TargetRatio_Array;

    // TextPopUp
    TArray<FVector> Text_Location_Array;
    TArray<FVector4> Text_Value_Style_Scale_Offset_Array;

    // Other
    TArray<bool> InsidePool_Array;


    FRenderBatchData(){};

    FRenderBatchData(const FRenderBatchData& Data)
    {
        LockFlag.store(Data.LockFlag.load());

        SpawnedNiagaraSystem = Data.SpawnedNiagaraSystem;

        Transforms=Data.Transforms;
        ValidTransforms=Data.ValidTransforms;
        FreeTransforms=Data.FreeTransforms;

        LocationArray=Data.LocationArray;
        OrientationArray=Data.OrientationArray;
        ScaleArray = Data.ScaleArray;

        Anim_Index0_Index1_PauseTime0_PauseTime1_Array = Data.Anim_Index0_Index1_PauseTime0_PauseTime1_Array;
        Anim_TimeStamp0_TimeStamp1_PlayRate0_Playrate1_Array = Data.Anim_TimeStamp0_TimeStamp1_PlayRate0_Playrate1_Array;
        Anim_Lerp_Array = Data.Anim_Lerp_Array;

        Mat_HitGlow_Freeze_Burn_Dissolve_Array = Data.Mat_HitGlow_Freeze_Burn_Dissolve_Array;

        HealthBar_Opacity_CurrentRatio_TargetRatio_Array = Data.HealthBar_Opacity_CurrentRatio_TargetRatio_Array;

        Text_Location_Array = Data.Text_Location_Array;
        Text_Value_Style_Scale_Offset_Array = Data.Text_Value_Style_Scale_Offset_Array;

        InsidePool_Array = Data.InsidePool_Array;
    }

    FRenderBatchData& operator=(const FRenderBatchData& Data)
    {
        LockFlag.store(Data.LockFlag.load());

        SpawnedNiagaraSystem = Data.SpawnedNiagaraSystem;

        Transforms = Data.Transforms;
        ValidTransforms = Data.ValidTransforms;
        FreeTransforms = Data.FreeTransforms;

        LocationArray = Data.LocationArray;
        OrientationArray = Data.OrientationArray;
        ScaleArray = Data.ScaleArray;

        Anim_Index0_Index1_PauseTime0_PauseTime1_Array = Data.Anim_Index0_Index1_PauseTime0_PauseTime1_Array;
        Anim_TimeStamp0_TimeStamp1_PlayRate0_Playrate1_Array = Data.Anim_TimeStamp0_TimeStamp1_PlayRate0_Playrate1_Array;
        Anim_Lerp_Array = Data.Anim_Lerp_Array;

        Mat_HitGlow_Freeze_Burn_Dissolve_Array = Data.Mat_HitGlow_Freeze_Burn_Dissolve_Array;

        HealthBar_Opacity_CurrentRatio_TargetRatio_Array = Data.HealthBar_Opacity_CurrentRatio_TargetRatio_Array;

        Text_Location_Array = Data.Text_Location_Array;
        Text_Value_Style_Scale_Offset_Array = Data.Text_Value_Style_Scale_Offset_Array;

        InsidePool_Array = Data.InsidePool_Array;

        return *this;
    }
};