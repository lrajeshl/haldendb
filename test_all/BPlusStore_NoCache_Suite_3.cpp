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

    class BPlusStore_NoCache_Suite_3 : public ::testing::TestWithParam<std::tuple<int, int, int>>
    {
    protected:
        void SetUp() override
        {
            std::tie(nDegree, nThreadCount, nTotalRecords) = GetParam();

            m_ptrTree = new BPlusStoreType(nDegree);
            m_ptrTree->init<DataNodeType>();
        }

        void TearDown() override {
            delete m_ptrTree;
        }

        BPlusStoreType* m_ptrTree;

        int nDegree;
        int nThreadCount;
        int nTotalRecords;
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
        std::iota(vtRandom.begin(), vtRandom.end(), nRangeStart);
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
        std::iota(vtRandom.begin(), vtRandom.end(), nRangeStart);
        std::random_device rd; // Obtain a random number from hardware
        std::mt19937 eng(rd()); // Seed the generator
        std::shuffle(vtRandom.begin(), vtRandom.end(), eng);

        for (size_t nCntr = 0; nCntr < nRangeEnd - nRangeStart; nCntr++)
        {
            int nValue = 0;
            ErrorCode ec = ptrTree->search(vtRandom[nCntr], nValue);

            assert(vtRandom[nCntr] == nValue && ec == ErrorCode::Success);
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
        std::iota(vtRandom.begin(), vtRandom.end(), nRangeStart);
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
        std::iota(vtRandom.begin(), vtRandom.end(), nRangeStart);
        std::random_device rd; // Obtain a random number from hardware
        std::mt19937 eng(rd()); // Seed the generator
        std::shuffle(vtRandom.begin(), vtRandom.end(), eng);

        for (size_t nCntr = 0; nCntr < nRangeEnd - nRangeStart; nCntr++)
        {
            ErrorCode ec = ptrTree->remove(vtRandom[nCntr]);
            assert(ec == ErrorCode::Success);
        }
    }

    TEST_P(BPlusStore_NoCache_Suite_3, Bulk_Insert_v1)
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

    TEST_P(BPlusStore_NoCache_Suite_3, Bulk_Insert_v2) 
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

    TEST_P(BPlusStore_NoCache_Suite_3, Bulk_Insert_v3)
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

    TEST_P(BPlusStore_NoCache_Suite_3, Bulk_Search_v1)
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

    TEST_P(BPlusStore_NoCache_Suite_3, Bulk_Search_v2) 
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

    TEST_P(BPlusStore_NoCache_Suite_3, Bulk_Search_v3)
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

    TEST_P(BPlusStore_NoCache_Suite_3, Bulk_Delete_v1) 
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

    TEST_P(BPlusStore_NoCache_Suite_3, Bulk_Delete_v2) 
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

    TEST_P(BPlusStore_NoCache_Suite_3, Bulk_Delete_v3)
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
        THREADED_TREE_WITH_KEY_AS_INT32_VAL_AS_INT32,
        BPlusStore_NoCache_Suite_3,
        ::testing::Values(
            std::make_tuple(3, 10, 100000),
            std::make_tuple(4, 10, 100000),
            std::make_tuple(5, 10, 100000),
            std::make_tuple(6, 10, 100000),
            std::make_tuple(7, 10, 100000),
            std::make_tuple(8, 10, 100000),
            std::make_tuple(15, 10, 100000),
            std::make_tuple(16, 10, 100000),
            std::make_tuple(32, 10, 100000),
            std::make_tuple(64, 10, 100000),
            std::make_tuple(128, 10, 1000000),
            std::make_tuple(256, 10, 1000000),
            std::make_tuple(512, 10, 1000000),
            std::make_tuple(1024, 10, 1000000),
            std::make_tuple(2048, 10, 1000000)
            ));
#endif //__CONCURRENT__
}
#endif //__TREE_WITH_CACHE__
