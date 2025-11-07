#pragma once
#include "Vector.h"

struct FLinearColor
{
	float R = 0.0f;
	float G = 0.0f;
    float B = 0.0f;
    float A = 0.0f;

	FLinearColor() : R(0), G(0), B(0), A(0) {}
	explicit FLinearColor(float InR, float InG, float InB, float InA = 1.0f) : R(InR), G(InG), B(InB), A(InA) {};
	explicit FLinearColor(const FVector& RGB) : R(RGB.X), G(RGB.Y), B(RGB.Z), A(1.0f) {};
	explicit FLinearColor(const FVector4& RGBA) : R(RGBA.X), G(RGBA.Y), B(RGBA.Z), A(RGBA.W) {};;

	static inline FLinearColor Zero()
	{
		return FLinearColor(); // 기본 생성자가 (0, 0, 0, 0)으로 초기화합니다.
	}

    inline bool operator==(const FLinearColor& Other) const
    {
        return (R == Other.R) && (G == Other.G) && (B == Other.B) && (A == Other.A);
    }

    inline bool operator!=(const FLinearColor& Other) const
    {
        return (R != Other.R) || (G != Other.G) || (B != Other.B) || (A != Other.A);
    }

	inline FLinearColor& operator=(const FVector& Other)
    {
        R = Other.X;
        G = Other.Y;
        B = Other.Z;
        A = 1.0f;
        return *this;
    }

	inline FLinearColor& operator=(const FVector4& Other)
    {
        R = Other.X;
        G = Other.Y;
        B = Other.Z;
        A = Other.W;
        return *this;
    }

    inline FLinearColor operator+(const FLinearColor& Other) const
    {
        return FLinearColor(R + Other.R, G + Other.G, B + Other.B, A + Other.A);
    }

    inline FLinearColor operator+=(const FLinearColor& Other)
    {
        R += Other.R;
        G += Other.G;
        B += Other.B;
        A += Other.A;
        return *this;
    }

    inline FLinearColor operator-(const FLinearColor& Other) const
    {
        return FLinearColor(R - Other.R, G - Other.G, B - Other.B, A - Other.A);
    }

    inline FLinearColor operator-=(const FLinearColor& Other)
    {
        R -= Other.R;
        G -= Other.G;
        B -= Other.B;
        A -= Other.A;
        return *this;
    }

    inline FLinearColor operator*(const FLinearColor& Other) const // Component-wise Multiplication
    {
        return FLinearColor(R * Other.R, G * Other.G, B * Other.B, A * Other.A);
    }

    inline FLinearColor operator*(float Scalar) const // Scaling
    {
        return FLinearColor(R * Scalar, G * Scalar, B * Scalar, A * Scalar);
    }

    inline FLinearColor& operator*=(const FLinearColor& Other)
    {
        R *= Other.R;
        G *= Other.G;
        B *= Other.B;
        A *= Other.A;
        return *this;
    }

    inline FLinearColor& operator*=(float Scalar)
    {
        R *= Scalar;
        G *= Scalar;
        B *= Scalar;
        A *= Scalar;
        return *this;
    }

    inline FLinearColor operator/(const FLinearColor& Other) const // Component-wise Division
    {
        return FLinearColor(R / Other.R, G / Other.G, B / Other.B, A / Other.A);
    }

    inline FLinearColor operator/(float Scalar) const
    {
        const float InvScalar = 1.0f / Scalar;
        return FLinearColor(R * InvScalar, G * InvScalar, B * InvScalar, A * InvScalar);
    }

    inline FLinearColor& operator/=(const FLinearColor& Other)
    {
        R /= Other.R;
        G /= Other.G;
        B /= Other.B;
        A /= Other.A;
        return *this;
    }

    inline FLinearColor& operator/=(float Scalar)
    {
        const float InvScalar = 1.0f / Scalar;
        R *= InvScalar;
        G *= InvScalar;
        B *= InvScalar;
        A *= InvScalar;
        return *this;
    }

	bool Equals(const FLinearColor& ColorB, float Tolerance) const; // Error tolerance Comparison
    FLinearColor GetClamped(float InMin, float InMax) const;

    float GetMax() const;
	float GetMin() const;

	FVector4 ToFVector4() const { return FVector4(R, G, B, A); }

    static float Dist(const FLinearColor& C1, const FLinearColor& C2);
    static float DistSquared(const FLinearColor& C1, const FLinearColor& C2);
    static FLinearColor Lerp(const FLinearColor& C1, const FLinearColor& C2, float T);
    static FLinearColor MakeRandomColor();
    static FLinearColor MakeRandomSeededColor(int32 Seed);
};