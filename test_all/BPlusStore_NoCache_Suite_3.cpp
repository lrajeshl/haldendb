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

    class BPlusStore_NoCache_Suite_3 : public ::testing::TestWithParam<std::tuple<int, int, int>>
    {
    protected:
        void SetUp() override
        {
            std::tie(nDegree, nThreadCount, nTotalEntries) = GetParam();

            m_ptrTree = new BPlusStoreType(nDegree);
            m_ptrTree->init<DataNodeType>();
        }

        void TearDown() override {
            delete m_ptrTree;
        }

        BPlusStoreType* m_ptrTree;

        int nDegree;
        int nThreadCount;
        int nTotalEntries;
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

    TEST_P(BPlusStore_NoCache_Suite_3, Bulk_Insert_v1) 
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

    TEST_P(BPlusStore_NoCache_Suite_3, Bulk_Insert_v2) 
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

    TEST_P(BPlusStore_NoCache_Suite_3, Bulk_Search_v1) 
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


    TEST_P(BPlusStore_NoCache_Suite_3, Bulk_Search_v2) 
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

    TEST_P(BPlusStore_NoCache_Suite_3, Bulk_Delete_v1) 
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

    TEST_P(BPlusStore_NoCache_Suite_3, Bulk_Delete_v2) 
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
        Bulk_Insert_Search_Delete,
        BPlusStore_NoCache_Suite_3,
        ::testing::Values(
            std::make_tuple(3, 10, 99999),
            std::make_tuple(4, 10, 99999),
            std::make_tuple(5, 10, 99999),
            std::make_tuple(6, 10, 99999),
            std::make_tuple(7, 10, 99999),
            std::make_tuple(8, 10, 99999),
            std::make_tuple(15, 10, 199999),
            std::make_tuple(16, 10, 199999),
            std::make_tuple(32, 10, 199999),
            std::make_tuple(64, 10, 199999)));
#endif __CONCURRENT__
}
#endif __TREE_WITH_CACHE__