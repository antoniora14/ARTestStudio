#include <gtest/gtest.h>

int main(int argumentCount, char** arguments)
{
	testing::InitGoogleTest(&argumentCount, arguments);
	return RUN_ALL_TESTS();
}
