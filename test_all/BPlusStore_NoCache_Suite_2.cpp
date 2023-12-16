#include "pch.h"
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <variant>
#include <typeinfo>
#include <type_traits>

#include "glog/logging.h"

#include "NoCache.h"
#include "IndexNode.hpp"
#include "DataNode.hpp"
#include "BPlusStore.hpp"
#include "NoCacheObject.hpp"
#include "TypeId.h"
namespace BPlusStore_NoCache_Suite
{
    class BPlusStore_NoCache_Suite_2 : public ::testing::TestWithParam<std::tuple<int, int, int>>
    {
    protected:
        typedef string KeyType;
        typedef string ValueType;
        typedef uintptr_t CacheKeyType;

        typedef DataNode<KeyType, ValueType, TYPE_UID::DATA_NODE_STRING_STRING> LeadNodeType;
        typedef IndexNode<KeyType, ValueType, CacheKeyType, TYPE_UID::DATA_NODE_STRING_STRING> InternalNodeType;

        typedef BPlusStore<KeyType, ValueType, NoCache<CacheKeyType, NoCacheObject, LeadNodeType, InternalNodeType>> BPlusStoreType;

        BPlusStoreType* m_ptrTree;

        void SetUp() override
        {
            std::tie(nDegree, nBegin_BulkInsert, nEnd_BulkInsert) = GetParam();

            //m_ptrTree = new BPlusStoreType(3);
            //m_ptrTree->template init<LeadNodeType>();
        }

        void TearDown() override {
            //delete m_ptrTree;
        }

        int nDegree;
        int nBegin_BulkInsert;
        int nEnd_BulkInsert;
    };

    TEST_P(BPlusStore_NoCache_Suite_2, Bulk_Insert_v1) {

        BPlusStoreType* ptrTree = new BPlusStoreType(nDegree);
        ptrTree->template init<LeadNodeType>();

        for (size_t nCntr = nBegin_BulkInsert; nCntr <= nEnd_BulkInsert; nCntr++)
        {
            ptrTree->template insert<InternalNodeType, LeadNodeType>(to_string(nCntr), to_string(nCntr));
        }

        delete ptrTree;
    }

    TEST_P(BPlusStore_NoCache_Suite_2, Bulk_Insert_v2) {

        BPlusStoreType* ptrTree = new BPlusStoreType(nDegree);
        ptrTree->template init<LeadNodeType>();

        for (size_t nCntr = nBegin_BulkInsert; nCntr <= nEnd_BulkInsert; nCntr = nCntr + 2)
        {
            ptrTree->template insert<InternalNodeType, LeadNodeType>(to_string(nCntr), to_string(nCntr));
        }

        for (size_t nCntr = nBegin_BulkInsert + 1; nCntr <= nEnd_BulkInsert; nCntr = nCntr + 2)
        {
            ptrTree->template insert<InternalNodeType, LeadNodeType>(to_string(nCntr), to_string(nCntr));
        }

        delete ptrTree;
    }

    TEST_P(BPlusStore_NoCache_Suite_2, Bulk_Insert_v3) {

        BPlusStoreType* ptrTree = new BPlusStoreType(nDegree);
        ptrTree->template init<LeadNodeType>();

        for (int nCntr = nEnd_BulkInsert; nCntr >= nBegin_BulkInsert; nCntr--)
        {
            ptrTree->template insert<InternalNodeType, LeadNodeType>(to_string(nCntr), to_string(nCntr));
        }

        delete ptrTree;
    }

    TEST_P(BPlusStore_NoCache_Suite_2, Bulk_Search_v1) {

        BPlusStoreType* ptrTree = new BPlusStoreType(nDegree);
        ptrTree->template init<LeadNodeType>();

        for (size_t nCntr = nBegin_BulkInsert; nCntr <= nEnd_BulkInsert; nCntr++)
        {
            ptrTree->template insert<InternalNodeType, LeadNodeType>(to_string(nCntr), to_string(nCntr));
        }

        for (size_t nCntr = nBegin_BulkInsert; nCntr <= nEnd_BulkInsert; nCntr++)
        {
            string nValue;
            ErrorCode code = ptrTree->template search<InternalNodeType, LeadNodeType>(to_string(nCntr), nValue);

            ASSERT_EQ(nValue, to_string(nCntr));
        }

        delete ptrTree;
    }

    TEST_P(BPlusStore_NoCache_Suite_2, Bulk_Search_v2) {

        BPlusStoreType* ptrTree = new BPlusStoreType(nDegree);
        ptrTree->template init<LeadNodeType>();

        for (size_t nCntr = nBegin_BulkInsert; nCntr <= nEnd_BulkInsert; nCntr = nCntr + 2)
        {
            ptrTree->template insert<InternalNodeType, LeadNodeType>(to_string(nCntr), to_string(nCntr));
        }

        for (size_t nCntr = nBegin_BulkInsert + 1; nCntr <= nEnd_BulkInsert; nCntr = nCntr + 2)
        {
            ptrTree->template insert<InternalNodeType, LeadNodeType>(to_string(nCntr), to_string(nCntr));
        }

        for (size_t nCntr = nBegin_BulkInsert; nCntr <= nEnd_BulkInsert; nCntr++)
        {
            string nValue;
            ErrorCode code = ptrTree->template search<InternalNodeType, LeadNodeType>(to_string(nCntr), nValue);

            ASSERT_EQ(nValue, to_string(nCntr));
        }

        delete ptrTree;
    }

