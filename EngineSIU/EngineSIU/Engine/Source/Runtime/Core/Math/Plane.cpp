#include "Plane.h"

#include "Misc/Parse.h"


FString FPlane::ToString() const
{
    return FString::Printf(TEXT("X=%3.3f Y=%3.3f Z=%3.3f W=%3.3f"), X, Y, Z, W);
}

bool FPlane::InitFromString(const FString& InSourceString)
{
    X = Y = Z = W = 0;
    
     const bool bSuccessful = FParse::Value(*InSourceString, TEXT("X="), X) && 
           FParse::Value(*InSourceString, TEXT("Y="), Y) && 
           FParse::Value(*InSourceString, TEXT("Z="), Z) &&
           FParse::Value(*InSourceString, TEXT("W="), W);
    return bSuccessful;
}
