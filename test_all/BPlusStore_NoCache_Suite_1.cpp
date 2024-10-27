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
    typedef int KeyType;
    typedef int ValueType;
    typedef uintptr_t ObjectUIDType;

    typedef DataNode<KeyType, ValueType, ObjectUIDType, TYPE_UID::DATA_NODE_INT_INT > DataNodeType;
    typedef IndexNode<KeyType, ValueType, ObjectUIDType, DataNodeType, TYPE_UID::INDEX_NODE_INT_INT > IndexNodeType;

    typedef BPlusStore<KeyType, ValueType, NoCache<ObjectUIDType, NoCacheObject, DataNodeType, IndexNodeType>> BPlusStoreType;

    class BPlusStore_NoCache_Suite_1 : public ::testing::TestWithParam<std::tuple<int, int>>
    {
    protected:
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
    
    TEST_P(BPlusStore_NoCache_Suite_1, Bulk_Insert_v1) 
    {
        std::vector<int> vtRandom(nTotalRecords);
        std::iota(vtRandom.begin(), vtRandom.end(), 1);
        std::random_device rd; // Obtain a random number from hardware
        std::mt19937 eng(rd()); // Seed the generator
        std::shuffle(vtRandom.begin(), vtRandom.end(), eng);

        for (int nCntr = 0; nCntr < nTotalRecords; nCntr++)
        {
            ErrorCode ec = m_ptrTree->insert(vtRandom[nCntr], vtRandom[nCntr]);
            assert(ec == ErrorCode::Success);
        }
    }

    TEST_P(BPlusStore_NoCache_Suite_1, Bulk_Insert_v2) 
    {
        for (int nCntr = 0; nCntr < nTotalRecords ; nCntr = nCntr + 2)
        {
            ErrorCode ec = m_ptrTree->insert(nCntr, nCntr);
            assert(ec == ErrorCode::Success);
        }

        for (int nCntr = 1; nCntr < nTotalRecords; nCntr = nCntr + 2)
        {
            ErrorCode ec = m_ptrTree->insert(nCntr, nCntr);
            assert(ec == ErrorCode::Success);
        }
    }

    TEST_P(BPlusStore_NoCache_Suite_1, Bulk_Insert_v3) 
    {
        for (int nCntr = nTotalRecords - 1; nCntr >= 0; nCntr--)
        {
            ErrorCode ec = m_ptrTree->insert(nCntr, nCntr);
            assert(ec == ErrorCode::Success);
        }
    }

    TEST_P(BPlusStore_NoCache_Suite_1, Bulk_Search_v1) 
    {
        for (int nCntr = 0; nCntr < nTotalRecords; nCntr++)
        {
            ErrorCode ec = m_ptrTree->insert(nCntr, nCntr);
            assert(ec == ErrorCode::Success);
        }

        for (int nCntr = 0; nCntr < nTotalRecords; nCntr++)
        {
            int nValue = 0;
            ErrorCode ec = m_ptrTree->search(nCntr, nValue);

            assert(nCntr == nValue && ec == ErrorCode::Success);
        }
    }
    
    TEST_P(BPlusStore_NoCache_Suite_1, Bulk_Search_v2) 
    {
        std::vector<int> vtRandom(nTotalRecords);
        std::iota(vtRandom.begin(), vtRandom.end(), 1);
        std::random_device rd; // Obtain a random number from hardware
        std::mt19937 eng(rd()); // Seed the generator
        std::shuffle(vtRandom.begin(), vtRandom.end(), eng);

        for (int nCntr = 0; nCntr < nTotalRecords; nCntr++)
        {
            ErrorCode ec = m_ptrTree->insert(vtRandom[nCntr], vtRandom[nCntr]);
            assert(ec == ErrorCode::Success);
        }

        for (int nCntr = 0; nCntr < nTotalRecords; nCntr++)
        {
            int nValue = 0;
            ErrorCode ec = m_ptrTree->search(vtRandom[nCntr], nValue);

            assert(nCntr == nValue && ec == ErrorCode::Success);
        }
    }

    TEST_P(BPlusStore_NoCache_Suite_1, Bulk_Search_v3) 
    {
        for (int nCntr = nTotalRecords - 1; nCntr >= 0; nCntr--)
        {
            ErrorCode ec = m_ptrTree->insert(nCntr, nCntr);
            assert(ec == ErrorCode::Success);
        }

        for (int nCntr = nTotalRecords - 1; nCntr >= 0; nCntr--)
        {
            int nValue = 0;
            ErrorCode ec = m_ptrTree->search(nCntr, nValue);

            assert(nCntr == nValue && ec == ErrorCode::Success);
        }
    }

    TEST_P(BPlusStore_NoCache_Suite_1, Bulk_Delete_v1) 
    {
        for (int nCntr = 0; nCntr < nTotalRecords; nCntr++)
        {
            ErrorCode ec = m_ptrTree->insert(nCntr, nCntr);
            assert(ec == ErrorCode::Success);
        }

        for (int nCntr = 0; nCntr < nTotalRecords; nCntr++)
        {
            ErrorCode ec = m_ptrTree->remove(nCntr);
            assert(ec == ErrorCode::Success);
        }

        for (int nCntr = 0; nCntr < nTotalRecords; nCntr++)
        {
            int nValue = 0;
            ErrorCode ec = m_ptrTree->search(nCntr, nValue);

            assert(nCntr == nValue && ec == ErrorCode::KeyDoesNotExist);
        }
    }

    TEST_P(BPlusStore_NoCache_Suite_1, Bulk_Delete_v2) 
    {
        std::vector<int> vtRandom(nTotalRecords);
        std::iota(vtRandom.begin(), vtRandom.end(), 1);
        std::random_device rd; // Obtain a random number from hardware
        std::mt19937 eng(rd()); // Seed the generator
        std::shuffle(vtRandom.begin(), vtRandom.end(), eng);

        for (int nCntr = 0; nCntr < nTotalRecords; nCntr++)
        {
            ErrorCode ec = m_ptrTree->insert(vtRandom[nCntr], vtRandom[nCntr]);
            assert(ec == ErrorCode::Success);
        }

        for (int nCntr = 0; nCntr < nTotalRecords; nCntr++)
        {
            ErrorCode ec = m_ptrTree->remove(vtRandom[nCntr]);
            assert(ec == ErrorCode::Success);
        }

        for (int nCntr = 0; nCntr < nTotalRecords; nCntr++)
        {
            int nValue = 0;
            ErrorCode ec = m_ptrTree->search(vtRandom[nCntr], nValue);

            assert(nCntr == nValue && ec == ErrorCode::KeyDoesNotExist);
        }
    }

    TEST_P(BPlusStore_NoCache_Suite_1, Bulk_Delete_v3) 
    {
        for (int nCntr = nTotalRecords - 1; nCntr >= 0; nCntr--)
        {
            ErrorCode ec = m_ptrTree->insert(nCntr, nCntr);
            assert(ec == ErrorCode::Success);
        }

        for (int nCntr = nTotalRecords - 1; nCntr >= 0; nCntr--)
        {
            ErrorCode ec = m_ptrTree->remove(nCntr);
            assert(ec == ErrorCode::Success);
        }

        for (int nCntr = nTotalRecords - 1; nCntr >= 0; nCntr--)
        {
            int nValue = 0;
            ErrorCode ec = m_ptrTree->search(nCntr, nValue);

            assert(nCntr == nValue && ec == ErrorCode::KeyDoesNotExist);
        }
    }

    TEST_P(BPlusStore_NoCache_Suite_1, AllOperations)
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
                ErrorCode ec = m_ptrTree->insert(vtRandom[nCntr], vtRandom[nCntr]);
                assert(ec == ErrorCode::Success);
            }

            for (int nCntr = 0; nCntr < nTotalRecords; nCntr++)
            {
                int nValue = 0;
                ErrorCode ec = m_ptrTree->search(vtRandom[nCntr], nValue);

                assert(nValue == vtRandom[nCntr]);
            }

            for (int nCntr = 0; nCntr < nTotalRecords; nCntr = nCntr + 2)
            {
                ErrorCode ec = m_ptrTree->remove(vtRandom[nCntr]);

                assert(ec == ErrorCode::Success);
            }
            for (int nCntr = 1; nCntr < nTotalRecords; nCntr = nCntr + 2)
            {
                ErrorCode ec = m_ptrTree->remove(vtRandom[nCntr]);

                assert(ec == ErrorCode::Success);
            }

            for (int nCntr = 0; nCntr < nTotalRecords; nCntr++)
            {
                int nValue = 0;
                ErrorCode ec = m_ptrTree->search(vtRandom[nCntr], nValue);

                assert(ec == ErrorCode::KeyDoesNotExist);
            }
        }

        for (int nTestCntr = 0; nTestCntr < 2; nTestCntr++)
        {
            for (int nCntr = nTotalRecords; nCntr >= 0; nCntr = nCntr - 2)
            {
                ErrorCode ec = m_ptrTree->insert(nCntr, nCntr);
                assert(ec == ErrorCode::Success);

            }
            for (int nCntr = nTotalRecords - 1; nCntr >= 0; nCntr = nCntr - 2)
            {
                ErrorCode ec = m_ptrTree->insert(nCntr, nCntr);
                assert(ec == ErrorCode::Success);
            }

            for (int nCntr = 0; nCntr < nTotalRecords; nCntr++)
            {
                int nValue = 0;
                ErrorCode ec = m_ptrTree->search(nCntr, nValue);

                assert(nValue == nCntr && ec == ErrorCode::Success);
            }

            for (int nCntr = nTotalRecords; nCntr >= 0; nCntr = nCntr - 2)
            {
                ErrorCode ec = m_ptrTree->remove(nCntr);
                assert(ec == ErrorCode::Success);
            }

            for (int nCntr = nTotalRecords - 1; nCntr >= 0; nCntr = nCntr - 2)
            {
                ErrorCode ec = m_ptrTree->remove(nCntr);
                assert(ec == ErrorCode::Success);
            }

            for (int nCntr = 0; nCntr < nTotalRecords; nCntr++)
            {
                int nValue = 0;
                ErrorCode ec = m_ptrTree->search(nCntr, nValue);

                assert(ec == ErrorCode::KeyDoesNotExist);
            }
        }
    }

    INSTANTIATE_TEST_CASE_P(
        Bulk_Insert_Search_Delete,
        BPlusStore_NoCache_Suite_1,
        ::testing::Values(
            std::make_tuple(3, 1000000),
            std::make_tuple(4, 1000000),
            std::make_tuple(5, 1000000),
            std::make_tuple(6, 1000000),
            std::make_tuple(7, 1000000),
            std::make_tuple(8, 1000000),
            std::make_tuple(15, 1000000),
            std::make_tuple(16, 1000000),
            std::make_tuple(32, 1000000),
            std::make_tuple(64, 1000000),
            std::make_tuple(128, 1000000),
            std::make_tuple(256, 1000000),
            std::make_tuple(512, 1000000),
            std::make_tuple(1024, 1000000),
            std::make_tuple(2048, 1000000)
        ));
}
#endif //__TREE_WITH_CACHE__