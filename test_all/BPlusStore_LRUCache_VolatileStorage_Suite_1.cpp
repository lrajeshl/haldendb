#include "pch.h"
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <variant>
#include <typeinfo>
#include <type_traits>

#include "glog/logging.h"

#include "LRUCache.hpp"
#include "IndexNode.hpp"
#include "DataNode.hpp"
#include "BPlusStore.hpp"
#include "LRUCacheObject.hpp"
#include "VolatileStorage.hpp"
#include "TypeMarshaller.hpp"
#include "TypeId.h"
#include "CacheObjectKey.h"


namespace BPlusStore_LRUCache_VolatileStorage_Suite
{

    class BPlusStore_LRUCache_VolatileStorage_Suite_1 : public ::testing::TestWithParam<std::tuple<int, int, int, int, int>>
    {
    protected:
        typedef int KeyType;
        typedef int ValueType;
        typedef CacheObjectKey CacheKeyType;

        typedef DataNode<KeyType, ValueType, TYPE_UID::DATA_NODE_INT_INT > LeadNodeType;
        typedef IndexNode<KeyType, ValueType, CacheKeyType, TYPE_UID::DATA_NODE_INT_INT > InternalNodeType;

        typedef IFlushCallback<CacheKeyType> ICallback;

        typedef BPlusStore<KeyType, ValueType, LRUCache<ICallback, VolatileStorage<ICallback, CacheKeyType, LRUCacheObject, TypeMarshaller, LeadNodeType, InternalNodeType>>> BPlusStoreType;

        //BPlusStoreType* m_ptrTree;

        void SetUp() override
        {
            std::tie(nDegree, nBegin_BulkInsert, nEnd_BulkInsert, nCacheSize, nStorageSize) = GetParam();

            //m_ptrTree = new BPlusStoreType(3);
            //m_ptrTree->template init<LeadNodeType>();
        }

        void TearDown() override {
            //delete m_ptrTree;
        }

        int nDegree;
        int nBegin_BulkInsert;
        int nEnd_BulkInsert;
        int nCacheSize;
        int nStorageSize;
    };

    TEST_P(BPlusStore_LRUCache_VolatileStorage_Suite_1, Bulk_Insert_v1) {

        BPlusStoreType* ptrTree = new BPlusStoreType(nDegree, nCacheSize, nStorageSize);
        ptrTree->template init<LeadNodeType>();

        for (size_t nCntr = nBegin_BulkInsert; nCntr <= nEnd_BulkInsert; nCntr++)
        {
            ptrTree->template insert<InternalNodeType, LeadNodeType>(nCntr, nCntr);
        }

        delete ptrTree;
    }

    TEST_P(BPlusStore_LRUCache_VolatileStorage_Suite_1, Bulk_Insert_v2) {

        BPlusStoreType* ptrTree = new BPlusStoreType(nDegree, nCacheSize, nStorageSize);
        ptrTree->template init<LeadNodeType>();

        for (size_t nCntr = nBegin_BulkInsert; nCntr <= nEnd_BulkInsert; nCntr = nCntr + 2)
        {
            ptrTree->template insert<InternalNodeType, LeadNodeType>(nCntr, nCntr);
        }

        for (size_t nCntr = nBegin_BulkInsert + 1; nCntr <= nEnd_BulkInsert; nCntr = nCntr + 2)
        {
            ptrTree->template insert<InternalNodeType, LeadNodeType>(nCntr, nCntr);
        }

        delete ptrTree;
    }

    TEST_P(BPlusStore_LRUCache_VolatileStorage_Suite_1, Bulk_Insert_v3) {

        BPlusStoreType* ptrTree = new BPlusStoreType(nDegree, nCacheSize, nStorageSize);
        ptrTree->template init<LeadNodeType>();

        for (int nCntr = nEnd_BulkInsert; nCntr >= nBegin_BulkInsert; nCntr--)
        {
            ptrTree->template insert<InternalNodeType, LeadNodeType>(nCntr, nCntr);
        }

        delete ptrTree;
    }

