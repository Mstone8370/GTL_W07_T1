#pragma once

#include "Vector.h"       // 기본 FVector 정의
#include "MathUtility.h"  // FMath 함수 및 상수
#include "Serialization/Archive.h" // 직렬화

/**
 * 3차원 평면을 위한 구조체입니다.
 * 평면 방정식 계수를 법선 벡터는 X,Y,Z로, D 성분은 W로 저장합니다 (Ax+By+Cz+D=0).
 * 참고: 법선 성분(X, Y, Z)을 위해 FVector로부터 상속받습니다.
 */
struct FPlane : public FVector
{
public:
	/** 평면 방정식의 D 성분 (Ax+By+Cz+D=0) */
	float W;

public:
	/** 기본 생성자 (평면을 Z=0 평면으로 설정, Normal=(0,0,1), W=0). */
	FPlane();

	/**
	 * 생성자.
	 * @param V 평면의 법선 벡터 성분. V가 정규화되었다고 가정합니다.
	 * @param InW 평면 방정식의 D 성분.
	 */
	FPlane(const FVector& V, float InW);

	/**
	 * 구성 요소로부터의 생성자.
	 * @param InX 법선 벡터의 X-성분.
	 * @param InY 법선 벡터의 Y-성분.
	 * @param InZ 법선 벡터의 Z-성분.
	 * @param InW 평면 방정식의 D 성분.
	 * 법선(InX, InY, InZ)이 정규화되었다고 가정합니다.
	 */
	FPlane(float InX, float InY, float InZ, float InW);

	/**
	 * 점과 법선 벡터로부터의 생성자.
	 * 생성자는 법선 벡터를 정규화합니다.
	 * @param InPoint 평면 위의 한 점.
	 * @param InNormal 평면에 대한 법선 벡터.
	 */
	FPlane(const FVector& InPoint, const FVector& InNormal);

	/**
	 * 동일 선상에 있지 않은 세 점으로부터의 생성자.
	 * 법선 벡터를 계산하고, 정규화하며, W를 계산합니다.
	 * @param A 평면 위의 첫 번째 점.
	 * @param B 평면 위의 두 번째 점.
	 * @param C 평면 위의 세 번째 점.
	 */
	FPlane(const FVector& A, const FVector& B, const FVector& C);

	//~ Begin FVector Interface (기본값은 법선의 X, Y, Z에 적합함)
	// 연산자 오버로딩 (+, -, *, / 와 FVector 또는 float)은 법선 부분에 대해 상속됩니다.
	// 내적/외적 (| ^)은 법선 부분에 대해 상속됩니다.
	// Length/Normalize 메서드는 법선 부분에 대해 상속됩니다.
	// 주의: FPlane에 대해 Normalize()를 호출하면 법선(X,Y,Z)은 정규화되지만 W 값은 올바르게 조정되지 않습니다. 대신 FPlane::Normalize 메서드를 사용해야 합니다.
	//~ End FVector Interface

public:
	/**
	 * 점에서 평면까지의 부호 있는 거리를 계산합니다.
	 * 평면이 정규화되었다고 가정합니다 (법선 벡터는 단위 길이).
	 * @param P 테스트할 점.
	 * @return 부호 있는 거리. 점이 법선 방향에 있으면 양수, 그렇지 않으면 음수.
	 */
	float PlaneDot(const FVector& P) const;

	/**
	 * 평면 방정식(법선 벡터와 W 성분)을 정규화합니다.
	 * 법선 벡터(X, Y, Z)가 단위 길이임을 보장하고 W를 그에 맞게 조정합니다.
	 * @param Tolerance 정규화를 위한 최소 법선 길이 제곱 값.
	 * @return 정규화에 성공하면 true, 그렇지 않으면 false.
	 */
	bool Normalize(float Tolerance = SMALL_NUMBER);

	/**
	 * 평면의 정규화된 복사본을 가져옵니다.
	 * @param Tolerance 정규화를 위한 최소 법선 길이 제곱 값.
	 * @return 평면의 정규화된 복사본.
	 */
	FPlane GetNormalized(float Tolerance = SMALL_NUMBER) const;

	/**
	 * 두 평면이 오차 범위 내에서 같은지 확인합니다. 법선과 W를 확인합니다.
	 * @param V 다른 평면.
	 * @param Tolerance 오차 허용 범위.
	 * @return 평면들이 오차 범위 내에서 같으면 true, 그렇지 않으면 false.
	 */
	bool Equals(const FPlane& V, float Tolerance = KINDA_SMALL_NUMBER) const;

	/** 동등 연산자. */
	bool operator==(const FPlane& V) const;

