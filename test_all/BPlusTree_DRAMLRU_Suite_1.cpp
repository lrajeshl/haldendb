#include "pch.h"

//#include "gtest/gtest.h"
//#include "BTree.h"
//#include "DRAMLRUCache.h"
//#include "DRAMCacheObject.h"
//
//#include "NVRAMLRUCache.h"
//#include "NVRAMCacheObject.h"
//#include "NVRAMCacheObjectKey.h"
//using namespace std;

//class BPlusStore_DRAMLRU_Suite_1 : public ::testing::Test 
//{
//protected:
//    BTree<int, string, DRAMLRUCache<DRAMVolatileStorage, CacheObjectKey, IDRAMCacheObject, DRAMCacheObject>>* m_ptrTree;
//
//    void SetUp() override {
//        m_ptrTree = new BTree<int, string, DRAMLRUCache<DRAMVolatileStorage, CacheObjectKey, IDRAMCacheObject, DRAMCacheObject>>(5, 10);
//    }
//
//    void TearDown() override {
//        delete m_ptrTree;
//    }
//};
//
//TEST_F(BPlusStore_DRAMLRU_Suite_1, Insert_1_Element) {
//    m_ptrTree->insert(1, "1");
//
//    string nValue;
//    ErrorCode code = m_ptrTree->search(1, nValue);
//
//    ASSERT_EQ(nValue, "1");
//}
//
//TEST_F(BPlusStore_DRAMLRU_Suite_1, Insert_100_Elements_v1) {
//    for (size_t nCntr = 0; nCntr < 1000; nCntr++)
//    {
//        m_ptrTree->insert(nCntr, to_string(nCntr));
//    }
//
//    for (size_t nCntr = 0; nCntr < 1000; nCntr++)
//    {
//        string nValue;
//        ErrorCode code = m_ptrTree->search(nCntr, nValue);
//
//        ASSERT_EQ(nValue, to_string(nCntr));
//    }
//}
//
//TEST_F(BPlusStore_DRAMLRU_Suite_1, Insert_100_Elements_v2) {
//    for (size_t nCntr = 50; nCntr < 100; nCntr++)
//    {
//        m_ptrTree->insert(nCntr, to_string(nCntr));
//    }
//
//    for (size_t nCntr = 50; nCntr < 100; nCntr++)
//    {
//        string nValue;
//        ErrorCode code = m_ptrTree->search(nCntr, nValue);
//
//        ASSERT_EQ(nValue, to_string(nCntr));
//    }
//
//    for (size_t nCntr = 0; nCntr < 50; nCntr++)
//    {
//        m_ptrTree->insert(nCntr, to_string(nCntr));
//    }
//
//    for (size_t nCntr = 0; nCntr < 50; nCntr++)
//    {
//        string nValue;
//        ErrorCode code = m_ptrTree->search(nCntr, nValue);
//
//        ASSERT_EQ(nValue, to_string(nCntr));
//    }
//}
//
//TEST_F(BPlusStore_DRAMLRU_Suite_1, Insert_100_Elements_v3) {
//    for (size_t nCntr = 0; nCntr < 1000; nCntr++)
//    {
//        m_ptrTree->insert(nCntr, to_string(nCntr * 2));
//    }
//
//    for (size_t nCntr = 0; nCntr < 1000; nCntr++)
//    {
//        string nValue;
//        ErrorCode code = m_ptrTree->search(nCntr, nValue);
//
//        ASSERT_EQ(nValue, to_string(nCntr * 2));
//    }
//}
//
//TEST_F(BPlusStore_DRAMLRU_Suite_1, Insert_100_Elements_v4) {
//    for (size_t nCntr = 0; nCntr < 1000; nCntr += 2)
//    {
//        m_ptrTree->insert(nCntr, to_string(nCntr * 2));
//    }
//
//    for (size_t nCntr = 0; nCntr < 1000; nCntr += 2)
//    {
//        string nValue;
//        ErrorCode code = m_ptrTree->search(nCntr, nValue);
//
//        ASSERT_EQ(nValue, to_string(nCntr * 2));
//    }
//}
