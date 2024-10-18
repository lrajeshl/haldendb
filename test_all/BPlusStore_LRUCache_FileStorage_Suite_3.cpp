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

#ifdef __TREE_WITH_CACHE__
namespace BPlusStore_LRUCache_FileStorage_Suite
{
    typedef int KeyType;
    typedef int ValueType;
    typedef ObjectFatUID ObjectUIDType;

    typedef DataNode<KeyType, ValueType, ObjectUIDType, TYPE_UID::DATA_NODE_INT_INT > DataNodeType;
    typedef IndexNode<KeyType, ValueType, ObjectUIDType, DataNodeType, TYPE_UID::INDEX_NODE_INT_INT> IndexNodeType;

    typedef LRUCacheObject<TypeMarshaller, DataNodeType, IndexNodeType> ObjectType;
    typedef IFlushCallback<ObjectUIDType, ObjectType> ICallback;

    typedef BPlusStore<ICallback, KeyType, ValueType, LRUCache<ICallback, FileStorage<ICallback, ObjectUIDType, LRUCacheObject, TypeMarshaller, DataNodeType, IndexNodeType>>> BPlusStoreType;

    class BPlusStore_LRUCache_FileStorage_Suite_3 : public ::testing::TestWithParam<std::tuple<int, int, int, int, int, int>>
    {
    protected:
        void SetUp() override
        {
            std::tie(nDegree, nThreadCount, nTotalEntries, nCacheSize, nFileStoreBlockSize, nFileStoreSize) = GetParam();

            m_ptrTree = new BPlusStoreType(nDegree, nCacheSize, nFileStoreBlockSize, nFileStoreSize, fsTempFileStore.string());
            m_ptrTree->init<DataNodeType>();
        }

        void TearDown() override 
        {
            delete m_ptrTree;
            std::filesystem::remove(fsTempFileStore);
        }

        BPlusStoreType* m_ptrTree;

        int nDegree;
        int nThreadCount;
        int nTotalEntries;
        int nCacheSize;
        int nFileStoreBlockSize;
        int nFileStoreSize;

        std::filesystem::path fsTempFileStore = std::filesystem::temp_directory_path() / "tempfilestore.hdb";
    };

    void insert_concurent(BPlusStoreType* ptrTree, int nRangeStart, int nRangeEnd) 
    {
        for (size_t nCntr = nRangeStart; nCntr < nRangeEnd; nCntr++)
        {
            ptrTree->insert(nCntr, nCntr);
        }
    }

    void reverse_insert_concurent(BPlusStoreType* ptrTree, int nRangeStart, int nRangeEnd) 
    {
        for (int nCntr = nRangeEnd - 1; nCntr >= nRangeStart; nCntr--)
        {
            ptrTree->insert(nCntr, nCntr);
        }
    }

    void search_concurent(BPlusStoreType* ptrTree, int nRangeStart, int nRangeEnd) 
    {
        for (size_t nCntr = nRangeStart; nCntr < nRangeEnd; nCntr++)
        {
            int nValue = 0;
            ErrorCode code = ptrTree->search(nCntr, nValue);

            ASSERT_EQ(nCntr, nValue);
        }
    }

    void search_not_found_concurent(BPlusStoreType* ptrTree, int nRangeStart, int nRangeEnd) 
    {
        for (size_t nCntr = nRangeStart; nCntr < nRangeEnd; nCntr++)
        {
            int nValue = 0;
            ErrorCode errCode = ptrTree->search(nCntr, nValue);

            ASSERT_EQ(errCode, ErrorCode::KeyDoesNotExist);
        }
    }

    void delete_concurent(BPlusStoreType* ptrTree, int nRangeStart, int nRangeEnd) 
    {
        for (size_t nCntr = nRangeStart; nCntr < nRangeEnd; nCntr++)
        {
            ErrorCode code = ptrTree->remove(nCntr);
        }
    }

    void reverse_delete_concurent(BPlusStoreType* ptrTree, int nRangeStart, int nRangeEnd) 
    {
        for (int nCntr = nRangeEnd - 1; nCntr >= nRangeStart; nCntr--)
        {
            ErrorCode code = ptrTree->remove(nCntr);
        }
    }

