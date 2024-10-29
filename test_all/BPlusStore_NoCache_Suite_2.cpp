#include "pch.h"
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <variant>
#include <typeinfo>
#include <type_traits>

#include "glog/logging.h"

#include "NoCache.hpp"
#include "IndexNode.hpp"
#include "DataNode.hpp"
#include "BPlusStore.hpp"
#include "NoCacheObject.hpp"
#include "TypeUID.h"
#include <set>
#include <random>
#include <numeric>

#ifndef __TREE_WITH_CACHE__
namespace BPlusStore_NoCache_Suite
{
    class BPlusStore_NoCache_Suite_2 : public ::testing::TestWithParam<std::tuple<int, int>>
    {
    protected:
        typedef string KeyType;
        typedef string ValueType;
        typedef uintptr_t ObjectUIDType;

        typedef DataNode<KeyType, ValueType, ObjectUIDType, TYPE_UID::DATA_NODE_STRING_STRING> DataNodeType;
        typedef IndexNode<KeyType, ValueType, ObjectUIDType, DataNodeType, TYPE_UID::INDEX_NODE_STRING_STRING> IndexNodeType;

        typedef BPlusStore<KeyType, ValueType, NoCache<ObjectUIDType, NoCacheObject, DataNodeType, IndexNodeType>> BPlusStoreType;

        void SetUp() override
        {
            std::tie(nDegree, nTotalRecords) = GetParam();

            m_ptrTree = new BPlusStoreType(nDegree);
            m_ptrTree->init<DataNodeType>();
        }

        void TearDown() override {
            delete m_ptrTree;
        }

        BPlusStoreType* m_ptrTree;

        int nDegree;
        int nTotalRecords;
    };

    TEST_P(BPlusStore_NoCache_Suite_2, Bulk_Insert_v1)
    {
        std::vector<int> vtRandom(nTotalRecords);
        std::iota(vtRandom.begin(), vtRandom.end(), 1);
        std::random_device rd; // Obtain a random number from hardware
        std::mt19937 eng(rd()); // Seed the generator
        std::shuffle(vtRandom.begin(), vtRandom.end(), eng);

        for (int nCntr = 0; nCntr < nTotalRecords; nCntr++)
        {
            ErrorCode ec = m_ptrTree->insert(to_string(vtRandom[nCntr]), to_string(vtRandom[nCntr]));
            assert(ec == ErrorCode::Success);
        }
    }

    TEST_P(BPlusStore_NoCache_Suite_2, Bulk_Insert_v2)
    {
        for (int nCntr = 0; nCntr < nTotalRecords; nCntr = nCntr + 2)
        {
            ErrorCode ec = m_ptrTree->insert(to_string(nCntr), to_string(nCntr));
            assert(ec == ErrorCode::Success);
        }

        for (int nCntr = 1; nCntr < nTotalRecords; nCntr = nCntr + 2)
        {
            ErrorCode ec = m_ptrTree->insert(to_string(nCntr), to_string(nCntr));
            assert(ec == ErrorCode::Success);
        }
    }

    TEST_P(BPlusStore_NoCache_Suite_2, Bulk_Insert_v3)
    {
        for (int nCntr = nTotalRecords - 1; nCntr >= 0; nCntr--)
        {
            ErrorCode ec = m_ptrTree->insert(to_string(nCntr), to_string(nCntr));
            assert(ec == ErrorCode::Success);
        }
    }

    TEST_P(BPlusStore_NoCache_Suite_2, Bulk_Search_v1)
    {
        for (int nCntr = 0; nCntr < nTotalRecords; nCntr++)
        {
            ErrorCode ec = m_ptrTree->insert(to_string(nCntr), to_string(nCntr));
            assert(ec == ErrorCode::Success);
        }

        for (int nCntr = 0; nCntr < nTotalRecords; nCntr++)
        {
            std::string stValue = "";
            ErrorCode ec = m_ptrTree->search(to_string(nCntr), stValue);

            assert(to_string(nCntr) == stValue && ec == ErrorCode::Success);
        }
    }

