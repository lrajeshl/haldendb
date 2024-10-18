#include "pch.h"
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <variant>
#include <typeinfo>
#include <type_traits>
#include <fstream>
#include <filesystem>

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
#include "IFlushCallback.h"

#ifdef __TREE_WITH_CACHE__
namespace BPlusStore_LRUCache_FileStorage_Suite
{
    typedef int KeyType;
    typedef int ValueType;

    typedef ObjectFatUID ObjectUIDType;

    typedef DataNode<KeyType, ValueType, ObjectUIDType, TYPE_UID::DATA_NODE_INT_INT > DataNodeType;
    typedef IndexNode<KeyType, ValueType, ObjectUIDType, DataNodeType, TYPE_UID::INDEX_NODE_INT_INT > IndexNodeType;

    typedef LRUCacheObject<TypeMarshaller, DataNodeType, IndexNodeType> ObjectType;
    typedef IFlushCallback<ObjectUIDType, ObjectType> ICallback;

    typedef BPlusStore<ICallback, KeyType, ValueType, LRUCache<ICallback, FileStorage<ICallback, ObjectUIDType, LRUCacheObject, TypeMarshaller, DataNodeType, IndexNodeType>>> BPlusStoreType;

    class BPlusStore_LRUCache_FileStorage_Suite_1 : public ::testing::TestWithParam<std::tuple<int, int, int, int, int, int>>
    {
    protected:
        void SetUp() override
        {
            std::tie(nDegree, nBulkInsert_StartKey, nBulkInsert_EndKey, nCacheSize, nFileStoreBlockSize, nFileStoreSize) = GetParam();

            m_ptrTree = new BPlusStoreType(nDegree, nCacheSize, nFileStoreBlockSize, nFileStoreSize, fsTempFileStore.string());
            m_ptrTree->init<DataNodeType>();
        }

        void TearDown() override
        {
            delete m_ptrTree;
            std::filesystem::remove(fsTempFileStore);
        }

        BPlusStoreType* m_ptrTree = nullptr;

        int nDegree;
        int nBulkInsert_StartKey;
        int nBulkInsert_EndKey;
        int nCacheSize;
        int nFileStoreBlockSize;
        int nFileStoreSize;

        std::filesystem::path fsTempFileStore = std::filesystem::temp_directory_path() / "tempfilestore.hdb";
    };

    TEST_P(BPlusStore_LRUCache_FileStorage_Suite_1, Insert_v1) 
    {
        for (size_t nCntr = nBulkInsert_StartKey; nCntr <= nBulkInsert_EndKey; nCntr++)
        {
            m_ptrTree->insert(nCntr, nCntr);
        }
    }

    TEST_P(BPlusStore_LRUCache_FileStorage_Suite_1, Insert_v2) 
    {
        for (size_t nCntr = nBulkInsert_StartKey; nCntr <= nBulkInsert_EndKey; nCntr = nCntr + 2)
        {
            m_ptrTree->insert(nCntr, nCntr);
        }

        for (size_t nCntr = nBulkInsert_StartKey + 1; nCntr <= nBulkInsert_EndKey; nCntr = nCntr + 2)
        {
            m_ptrTree->insert(nCntr, nCntr);
        }
    }

    TEST_P(BPlusStore_LRUCache_FileStorage_Suite_1, Insert_v3) 
    {
        for (int nCntr = nBulkInsert_EndKey; nCntr >= nBulkInsert_StartKey; nCntr--)
        {
            m_ptrTree->insert(nCntr, nCntr);
        }
    }

    TEST_P(BPlusStore_LRUCache_FileStorage_Suite_1, Search_v1) 
    {
        for (size_t nCntr = nBulkInsert_StartKey; nCntr <= nBulkInsert_EndKey; nCntr++)
        {
            m_ptrTree->insert(nCntr, nCntr);
        }

        for (size_t nCntr = nBulkInsert_StartKey; nCntr <= nBulkInsert_EndKey; nCntr++)
        {
            int nValue = 0;
            ErrorCode code = m_ptrTree->search(nCntr, nValue);

            ASSERT_EQ(nValue, nCntr);
        }
    }

