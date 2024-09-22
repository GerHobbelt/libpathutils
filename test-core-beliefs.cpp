
#include "gtest/gtest.h"


TEST(puCoreBelief, ThrownCpluplusExceptionIsCaught) {
	throw std::exception("aborting");
}

TEST(puCoreBelief, ThrownCpluplusExceptionPointerIsCaught) {
	throw new std::exception("aborting");
}

TEST(puCoreBelief, ThrownCpluplusArbitraryExceptionIsCaught) {
	throw 42;
}

TEST(puCoreBelief, ThrownCpluplusArbitraryExceptionPointerIsCaught) {
	throw NULL;
}

