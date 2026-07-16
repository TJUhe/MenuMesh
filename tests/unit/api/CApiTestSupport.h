/**
 * @file tests/unit/api/CApiTestSupport.h
 * @brief Verifies capi test support behavior in the ManuMesh tests.
 * @ingroup manumesh_tests
 *
 * @details The fixture and assertions document observable contracts, numeric tolerances, determinism requirements, and previously fixed regressions.
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