    TEST_P(BPlusStore_LRUCache_FileStorage_Suite_1, Search_v2) 
    {
        for (size_t nCntr = nBulkInsert_StartKey; nCntr <= nBulkInsert_EndKey; nCntr = nCntr + 2)
        {
            m_ptrTree->insert(nCntr, nCntr);
        }

        for (size_t nCntr = nBulkInsert_StartKey + 1; nCntr <= nBulkInsert_EndKey; nCntr = nCntr + 2)
        {
            m_ptrTree->insert(nCntr, nCntr);
        }

        for (size_t nCntr = nBulkInsert_StartKey; nCntr <= nBulkInsert_EndKey; nCntr++)
        {
            int nValue = 0;
            ErrorCode code = m_ptrTree->search(nCntr, nValue);

            ASSERT_EQ(nValue, nCntr);
        }
    }

    TEST_P(BPlusStore_LRUCache_FileStorage_Suite_1, Search_v3) 
    {
        for (int nCntr = nBulkInsert_EndKey; nCntr >= nBulkInsert_StartKey; nCntr--)
        {
            m_ptrTree->insert(nCntr, nCntr);
        }

        for (size_t nCntr = nBulkInsert_StartKey; nCntr <= nBulkInsert_EndKey; nCntr++)
        {
            int nValue = 0;
            ErrorCode code = m_ptrTree->search(nCntr, nValue);

            ASSERT_EQ(nValue, nCntr);
        }
    }

    TEST_P(BPlusStore_LRUCache_FileStorage_Suite_1, Delete_v1) 
    {
        for (size_t nCntr = nBulkInsert_StartKey; nCntr <= nBulkInsert_EndKey; nCntr++)
        {
            m_ptrTree->insert(nCntr, nCntr);
        }

        for (size_t nCntr = nBulkInsert_StartKey; nCntr <= nBulkInsert_EndKey; nCntr++)
        {
            int nValue = 0;
            ErrorCode code = m_ptrTree->search(nCntr, nValue);

            ASSERT_EQ(nValue, nCntr);
        }

        for (size_t nCntr = nBulkInsert_StartKey; nCntr <= nBulkInsert_EndKey; nCntr++)
        {
            ErrorCode code = m_ptrTree->remove(nCntr);

            ASSERT_EQ(code, ErrorCode::Success);
        }

        for (size_t nCntr = nBulkInsert_StartKey; nCntr <= nBulkInsert_EndKey; nCntr++)
        {
            int nValue = 0;
            ErrorCode code = m_ptrTree->search(nCntr, nValue);

            ASSERT_EQ(code, ErrorCode::KeyDoesNotExist);
        }
    }

    TEST_P(BPlusStore_LRUCache_FileStorage_Suite_1, Delete_v2) 
    {
        for (size_t nCntr = nBulkInsert_StartKey; nCntr <= nBulkInsert_EndKey; nCntr = nCntr + 2)
        {
            m_ptrTree->insert(nCntr, nCntr);
        }

        for (size_t nCntr = nBulkInsert_StartKey + 1; nCntr <= nBulkInsert_EndKey; nCntr = nCntr + 2)
        {
            m_ptrTree->insert(nCntr, nCntr);
        }

        for (size_t nCntr = nBulkInsert_StartKey; nCntr <= nBulkInsert_EndKey; nCntr++)
        {
            int nValue = 0;
            ErrorCode code = m_ptrTree->search(nCntr, nValue);

            ASSERT_EQ(nValue, nCntr);
        }

        for (size_t nCntr = nBulkInsert_StartKey; nCntr <= nBulkInsert_EndKey; nCntr++)
        {
            ErrorCode code = m_ptrTree->remove(nCntr);

            ASSERT_EQ(code, ErrorCode::Success);
        }

        for (size_t nCntr = nBulkInsert_StartKey; nCntr <= nBulkInsert_EndKey; nCntr++)
        {
            int nValue = 0;
            ErrorCode code = m_ptrTree->search(nCntr, nValue);

            ASSERT_EQ(code, ErrorCode::KeyDoesNotExist);
        }
    }