    TEST_P(BPlusStore_NoCache_Suite_2, Bulk_Search_v2)
    {
        std::vector<int> vtRandom(nTotalRecords);
        std::iota(vtRandom.begin(), vtRandom.end(), 1);
        std::random_device rd; // Obtain a random number from hardware
        std::mt19937 eng(rd()); // Seed the generator
        std::shuffle(vtRandom.begin(), vtRandom.end(), eng);

        for (int nCntr = 0; nCntr < nTotalRecords; nCntr++)
        {
            ErrorCode ec = m_ptrTree->insert(to_string(vtRandom[nCntr]), to_string(vtRandom[nCntr]));
            assert(ec == ErrorCode::Success);
        }

        for (int nCntr = 0; nCntr < nTotalRecords; nCntr++)
        {
            std::string stValue = "";
            ErrorCode ec = m_ptrTree->search(to_string(vtRandom[nCntr]), stValue);

            assert(to_string(vtRandom[nCntr]) == stValue && ec == ErrorCode::Success);
        }
    }

    TEST_P(BPlusStore_NoCache_Suite_2, Bulk_Search_v3)
    {
        for (int nCntr = nTotalRecords - 1; nCntr >= 0; nCntr--)
        {
            ErrorCode ec = m_ptrTree->insert(to_string(nCntr), to_string(nCntr));
            assert(ec == ErrorCode::Success);
        }

        for (int nCntr = nTotalRecords - 1; nCntr >= 0; nCntr--)
        {
            std::string stValue = "";
            ErrorCode ec = m_ptrTree->search(to_string(nCntr), stValue);

            assert(to_string(nCntr) == stValue && ec == ErrorCode::Success);
        }
    }

    TEST_P(BPlusStore_NoCache_Suite_2, Bulk_Delete_v1)
    {
        for (int nCntr = 0; nCntr < nTotalRecords; nCntr++)
        {
            ErrorCode ec = m_ptrTree->insert(to_string(nCntr), to_string(nCntr));
            assert(ec == ErrorCode::Success);
        }

        for (int nCntr = 0; nCntr < nTotalRecords; nCntr++)
        {
            ErrorCode ec = m_ptrTree->remove(to_string(nCntr));
            assert(ec == ErrorCode::Success);
        }

        for (int nCntr = 0; nCntr < nTotalRecords; nCntr++)
        {
            std::string stValue = "";
            ErrorCode ec = m_ptrTree->search(to_string(nCntr), stValue);

            assert(ec == ErrorCode::KeyDoesNotExist);
        }
    }

    TEST_P(BPlusStore_NoCache_Suite_2, Bulk_Delete_v2)
    {
        std::vector<int> vtRandom(nTotalRecords);
        std::iota(vtRandom.begin(), vtRandom.end(), 1);
        std::random_device rd; // Obtain a random number from hardware
        std::mt19937 eng(rd()); // Seed the generator
        std::shuffle(vtRandom.begin(), vtRandom.end(), eng);

        for (int nCntr = 0; nCntr < nTotalRecords; nCntr++)
        {
            ErrorCode ec = m_ptrTree->insert(to_string(vtRandom[nCntr]), to_string(vtRandom[nCntr]));
            assert(ec == ErrorCode::Success);
        }

        for (int nCntr = 0; nCntr < nTotalRecords; nCntr++)
        {
            ErrorCode ec = m_ptrTree->remove(to_string(vtRandom[nCntr]));
            assert(ec == ErrorCode::Success);
        }

        for (int nCntr = 0; nCntr < nTotalRecords; nCntr++)
        {
            std::string stValue = "";
            ErrorCode ec = m_ptrTree->search(to_string(vtRandom[nCntr]), stValue);

            assert(ec == ErrorCode::KeyDoesNotExist);
        }
    }

    TEST_P(BPlusStore_NoCache_Suite_2, Bulk_Delete_v3)
    {
        for (int nCntr = nTotalRecords - 1; nCntr >= 0; nCntr--)
        {
            ErrorCode ec = m_ptrTree->insert(to_string(nCntr), to_string(nCntr));
            assert(ec == ErrorCode::Success);
        }

        for (int nCntr = nTotalRecords - 1; nCntr >= 0; nCntr--)
        {
            ErrorCode ec = m_ptrTree->remove(to_string(nCntr));
            assert(ec == ErrorCode::Success);
        }

        for (int nCntr = nTotalRecords - 1; nCntr >= 0; nCntr--)
        {
            std::string stValue = "";
            ErrorCode ec = m_ptrTree->search(to_string(nCntr), stValue);

            assert(ec == ErrorCode::KeyDoesNotExist);
        }
    }

