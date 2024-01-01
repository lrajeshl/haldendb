#pragma once
#include <memory>
#include <iostream>
#include <stack>
#include <optional>
#include <mutex>
#include <shared_mutex>
#include <syncstream>
#include <thread>
#include <cmath>
#include <exception>
#include <variant>
#include <unordered_map>
#include "CacheErrorCodes.h"
#include "ErrorCodes.h"
#include "VariadicNthType.h"
#include <tuple>

#include <iostream>
#include <fstream>
#include <assert.h>


#define __CONCURRENT__
#define __TREE_AWARE_CACHE__

#ifdef __TREE_AWARE_CACHE__
template <typename ICallback, typename KeyType, typename ValueType, typename CacheType>
class BPlusStore : public ICallback
#else // !__TREE_AWARE_CACHE__
template <typename KeyType, typename ValueType, typename CacheType>
class BPlusStore
#endif __TREE_AWARE_CACHE__
{
    typedef CacheType::ObjectUIDType ObjectUIDType;
    typedef CacheType::ObjectType ObjectType;
    typedef CacheType::ObjectTypePtr ObjectTypePtr;

    using DataNodeType = typename std::tuple_element<0, typename ObjectType::ObjectCoreTypes>::type;
    using IndexNodeType = typename std::tuple_element<1, typename ObjectType::ObjectCoreTypes>::type;

private:
    uint32_t m_nDegree;
    std::shared_ptr<CacheType> m_ptrCache;
    std::optional<ObjectUIDType> m_uidRootNode;

#ifdef __CONCURRENT__
    mutable std::shared_mutex m_mutex;
#endif __CONCURRENT__

public:
    ~BPlusStore()
    {
    }

    template<typename... CacheArgs>
    BPlusStore(uint32_t nDegree, CacheArgs... args)
        : m_nDegree(nDegree)
        , m_uidRootNode(std::nullopt)
    {
        m_ptrCache = std::make_shared<CacheType>(args...);
    }

    template <typename DefaultNodeType>
    void init()
    {
#ifdef __TREE_AWARE_CACHE__
        m_ptrCache->init(this);
#endif __TREE_AWARE_CACHE__

        m_ptrCache->template createObjectOfType<DefaultNodeType>(m_uidRootNode);
    }

    ErrorCode insert(const KeyType& key, const ValueType& value)
    {
        std::vector<std::pair<ObjectUIDType, ObjectTypePtr>> vtAccessedNodes;

#ifdef __CONCURRENT__
        std::vector<std::unique_lock<std::shared_mutex>> vtLocks;
#endif __CONCURRENT__

        ObjectUIDType uidLastNode, uidCurrentNode;  // TODO: make Optional!
        ObjectTypePtr ptrLastNode = nullptr, ptrCurrentNode = nullptr;

        std::vector<std::pair<ObjectUIDType, ObjectTypePtr>> vtNodes;

#ifdef __CONCURRENT__
        vtLocks.push_back(std::unique_lock<std::shared_mutex>(m_mutex));
#endif __CONCURRENT__

        uidCurrentNode = m_uidRootNode.value();

        do
        {
            std::optional<ObjectUIDType> uidUpdated = std::nullopt;
            m_ptrCache->getObject(uidCurrentNode, ptrCurrentNode, uidUpdated);    //TODO: lock

            if (uidUpdated != std::nullopt)
            {
                if (ptrLastNode != nullptr)
                {
                    if (std::holds_alternative<std::shared_ptr<IndexNodeType>>(*ptrLastNode->data))
                    {
                        std::shared_ptr<IndexNodeType> ptrIndexNode = std::get<std::shared_ptr<IndexNodeType>>(*ptrLastNode->data);
                        ptrIndexNode->updateChildUID(uidCurrentNode, *uidUpdated);
                        ptrLastNode->dirty = true;
                    }
                    else //if (std::holds_alternative<std::shared_ptr<DataNodeType>>(*ptrLastNode->data))
                    {
                        throw new std::exception("should not occur!");
                    }
                }
                else
                {
                    //ptrLastNode->dirty = true; do ths ame for root!
                    assert(uidCurrentNode == *m_uidRootNode);
                    m_uidRootNode = uidUpdated;
                }

                uidCurrentNode = *uidUpdated;
            }

#ifdef __CONCURRENT__
            vtLocks.push_back(std::unique_lock<std::shared_mutex>(ptrCurrentNode->mutex));
#endif __CONCURRENT__

            if (ptrCurrentNode == nullptr)
            {
                throw new std::exception("should not occur!");   // TODO: critical log.
            }

            vtAccessedNodes.push_back(std::make_pair(uidCurrentNode, ptrCurrentNode));

            if (std::holds_alternative<std::shared_ptr<IndexNodeType>>(*ptrCurrentNode->data))
            {
                std::shared_ptr<IndexNodeType> ptrIndexNode = std::get<std::shared_ptr<IndexNodeType>>(*ptrCurrentNode->data);

                if (ptrIndexNode->canTriggerSplit(m_nDegree))
                {
                    vtNodes.push_back(std::pair<ObjectUIDType, ObjectTypePtr>(uidLastNode, ptrLastNode));
                }
                else
                {
#ifdef __CONCURRENT__
                    vtLocks.erase(vtLocks.begin(), vtLocks.begin() + vtLocks.size() - 1);
#endif __CONCURRENT__
                    vtNodes.clear(); //TODO: release locks
                }

                uidLastNode = uidCurrentNode;
                ptrLastNode = ptrCurrentNode;

                uidCurrentNode = ptrIndexNode->getChild(key);
            }
            else if (std::holds_alternative<std::shared_ptr<DataNodeType>>(*ptrCurrentNode->data))
            {
                std::shared_ptr<DataNodeType> ptrDataNode = std::get<std::shared_ptr<DataNodeType>>(*ptrCurrentNode->data);

                ptrCurrentNode->dirty = true;

                if (ptrDataNode->insert(key, value) != ErrorCode::Success)
                {
                    vtNodes.clear();

#ifdef __CONCURRENT__
                    vtLocks.clear();
#endif __CONCURRENT__
                    return ErrorCode::InsertFailed;
                }

                if (ptrDataNode->requireSplit(m_nDegree))
                {
                    vtNodes.push_back(std::pair<ObjectUIDType, ObjectTypePtr>(uidLastNode, ptrLastNode));
                    vtNodes.push_back(std::pair<ObjectUIDType, ObjectTypePtr>(uidCurrentNode, ptrCurrentNode));
                }
                else
                {
#ifdef __CONCURRENT__
                    vtLocks.clear();
#endif __CONCURRENT__
                    vtNodes.clear(); //TODO: release locks
                }

                break;
            }
        } while (true);

        KeyType pivotKey;
        ObjectUIDType uidLHSNode;
        std::optional<ObjectUIDType> uidRHSNode;

        while (vtNodes.size() > 0)
        {
            std::pair<ObjectUIDType, ObjectTypePtr> prNodeDetails = vtNodes.back();

            if (prNodeDetails.second == nullptr)
            {
                if (vtNodes.size() != 1)
                {
                    throw new std::exception("should not occur!");
                }

                m_ptrCache->template createObjectOfType<IndexNodeType>(m_uidRootNode, pivotKey, uidLHSNode, *uidRHSNode);

                int idx = 0;
                auto it_a = vtAccessedNodes.begin();
                while (it_a != vtAccessedNodes.end())
                {
                    idx++;
                    if ((*it_a).first == prNodeDetails.first)
                    {
                        //since dont have pointer to object.. addding nullptr.. to do ..fix it later..
                        vtAccessedNodes.insert(vtAccessedNodes.begin() + idx, std::make_pair(*uidRHSNode, nullptr));
                        break;
                    }
                    it_a++;

                }


                break;
            }

            if (std::holds_alternative<std::shared_ptr<IndexNodeType>>(*prNodeDetails.second->data))
            {
                std::shared_ptr<IndexNodeType> ptrIndexNode = std::get<std::shared_ptr<IndexNodeType>>(*prNodeDetails.second->data);

                int idx = 0;
                auto it_a = vtAccessedNodes.begin();
                while (it_a != vtAccessedNodes.end())
                {
                    idx++;
                    if ((*it_a).first == prNodeDetails.first)
                    {
                        //since dont have pointer to object.. addding nullptr.. to do ..fix it later..
                        vtAccessedNodes.insert(vtAccessedNodes.begin() +  idx, std::make_pair(* uidRHSNode, nullptr));
                        break;
                    }
                    it_a++;
                }

                if (ptrIndexNode->insert(pivotKey, *uidRHSNode) != ErrorCode::Success)
                {
                    // TODO: Should update be performed on cloned objects first?
                    throw new std::exception("should not occur!"); // for the time being!
                }

                prNodeDetails.second->dirty = true;

                uidRHSNode = std::nullopt;

                if (ptrIndexNode->requireSplit(m_nDegree))
                {
                    ErrorCode errCode = ptrIndexNode->template split<std::shared_ptr<CacheType>>(m_ptrCache, uidRHSNode, pivotKey);

                    if (errCode != ErrorCode::Success)
                    {
                        // TODO: Should update be performed on cloned objects first?
                        throw new std::exception("should not occur!"); // for the time being!
                    }

                }
            }
            else if (std::holds_alternative<std::shared_ptr<DataNodeType>>(*prNodeDetails.second->data))
            {
                std::shared_ptr<DataNodeType> ptrDataNode = std::get<std::shared_ptr<DataNodeType>>(*prNodeDetails.second->data);

                ErrorCode errCode = ptrDataNode->template split<std::shared_ptr<CacheType>, ObjectUIDType>(m_ptrCache, uidRHSNode, pivotKey);

                if (errCode != ErrorCode::Success)
                {
                    // TODO: Should update be performed on cloned objects first?
                    throw new std::exception("should not occur!"); // for the time being!
                }

                prNodeDetails.second->dirty = true;

            }

            uidLHSNode = prNodeDetails.first;

            vtNodes.pop_back();
        }

        m_ptrCache->reorder(vtAccessedNodes);
        vtAccessedNodes.clear();

        return ErrorCode::Success;
    }

    ErrorCode search(const KeyType& key, ValueType& value)
    {
        ErrorCode errCode = ErrorCode::Error;

        std::vector<std::pair<ObjectUIDType, ObjectTypePtr>> vtAccessedNodes;

#ifdef __CONCURRENT__
        std::vector<std::shared_lock<std::shared_mutex>> vtLocks;
        vtLocks.push_back(std::shared_lock<std::shared_mutex>(m_mutex));
#endif __CONCURRENT__

        ObjectUIDType uidCurrentNode = *m_uidRootNode;
        do
        {
            ObjectTypePtr prNodeDetails = nullptr;

            std::optional<ObjectUIDType> uidUpdated = std::nullopt;
            m_ptrCache->getObject(uidCurrentNode, prNodeDetails, uidUpdated);    //TODO: lock

            if (uidUpdated != std::nullopt)
            {
                ObjectTypePtr ptrLastNode = vtAccessedNodes.size() > 0 ? vtAccessedNodes[vtAccessedNodes.size() - 1].second : nullptr;
                if (ptrLastNode != nullptr)
                {
                    if (std::holds_alternative<std::shared_ptr<IndexNodeType>>(*ptrLastNode->data))
                    {
                        std::shared_ptr<IndexNodeType> ptrIndexNode = std::get<std::shared_ptr<IndexNodeType>>(*ptrLastNode->data);
                        ptrIndexNode->updateChildUID(uidCurrentNode, *uidUpdated);

                        ptrLastNode->dirty = true;
                    }
                    else //if (std::holds_alternative<std::shared_ptr<DataNodeType>>(*ptrLastNode->data))
                    {
                        throw new std::exception("should not occur!");
                    }
                }
                else
                {
                    //ptrLastNode->dirty = true;  do the same thing here... atm root never leaves cache. but later fix this.
                    assert(uidCurrentNode == *m_uidRootNode);
                    m_uidRootNode = uidUpdated;
                }

                uidCurrentNode = *uidUpdated;
            }



#ifdef __CONCURRENT__
            vtLocks.push_back(std::shared_lock<std::shared_mutex>(prNodeDetails->mutex));
            vtLocks.erase(vtLocks.begin());
#endif __CONCURRENT__

            if (prNodeDetails == nullptr)
            {
                throw new std::exception("should not occur!");
            }

            vtAccessedNodes.push_back(std::make_pair(uidCurrentNode, prNodeDetails));

            if (std::holds_alternative<std::shared_ptr<IndexNodeType>>(*prNodeDetails->data))
            {
                std::shared_ptr<IndexNodeType> ptrIndexNode = std::get<std::shared_ptr<IndexNodeType>>(*prNodeDetails->data);

                uidCurrentNode = ptrIndexNode->getChild(key);
            }
            else if (std::holds_alternative<std::shared_ptr<DataNodeType>>(*prNodeDetails->data))
            {
                std::shared_ptr<DataNodeType> ptrDataNode = std::get<std::shared_ptr<DataNodeType>>(*prNodeDetails->data);

                errCode = ptrDataNode->getValue(key, value);

                break;
            }


        } while (true);

        m_ptrCache->reorder(vtAccessedNodes);
        vtAccessedNodes.clear();

        return errCode;
    }

    ErrorCode remove(const KeyType& key)
    {   
        std::vector<std::pair<ObjectUIDType, ObjectTypePtr>> vtAccessedNodes;

#ifdef __CONCURRENT__
        std::vector<std::unique_lock<std::shared_mutex>> vtLocks;
#endif __CONCURRENT__

        ObjectUIDType uidLastNode, uidCurrentNode;
        ObjectTypePtr ptrLastNode = nullptr, ptrCurrentNode = nullptr;

        std::vector<std::pair<ObjectUIDType, ObjectTypePtr>> vtNodes;

#ifdef __CONCURRENT__
        vtLocks.push_back(std::unique_lock<std::shared_mutex>(m_mutex));
#endif __CONCURRENT__

        uidCurrentNode = m_uidRootNode.value();

        do
        {
            std::optional<ObjectUIDType> uidUpdated = std::nullopt;
            m_ptrCache->getObject(uidCurrentNode, ptrCurrentNode, uidUpdated);    //TODO: lock
            

            if (uidUpdated != std::nullopt)
            {
                if (ptrLastNode != nullptr)
                {
                    if (std::holds_alternative<std::shared_ptr<IndexNodeType>>(*ptrLastNode->data))
                    {
                        std::shared_ptr<IndexNodeType> ptrIndexNode = std::get<std::shared_ptr<IndexNodeType>>(*ptrLastNode->data);
                        ptrIndexNode->updateChildUID(uidCurrentNode, *uidUpdated);
                        ptrLastNode->dirty = true;
                    }
                    else //if (std::holds_alternative<std::shared_ptr<DataNodeType>>(*ptrLastNode->data))
                    {
                        throw new std::exception("should not occur!");
                    }
                }
                else
                {
                    // ptrLastNode->dirty = true; todo: do for parent as well..
                    assert(uidCurrentNode == *m_uidRootNode);
                    m_uidRootNode = uidUpdated;
                }

                uidCurrentNode = *uidUpdated;
            }



#ifdef __CONCURRENT__
            vtLocks.push_back(std::unique_lock<std::shared_mutex>(ptrCurrentNode->mutex));
#endif __CONCURRENT__

            if (ptrCurrentNode == nullptr)
            {
                throw new std::exception("should not occur!");
            }

            vtAccessedNodes.push_back(std::make_pair(uidCurrentNode, ptrCurrentNode));

            if (std::holds_alternative<std::shared_ptr<IndexNodeType>>(*ptrCurrentNode->data))
            {
                std::shared_ptr<IndexNodeType> ptrIndexNode = std::get<std::shared_ptr<IndexNodeType>>(*ptrCurrentNode->data);

                if (ptrIndexNode->canTriggerMerge(m_nDegree))
                {
                    vtNodes.push_back(std::pair<ObjectUIDType, ObjectTypePtr>(uidLastNode, ptrLastNode));
                }
                else
                {
#ifdef __CONCURRENT__
                    if (vtLocks.size() > 3) //TODO: 3 seems to be working.. but how and why.. investiage....!
                    {
                        vtLocks.erase(vtLocks.begin());
                    }
#endif __CONCURRENT__
                    vtNodes.clear(); //TODO: release locks
                }

                uidLastNode = uidCurrentNode;
                ptrLastNode = ptrCurrentNode;

                uidCurrentNode = ptrIndexNode->getChild(key); // fid it.. there are two kinds of methods..
            }
            else if (std::holds_alternative<std::shared_ptr<DataNodeType>>(*ptrCurrentNode->data))
            {
                std::shared_ptr<DataNodeType> ptrDataNode = std::get<std::shared_ptr<DataNodeType>>(*ptrCurrentNode->data);

                if (ptrDataNode->remove(key) == ErrorCode::KeyDoesNotExist)
                {
                    throw new std::exception("should not occur!");
                }

                ptrCurrentNode->dirty = true;

                if (ptrDataNode->requireMerge(m_nDegree))
                {
                    vtNodes.push_back(std::pair<ObjectUIDType, ObjectTypePtr>(uidLastNode, ptrLastNode));
                    vtNodes.push_back(std::pair<ObjectUIDType, ObjectTypePtr>(uidCurrentNode, ptrCurrentNode));
                }
                else
                {
#ifdef __CONCURRENT__
                    vtLocks.clear();
#endif __CONCURRENT__
                    vtNodes.clear(); //TODO: release locks
                }

                break;
            }
        } while (true);

        ObjectUIDType uidChildNode;
        ObjectTypePtr ptrChildNode = nullptr;

        while (vtNodes.size() > 0)
        {
            std::pair<ObjectUIDType, ObjectTypePtr> prNodeDetails = vtNodes.back();

            if (prNodeDetails.second == nullptr)
            {
                if (vtNodes.size() != 1)
                {
                    throw new std::exception("should not occur!");
                }

                if (*m_uidRootNode != uidChildNode)
                {
                    throw new std::exception("should not occur!");
                }

                ObjectTypePtr ptrCurrentRoot;
                std::optional<ObjectUIDType> uidUpdated = std::nullopt;
                m_ptrCache->getObject(*m_uidRootNode, ptrCurrentRoot, uidUpdated);

                assert(uidUpdated == std::nullopt);

                ptrCurrentRoot->dirty = true;

                if (std::holds_alternative<std::shared_ptr<IndexNodeType>>(*ptrCurrentRoot->data))
                {
                    std::shared_ptr<IndexNodeType> ptrInnerNode = std::get<std::shared_ptr<IndexNodeType>>(*ptrCurrentRoot->data);
                    if (ptrInnerNode->getKeysCount() == 0) {
                        ObjectUIDType _tmp = ptrInnerNode->getChildAt(0);
                        m_ptrCache->remove(*m_uidRootNode);
                        m_uidRootNode = _tmp;

                        ptrCurrentRoot->dirty = true;
                    }
                }
                else if (std::holds_alternative<std::shared_ptr<DataNodeType>>(*ptrCurrentRoot->data))
                {
                    int i = 0;
                    /*std::shared_ptr<DataNodeType> ptrDataNode = std::get<std::shared_ptr<DataNodeType>>(*ptrCurrentRoot->data);
                    if (ptrDataNode->getKeysCount() == 0) {
                        m_ptrCache->remove(*m_uidRootNode);
                    }*/
                }

                break;
            }

            if (std::holds_alternative<std::shared_ptr<IndexNodeType>>(*prNodeDetails.second->data))
            {
                std::shared_ptr<IndexNodeType> ptrParentIndexNode = std::get<std::shared_ptr<IndexNodeType>>(*prNodeDetails.second->data);

                if (std::holds_alternative<std::shared_ptr<IndexNodeType>>(*ptrChildNode->data))
                {
                    std::optional<ObjectUIDType> uidToDelete = std::nullopt;

                    std::shared_ptr<IndexNodeType> ptrChildIndexNode = std::get<std::shared_ptr<IndexNodeType>>(*ptrChildNode->data);

                    if (ptrChildIndexNode->requireMerge(m_nDegree))
                    {
                        ptrParentIndexNode->template rebalanceIndexNode<std::shared_ptr<CacheType>, shared_ptr<IndexNodeType>>(m_ptrCache, uidChildNode, ptrChildIndexNode, key, m_nDegree, uidToDelete);

                        prNodeDetails.second->dirty = true;
                        ptrChildNode->dirty = true;

                        if (uidToDelete)
                        {
#ifdef __CONCURRENT__
                            if (*uidToDelete == uidChildNode)
                            {
                                auto it = vtLocks.begin();
                                while (it != vtLocks.end()) {
                                    if ((*it).mutex() == &ptrChildNode->mutex)
                                    {
                                        break;
                                    }
                                    it++;
                                }

                                if (it != vtLocks.end())
                                    vtLocks.erase(it);
                            }
#endif __CONCURRENT__

                            m_ptrCache->remove(*uidToDelete);
                        }
                    }
                }
                else if (std::holds_alternative<std::shared_ptr<DataNodeType>>(*ptrChildNode->data))
                {
                    std::optional<ObjectUIDType> uidToDelete = std::nullopt;

                    std::shared_ptr<DataNodeType> ptrChildDataNode = std::get<std::shared_ptr<DataNodeType>>(*ptrChildNode->data);

                    ptrParentIndexNode->template rebalanceDataNode<std::shared_ptr<CacheType>, shared_ptr<DataNodeType>>(m_ptrCache, uidChildNode, ptrChildDataNode, key, m_nDegree, uidToDelete);

                    prNodeDetails.second->dirty = true;
                    ptrChildNode->dirty = true;

                    if (uidToDelete)
                    {
#ifdef __CONCURRENT__
                        if (*uidToDelete == uidChildNode)
                        {
                            auto it = vtLocks.begin();
                            while (it != vtLocks.end()) {
                                if ((*it).mutex() == &ptrChildNode->mutex)
                                {
                                    break;
                                }
                                it++;
                            }

                            if (it != vtLocks.end())
                                vtLocks.erase(it);
                        }
#endif __CONCURRENT__

                        m_ptrCache->remove(*uidToDelete);
                    }
                }
            }

            uidChildNode = prNodeDetails.first;
            ptrChildNode = prNodeDetails.second;
            vtNodes.pop_back();
        }
        
        m_ptrCache->reorder(vtAccessedNodes, false);
        vtAccessedNodes.clear();

        return ErrorCode::Success;
    }

    void print(std::ofstream & out)
    {
        int nSpace = 7;

        std::string prefix;

        out << prefix << "|" << std::endl;
        out << prefix << "|" << std::string(nSpace, '-').c_str() << "(root)";

        out << std::endl;

        ObjectTypePtr ptrRootNode = nullptr;
        std::optional<ObjectUIDType> uidUpdated = std::nullopt;
        m_ptrCache->getObject(m_uidRootNode.value(), ptrRootNode, uidUpdated);

        if (uidUpdated != std::nullopt)
        {
            m_uidRootNode = uidUpdated;
        }

        if (std::holds_alternative<std::shared_ptr<IndexNodeType>>(*ptrRootNode->data))
        {
            std::shared_ptr<IndexNodeType> ptrIndexNode = std::get<std::shared_ptr<IndexNodeType>>(*ptrRootNode->data);

            ptrIndexNode->template print<std::shared_ptr<CacheType>, ObjectTypePtr, DataNodeType>(out, m_ptrCache, 0, prefix);
        }
        else if (std::holds_alternative<std::shared_ptr<DataNodeType>>(*ptrRootNode->data))
        {
            std::shared_ptr<DataNodeType> ptrDataNode = std::get<std::shared_ptr<DataNodeType>>(*ptrRootNode->data);

            ptrDataNode->print(out, 0, prefix);
        }
    }

    void getCacheState(size_t& lru, size_t& map)
    {
        return m_ptrCache->getCacheState(lru, map);
    }

#ifdef __TREE_AWARE_CACHE__
public:
    CacheErrorCode updateChildUID(const std::optional<ObjectUIDType>& uidObject, const ObjectUIDType& uidChildOld, const ObjectUIDType& uidChildNew)
    {
        return CacheErrorCode::Success;
    }

    CacheErrorCode updateChildUID(std::vector<std::pair<ObjectUIDType, std::pair<ObjectUIDType, ObjectUIDType>>> vtUpdatedUIDs)
    {
        throw new std::exception("no implementation!");
    }

    void applyExistingUpdates(std::vector<std::pair<ObjectUIDType, std::pair<std::optional<ObjectUIDType>, std::shared_ptr<ObjectType>>>>& vtNodes
        , std::unordered_map<ObjectUIDType, std::pair<std::optional<ObjectUIDType>, std::shared_ptr<ObjectType>>>& mpUpdatedUIDs)
    {
        auto it = vtNodes.begin();
        while (it != vtNodes.end())
        {
            if (std::holds_alternative<std::shared_ptr<IndexNodeType>>(*(*it).second.second->data))
            {
                std::shared_ptr<IndexNodeType> ptrIndexNode = std::get<std::shared_ptr<IndexNodeType>>(*(*it).second.second->data);

                auto it_children = ptrIndexNode->m_ptrData->m_vtChildren.begin();
                while (it_children != ptrIndexNode->m_ptrData->m_vtChildren.end())
                {
                    if (mpUpdatedUIDs.find(*it_children) != mpUpdatedUIDs.end())
                    {
                        ObjectUIDType uidTemp = *it_children;

                        *it_children = *(mpUpdatedUIDs[*it_children].first);
                        mpUpdatedUIDs.erase(uidTemp);  // Update applied!

                        (*it).second.second->dirty = true;
                    }
                    it_children++;
                }
            }
            else //if (std::holds_alternative<std::shared_ptr<DataNodeType>>(*(*it).second.second->data))
            {
                //throw new std::exception("should not occur!");   // TODO: critical log.
            }
            it++;
        }
    }

    void applyExistingUpdates(std::shared_ptr<ObjectType> ptrObject
        , std::unordered_map<ObjectUIDType, std::pair<std::optional<ObjectUIDType>, std::shared_ptr<ObjectType>>>& mpUpdatedUIDs)
    {
        if (std::holds_alternative<std::shared_ptr<IndexNodeType>>(*ptrObject->data))
        {
            std::shared_ptr<IndexNodeType> ptrIndexNode = std::get<std::shared_ptr<IndexNodeType>>(*ptrObject->data);

            auto it_children = ptrIndexNode->m_ptrData->m_vtChildren.begin();
            while (it_children != ptrIndexNode->m_ptrData->m_vtChildren.end())
            {
                if (mpUpdatedUIDs.find(*it_children) != mpUpdatedUIDs.end())
                {
                    ObjectUIDType uidTemp = *it_children;
                    *it_children = *(mpUpdatedUIDs[*it_children].first);
                    mpUpdatedUIDs.erase(uidTemp);  // Update applied!

                    ptrObject->dirty = true;
                }
                it_children++;
            }
        }
        else //if (std::holds_alternative<std::shared_ptr<DataNodeType>>(*(*it).second.second->data))
        {
            //throw new std::exception("should not occur!");   // TODO: critical log.
        }
    }

    void prepareFlush(std::vector<std::pair<ObjectUIDType, std::pair<std::optional<ObjectUIDType>, std::shared_ptr<ObjectType>>>>& vtObjects
        , size_t& nOffset, size_t nPointerSize)
    {
        auto it = vtObjects.begin();
        while (it != vtObjects.end())
        {
            if (!(*it).second.second->dirty) // dont proceed.. assumption.. nodes are always in the order.. related leaf nodes and then parent nodes..
            {
                (*it).second.first = std::nullopt;
                (*it).second.second = nullptr;
                continue;
            }

            if (std::holds_alternative<std::shared_ptr<IndexNodeType>>(*(*it).second.second->data))
            {
                std::shared_ptr<IndexNodeType> ptrObject = std::get<std::shared_ptr<IndexNodeType>>(*(*it).second.second->data);
                size_t nSize = ptrObject->getSize();

                //ObjectUIDType uidUpdated = ObjectUIDType::createAddressFromDRAMCacheCounter(nOffset);
                ObjectUIDType uidUpdated = ObjectUIDType::createAddressFromFileOffset(nOffset * nPointerSize, nSize);
                (*it).second.first = uidUpdated;

                //nOffset += nPointerSize;
                nOffset += std::ceil(nSize / (float)nPointerSize);
            }
            else if (std::holds_alternative<std::shared_ptr<DataNodeType>>(*(*it).second.second->data))
            {
                std::shared_ptr<DataNodeType> ptrObject = std::get<std::shared_ptr<DataNodeType>>(*(*it).second.second->data);
                size_t nSize = ptrObject->getSize();

                //ObjectUIDType uidUpdated = ObjectUIDType::createAddressFromDRAMCacheCounter(nOffset);
                ObjectUIDType uidUpdated = ObjectUIDType::createAddressFromFileOffset(nOffset * nPointerSize, nSize);
                (*it).second.first = uidUpdated;

                //nOffset += nPointerSize;
                nOffset += std::ceil(nSize / (float)nPointerSize);
            }
            it++;
        }
        
        std::vector<bool> vtApplied;
        vtApplied.resize(vtObjects.size(), false);

        for (int idx = 0; idx < vtObjects.size(); idx++)
        {
            if (vtObjects[idx].second.second == nullptr) //make solution better... ids are already generated in this case..
             continue;

            if (std::holds_alternative<std::shared_ptr<IndexNodeType>>(*(vtObjects[idx].second.second->data)))
            {
                std::shared_ptr<IndexNodeType> ptrIndexNode = std::get<std::shared_ptr<IndexNodeType>>(*(vtObjects[idx].second.second->data));
                
                auto it = ptrIndexNode->m_ptrData->m_vtChildren.begin();
                while (it != ptrIndexNode->m_ptrData->m_vtChildren.end())
                {
                    for (int jdx = 0; jdx < vtObjects.size(); jdx++)
                    {
                        if (vtObjects[jdx].second.second == nullptr) //make solution better... ids are already generated in this case..
                            continue;

                        if (idx == jdx || vtApplied[jdx])
                            continue;

                        if (*it == vtObjects[jdx].first)
                        {
                            *it = *vtObjects[jdx].second.first;
                            vtApplied[jdx] = true;
                            break;
                        }
                    }
                    it++;
                }               
            }
            else //if (std::holds_alternative<std::shared_ptr<DataNodeType>>(vtItems[jdx].second.second))
            {
                continue;
            }
        }
    }
#endif __TREE_AWARE_CACHE__
};