    TEST_P(BPlusStore_LRUCache_FileStorage_Suite_1, Delete_v3) 
    {
        for (int nCntr = nBulkInsert_EndKey; nCntr >= nBulkInsert_StartKey; nCntr--)
        {
            m_ptrTree->insert(nCntr, nCntr);
        }

        for (size_t nCntr = nBulkInsert_StartKey; nCntr <= nBulkInsert_EndKey; nCntr++)
        {
            int nValue = 0;
            ErrorCode code = m_ptrTree->search(nCntr, nValue);

            ASSERT_EQ(nValue, nCntr);
        }

        for (int nCntr = nBulkInsert_EndKey; nCntr >= nBulkInsert_StartKey; nCntr--)
        {
            ErrorCode code = m_ptrTree->remove(nCntr);

            ASSERT_EQ(code, ErrorCode::Success);
        }

        for (size_t nCntr = nBulkInsert_StartKey; nCntr <= nBulkInsert_EndKey; nCntr++)
        {
            int nValue = 0;
            ErrorCode code = m_ptrTree->search(nCntr, nValue);

            ASSERT_EQ(code, ErrorCode::KeyDoesNotExist);
        }
    }

    TEST_P(BPlusStore_LRUCache_FileStorage_Suite_1, Flush_v1)
    {
        for (size_t nCntr = nBulkInsert_StartKey; nCntr <= nBulkInsert_EndKey; nCntr++)
        {
            m_ptrTree->insert(nCntr, nCntr);
        }

        //TODO: A proper way would be to read/reload the entrie tree from the store.
        ASSERT_EQ(m_ptrTree->flush(), ErrorCode::Success);
    }

    TEST_P(BPlusStore_LRUCache_FileStorage_Suite_1, Flush_v2)
    {
        for (size_t nCntr = nBulkInsert_StartKey; nCntr <= nBulkInsert_EndKey; nCntr = nCntr + 2)
        {
            m_ptrTree->insert(nCntr, nCntr);
        }

        for (size_t nCntr = nBulkInsert_StartKey + 1; nCntr <= nBulkInsert_EndKey; nCntr = nCntr + 2)
        {
            m_ptrTree->insert(nCntr, nCntr);
        }

        //TODO: A proper way would be to read/reload the entrie tree from the store.
        ASSERT_EQ(m_ptrTree->flush(), ErrorCode::Success);
    }

    TEST_P(BPlusStore_LRUCache_FileStorage_Suite_1, Flush_v3)
    {
        for (int nCntr = nBulkInsert_EndKey; nCntr >= nBulkInsert_StartKey; nCntr--)
        {
            m_ptrTree->insert(nCntr, nCntr);
        }

        //TODO: A proper way would be to read/reload the entrie tree from the store.
        ASSERT_EQ(m_ptrTree->flush(), ErrorCode::Success);
    }

    INSTANTIATE_TEST_CASE_P(
        Insert_Search_Delete_Flush,
        BPlusStore_LRUCache_FileStorage_Suite_1,
        ::testing::Values(
            std::make_tuple(3, 0, 99999, 100, 1024, 1024 * 1024 * 1024),
            std::make_tuple(4, 0, 99999, 100, 1024, 1024 * 1024 * 1024),
            std::make_tuple(5, 0, 99999, 100, 1024, 1024 * 1024 * 1024),
            std::make_tuple(6, 0, 99999, 100, 1024, 1024 * 1024 * 1024),
            std::make_tuple(7, 0, 99999, 100, 1024, 1024 * 1024 * 1024),
            std::make_tuple(8, 0, 99999, 100, 1024, 1024 * 1024 * 1024),
            std::make_tuple(15, 0, 199999, 100, 1024, 1024* 1024 * 1024),
            std::make_tuple(16, 0, 199999, 100, 1024, 1024* 1024 * 1024),
            std::make_tuple(32, 0, 199999, 100, 1024, 1024* 1024 * 1024),
            std::make_tuple(64, 0, 199999, 100, 2048, 1024* 1024 * 1024)
        ));    
}
#endif //__TREE_WITH_CACHE__