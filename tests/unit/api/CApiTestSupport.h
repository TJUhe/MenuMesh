/**
 * @file tests/unit/api/CApiTestSupport.h
 * @brief 提供自动创建和销毁 ManuMesh C API 上下文的测试夹具。
 * @ingroup manumesh_tests
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
