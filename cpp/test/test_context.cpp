#include "context.h"
#include "mymath.h"

#include "gtest/gtest.h"

#include <string>

extern std::string config_file_name;

namespace {

class ContextTest : public ::testing::Test {
protected:
  void SetUp() override {
    context = IceHalo::SimulationContext::CreateFromFile(config_file_name.c_str());
  }

  IceHalo::SimulationContextPtr context;
};


TEST_F(ContextTest, CreateNotNull) {
  ASSERT_NE(context, nullptr);
}


TEST_F(ContextTest, CheckSunDir) {
  auto sun_dir = context->GetSunRayDir();
  EXPECT_NEAR(sun_dir[0], 0.0f, IceHalo::Math::kFloatEps);
  EXPECT_NEAR(sun_dir[1], -0.906308f, IceHalo::Math::kFloatEps);
  EXPECT_NEAR(sun_dir[2], -0.422618f, IceHalo::Math::kFloatEps);
}


TEST_F(ContextTest, FillSunDir) {
  auto sun_dir = context->GetSunRayDir();
  auto sun_d = context->GetSunDiameter();

  constexpr int kRayNum = 200;
  float dir[3 * kRayNum];
  auto sampler = IceHalo::Math::RandomSampler::GetInstance();
  sampler->SampleSphericalPointsCart(sun_dir, sun_d / 2, dir, kRayNum);

  for (int i = 0; i < kRayNum; i++) {
    float a = std::acos(IceHalo::Math::Dot3(sun_dir, dir + i * 3));    // In rad
    a *= IceHalo::Math::kRadToDegree;    // To degree
    EXPECT_TRUE(a < sun_d / 2 + 1e-3);
  }
}

}  // namespace
