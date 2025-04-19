#pragma once

#include "BillboardRenderPass.h"

class FWorldBillboardRenderPass : public FBillboardRenderPass
{
public:
    FWorldBillboardRenderPass();
    virtual ~FWorldBillboardRenderPass() = default;

    virtual void PrepareRenderArr() override;
};
