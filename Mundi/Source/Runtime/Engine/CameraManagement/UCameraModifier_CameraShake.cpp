#include "pch.h"
#include "UCameraModifier_CameraShake.h"

IMPLEMENT_CLASS(UCameraModifier_CameraShake)

BEGIN_PROPERTIES(UCameraModifier_CameraShake)
	// 펄린 노이즈 커브는 내부적으로 관리되므로 에디터에 노출하지 않음
END_PROPERTIES()

UCameraModifier_CameraShake::UCameraModifier_CameraShake()
{
    SetAlphaInTime(2.f);
   SetAlphaOutTime(2.f);
   SetAlpha(0.f);
   SetIsFadingIn(true);
   EnableModifier();
   SetPriority(0);

   SetNumSamples(12);

   // Curve 한 번만 생성
   GetNewShake();
}

void UCameraModifier_CameraShake::GetNewShake()
{
   PerlinNoiseXAxis.RenewCurve();
   PerlinNoiseYAxis.RenewCurve();
   PerlinNoiseZAxis.RenewCurve();
}

void UCameraModifier_CameraShake::ModifyCamera(
   float DeltaTime,
   FVector& InOutViewLocation,
   FQuat& InOutViewRotation,
   float& InOutFOV
)
{
   UCameraModifier::ModifyCamera(
      DeltaTime,
      InOutViewLocation,
      InOutViewRotation,
      InOutFOV
   );

   // CurveTime: 곡선의 시간 축에서의 위치 [0, AlphaInTime]
   float CurveTime = FMath::Lerp(0.f, AlphaInTime, Alpha);
   int32 NumSamples = PerlinNoiseXAxis.GetNumSamples();

   const TArray<FRichCurveKey>& XKeys = PerlinNoiseXAxis.GetCurve().Keys;
   const TArray<FRichCurveKey>& YKeys = PerlinNoiseYAxis.GetCurve().Keys;
   const TArray<FRichCurveKey>& ZKeys = PerlinNoiseZAxis.GetCurve().Keys;

   FVector ViewNewRotation;

   for (int32 i = 0; i < NumSamples - 1; i++)
   {
      if (CurveTime >= XKeys[i].Time && CurveTime <= XKeys[i + 1].Time)
      {
         // Curve 한 번만 생성
         // PerlinNoiseXAxis.RenewCurve();
         // PerlinNoiseYAxis.RenewCurve();
         // PerlinNoiseZAxis.RenewCurve();
         
         // InterpAlpha: 두 키 사이에서의 보간 비율 [0, 1]
         float InterpAlpha = FMath::GetRangePct(XKeys[i].Time, XKeys[i + 1].Time, CurveTime);

         // 각 축별로 비선형 보간 (InterpEaseInOut으로 부드러운 흔들림)
         ViewNewRotation.X = FMath::InterpEaseInOut(
            XKeys[i].Value,
            XKeys[i + 1].Value,
            InterpAlpha,
            2.f
         ) * RotationAmplitude;

         ViewNewRotation.Y = FMath::InterpEaseInOut(
            YKeys[i].Value,
            YKeys[i + 1].Value,
            InterpAlpha,
            2.f
         ) * RotationAmplitude;

         ViewNewRotation.Z = FMath::InterpEaseInOut(
            ZKeys[i].Value,
            ZKeys[i + 1].Value,
            InterpAlpha,
            2.f
         ) * RotationAmplitude;


         // [Alternative Unreal 방식 - 주석]
         // Base Rotation에 Shake Offset을 Euler 각도로 더하는 방식:
         FVector CurrentEuler = InOutViewRotation.ToEulerZYXDeg();
         FVector NewEuler = CurrentEuler + ViewNewRotation;
         InOutViewRotation = FQuat::MakeFromEulerZYX(NewEuler);
         // 이 방식은 Shake가 끝나면 자동으로 원래 회전으로 복귀함

         break;
      }
   }
}

float UCameraModifier_CameraShake::GetRotationAmplitude() const
{
   return RotationAmplitude;
}
void UCameraModifier_CameraShake::SetRotationAmplitude(const float InRotationAmplitude)
{
   RotationAmplitude = InRotationAmplitude;
}

// CameraShake 기간과 Curve의 TimeDuration을 동일하게
void UCameraModifier_CameraShake::SetAlphaInTime(const float InAlphaInTime)
{
   UCameraModifier::SetAlphaInTime(InAlphaInTime);

   PerlinNoiseXAxis.SetTimeRange(InAlphaInTime);
   PerlinNoiseYAxis.SetTimeRange(InAlphaInTime);
   PerlinNoiseZAxis.SetTimeRange(InAlphaInTime);
}

void UCameraModifier_CameraShake::SetNumSamples(const float InNumSamples)
{
   PerlinNoiseXAxis.SetNumSamples(InNumSamples);
   PerlinNoiseYAxis.SetNumSamples(InNumSamples);
   PerlinNoiseZAxis.SetNumSamples(InNumSamples);
}