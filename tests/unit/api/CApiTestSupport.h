/**
 * @file tests/unit/api/CApiTestSupport.h
 * @brief 验证 ManuMesh 测试中的 C API 测试支持行为。
 * @ingroup manumesh_tests
 *
 * @details 测试夹具和断言记录可观察契约、数值容差、确定性要求以及已修复的回归问题。
 */

#pragma once

#include "TestSupport.h"
#include "api/CApi.h"

#include <gtest/gtest.h>

class CApiTest : public ::testing::Test {
protected:
    void SetUp() override {
        context = manumesh_context_create();
        ASSERT_NE(context, nullptr);
    }

    void TearDown() override { manumesh_context_destroy(context); }

    ManuMeshContext* context = nullptr;
};
