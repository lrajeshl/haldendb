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
#include "TypeUID.h"
#include "ObjectFatUID.h"

#ifdef __TREE_WITH_CACHE__
namespace BPlusStore_LRUCache_VolatileStorage_Suite
{
    typedef int KeyType;
    typedef int ValueType;
    typedef ObjectFatUID ObjectUIDType;

    typedef DataNode<KeyType, ValueType, ObjectUIDType, TYPE_UID::DATA_NODE_INT_INT > DataNodeType;
    typedef IndexNode<KeyType, ValueType, ObjectUIDType, DataNodeType, TYPE_UID::INDEX_NODE_INT_INT > IndexNodeType;

    typedef LRUCacheObject<TypeMarshaller, DataNodeType, IndexNodeType> ObjectType;
    typedef IFlushCallback<ObjectUIDType, ObjectType> ICallback;

    typedef BPlusStore<ICallback, KeyType, ValueType, LRUCache<ICallback, VolatileStorage<ICallback, ObjectUIDType, LRUCacheObject, TypeMarshaller, DataNodeType, IndexNodeType>>> BPlusStoreType;

    class BPlusStore_LRUCache_VolatileStorage_Suite_3 : public ::testing::TestWithParam<std::tuple<int, int, int, int, int, int>>
    {
    protected:
        void SetUp() override
        {
            std::tie(nDegree, nThreadCount, nTotalEntries, nCacheSize, nBlockSize, nStorageSize) = GetParam();

            m_ptrTree = new BPlusStoreType(nDegree, nCacheSize, nBlockSize, nStorageSize);
            m_ptrTree->init<DataNodeType>();
        }

        void TearDown() override {
            delete m_ptrTree;
        }

        BPlusStoreType* m_ptrTree;

        int nDegree;
        int nThreadCount;
        int nTotalEntries;
        int nCacheSize;
        int nBlockSize;
        int nStorageSize;
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

    TEST_P(BPlusStore_LRUCache_VolatileStorage_Suite_3, Insert_v1) 
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

    TEST_P(BPlusStore_LRUCache_VolatileStorage_Suite_3, Insert_v2) 
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

    TEST_P(BPlusStore_LRUCache_VolatileStorage_Suite_3, Search_v1) 
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


    TEST_P(BPlusStore_LRUCache_VolatileStorage_Suite_3, Search_v2) 
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

    TEST_P(BPlusStore_LRUCache_VolatileStorage_Suite_3, Delete_v1) 
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

    TEST_P(BPlusStore_LRUCache_VolatileStorage_Suite_3, Delete_v2) 
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

#ifdef __CONCURRENT__
    INSTANTIATE_TEST_CASE_P(
        Insert_Search_Delete,
        BPlusStore_LRUCache_VolatileStorage_Suite_3,
        ::testing::Values(
            std::make_tuple(3, 2, 99999, 100, 1024, 20000000),
            std::make_tuple(4, 2, 99999, 100, 1024, 20000000),
            std::make_tuple(5, 2, 99999, 100, 1024, 20000000),
            std::make_tuple(6, 2, 99999, 100, 1024, 20000000),
            std::make_tuple(7, 2, 99999, 100, 1024, 20000000),
            std::make_tuple(8, 2, 99999, 100, 1024, 20000000),
            std::make_tuple(15, 2, 199999, 100, 1024, 20000000),
            std::make_tuple(16, 2, 199999, 100, 1024, 20000000),
            std::make_tuple(32, 2, 199999, 100, 1024, 20000000),
            std::make_tuple(64, 2, 199999, 100, 1024, 20000000)));
#endif //__CONCURRENT__
}
#endif //__TREE_WITH_CACHE__