    TEST_P(BPlusStore_LRUCache_VolatileStorage_Suite_1, Bulk_Search_v1) {

        BPlusStoreType* ptrTree = new BPlusStoreType(nDegree, nCacheSize, nStorageSize);
        ptrTree->template init<LeadNodeType>();

        for (size_t nCntr = nBegin_BulkInsert; nCntr <= nEnd_BulkInsert; nCntr++)
        {
            ptrTree->template insert<InternalNodeType, LeadNodeType>(nCntr, nCntr);
        }

        for (size_t nCntr = nBegin_BulkInsert; nCntr <= nEnd_BulkInsert; nCntr++)
        {
            int nValue = 0;
            ErrorCode code = ptrTree->template search<InternalNodeType, LeadNodeType>(nCntr, nValue);

            ASSERT_EQ(nValue, nCntr);
        }

        delete ptrTree;
    }

    TEST_P(BPlusStore_LRUCache_VolatileStorage_Suite_1, Bulk_Search_v2) {

        BPlusStoreType* ptrTree = new BPlusStoreType(nDegree, nCacheSize, nStorageSize);
        ptrTree->template init<LeadNodeType>();

        for (size_t nCntr = nBegin_BulkInsert; nCntr <= nEnd_BulkInsert; nCntr = nCntr + 2)
        {
            ptrTree->template insert<InternalNodeType, LeadNodeType>(nCntr, nCntr);
        }

        for (size_t nCntr = nBegin_BulkInsert + 1; nCntr <= nEnd_BulkInsert; nCntr = nCntr + 2)
        {
            ptrTree->template insert<InternalNodeType, LeadNodeType>(nCntr, nCntr);
        }

        for (size_t nCntr = nBegin_BulkInsert; nCntr <= nEnd_BulkInsert; nCntr++)
        {
            int nValue = 0;
            ErrorCode code = ptrTree->template search<InternalNodeType, LeadNodeType>(nCntr, nValue);

            ASSERT_EQ(nValue, nCntr);
        }

        delete ptrTree;
    }

    TEST_P(BPlusStore_LRUCache_VolatileStorage_Suite_1, Bulk_Search_v3) {

        BPlusStoreType* ptrTree = new BPlusStoreType(nDegree, nCacheSize, nStorageSize);
        ptrTree->template init<LeadNodeType>();

        for (int nCntr = nEnd_BulkInsert; nCntr >= nBegin_BulkInsert; nCntr--)
        {
            ptrTree->template insert<InternalNodeType, LeadNodeType>(nCntr, nCntr);
        }

        for (size_t nCntr = nBegin_BulkInsert; nCntr <= nEnd_BulkInsert; nCntr++)
        {
            int nValue = 0;
            ErrorCode code = ptrTree->template search<InternalNodeType, LeadNodeType>(nCntr, nValue);

            ASSERT_EQ(nValue, nCntr);
        }

        delete ptrTree;
    }

    TEST_P(BPlusStore_LRUCache_VolatileStorage_Suite_1, Bulk_Delete_v1) {

        BPlusStoreType* ptrTree = new BPlusStoreType(nDegree, nCacheSize, nStorageSize);
        ptrTree->template init<LeadNodeType>();

        for (size_t nCntr = nBegin_BulkInsert; nCntr <= nEnd_BulkInsert; nCntr++)
        {
            ptrTree->template insert<InternalNodeType, LeadNodeType>(nCntr, nCntr);
        }

        for (size_t nCntr = nBegin_BulkInsert; nCntr <= nEnd_BulkInsert; nCntr++)
        {
            int nValue = 0;
            ErrorCode code = ptrTree->template search<InternalNodeType, LeadNodeType>(nCntr, nValue);

            ASSERT_EQ(nValue, nCntr);
        }

        for (size_t nCntr = nBegin_BulkInsert; nCntr <= nEnd_BulkInsert; nCntr++)
        {
            ErrorCode code = ptrTree->template remove<InternalNodeType, LeadNodeType>(nCntr);

            ASSERT_EQ(code, ErrorCode::Success);
        }

        for (size_t nCntr = nBegin_BulkInsert; nCntr <= nEnd_BulkInsert; nCntr++)
        {
            int nValue = 0;
            ErrorCode code = ptrTree->template search<InternalNodeType, LeadNodeType>(nCntr, nValue);

            ASSERT_EQ(code, ErrorCode::KeyDoesNotExist);
        }

        delete ptrTree;
    }