    TEST_P(BPlusStore_NoCache_Suite_2, AllOperations)
    {
        std::vector<int> vtRandom(nTotalRecords);
        std::iota(vtRandom.begin(), vtRandom.end(), 1);
        std::random_device rd; // Obtain a random number from hardware
        std::mt19937 eng(rd()); // Seed the generator
        std::shuffle(vtRandom.begin(), vtRandom.end(), eng);

        for (int nTestCntr = 0; nTestCntr < 2; nTestCntr++)
        {
            for (int nCntr = 0; nCntr < nTotalRecords; nCntr = nCntr + 1)
            {
                ErrorCode ec = m_ptrTree->insert(to_string(vtRandom[nCntr]), to_string(vtRandom[nCntr]));
                assert(ec == ErrorCode::Success);
            }

            for (int nCntr = 0; nCntr < nTotalRecords; nCntr++)
            {
                std::string stValue = "";
                ErrorCode ec = m_ptrTree->search(to_string(vtRandom[nCntr]), stValue);

                assert(stValue == to_string(vtRandom[nCntr]));
            }

            for (int nCntr = 0; nCntr < nTotalRecords; nCntr = nCntr + 2)
            {
                ErrorCode ec = m_ptrTree->remove(to_string(vtRandom[nCntr]));

                assert(ec == ErrorCode::Success);
            }
            for (int nCntr = 1; nCntr < nTotalRecords; nCntr = nCntr + 2)
            {
                ErrorCode ec = m_ptrTree->remove(to_string(vtRandom[nCntr]));

                assert(ec == ErrorCode::Success);
            }

            for (int nCntr = 0; nCntr < nTotalRecords; nCntr++)
            {
                std::string stValue = "";
                ErrorCode ec = m_ptrTree->search(to_string(vtRandom[nCntr]), stValue);

                assert(ec == ErrorCode::KeyDoesNotExist);
            }
        }

        for (int nTestCntr = 0; nTestCntr < 2; nTestCntr++)
        {
            for (int nCntr = nTotalRecords; nCntr >= 0; nCntr = nCntr - 2)
            {
                ErrorCode ec = m_ptrTree->insert(to_string(nCntr), to_string(nCntr));
                assert(ec == ErrorCode::Success);

            }
            for (int nCntr = nTotalRecords - 1; nCntr >= 0; nCntr = nCntr - 2)
            {
                ErrorCode ec = m_ptrTree->insert(to_string(nCntr), to_string(nCntr));
                assert(ec == ErrorCode::Success);
            }

            for (int nCntr = 0; nCntr < nTotalRecords; nCntr++)
            {
                std::string stValue = "";
                ErrorCode ec = m_ptrTree->search(to_string(nCntr), stValue);

                assert(stValue == to_string(nCntr) && ec == ErrorCode::Success);
            }

            for (int nCntr = nTotalRecords; nCntr >= 0; nCntr = nCntr - 2)
            {
                ErrorCode ec = m_ptrTree->remove(to_string(nCntr));
                assert(ec == ErrorCode::Success);
            }

            for (int nCntr = nTotalRecords - 1; nCntr >= 0; nCntr = nCntr - 2)
            {
                ErrorCode ec = m_ptrTree->remove(to_string(nCntr));
                assert(ec == ErrorCode::Success);
            }

            for (int nCntr = 0; nCntr < nTotalRecords; nCntr++)
            {
                std::string stValue = "";
                ErrorCode ec = m_ptrTree->search(to_string(nCntr), stValue);

                assert(ec == ErrorCode::KeyDoesNotExist);
            }
        }
    }

    INSTANTIATE_TEST_CASE_P(
        KEY_AND_VAL_AS_4BYTES_STRINGS,
        BPlusStore_NoCache_Suite_2,
        ::testing::Values(
            std::make_tuple(3, 50000),
            std::make_tuple(4, 50000),
            std::make_tuple(5, 50000),
            std::make_tuple(6, 50000),
            std::make_tuple(7, 50000),
            std::make_tuple(8, 50000),
            std::make_tuple(15, 50000),
            std::make_tuple(16, 50000),
            std::make_tuple(32, 50000),
            std::make_tuple(64, 50000),
            std::make_tuple(128, 50000),
            std::make_tuple(256, 50000),
            std::make_tuple(512, 50000),
            std::make_tuple(1024, 50000),
            std::make_tuple(2048, 50000)
            ));
}
#endif //__TREE_WITH_CACHE__