	/** 부등 연산자. */
	bool operator!=(const FPlane& V) const;

	/**
	 * 평면의 텍스트 표현을 가져옵니다.
	 * @return 평면을 설명하는 텍스트.
	 */
	FString ToString() const;

    
    bool InitFromString(const FString& InSourceString);


	// 직렬화를 위한 friend
	friend FArchive& operator<<(FArchive& Ar, FPlane& P);
};


// 인라인 구현

inline FPlane::FPlane()
	: FVector(0.f, 0.f, 1.f) // 기본 법선 (0,0,1)
	, W(0.f)                 // 기본 D=0
{
}

inline FPlane::FPlane(const FVector& V, float InW)
	: FVector(V) // 법선 X, Y, Z 설정
	, W(InW)     // W 설정
{
	// 나중에 거리 함수에 필요하다면 호출자가 V를 이미 정규화했다고 가정합니다.
}

inline FPlane::FPlane(float InX, float InY, float InZ, float InW)
	: FVector(InX, InY, InZ) // 법선 X, Y, Z 설정
	, W(InW)                 // W 설정
{
	// 필요하다면 호출자가 (InX, InY, InZ)를 이미 정규화했다고 가정합니다.
}

inline FPlane::FPlane(const FVector& InPoint, const FVector& InNormal)
{
	// 법선 벡터 정규화
	FVector NormalizedNormal = InNormal.GetSafeNormal();
	X = NormalizedNormal.X;
	Y = NormalizedNormal.Y;
	Z = NormalizedNormal.Z;

	// D = -(Normal dot Point) 계산
	W = -(NormalizedNormal | InPoint);
}

inline FPlane::FPlane(const FVector& A, const FVector& B, const FVector& C)
{
	// 법선 벡터 계산: (B-A) 외적 (C-A)
	FVector Normal = FVector::CrossProduct(B - A, C - A);

	// 법선 벡터 정규화
	FVector NormalizedNormal = Normal.GetSafeNormal();
	X = NormalizedNormal.X;
	Y = NormalizedNormal.Y;
	Z = NormalizedNormal.Z;

	// 점 중 하나를 사용하여 D = -(Normal dot A) 계산
	W = -(NormalizedNormal | A);
}

inline float FPlane::PlaneDot(const FVector& P) const
{
	// 이 평면이 이미 정규화되었다고 가정합니다!
	// Ax + By + Cz + D 계산
	// 법선 부분(X,Y,Z)과 P의 내적에 FVector::operator| 사용
	return (static_cast<const FVector&>(*this) | P) + W;
    // 또는 명시적으로: return X * P.X + Y * P.Y + Z * P.Z + W;
}

inline bool FPlane::Normalize(float Tolerance)
{
	const float NormalLengthSq = X * X + Y * Y + Z * Z; // FVector::LengthSquared()

	if (NormalLengthSq > Tolerance)
	{
		const float NormalLength = FMath::Sqrt(NormalLengthSq);
		const float InvNormalLength = 1.0f / NormalLength;
		X *= InvNormalLength;
		Y *= InvNormalLength;
		Z *= InvNormalLength;
		W *= InvNormalLength; // W 성분도 조정해야 합니다!
		return true;
	}
	// 선택 사항: 정규화 실패 시 기본 평면으로 설정?
	// X = Y = 0.f; Z = 1.f; W = 0.f;
	return false;
}

inline FPlane FPlane::GetNormalized(float Tolerance) const
{
	FPlane Result = *this;
	Result.Normalize(Tolerance);
	return Result;
}

inline bool FPlane::Equals(const FPlane& V, float Tolerance) const
{
    return (FMath::Abs(X - V.X) < Tolerance) && (FMath::Abs(Y - V.Y) < Tolerance) && (FMath::Abs(Z - V.Z) < Tolerance) && (FMath::Abs(W - V.W) < Tolerance);
}

inline bool FPlane::operator==(const FPlane& V) const
{
    return (X == V.X) && (Y == V.Y) && (Z == V.Z) && (W == V.W);
}

inline bool FPlane::operator!=(const FPlane& V) const
{
	return (X != V.X) || (Y != V.Y) || (Z != V.Z) || (W != V.W);
}



// 직렬화는 기본 클래스 부분과 W 부분을 처리해야 합니다
inline FArchive& operator<<(FArchive& Ar, FPlane& P)
{
	// FVector 부분 (X, Y, Z)을 먼저 직렬화
	Ar << static_cast<FVector&>(P);
	// 그런 다음 W 성분 직렬화
	Ar << P.W;
	return Ar;
}
