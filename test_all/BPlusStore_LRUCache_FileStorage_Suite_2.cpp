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
#include "FileStorage.hpp"
#include "TypeMarshaller.hpp"

#include "TypeUID.h"
#include "ObjectFatUID.h"

//#ifdef __TREE_WITH_CACHE__
//namespace BPlusStore_LRUCache_FileStorage_Suite
//{
//    class BPlusStore_LRUCache_FileStorage_Suite_2 : public ::testing::TestWithParam<std::tuple<int, int, int, int, int, int, string>>
//    {
//    protected:
//        typedef string KeyType;
//        typedef string ValueType;
//        typedef ObjectFatUID ObjectUIDType;
//        
//        typedef DataNode<KeyType, ValueType, ObjectUIDType, TYPE_UID::DATA_NODE_STRING_STRING> DataNodeType;
//        typedef IndexNode<KeyType, ValueType, ObjectUIDType, TYPE_UID::INDEX_NODE_STRING_STRING> IndexNodeType;
//
//        typedef LRUCacheObject<TypeMarshaller, DataNodeType, IndexNodeType> ObjectType;
//        typedef IFlushCallback<ObjectUIDType, ObjectType> ICallback;
//
//        typedef BPlusStore<ICallback, KeyType, ValueType, LRUCache<ICallback, FileStorage<ICallback, ObjectUIDType, LRUCacheObject, TypeMarshaller, DataNodeType, IndexNodeType>>> BPlusStoreType;
//
//        BPlusStoreType* m_ptrTree;
//
//        void SetUp() override
//        {
//            std::tie(nDegree, nBegin_BulkInsert, nEnd_BulkInsert, nCacheSize, nBlockSize, nFileSize, stFileName) = GetParam();
//
//            //m_ptrTree = new BPlusStoreType(3);
//            //m_ptrTree->template init<DataNodeType>();
//        }
//
//        void TearDown() override {
//            //delete m_ptrTree;
//        }
//
//        int nDegree;
//        int nBegin_BulkInsert;
//        int nEnd_BulkInsert;
//        int nCacheSize;
//        int nBlockSize;
//        int nFileSize;
//        string stFileName;
//    };
//
//    TEST_P(BPlusStore_LRUCache_FileStorage_Suite_2, Bulk_Insert_v1) {
//
//        BPlusStoreType* ptrTree = new BPlusStoreType(nDegree, nCacheSize, nBlockSize, nFileSize, stFileName);
//        ptrTree->template init<DataNodeType>();
//
//        for (size_t nCntr = nBegin_BulkInsert; nCntr <= nEnd_BulkInsert; nCntr++)
//        {
//            ptrTree->insert(to_string(nCntr), to_string(nCntr));
//        }
//
//        delete ptrTree;
//    }
//
//    TEST_P(BPlusStore_LRUCache_FileStorage_Suite_2, Bulk_Insert_v2) {
//
//        BPlusStoreType* ptrTree = new BPlusStoreType(nDegree, nCacheSize, nBlockSize, nFileSize, stFileName);
//        ptrTree->template init<DataNodeType>();
//
//        for (size_t nCntr = nBegin_BulkInsert; nCntr <= nEnd_BulkInsert; nCntr = nCntr + 2)
//        {
//            ptrTree->insert(to_string(nCntr), to_string(nCntr));
//        }
//
//        for (size_t nCntr = nBegin_BulkInsert + 1; nCntr <= nEnd_BulkInsert; nCntr = nCntr + 2)
//        {
//            ptrTree->insert(to_string(nCntr), to_string(nCntr));
//        }
//
//        delete ptrTree;
//    }
//
//    TEST_P(BPlusStore_LRUCache_FileStorage_Suite_2, Bulk_Insert_v3) {
//
//        BPlusStoreType* ptrTree = new BPlusStoreType(nDegree, nCacheSize, nBlockSize, nFileSize, stFileName);
//        ptrTree->template init<DataNodeType>();
//
//        for (int nCntr = nEnd_BulkInsert; nCntr >= nBegin_BulkInsert; nCntr--)
//        {
//            ptrTree->insert(to_string(nCntr), to_string(nCntr));
//        }
//
//        delete ptrTree;
//    }
//
//    TEST_P(BPlusStore_LRUCache_FileStorage_Suite_2, Bulk_Search_v1) {
//
//        BPlusStoreType* ptrTree = new BPlusStoreType(nDegree, nCacheSize, nBlockSize, nFileSize, stFileName);
//        ptrTree->template init<DataNodeType>();
//
//        for (size_t nCntr = nBegin_BulkInsert; nCntr <= nEnd_BulkInsert; nCntr++)
//        {
//            ptrTree->insert(to_string(nCntr), to_string(nCntr));
//        }
//
//        for (size_t nCntr = nBegin_BulkInsert; nCntr <= nEnd_BulkInsert; nCntr++)
//        {
//            string nValue;
//            ErrorCode code = ptrTree->search(to_string(nCntr), nValue);
//
//            ASSERT_EQ(nValue, to_string(nCntr));
//        }
//
//        delete ptrTree;
//    }
//
//    TEST_P(BPlusStore_LRUCache_FileStorage_Suite_2, Bulk_Search_v2) {
//
//        BPlusStoreType* ptrTree = new BPlusStoreType(nDegree, nCacheSize, nBlockSize, nFileSize, stFileName);
//        ptrTree->template init<DataNodeType>();
//
//        for (size_t nCntr = nBegin_BulkInsert; nCntr <= nEnd_BulkInsert; nCntr = nCntr + 2)
//        {
//            ptrTree->insert(to_string(nCntr), to_string(nCntr));
//        }
//
//        for (size_t nCntr = nBegin_BulkInsert + 1; nCntr <= nEnd_BulkInsert; nCntr = nCntr + 2)
//        {
//            ptrTree->insert(to_string(nCntr), to_string(nCntr));
//        }
//
//        for (size_t nCntr = nBegin_BulkInsert; nCntr <= nEnd_BulkInsert; nCntr++)
//        {
//            string nValue;
//            ErrorCode code = ptrTree->search(to_string(nCntr), nValue);
//
//            ASSERT_EQ(nValue, to_string(nCntr));
//        }
//
//        delete ptrTree;
//    }
//
//    TEST_P(BPlusStore_LRUCache_FileStorage_Suite_2, Bulk_Search_v3) {
//
//        BPlusStoreType* ptrTree = new BPlusStoreType(nDegree, nCacheSize, nBlockSize, nFileSize, stFileName);
//        ptrTree->template init<DataNodeType>();
//
//        for (int nCntr = nEnd_BulkInsert; nCntr >= nBegin_BulkInsert; nCntr--)
//        {
//            ptrTree->insert(to_string(nCntr), to_string(nCntr));
//        }
//
//        for (size_t nCntr = nBegin_BulkInsert; nCntr <= nEnd_BulkInsert; nCntr++)
//        {
//            string nValue;
//            ErrorCode code = ptrTree->search(to_string(nCntr), nValue);
//
//            ASSERT_EQ(nValue, to_string(nCntr));
//        }
//
//        delete ptrTree;
//    }
//
//    TEST_P(BPlusStore_LRUCache_FileStorage_Suite_2, Bulk_Delete_v1) {
//
//        BPlusStoreType* ptrTree = new BPlusStoreType(nDegree, nCacheSize, nBlockSize, nFileSize, stFileName);
//        ptrTree->template init<DataNodeType>();
//
//        for (size_t nCntr = nBegin_BulkInsert; nCntr <= nEnd_BulkInsert; nCntr++)
//        {
//            ptrTree->insert(to_string(nCntr), to_string(nCntr));
//        }
//
//        for (size_t nCntr = nBegin_BulkInsert; nCntr <= nEnd_BulkInsert; nCntr++)
//        {
//            string nValue;
//            ErrorCode code = ptrTree->search(to_string(nCntr), nValue);
//
//            ASSERT_EQ(nValue, to_string(nCntr));
//        }
//
//        for (size_t nCntr = nBegin_BulkInsert; nCntr <= nEnd_BulkInsert; nCntr++)
//        {
//            ErrorCode code = ptrTree->remove(to_string(nCntr));
//
//            ASSERT_EQ(code, ErrorCode::Success);
//        }
//
//        for (size_t nCntr = nBegin_BulkInsert; nCntr <= nEnd_BulkInsert; nCntr++)
//        {
//            string nValue;
//            ErrorCode code = ptrTree->search(to_string(nCntr), nValue);
//
//            ASSERT_EQ(code, ErrorCode::KeyDoesNotExist);
//        }
//
//        delete ptrTree;
//    }
//
//    TEST_P(BPlusStore_LRUCache_FileStorage_Suite_2, Bulk_Delete_v2) {
//
//        BPlusStoreType* ptrTree = new BPlusStoreType(nDegree, nCacheSize, nBlockSize, nFileSize, stFileName);
//        ptrTree->template init<DataNodeType>();
//
//        for (size_t nCntr = nBegin_BulkInsert; nCntr <= nEnd_BulkInsert; nCntr = nCntr + 2)
//        {
//            ptrTree->insert(to_string(nCntr), to_string(nCntr));
//        }
//
//        for (size_t nCntr = nBegin_BulkInsert + 1; nCntr <= nEnd_BulkInsert; nCntr = nCntr + 2)
//        {
//            ptrTree->insert(to_string(nCntr), to_string(nCntr));
//        }
//
//        for (size_t nCntr = nBegin_BulkInsert; nCntr <= nEnd_BulkInsert; nCntr++)
//        {
//            string nValue;
//            ErrorCode code = ptrTree->search(to_string(nCntr), nValue);
//
//            ASSERT_EQ(nValue, to_string(nCntr));
//        }
//
//        for (size_t nCntr = nBegin_BulkInsert; nCntr <= nEnd_BulkInsert; nCntr++)
//        {
//            ErrorCode code = ptrTree->remove(to_string(nCntr));
//
//            ASSERT_EQ(code, ErrorCode::Success);
//        }
//
//        for (size_t nCntr = nBegin_BulkInsert; nCntr <= nEnd_BulkInsert; nCntr++)
//        {
//            string  nValue;
//            ErrorCode code = ptrTree->search(to_string(nCntr), nValue);
//            ASSERT_EQ(code, ErrorCode::KeyDoesNotExist);
//        }
//
//        delete ptrTree;
//    }
//
//    TEST_P(BPlusStore_LRUCache_FileStorage_Suite_2, Bulk_Delete_v3) {
//
//        BPlusStoreType* ptrTree = new BPlusStoreType(nDegree, nCacheSize, nBlockSize, nFileSize, stFileName);
//        ptrTree->template init<DataNodeType>();
//
//        for (int nCntr = nEnd_BulkInsert; nCntr >= nBegin_BulkInsert; nCntr--)
//        {
//            ptrTree->insert(to_string(nCntr), to_string(nCntr));
//        }
//
//        for (size_t nCntr = nBegin_BulkInsert; nCntr <= nEnd_BulkInsert; nCntr++)
//        {
//            string nValue;
//            ErrorCode code = ptrTree->search(to_string(nCntr), nValue);
//
//            ASSERT_EQ(nValue, to_string(nCntr));
//        }
//
//        for (int nCntr = nEnd_BulkInsert; nCntr >= nBegin_BulkInsert; nCntr--)
//        {
//            ErrorCode code = ptrTree->remove(to_string(nCntr));
//
//            ASSERT_EQ(code, ErrorCode::Success);
//        }
//
//        for (size_t nCntr = nBegin_BulkInsert; nCntr <= nEnd_BulkInsert; nCntr++)
//        {
//            string nValue;
//            ErrorCode code = ptrTree->search(to_string(nCntr), nValue);
//
//            ASSERT_EQ(code, ErrorCode::KeyDoesNotExist);
//        }
//
//
//        delete ptrTree;
//    }
//
//    INSTANTIATE_TEST_CASE_P(
//        Bulk_Insert_Search_Delete,
//        BPlusStore_LRUCache_FileStorage_Suite_2,
//        ::testing::Values(
//            std::make_tuple(3, 0, 99999, 100, 1024, 1024, ""),
//            std::make_tuple(4, 0, 99999, 100, 1024, 1024, ""),
//            std::make_tuple(5, 0, 99999, 100, 1024, 1024, ""),
//            std::make_tuple(6, 0, 99999, 100, 1024, 1024, ""),
//            std::make_tuple(7, 0, 99999, 100, 1024, 1024, ""),
//            std::make_tuple(8, 0, 99999, 100, 1024, 1024, ""),
//            std::make_tuple(15, 0, 199999, 100, 1024, 1024, ""),
//            std::make_tuple(16, 0, 199999, 100, 1024, 1024, ""),
//            std::make_tuple(32, 0, 199999, 100, 1024, 1024, ""),
//            std::make_tuple(64, 0, 199999, 100, 1024, 1024, "")));
//}
//#endif __TREE_WITH_CACHE__