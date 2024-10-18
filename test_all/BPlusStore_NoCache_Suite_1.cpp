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

#ifndef __TREE_WITH_CACHE__
namespace BPlusStore_NoCache_Suite
{
    typedef int KeyType;
    typedef int ValueType;
    typedef uintptr_t ObjectUIDType;

    typedef DataNode<KeyType, ValueType, ObjectUIDType, TYPE_UID::DATA_NODE_INT_INT > DataNodeType;
    typedef IndexNode<KeyType, ValueType, ObjectUIDType, DataNodeType, TYPE_UID::INDEX_NODE_INT_INT > IndexNodeType;

    typedef BPlusStore<KeyType, ValueType, NoCache<ObjectUIDType, NoCacheObject, DataNodeType, IndexNodeType>> BPlusStoreType;

    class BPlusStore_NoCache_Suite_1 : public ::testing::TestWithParam<std::tuple<int, int, int>>
    {
    protected:
        void SetUp() override
        {
            std::tie(nDegree, nBegin_BulkInsert, nEnd_BulkInsert) = GetParam();

            m_ptrTree = new BPlusStoreType(nDegree);
            m_ptrTree->init<DataNodeType>();
        }

        void TearDown() override {
            //delete m_ptrTree;
        }

        BPlusStoreType* m_ptrTree;

        int nDegree;
        int nBegin_BulkInsert;
        int nEnd_BulkInsert;
    };

    TEST_P(BPlusStore_NoCache_Suite_1, Bulk_Insert_v1) 
    {
        for (size_t nCntr = nBegin_BulkInsert; nCntr <= nEnd_BulkInsert; nCntr++)
        {
            m_ptrTree->insert(nCntr, nCntr);
        }
    }

    TEST_P(BPlusStore_NoCache_Suite_1, Bulk_Insert_v2) 
    {
        for (size_t nCntr = nBegin_BulkInsert; nCntr <= nEnd_BulkInsert; nCntr = nCntr + 2)
        {
            m_ptrTree->insert(nCntr, nCntr);
        }

        for (size_t nCntr = nBegin_BulkInsert + 1; nCntr <= nEnd_BulkInsert; nCntr = nCntr + 2)
        {
            m_ptrTree->insert(nCntr, nCntr);
        }
    }

    TEST_P(BPlusStore_NoCache_Suite_1, Bulk_Insert_v3) 
    {
        for (int nCntr = nEnd_BulkInsert; nCntr >= nBegin_BulkInsert; nCntr--)
        {
            m_ptrTree->insert(nCntr, nCntr);
        }
    }

    TEST_P(BPlusStore_NoCache_Suite_1, Bulk_Search_v1) 
    {
        for (size_t nCntr = nBegin_BulkInsert; nCntr <= nEnd_BulkInsert; nCntr++)
        {
            m_ptrTree->insert(nCntr, nCntr);
        }

        for (size_t nCntr = nBegin_BulkInsert; nCntr <= nEnd_BulkInsert; nCntr++)
        {
            int nValue = 0;
            ErrorCode code = m_ptrTree->search(nCntr, nValue);

            ASSERT_EQ(nValue, nCntr);
        }
    }

    TEST_P(BPlusStore_NoCache_Suite_1, Bulk_Search_v2) 
    {
        for (size_t nCntr = nBegin_BulkInsert; nCntr <= nEnd_BulkInsert; nCntr = nCntr + 2)
        {
            m_ptrTree->insert(nCntr, nCntr);
        }

        for (size_t nCntr = nBegin_BulkInsert + 1; nCntr <= nEnd_BulkInsert; nCntr = nCntr + 2)
        {
            m_ptrTree->insert(nCntr, nCntr);
        }

        for (size_t nCntr = nBegin_BulkInsert; nCntr <= nEnd_BulkInsert; nCntr++)
        {
            int nValue = 0;
            ErrorCode code = m_ptrTree->search(nCntr, nValue);

            ASSERT_EQ(nValue, nCntr);
        }
    }

    TEST_P(BPlusStore_NoCache_Suite_1, Bulk_Search_v3) 
    {
        for (int nCntr = nEnd_BulkInsert; nCntr >= nBegin_BulkInsert; nCntr--)
        {
            m_ptrTree->insert(nCntr, nCntr);
        }

        for (size_t nCntr = nBegin_BulkInsert; nCntr <= nEnd_BulkInsert; nCntr++)
        {
            int nValue = 0;
            ErrorCode code = m_ptrTree->search(nCntr, nValue);

            ASSERT_EQ(nValue, nCntr);
        }
    }