    TEST_P(BPlusStore_LRUCache_FileStorage_Suite_3, Insert_v1) 
    {
        std::vector<std::thread> vtThreads;

        for (int nIdx = 0; nIdx < nThreadCount; nIdx++)
        {
            int nTotal = nTotalEntries / nThreadCount;
            vtThreads.push_back(std::thread(insert_concurent, m_ptrTree, nIdx * nTotal, nIdx * nTotal + nTotal));
        }

        auto it = vtThreads.begin();
        while (it != vtThreads.end())
        {
            (*it).join();
            it++;
        }
    }

    TEST_P(BPlusStore_LRUCache_FileStorage_Suite_3, Insert_v2) 
    {
        std::vector<std::thread> vtThreads;

        for (int nIdx = 0; nIdx < nThreadCount; nIdx++)
        {
            int nTotal = nTotalEntries / nThreadCount;
            vtThreads.push_back(std::thread(reverse_insert_concurent, m_ptrTree, nIdx * nTotal, nIdx * nTotal + nTotal));
        }

        auto it = vtThreads.begin();
        while (it != vtThreads.end())
        {
            (*it).join();
            it++;
        }
    }

    TEST_P(BPlusStore_LRUCache_FileStorage_Suite_3, Search_v1) 
    {
        std::vector<std::thread> vtThreads;

        for (int nIdx = 0; nIdx < nThreadCount; nIdx++)
        {
            int nTotal = nTotalEntries / nThreadCount;
            vtThreads.push_back(std::thread(insert_concurent, m_ptrTree, nIdx * nTotal, nIdx * nTotal + nTotal));
        }

        auto it = vtThreads.begin();
        while (it != vtThreads.end())
        {
            (*it).join();
            it++;
        }

        vtThreads.clear();

        for (int nIdx = 0; nIdx < nThreadCount; nIdx++)
        {
            int nTotal = nTotalEntries / nThreadCount;
            vtThreads.push_back(std::thread(search_concurent, m_ptrTree, nIdx * nTotal, nIdx * nTotal + nTotal));
        }

        it = vtThreads.begin();
        while (it != vtThreads.end())
        {
            (*it).join();
            it++;
        }
    }

    TEST_P(BPlusStore_LRUCache_FileStorage_Suite_3, Search_v2) 
    {
        std::vector<std::thread> vtThreads;

        for (int nIdx = 0; nIdx < nThreadCount; nIdx++)
        {
            int nTotal = nTotalEntries / nThreadCount;
            vtThreads.push_back(std::thread(reverse_insert_concurent, m_ptrTree, nIdx * nTotal, nIdx * nTotal + nTotal));
        }

        auto it = vtThreads.begin();
        while (it != vtThreads.end())
        {
            (*it).join();
            it++;
        }

        vtThreads.clear();

        for (int nIdx = 0; nIdx < nThreadCount; nIdx++)
        {
            int nTotal = nTotalEntries / nThreadCount;
            vtThreads.push_back(std::thread(search_concurent, m_ptrTree, nIdx * nTotal, nIdx * nTotal + nTotal));
        }

        it = vtThreads.begin();
        while (it != vtThreads.end())
        {
            (*it).join();
            it++;
        }
    }

    TEST_P(BPlusStore_LRUCache_FileStorage_Suite_3, Delete_v1) 
    {
        std::vector<std::thread> vtThreads;

        for (int nIdx = 0; nIdx < nThreadCount; nIdx++)
        {
            int nTotal = nTotalEntries / nThreadCount;
            vtThreads.push_back(std::thread(insert_concurent, m_ptrTree, nIdx * nTotal, nIdx * nTotal + nTotal));
        }

        auto it = vtThreads.begin();
        while (it != vtThreads.end())
        {
            (*it).join();
            it++;
        }

        vtThreads.clear();

        for (int nIdx = 0; nIdx < nThreadCount; nIdx++)
        {
            int nTotal = nTotalEntries / nThreadCount;
            vtThreads.push_back(std::thread(delete_concurent, m_ptrTree, nIdx * nTotal, nIdx * nTotal + nTotal));
        }

        it = vtThreads.begin();
        while (it != vtThreads.end())
        {
            (*it).join();
            it++;
        }

        vtThreads.clear();

        for (int nIdx = 0; nIdx < nThreadCount; nIdx++)
        {
            int nTotal = nTotalEntries / nThreadCount;
            vtThreads.push_back(std::thread(search_not_found_concurent, m_ptrTree, nIdx * nTotal, nIdx * nTotal + nTotal));
        }

        it = vtThreads.begin();
        while (it != vtThreads.end())
        {
            (*it).join();
            it++;
        }
    }

