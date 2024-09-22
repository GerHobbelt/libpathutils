
#include "gtest/gtest.h"

#include <iostream>

#if 0

#include "googletest/include/monolithic_examples.h"

class CastomExceptionBase {
 public:
  CastomExceptionBase() noexcept{};
  CastomExceptionBase(const CastomExceptionBase&) noexcept{};
  CastomExceptionBase& operator=(const CastomExceptionBase&) noexcept{};
  virtual ~CastomExceptionBase(){};
  virtual const char* WhatIsError() const noexcept = 0;
};

class OK : public CastomExceptionBase {
 public:
  virtual ~OK(){};
  [[nodiscard]]
  const char* WhatIsError() const noexcept final{
    return "All OK";
  };
};

namespace {

TEST(Counter, Increment) {
  throw OK();
}

}  // namespace


#if defined(BUILD_MONOLITHIC)
#define main(cnt, arr) gtest_sample11_main(cnt, arr)
#endif

int main(int argc, const char** argv) {
  testing::InitGoogleTest(&argc, argv);

  testing::UnitTest::GetInstance()->UserExceptionHandler = [](std::exception_ptr eptr){
    try
    {
      std::rethrow_exception(eptr);
    }
    catch (const CastomExceptionBase& exception)
    {
      std::cout << exception.WhatIsError() << std::endl;
    }
  };

  return RUN_ALL_TESTS();
}

#endif
