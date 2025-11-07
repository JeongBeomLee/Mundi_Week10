#pragma once
#include "ViewTarget.h"

class APlayerCameraManager;

class UCameraModifier : public UObject
{
    DECLARE_CLASS(UCameraModifier, UObject)
    GENERATED_REFLECTION_BODY()
public:
    /* Initialize */
    
    /** 
     * Allows any custom initialization. Called immediately after creation.
     * @param Camera - The camera this modifier should be associated with.
     */
    virtual void AddedToCamera( APlayerCameraManager* Camera );

    /* Modify Camera */
    
    /**
     * Directly modifies variables in the owning camera
     * @param	DeltaTime	Change in time since last update
     * @param	InOutPOV	Current Point of View, to be updated.
     * @return	bool		True if should STOP looping the chain, false otherwise
     */
    virtual void ModifyCamera(
        float DeltaTime,
        FVector& InOutViewLocation,
        FQuat& InOutViewRotation,
        float& InOutFOV
    );

    /** Allows modifying the post process in native code. */
    virtual void ModifyPostProcess(
        float DeltaTime,
        float& PostProcessBlendWeight,
        FPostProcessSettings& PostProcessSettings
    );
    
    /* Get & Set & Update */

    /** @return Returns the ideal blend alpha for this modifier. Interpolation will seek this value. */
    virtual float GetTargetAlpha();

    /** @return Returns true if modifier is disabled, false otherwise. */
    virtual bool IsDisabled() const;

    /** Enables this modifier. */
    virtual void EnableModifier();

    /** 
     *  Disables this modifier.
     *  @param  bImmediate  - true to disable with no blend out, false (default) to allow blend out
     */
    virtual void DisableModifier();

    /**
     * Responsible for updating alpha blend value.
     *
     * @param	Camera		- Camera that is being updated
     * @param	DeltaTime	- Amount of time since last update
     */
    virtual void UpdateAlpha(float DeltaTime);

    float GetAlphaInTime() const;
    virtual void SetAlphaInTime(const float InAlphaInTime);
    
    float GetAlphaOutTime() const;
    void SetAlphaOutTime(const float InAlphaOutTime);
    
    float GetAlpha() const;
    void SetAlpha(const float InAlpha);

    bool IsFadingIn() const;
    void SetIsFadingIn(const bool InIsFadingIn);

    uint8 GetPriority() const;
    void SetPriority(const uint8 InPriority);

	bool IsUsingRealDeltaTime() const { return bUseRealDeltaTime; }
	void SetUseRealDeltaTime(const bool bInUseRealDeltaTime) { bUseRealDeltaTime = bInUseRealDeltaTime; }

protected:
    APlayerCameraManager* CameraOwner = nullptr;
    float AlphaInTime = 1.0f;
    float AlphaOutTime = 1.0f;
    float Alpha = 0.0f;
    bool bIsFadingIn = false;
    bool bDisabled = false;
    uint8 Priority = 0;

	bool bUseRealDeltaTime = false;
};