
#include "gtest/gtest.h"

#include <iostream>
#include <sstream>

using namespace std;

using ::testing::InitGoogleTest;
using ::testing::TestEventListeners;
using ::testing::TestWithParam;
using ::testing::UnitTest;
using ::testing::Test;
using ::testing::Values;
using ::testing::TestInfo;


class ExpectNFailuresListener : public testing::EmptyTestEventListener {
public:
	ExpectNFailuresListener(int n) :
		expected_failure_count(n), expected_failure_remain(0)
	{}

	testing::TestPartResult OnTestPartResult(const testing::TestPartResult& result) override {
		testing::TestPartResult r = result;

		if (r.type() != testing::TestPartResult::kSuccess && expected_failure_remain > 0)
		{
			expected_failure_remain--;
			r.change_type(testing::TestPartResult::kSuccess);
		}
		return r;
	}

	void OnTestIterationStart(const testing::UnitTest& unit_test, int iteration) override {
		expected_failure_remain = expected_failure_count;
	}

	void OnTestIterationEnd(const testing::UnitTest& unit_test, int iteration) override {
		if (expected_failure_remain != 0 && expected_failure_count != 0)
		{
			ostringstream os;
			os << "Expected " << expected_failure_count << " failures, but observed " << (expected_failure_count - expected_failure_remain) << " failures instead.";

			// ZERO the expected count: this way all errors show up in the next test round (if any)
			expected_failure_count = 0;

			throw std::runtime_error(os.str());
		}
	}

private:
	int expected_failure_count;
	int expected_failure_remain;
};


// This printer prints the full test names only, starting each test
// iteration with a "----" marker.
//
// Technical Info: replaces gtest's internal PrettyUnitTestResultPrinter class.
class TestNamePrinter: public ExpectNFailuresListener {
public:
	TestNamePrinter(int n):
		ExpectNFailuresListener(n)
	{}

	virtual void OnTestIterationStart(const UnitTest& /* unit_test */,
																		int /* iteration */) {
		printf("----\n");
	}

	virtual void OnTestStart(const TestInfo& test_info) {
		printf("%s.%s\n", test_info.test_case_name(), test_info.name());
	}
};

#if defined(BUILD_MONOLITHIC)
#define main  pathutils_test_main
#endif

int main(int argc, const char** argv)
{
	int rv = 0;
	const int expected_number_of_failures = 1;

	InitGoogleTest(&argc, argv);

	TestEventListeners& listeners = UnitTest::GetInstance()->listeners();
	listeners.Append(new ExpectNFailuresListener(expected_number_of_failures));

#if 0
	// Replaces the default printer with TestNamePrinter, which prints
	// the test name only.
	listeners.SetDefaultResultPrinter(new TestNamePrinter(expected_number_of_failures));
#endif

	//rv |= gtest_main(argc, argv);

	rv |= RUN_ALL_TESTS();

	// Using the UnitTest reflection API to inspect test
	// results, we discount failures from the tests we expected to fail.
	{
		using namespace testing;

		int unexpectedly_failed_tests = 0;
		int total_failed_tests = 0;
		int total_run_tests = 0;
		int total_tests = 0;
		UnitTest& unit_test = *UnitTest::GetInstance();

		for (int i = 0; i < unit_test.total_test_suite_count(); ++i) {
			const testing::TestSuite& test_suite = *unit_test.GetTestSuite(i);
			for (int j = 0; j < test_suite.total_test_count(); ++j) {
				const TestInfo& test_info = *test_suite.GetTestInfo(j);
				total_tests++;
				if (test_info.should_run())
					total_run_tests++;

				// Counts failed tests that were not meant to fail (those without
				// 'Fails' in the name).
				if (test_info.result()->Failed()) {
					total_failed_tests++;
					if (strcmp(test_info.name(), "Fails") != 0) {
						unexpectedly_failed_tests++;
					}
				}
			}
		}

		// Test that were meant to fail should not affect the test program outcome.
		if (unexpectedly_failed_tests == 0) 
			rv = 0;

		char msgbuf[500];
		snprintf(msgbuf, sizeof(msgbuf), 
			"\n\nSummary:\n"
			"  Tests #: ................. %6d\n"
			"  Tests run #: ............. %6d\n"
			"  Failed #: ................ %6d\n"
			"  %sUNEXPECTED Fail #: ....... %6d@D\n\n\n",
			total_tests,
			total_run_tests,
			total_failed_tests,
			(rv == 0 ? "@G" : "@R"),
			unexpectedly_failed_tests);
		PrintColorEncoded(msgbuf);
	}

	return rv;
}
