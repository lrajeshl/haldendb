#include "pch.h"

#include "gtest/gtest.h"

int main(int argc, char** argv)
{
	::testing::InitGoogleTest(&argc, argv);
	//::testing::GTEST_FLAG(filter) = "-" \
					"KEY_AS_INT32_VAL_AS_INT32*:" \
					"KEY_AND_VAL_AS_4BYTES_STRINGS*:" \
					"THREADED_TREE_WITH_KEY_AS_INT32_VAL_AS_INT32*:"\
					"TREE_WITH_KEY_AND_VAL_AS_INT32_AND_WITH_VOLATILE_STORAGE*:"\
					"THREADED_TREE_WITH_KEY_AND_VAL_AS_INT32_AND_WITH_VOLATILE_STORAGE*:"\
					"TREE_WITH_KEY_AND_VAL_AS_INT32_AND_WITH_FILE_STORAGE*:"\
					"THREADED_TREE_WITH_KEY_AND_VAL_AS_INT32_AND_WITH_TREE_STORAGE*";
	return RUN_ALL_TESTS();
}