    TEST_P(BPlusStore_NoCache_Suite_2, Bulk_Search_v3) {

        BPlusStoreType* ptrTree = new BPlusStoreType(nDegree);
        ptrTree->template init<LeadNodeType>();

        for (int nCntr = nEnd_BulkInsert; nCntr >= nBegin_BulkInsert; nCntr--)
        {
            ptrTree->template insert<InternalNodeType, LeadNodeType>(to_string(nCntr), to_string(nCntr));
        }

        for (size_t nCntr = nBegin_BulkInsert; nCntr <= nEnd_BulkInsert; nCntr++)
        {
            string nValue;
            ErrorCode code = ptrTree->template search<InternalNodeType, LeadNodeType>(to_string(nCntr), nValue);

            ASSERT_EQ(nValue, to_string(nCntr));
        }

        delete ptrTree;
    }

    TEST_P(BPlusStore_NoCache_Suite_2, Bulk_Delete_v1) {

        BPlusStoreType* ptrTree = new BPlusStoreType(nDegree);
        ptrTree->template init<LeadNodeType>();

        for (size_t nCntr = nBegin_BulkInsert; nCntr <= nEnd_BulkInsert; nCntr++)
        {
            ptrTree->template insert<InternalNodeType, LeadNodeType>(to_string(nCntr), to_string(nCntr));
        }

        for (size_t nCntr = nBegin_BulkInsert; nCntr <= nEnd_BulkInsert; nCntr++)
        {
            string nValue;
            ErrorCode code = ptrTree->template search<InternalNodeType, LeadNodeType>(to_string(nCntr), nValue);

            ASSERT_EQ(nValue, to_string(nCntr));
        }

        for (size_t nCntr = nBegin_BulkInsert; nCntr <= nEnd_BulkInsert; nCntr++)
        {
            ErrorCode code = ptrTree->template remove<InternalNodeType, LeadNodeType>(to_string(nCntr));

            ASSERT_EQ(code, ErrorCode::Success);
        }

        for (size_t nCntr = nBegin_BulkInsert; nCntr <= nEnd_BulkInsert; nCntr++)
        {
            string nValue;
            ErrorCode code = ptrTree->template search<InternalNodeType, LeadNodeType>(to_string(nCntr), nValue);

            ASSERT_EQ(code, ErrorCode::KeyDoesNotExist);
        }

        delete ptrTree;
    }

    TEST_P(BPlusStore_NoCache_Suite_2, Bulk_Delete_v2) {

        BPlusStoreType* ptrTree = new BPlusStoreType(nDegree);
        ptrTree->template init<LeadNodeType>();

        for (size_t nCntr = nBegin_BulkInsert; nCntr <= nEnd_BulkInsert; nCntr = nCntr + 2)
        {
            ptrTree->template insert<InternalNodeType, LeadNodeType>(to_string(nCntr), to_string(nCntr));
        }

        for (size_t nCntr = nBegin_BulkInsert + 1; nCntr <= nEnd_BulkInsert; nCntr = nCntr + 2)
        {
            ptrTree->template insert<InternalNodeType, LeadNodeType>(to_string(nCntr), to_string(nCntr));
        }

        for (size_t nCntr = nBegin_BulkInsert; nCntr <= nEnd_BulkInsert; nCntr++)
        {
            string nValue;
            ErrorCode code = ptrTree->template search<InternalNodeType, LeadNodeType>(to_string(nCntr), nValue);

            ASSERT_EQ(nValue, to_string(nCntr));
        }

        for (size_t nCntr = nBegin_BulkInsert; nCntr <= nEnd_BulkInsert; nCntr++)
        {
            ErrorCode code = ptrTree->template remove<InternalNodeType, LeadNodeType>(to_string(nCntr));

            ASSERT_EQ(code, ErrorCode::Success);
        }

        for (size_t nCntr = nBegin_BulkInsert; nCntr <= nEnd_BulkInsert; nCntr++)
        {
            string  nValue;
            ErrorCode code = ptrTree->template search<InternalNodeType, LeadNodeType>(to_string(nCntr), nValue);
            ASSERT_EQ(code, ErrorCode::KeyDoesNotExist);
        }

        delete ptrTree;
    }

    TEST_P(BPlusStore_NoCache_Suite_2, Bulk_Delete_v3) {

        BPlusStoreType* ptrTree = new BPlusStoreType(nDegree);
        ptrTree->template init<LeadNodeType>();

        for (int nCntr = nEnd_BulkInsert; nCntr >= nBegin_BulkInsert; nCntr--)
        {
            ptrTree->template insert<InternalNodeType, LeadNodeType>(to_string(nCntr), to_string(nCntr));
        }

        for (size_t nCntr = nBegin_BulkInsert; nCntr <= nEnd_BulkInsert; nCntr++)
        {
            string nValue;
            ErrorCode code = ptrTree->template search<InternalNodeType, LeadNodeType>(to_string(nCntr), nValue);

            ASSERT_EQ(nValue, to_string(nCntr));
        }

        for (int nCntr = nEnd_BulkInsert; nCntr >= nBegin_BulkInsert; nCntr--)
        {
            ErrorCode code = ptrTree->template remove<InternalNodeType, LeadNodeType>(to_string(nCntr));

            ASSERT_EQ(code, ErrorCode::Success);
        }

        for (size_t nCntr = nBegin_BulkInsert; nCntr <= nEnd_BulkInsert; nCntr++)
        {
            string nValue;
            ErrorCode code = ptrTree->template search<InternalNodeType, LeadNodeType>(to_string(nCntr), nValue);

            ASSERT_EQ(code, ErrorCode::KeyDoesNotExist);
        }


        delete ptrTree;
    }

    INSTANTIATE_TEST_CASE_P(
        Bulk_Insert_Search_Delete,
        BPlusStore_NoCache_Suite_2,
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