    TEST_P(BPlusStore_LRUCache_FileStorage_Suite_3, Delete_v2) 
    {
        std::vector<std::thread> vtThreads;

        for (int nIdx = 0; nIdx < nThreadCount; nIdx++)
        {
            int nTotal = nTotalEntries / nThreadCount;
            vtThreads.push_back(std::thread(reverse_insert_concurent, m_ptrTree, nIdx * nTotal, nIdx * nTotal + nTotal));
        }

        auto it = vtThreads.begin();
        while (it != vtThreads.end())
        {
            (*it).join();
            it++;
        }

        vtThreads.clear();

        for (int nIdx = 0; nIdx < nThreadCount; nIdx++)
        {
            int nTotal = nTotalEntries / nThreadCount;
            vtThreads.push_back(std::thread(reverse_delete_concurent, m_ptrTree, nIdx * nTotal, nIdx * nTotal + nTotal));
        }

        it = vtThreads.begin();
        while (it != vtThreads.end())
        {
            (*it).join();
            it++;
        }

        vtThreads.clear();

        for (int nIdx = 0; nIdx < nThreadCount; nIdx++)
        {
            int nTotal = nTotalEntries / nThreadCount;
            vtThreads.push_back(std::thread(search_not_found_concurent, m_ptrTree, nIdx * nTotal, nIdx * nTotal + nTotal));
        }

        it = vtThreads.begin();
        while (it != vtThreads.end())
        {
            (*it).join();
            it++;
        }
    }

    TEST_P(BPlusStore_LRUCache_FileStorage_Suite_3, Flush_v1)
    {
        std::vector<std::thread> vtThreads;

        for (int nIdx = 0; nIdx < nThreadCount; nIdx++)
        {
            int nTotal = nTotalEntries / nThreadCount;
            vtThreads.push_back(std::thread(insert_concurent, m_ptrTree, nIdx * nTotal, nIdx * nTotal + nTotal));
        }

        auto it = vtThreads.begin();
        while (it != vtThreads.end())
        {
            (*it).join();
            it++;
        }

        //TODO: A proper way would be to read/reload the entrie tree from the store.
        //Also, the current implementation blocks the whole tree, should not be flush allowed at the node!
        ASSERT_EQ(m_ptrTree->flush(), ErrorCode::Success);
    }

    TEST_P(BPlusStore_LRUCache_FileStorage_Suite_3, Flush_v2)
    {
        std::vector<std::thread> vtThreads;

        for (int nIdx = 0; nIdx < nThreadCount; nIdx++)
        {
            int nTotal = nTotalEntries / nThreadCount;
            vtThreads.push_back(std::thread(reverse_insert_concurent, m_ptrTree, nIdx * nTotal, nIdx * nTotal + nTotal));
        }

        auto it = vtThreads.begin();
        while (it != vtThreads.end())
        {
            (*it).join();
            it++;
        }

        //TODO: A proper way would be to read/reload the entrie tree from the store.
        //Also, the current implementation blocks the whole tree, should not be flush allowed at the node!
        ASSERT_EQ(m_ptrTree->flush(), ErrorCode::Success);
    }

#ifdef __CONCURRENT__
    INSTANTIATE_TEST_CASE_P(
        Insert_Search_Delete,
        BPlusStore_LRUCache_FileStorage_Suite_3,
        ::testing::Values(
            std::make_tuple(3, 10, 99999, 100, 128, 1024 * 1024 * 1024),
            std::make_tuple(4, 10, 99999, 100, 128, 1024 * 1024 * 1024),
            std::make_tuple(5, 10, 99999, 100, 128, 1024 * 1024 * 1024),
            std::make_tuple(6, 10, 99999, 100, 128, 1024 * 1024 * 1024),
            std::make_tuple(7, 10, 99999, 100, 128, 1024 * 1024 * 1024),
            std::make_tuple(8, 10, 99999, 100, 128, 1024 * 1024 * 1024),
            std::make_tuple(15, 10, 199999, 100, 128, 1024 * 1024 * 1024),
            std::make_tuple(16, 10, 199999, 100, 128, 1024 * 1024 * 1024),
            std::make_tuple(32, 10, 199999, 100, 128, 1024 * 1024 * 1024),
            std::make_tuple(64, 10, 199999, 100, 128, 1024 * 1024 * 1024)));
#endif //__CONCURRENT__
}
#endif //__TREE_WITH_CACHE__