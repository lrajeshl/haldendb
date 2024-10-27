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
#include <set>
#include <random>
#include <numeric>

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

    class BPlusStore_LRUCache_VolatileStorage_Suite_3 : public ::testing::TestWithParam<std::tuple<size_t, size_t, size_t, size_t, size_t, size_t>>
    {
    protected:
        void SetUp() override
        {
            std::tie(nDegree, nTotalRecords, nCacheSize, nBlockSize, nStorageSize, nThreadCount) = GetParam();

            m_ptrTree = new BPlusStoreType(nDegree, nCacheSize, nBlockSize, nStorageSize);
            m_ptrTree->init<DataNodeType>();
        }

        void TearDown() override
        {
            delete m_ptrTree;
        }

        BPlusStoreType* m_ptrTree;

        size_t nDegree;
        size_t nTotalRecords;
        size_t nCacheSize;
        size_t nBlockSize;
        size_t nStorageSize;
        size_t nThreadCount;
    };

    void insert_concurent(BPlusStoreType* ptrTree, int nRangeStart, int nRangeEnd)
    {
        for (int nCntr = nRangeEnd - 1; nCntr >= nRangeStart; nCntr--)
        {
            ErrorCode ec = ptrTree->insert(nCntr, nCntr);
            assert(ec == ErrorCode::Success);
        }
    }

    void insert_concurent_reverse(BPlusStoreType* ptrTree, int nRangeStart, int nRangeEnd)
    {
        for (int nCntr = nRangeEnd - 1; nCntr >= nRangeStart; nCntr--)
        {
            ErrorCode ec = ptrTree->insert(nCntr, nCntr);
            assert(ec == ErrorCode::Success);
        }
    }

    void insert_concurent_random(BPlusStoreType* ptrTree, int nRangeStart, int nRangeEnd)
    {
        std::vector<int> vtRandom(nRangeEnd - nRangeStart);
        std::iota(vtRandom.begin(), vtRandom.end(), 1);
        std::random_device rd; // Obtain a random number from hardware
        std::mt19937 eng(rd()); // Seed the generator
        std::shuffle(vtRandom.begin(), vtRandom.end(), eng);

        for (size_t nCntr = 0; nCntr < nRangeEnd - nRangeStart; nCntr++)
        {
            ErrorCode ec = ptrTree->insert(vtRandom[nCntr], vtRandom[nCntr]);
            assert(ec == ErrorCode::Success);
        }
    }

    void search_concurent(BPlusStoreType* ptrTree, int nRangeStart, int nRangeEnd)
    {
        for (size_t nCntr = nRangeStart; nCntr < nRangeEnd; nCntr++)
        {
            int nValue = 0;
            ErrorCode ec = ptrTree->search(nCntr, nValue);

            assert(nCntr == nValue && ec == ErrorCode::Success);
        }
    }

    void search_concurent_reverese(BPlusStoreType* ptrTree, int nRangeStart, int nRangeEnd)
    {
        for (int nCntr = nRangeEnd - 1; nCntr >= nRangeStart; nCntr--)
        {
            int nValue = 0;
            ErrorCode ec = ptrTree->search(nCntr, nValue);

            assert(nCntr == nValue && ec == ErrorCode::Success);
        }
    }

    void search_concurent_random(BPlusStoreType* ptrTree, int nRangeStart, int nRangeEnd)
    {
        std::vector<int> vtRandom(nRangeEnd - nRangeStart);
        std::iota(vtRandom.begin(), vtRandom.end(), 1);
        std::random_device rd; // Obtain a random number from hardware
        std::mt19937 eng(rd()); // Seed the generator
        std::shuffle(vtRandom.begin(), vtRandom.end(), eng);

        for (size_t nCntr = 0; nCntr < nRangeEnd - nRangeStart; nCntr++)
        {
            int nValue = 0;
            ErrorCode ec = ptrTree->search(vtRandom[nCntr], nValue);

            assert(nCntr == nValue && ec == ErrorCode::Success);
        }
    }

    void search_not_found_concurent(BPlusStoreType* ptrTree, int nRangeStart, int nRangeEnd)
    {
        for (size_t nCntr = nRangeStart; nCntr < nRangeEnd; nCntr++)
        {
            int nValue = 0;
            ErrorCode ec = ptrTree->search(nCntr, nValue);

            assert(ec == ErrorCode::KeyDoesNotExist);
        }
    }

    void search_not_found_concurent_reverse(BPlusStoreType* ptrTree, int nRangeStart, int nRangeEnd)
    {
        for (int nCntr = nRangeEnd - 1; nCntr >= nRangeStart; nCntr--)
        {
            int nValue = 0;
            ErrorCode ec = ptrTree->search(nCntr, nValue);

            assert(ec == ErrorCode::KeyDoesNotExist);
        }
    }

    void search_not_found_concurent_random(BPlusStoreType* ptrTree, int nRangeStart, int nRangeEnd)
    {
        std::vector<int> vtRandom(nRangeEnd - nRangeStart);
        std::iota(vtRandom.begin(), vtRandom.end(), 1);
        std::random_device rd; // Obtain a random number from hardware
        std::mt19937 eng(rd()); // Seed the generator
        std::shuffle(vtRandom.begin(), vtRandom.end(), eng);

        for (size_t nCntr = 0; nCntr < nRangeEnd - nRangeStart; nCntr++)
        {
            int nValue = 0;
            ErrorCode ec = ptrTree->search(vtRandom[nCntr], nValue);

            assert(ec == ErrorCode::KeyDoesNotExist);
        }
    }

    void delete_concurent(BPlusStoreType* ptrTree, int nRangeStart, int nRangeEnd)
    {
        for (size_t nCntr = nRangeStart; nCntr < nRangeEnd; nCntr++)
        {
            ErrorCode ec = ptrTree->remove(nCntr);
            assert(ec == ErrorCode::Success);
        }
    }

    void delete_concurent_reverse(BPlusStoreType* ptrTree, int nRangeStart, int nRangeEnd)
    {
        for (int nCntr = nRangeEnd - 1; nCntr >= nRangeStart; nCntr--)
        {
            ErrorCode ec = ptrTree->remove(nCntr);
            assert(ec == ErrorCode::Success);
        }
    }

    void delete_concurent_random(BPlusStoreType* ptrTree, int nRangeStart, int nRangeEnd)
    {
        std::vector<int> vtRandom(nRangeEnd - nRangeStart);
        std::iota(vtRandom.begin(), vtRandom.end(), 1);
        std::random_device rd; // Obtain a random number from hardware
        std::mt19937 eng(rd()); // Seed the generator
        std::shuffle(vtRandom.begin(), vtRandom.end(), eng);

        for (size_t nCntr = 0; nCntr < nRangeEnd - nRangeStart; nCntr++)
        {
            ErrorCode ec = ptrTree->remove(vtRandom[nCntr]);
            assert(ec == ErrorCode::Success);
        }
    }

    TEST_P(BPlusStore_LRUCache_VolatileStorage_Suite_3, Bulk_Insert_v1)
    {
        std::vector<std::thread> vtThreads;

        for (int nIdx = 0; nIdx < nThreadCount; nIdx++)
        {
            int nTotal = nTotalRecords / nThreadCount;
            vtThreads.push_back(std::thread(insert_concurent, m_ptrTree, nIdx * nTotal, nIdx * nTotal + nTotal));
        }

        auto it = vtThreads.begin();
        while (it != vtThreads.end())
        {
            (*it).join();
            it++;
        }
    }

    TEST_P(BPlusStore_LRUCache_VolatileStorage_Suite_3, Bulk_Insert_v2)
    {
        std::vector<std::thread> vtThreads;

        for (int nIdx = 0; nIdx < nThreadCount; nIdx++)
        {
            int nTotal = nTotalRecords / nThreadCount;
            vtThreads.push_back(std::thread(insert_concurent_reverse, m_ptrTree, nIdx * nTotal, nIdx * nTotal + nTotal));
        }

        auto it = vtThreads.begin();
        while (it != vtThreads.end())
        {
            (*it).join();
            it++;
        }
    }

    TEST_P(BPlusStore_LRUCache_VolatileStorage_Suite_3, Bulk_Insert_v3)
    {
        std::vector<std::thread> vtThreads;

        for (int nIdx = 0; nIdx < nThreadCount; nIdx++)
        {
            int nTotal = nTotalRecords / nThreadCount;
            vtThreads.push_back(std::thread(insert_concurent_random, m_ptrTree, nIdx * nTotal, nIdx * nTotal + nTotal));
        }

        auto it = vtThreads.begin();
        while (it != vtThreads.end())
        {
            (*it).join();
            it++;
        }
    }

    TEST_P(BPlusStore_LRUCache_VolatileStorage_Suite_3, Bulk_Search_v1)
    {
        std::vector<std::thread> vtThreads;

        for (int nIdx = 0; nIdx < nThreadCount; nIdx++)
        {
            int nTotal = nTotalRecords / nThreadCount;
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
            int nTotal = nTotalRecords / nThreadCount;
            vtThreads.push_back(std::thread(search_concurent, m_ptrTree, nIdx * nTotal, nIdx * nTotal + nTotal));
        }

        it = vtThreads.begin();
        while (it != vtThreads.end())
        {
            (*it).join();
            it++;
        }
    }

    TEST_P(BPlusStore_LRUCache_VolatileStorage_Suite_3, Bulk_Search_v2)
    {
        std::vector<std::thread> vtThreads;

        for (int nIdx = 0; nIdx < nThreadCount; nIdx++)
        {
            int nTotal = nTotalRecords / nThreadCount;
            vtThreads.push_back(std::thread(insert_concurent_reverse, m_ptrTree, nIdx * nTotal, nIdx * nTotal + nTotal));
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
            int nTotal = nTotalRecords / nThreadCount;
            vtThreads.push_back(std::thread(search_concurent_reverese, m_ptrTree, nIdx * nTotal, nIdx * nTotal + nTotal));
        }

        it = vtThreads.begin();
        while (it != vtThreads.end())
        {
            (*it).join();
            it++;
        }
    }

    TEST_P(BPlusStore_LRUCache_VolatileStorage_Suite_3, Bulk_Search_v3)
    {
        std::vector<std::thread> vtThreads;

        for (int nIdx = 0; nIdx < nThreadCount; nIdx++)
        {
            int nTotal = nTotalRecords / nThreadCount;
            vtThreads.push_back(std::thread(insert_concurent_random, m_ptrTree, nIdx * nTotal, nIdx * nTotal + nTotal));
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
            int nTotal = nTotalRecords / nThreadCount;
            vtThreads.push_back(std::thread(search_concurent_random, m_ptrTree, nIdx * nTotal, nIdx * nTotal + nTotal));
        }

        it = vtThreads.begin();
        while (it != vtThreads.end())
        {
            (*it).join();
            it++;
        }
    }

    TEST_P(BPlusStore_LRUCache_VolatileStorage_Suite_3, Bulk_Delete_v1)
    {
        std::vector<std::thread> vtThreads;

        for (int nIdx = 0; nIdx < nThreadCount; nIdx++)
        {
            int nTotal = nTotalRecords / nThreadCount;
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
            int nTotal = nTotalRecords / nThreadCount;
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
            int nTotal = nTotalRecords / nThreadCount;
            vtThreads.push_back(std::thread(search_not_found_concurent, m_ptrTree, nIdx * nTotal, nIdx * nTotal + nTotal));
        }

        it = vtThreads.begin();
        while (it != vtThreads.end())
        {
            (*it).join();
            it++;
        }
    }

    TEST_P(BPlusStore_LRUCache_VolatileStorage_Suite_3, Bulk_Delete_v2)
    {
        std::vector<std::thread> vtThreads;

        for (int nIdx = 0; nIdx < nThreadCount; nIdx++)
        {
            int nTotal = nTotalRecords / nThreadCount;
            vtThreads.push_back(std::thread(insert_concurent_reverse, m_ptrTree, nIdx * nTotal, nIdx * nTotal + nTotal));
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
            int nTotal = nTotalRecords / nThreadCount;
            vtThreads.push_back(std::thread(delete_concurent_reverse, m_ptrTree, nIdx * nTotal, nIdx * nTotal + nTotal));
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
            int nTotal = nTotalRecords / nThreadCount;
            vtThreads.push_back(std::thread(search_not_found_concurent_reverse, m_ptrTree, nIdx * nTotal, nIdx * nTotal + nTotal));
        }

        it = vtThreads.begin();
        while (it != vtThreads.end())
        {
            (*it).join();
            it++;
        }
    }

    TEST_P(BPlusStore_LRUCache_VolatileStorage_Suite_3, Bulk_Delete_v3)
    {
        std::vector<std::thread> vtThreads;

        for (int nIdx = 0; nIdx < nThreadCount; nIdx++)
        {
            int nTotal = nTotalRecords / nThreadCount;
            vtThreads.push_back(std::thread(insert_concurent_random, m_ptrTree, nIdx * nTotal, nIdx * nTotal + nTotal));
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
            int nTotal = nTotalRecords / nThreadCount;
            vtThreads.push_back(std::thread(delete_concurent_random, m_ptrTree, nIdx * nTotal, nIdx * nTotal + nTotal));
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
            int nTotal = nTotalRecords / nThreadCount;
            vtThreads.push_back(std::thread(search_not_found_concurent_random, m_ptrTree, nIdx * nTotal, nIdx * nTotal + nTotal));
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
            std::make_tuple(3, 10000, 1024, 64, 4ULL * 1024 * 1024 * 1024, 8),
            std::make_tuple(4, 10000, 1024, 64, 4ULL * 1024 * 1024 * 1024, 8),
            std::make_tuple(5, 10000, 1024, 64, 4ULL * 1024 * 1024 * 1024, 8),
            std::make_tuple(6, 10000, 1024, 64, 4ULL * 1024 * 1024 * 1024, 8),
            std::make_tuple(7, 10000, 1024, 128, 4ULL * 1024 * 1024 * 1024, 8),
            std::make_tuple(8, 10000, 1024, 128, 4ULL * 1024 * 1024 * 1024, 8),
            std::make_tuple(15, 10000, 1024, 128, 4ULL * 1024 * 1024 * 1024, 8),
            std::make_tuple(16, 10000, 1024, 128, 4ULL * 1024 * 1024 * 1024, 8),
            std::make_tuple(32, 10000, 1024, 256, 4ULL * 1024 * 1024 * 1024, 8),
            std::make_tuple(64, 10000, 1024, 256, 4ULL * 1024 * 1024 * 1024, 8),
            std::make_tuple(128, 10000, 1024, 256, 4ULL * 1024 * 1024 * 1024, 8),
            std::make_tuple(256, 10000, 1024, 256, 10ULL * 1024 * 1024 * 1024, 8),
            std::make_tuple(512, 10000, 1024, 256, 10ULL * 1024 * 1024 * 1024, 8),
            std::make_tuple(1024, 10000, 1024, 256, 10ULL * 1024 * 1024 * 1024, 8),
            std::make_tuple(2048, 10000, 1024, 256, 10ULL * 1024 * 1024 * 1024, 8)
        ));
#endif //__CONCURRENT__
}
#endif //__TREE_WITH_CACHE__