    TEST_P(BPlusStore_NoCache_Suite_1, Bulk_Delete_v1) 
    {
        for (size_t nCntr = nBegin_BulkInsert; nCntr <= nEnd_BulkInsert; nCntr++)
        {
            m_ptrTree->insert(nCntr, nCntr);
        }

        for (size_t nCntr = nBegin_BulkInsert; nCntr <= nEnd_BulkInsert; nCntr++)
        {
            int nValue = 0;
            ErrorCode code = m_ptrTree->search(nCntr, nValue);

            ASSERT_EQ(nValue, nCntr);
        }

        for (size_t nCntr = nBegin_BulkInsert; nCntr <= nEnd_BulkInsert; nCntr++)
        {
            ErrorCode code = m_ptrTree->remove(nCntr);

            ASSERT_EQ(code, ErrorCode::Success);
        }

        for (size_t nCntr = nBegin_BulkInsert; nCntr <= nEnd_BulkInsert; nCntr++)
        {
            int nValue = 0;
            ErrorCode code = m_ptrTree->search(nCntr, nValue);

            ASSERT_EQ(code, ErrorCode::KeyDoesNotExist);
        }
    }

    TEST_P(BPlusStore_NoCache_Suite_1, Bulk_Delete_v2) 
    {
        for (size_t nCntr = nBegin_BulkInsert; nCntr <= nEnd_BulkInsert; nCntr = nCntr + 2)
        {
            m_ptrTree->insert(nCntr, nCntr);
        }

        for (size_t nCntr = nBegin_BulkInsert + 1; nCntr <= nEnd_BulkInsert; nCntr = nCntr + 2)
        {
            m_ptrTree->insert(nCntr, nCntr);
        }

        for (size_t nCntr = nBegin_BulkInsert; nCntr <= nEnd_BulkInsert; nCntr++)
        {
            int nValue = 0;
            ErrorCode code = m_ptrTree->search(nCntr, nValue);

            ASSERT_EQ(nValue, nCntr);
        }

        for (size_t nCntr = nBegin_BulkInsert; nCntr <= nEnd_BulkInsert; nCntr++)
        {
            ErrorCode code = m_ptrTree->remove(nCntr);

            ASSERT_EQ(code, ErrorCode::Success);
        }

        for (size_t nCntr = nBegin_BulkInsert; nCntr <= nEnd_BulkInsert; nCntr++)
        {
            int nValue = 0;
            ErrorCode code = m_ptrTree->search(nCntr, nValue);

            ASSERT_EQ(code, ErrorCode::KeyDoesNotExist);
        }
    }

    TEST_P(BPlusStore_NoCache_Suite_1, Bulk_Delete_v3) 
    {
        for (int nCntr = nEnd_BulkInsert; nCntr >= nBegin_BulkInsert; nCntr--)
        {
            m_ptrTree->insert(nCntr, nCntr);
        }

        for (size_t nCntr = nBegin_BulkInsert; nCntr <= nEnd_BulkInsert; nCntr++)
        {
            int nValue = 0;
            ErrorCode code = m_ptrTree->search(nCntr, nValue);

            ASSERT_EQ(nValue, nCntr);
        }

        for (int nCntr = nEnd_BulkInsert; nCntr >= nBegin_BulkInsert; nCntr--)
        {
            ErrorCode code = m_ptrTree->remove(nCntr);

            ASSERT_EQ(code, ErrorCode::Success);
        }

        for (size_t nCntr = nBegin_BulkInsert; nCntr <= nEnd_BulkInsert; nCntr++)
        {
            int nValue = 0;
            ErrorCode code = m_ptrTree->search(nCntr, nValue);

            ASSERT_EQ(code, ErrorCode::KeyDoesNotExist);
        }
    }

    INSTANTIATE_TEST_CASE_P(
        Bulk_Insert_Search_Delete,
        BPlusStore_NoCache_Suite_1,
        ::testing::Values(
            std::make_tuple(3, 0, 99999),
            std::make_tuple(4, 0, 99999),
            std::make_tuple(5, 0, 99999),
            std::make_tuple(6, 0, 99999),
            std::make_tuple(7, 0, 99999),
            std::make_tuple(8, 0, 99999),
            std::make_tuple(15, 0, 199999),
            std::make_tuple(16, 0, 199999),
            std::make_tuple(32, 0, 199999),
            std::make_tuple(64, 0, 199999)));   
}
#endif //__TREE_WITH_CACHE__