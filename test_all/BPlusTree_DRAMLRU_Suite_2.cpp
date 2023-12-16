#include "pch.h"

//#include "gtest/gtest.h"
//#include "BTree.h"
//#include "DRAMLRUCache.h"
//#include "DRAMCacheObject.h"
//
//#include "DRAMLRUCache.h"
//#include "DRAMCacheObject.h"
//#include "DRAMCacheObjectKey.h"
//#include "DRAMVolatileStorage.h"
//#include <iostream>
//#include "NoCache.h"

//class BPlusStore_DRAMLRU_Suite_2 : public ::testing::Test
//{
//protected:
//    BTree<int, int, DRAMLRUCache<DRAMVolatileStorage, CacheObjectKey, IDRAMCacheObject, DRAMCacheObject>>* m_ptrTree;
//
//    void SetUp() override
//    {
//        m_ptrTree = new BTree<int, int, DRAMLRUCache<DRAMVolatileStorage, CacheObjectKey, IDRAMCacheObject, DRAMCacheObject>>(5, 10);
//    }
//
//    void TearDown() override {
//        delete m_ptrTree;
//    }
//};
//
//TEST_F(BPlusStore_DRAMLRU_Suite_2, Insert_1_Element) {
//    m_ptrTree->insert(1, 1);
//
//    int nValue;
//    ErrorCode code = m_ptrTree->search(1, nValue);
//
//    ASSERT_EQ(nValue, 1);
//}
//
//TEST_F(BPlusStore_DRAMLRU_Suite_2, Insert_100_Elements_v1) {
//    for (size_t nCntr = 0; nCntr < 1000; nCntr++)
//    {
//        m_ptrTree->insert(nCntr, nCntr);
//    }
//
//    for (size_t nCntr = 0; nCntr < 1000; nCntr++)
//    {
//        int nValue = 0;
//        ErrorCode code = m_ptrTree->search(nCntr, nValue);
//
//        ASSERT_EQ(nValue, nCntr);
//    }
//}
//
//TEST_F(BPlusStore_DRAMLRU_Suite_2, Insert_100_Elements_v2) {
//    for (size_t nCntr = 50; nCntr < 100; nCntr++)
//    {
//        m_ptrTree->insert(nCntr, nCntr);
//    }
//
//    for (size_t nCntr = 50; nCntr < 100; nCntr++)
//    {
//        int nValue = 0;
//        ErrorCode code = m_ptrTree->search(nCntr, nValue);
//
//        ASSERT_EQ(nValue, nCntr);
//    }
//
//    for (size_t nCntr = 0; nCntr < 50; nCntr++)
//    {
//        m_ptrTree->insert(nCntr, nCntr);
//    }
//
//    for (size_t nCntr = 0; nCntr < 50; nCntr++)
//    {
//        int nValue = 0;
//        ErrorCode code = m_ptrTree->search(nCntr, nValue);
//
//        ASSERT_EQ(nValue, nCntr);
//    }
//}
//
//TEST_F(BPlusStore_DRAMLRU_Suite_2, Insert_100_Elements_v3) {
//    for (size_t nCntr = 0; nCntr < 1000; nCntr++)
//    {
//        m_ptrTree->insert(nCntr, nCntr * 2);
//    }
//
//    for (size_t nCntr = 0; nCntr < 1000; nCntr++)
//    {
//        int nValue = 0;
//        ErrorCode code = m_ptrTree->search(nCntr, nValue);
//
//        ASSERT_EQ(nValue, nCntr * 2);
//    }
//}
//
//TEST_F(BPlusStore_DRAMLRU_Suite_2, Insert_100_Elements_v4) {
//    for (size_t nCntr = 0; nCntr < 1000; nCntr += 2)
//    {
//        m_ptrTree->insert(nCntr, nCntr * 2);
//    }
//
//    for (size_t nCntr = 0; nCntr < 1000; nCntr += 2)
//    {
//        int nValue = 0;
//        ErrorCode code = m_ptrTree->search(nCntr, nValue);
//
//        ASSERT_EQ(nValue, nCntr * 2);
//    }
//}
