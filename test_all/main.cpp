#include "pch.h"

#include "gtest/gtest.h"

int main(int argc, char** argv)
{
	::testing::InitGoogleTest(&argc, argv);
	//::testing::GTEST_FLAG(filter) = "BPlusStore_LRUCache_VolatileStorage_Suite_1";
	return RUN_ALL_TESTS();
}