    TEST_P(BPlusStore_LRUCache_VolatileStorage_Suite_1, Bulk_Delete_v2) {

        BPlusStoreType* ptrTree = new BPlusStoreType(nDegree, nCacheSize, nStorageSize);
        ptrTree->template init<LeadNodeType>();

        for (size_t nCntr = nBegin_BulkInsert; nCntr <= nEnd_BulkInsert; nCntr = nCntr + 2)
        {
            ptrTree->template insert<InternalNodeType, LeadNodeType>(nCntr, nCntr);
        }

        for (size_t nCntr = nBegin_BulkInsert + 1; nCntr <= nEnd_BulkInsert; nCntr = nCntr + 2)
        {
            ptrTree->template insert<InternalNodeType, LeadNodeType>(nCntr, nCntr);
        }

        for (size_t nCntr = nBegin_BulkInsert; nCntr <= nEnd_BulkInsert; nCntr++)
        {
            int nValue = 0;
            ErrorCode code = ptrTree->template search<InternalNodeType, LeadNodeType>(nCntr, nValue);

            ASSERT_EQ(nValue, nCntr);
        }

        for (size_t nCntr = nBegin_BulkInsert; nCntr <= nEnd_BulkInsert; nCntr++)
        {
            ErrorCode code = ptrTree->template remove<InternalNodeType, LeadNodeType>(nCntr);

            ASSERT_EQ(code, ErrorCode::Success);
        }

        for (size_t nCntr = nBegin_BulkInsert; nCntr <= nEnd_BulkInsert; nCntr++)
        {
            int nValue = 0;
            ErrorCode code = ptrTree->template search<InternalNodeType, LeadNodeType>(nCntr, nValue);

            ASSERT_EQ(code, ErrorCode::KeyDoesNotExist);
        }

        delete ptrTree;
    }

    TEST_P(BPlusStore_LRUCache_VolatileStorage_Suite_1, Bulk_Delete_v3) {

        BPlusStoreType* ptrTree = new BPlusStoreType(nDegree, nCacheSize, nStorageSize);
        ptrTree->template init<LeadNodeType>();

        for (int nCntr = nEnd_BulkInsert; nCntr >= nBegin_BulkInsert; nCntr--)
        {
            ptrTree->template insert<InternalNodeType, LeadNodeType>(nCntr, nCntr);
        }

        for (size_t nCntr = nBegin_BulkInsert; nCntr <= nEnd_BulkInsert; nCntr++)
        {
            int nValue = 0;
            ErrorCode code = ptrTree->template search<InternalNodeType, LeadNodeType>(nCntr, nValue);

            ASSERT_EQ(nValue, nCntr);
        }

        for (int nCntr = nEnd_BulkInsert; nCntr >= nBegin_BulkInsert; nCntr--)
        {
            ErrorCode code = ptrTree->template remove<InternalNodeType, LeadNodeType>(nCntr);

            ASSERT_EQ(code, ErrorCode::Success);
        }

        for (size_t nCntr = nBegin_BulkInsert; nCntr <= nEnd_BulkInsert; nCntr++)
        {
            int nValue = 0;
            ErrorCode code = ptrTree->template search<InternalNodeType, LeadNodeType>(nCntr, nValue);

            ASSERT_EQ(code, ErrorCode::KeyDoesNotExist);
        }


        delete ptrTree;
    }

    INSTANTIATE_TEST_CASE_P(
        Bulk_Insert_Search_Delete,
        BPlusStore_LRUCache_VolatileStorage_Suite_1,
        ::testing::Values(
            std::make_tuple(3, 0, 99999, 10000, 900000000),
            std::make_tuple(4, 0, 99999, 10000, 900000000),
            std::make_tuple(5, 0, 99999, 10000, 900000000),
            std::make_tuple(6, 0, 99999, 10000, 900000000),
            std::make_tuple(7, 0, 99999, 10000, 900000000),
            std::make_tuple(8, 0, 99999, 10000, 900000000),
            std::make_tuple(15, 0, 199999, 10000, 900000000),
            std::make_tuple(16, 0, 199999, 10000, 900000000),
            std::make_tuple(32, 0, 199999, 10000, 900000000),
            std::make_tuple(64, 0, 199999, 10000, 900000000)));
    
}