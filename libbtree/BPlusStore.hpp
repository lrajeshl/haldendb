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
#include <vector>
#include <stdexcept>
#include <thread>
#include <iostream>
#include <fstream>
#include <assert.h>

using namespace std::chrono_literals;

#ifdef __TREE_WITH_CACHE__
template <typename ICallback, typename KeyType, typename ValueType, typename CacheType>
class BPlusStore : public ICallback
#else // !__TREE_WITH_CACHE__
template <typename KeyType, typename ValueType, typename CacheType>
class BPlusStore
#endif __TREE_WITH_CACHE__
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
#ifdef __TREE_WITH_CACHE__
        m_ptrCache->init(this);
#endif __TREE_WITH_CACHE__

        m_ptrCache->template createObjectOfType<DefaultNodeType>(m_uidRootNode);
    }

    ErrorCode insert(const KeyType& key, const ValueType& value, bool print = false)
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
        int i=0;
        uidCurrentNode = m_uidRootNode.value();

//if (print) { std::cout << "++++++++++++" << key << "+++++++++";}
        do
        {
//            if (print) { std::this_thread::sleep_for(10ms);
//std::cout << uidCurrentNode.toString().c_str() << std::endl;}

#ifdef __TREE_WITH_CACHE__
            std::optional<ObjectUIDType> uidUpdated = std::nullopt;
            m_ptrCache->getObject(uidCurrentNode, ptrCurrentNode, uidUpdated);    //TODO: lock

            if (uidUpdated != std::nullopt)
            {
                //if (print) { std::cout << "1.1" << std::endl;}

                if (ptrLastNode != nullptr)
                {
                    if (std::holds_alternative<std::shared_ptr<IndexNodeType>>(*ptrLastNode->data))
                    {
                        //if (print) { std::cout << "1.1.1" << std::endl;}
                        std::shared_ptr<IndexNodeType> ptrIndexNode = std::get<std::shared_ptr<IndexNodeType>>(*ptrLastNode->data);
                        ptrIndexNode->updateChildUID(uidCurrentNode, *uidUpdated);
                        ptrLastNode->dirty = true;
                    }
                    else //if (std::holds_alternative<std::shared_ptr<DataNodeType>>(*ptrLastNode->data))
                    {
                        //if (print) { std::cout << "1.1.2" << std::endl;}
                        throw new std::logic_error("should not occur!");
                    }
                }
                else
                {
                       // if (print) { std::cout << "1.1.3" << std::endl;}
                    //ptrLastNode->dirty = true; //do ths ame for root!
                    assert(uidCurrentNode == *m_uidRootNode);
                    m_uidRootNode = uidUpdated;
                }

                uidCurrentNode = *uidUpdated;
            }
#else __TREE_WITH_CACHE__
            m_ptrCache->getObject(uidCurrentNode, ptrCurrentNode);    //TODO: lock
#endif __TREE_WITH_CACHE__

#ifdef __CONCURRENT__
            vtLocks.push_back(std::unique_lock<std::shared_mutex>(ptrCurrentNode->mutex));
#endif __CONCURRENT__

            if (ptrCurrentNode == nullptr)
            {
                throw new std::logic_error("should not occur!");   // TODO: critical log.
            }

            vtAccessedNodes.push_back(std::make_pair(uidCurrentNode, ptrCurrentNode));

            if (std::holds_alternative<std::shared_ptr<IndexNodeType>>(*ptrCurrentNode->data))
            {
                //if (print) { std::cout << "2." << std::endl;}

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
                //if (print) { std::cout << "3." << std::endl;}
                std::shared_ptr<DataNodeType> ptrDataNode = std::get<std::shared_ptr<DataNodeType>>(*ptrCurrentNode->data);

#ifdef __TREE_WITH_CACHE__
                ptrCurrentNode->dirty = true;
#endif __TREE_WITH_CACHE__

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

                //if (print) { std::cout << "4." << std::endl;}
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
                    throw new std::logic_error("should not occur!");
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
                    throw new std::logic_error("should not occur!"); // for the time being!
                }

#ifdef __TREE_WITH_CACHE__
                prNodeDetails.second->dirty = true;
#endif __TREE_WITH_CACHE__

                uidRHSNode = std::nullopt;

                if (ptrIndexNode->requireSplit(m_nDegree))
                {
                    ErrorCode errCode = ptrIndexNode->template split<std::shared_ptr<CacheType>>(m_ptrCache, uidRHSNode, pivotKey);

                    if (errCode != ErrorCode::Success)
                    {
                        // TODO: Should update be performed on cloned objects first?
                        throw new std::logic_error("should not occur!"); // for the time being!
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
                    throw new std::logic_error("should not occur!"); // for the time being!
                }

#ifdef __TREE_WITH_CACHE__
                prNodeDetails.second->dirty = true;
#endif __TREE_WITH_CACHE__
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

#ifdef __TREE_WITH_CACHE__
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
                        throw new std::logic_error("should not occur!");
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
#else __TREE_WITH_CACHE__
            m_ptrCache->getObject(uidCurrentNode, prNodeDetails);    //TODO: lock
#endif __TREE_WITH_CACHE__


#ifdef __CONCURRENT__
            vtLocks.push_back(std::shared_lock<std::shared_mutex>(prNodeDetails->mutex));
            vtLocks.erase(vtLocks.begin());
#endif __CONCURRENT__

            if (prNodeDetails == nullptr)
            {
                throw new std::logic_error("should not occur!");
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
#ifdef __TREE_WITH_CACHE__
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
                        throw new std::logic_error("should not occur!");
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
#else __TREE_WITH_CACHE__
            m_ptrCache->getObject(uidCurrentNode, ptrCurrentNode);    //TODO: lock
#endif __TREE_WITH_CACHE__


#ifdef __CONCURRENT__
            vtLocks.push_back(std::unique_lock<std::shared_mutex>(ptrCurrentNode->mutex));
#endif __CONCURRENT__

            if (ptrCurrentNode == nullptr)
            {
                throw new std::logic_error("should not occur!");
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
                    throw new std::logic_error("should not occur!");
                }

#ifdef __TREE_WITH_CACHE__
                ptrCurrentNode->dirty = true;
#endif __TREE_WITH_CACHE__

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
                    throw new std::logic_error("should not occur!");
                }

                if (*m_uidRootNode != uidChildNode)
                {
                    throw new std::logic_error("should not occur!");
                }

                ObjectTypePtr ptrCurrentRoot;

#ifdef __TREE_WITH_CACHE__
                std::optional<ObjectUIDType> uidUpdated = std::nullopt;
                m_ptrCache->getObject(*m_uidRootNode, ptrCurrentRoot, uidUpdated);

                assert(uidUpdated == std::nullopt);

                ptrCurrentRoot->dirty = true;
#else __TREE_WITH_CACHE__
                m_ptrCache->getObject(*m_uidRootNode, ptrCurrentRoot);
#endif __TREE_WITH_CACHE__

                if (std::holds_alternative<std::shared_ptr<IndexNodeType>>(*ptrCurrentRoot->data))
                {
                    std::shared_ptr<IndexNodeType> ptrInnerNode = std::get<std::shared_ptr<IndexNodeType>>(*ptrCurrentRoot->data);
                    if (ptrInnerNode->getKeysCount() == 0) {
                        ObjectUIDType _tmp = ptrInnerNode->getChildAt(0);
                        m_ptrCache->remove(*m_uidRootNode);
                        m_uidRootNode = _tmp;

#ifdef __TREE_WITH_CACHE__
                        ptrCurrentRoot->dirty = true;
#endif __TREE_WITH_CACHE__
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
                        ptrParentIndexNode->template rebalanceIndexNode<std::shared_ptr<CacheType>, std::shared_ptr<IndexNodeType>>(m_ptrCache, uidChildNode, ptrChildIndexNode, key, m_nDegree, uidToDelete);

#ifdef __TREE_WITH_CACHE__
                        prNodeDetails.second->dirty = true;
                        ptrChildNode->dirty = true;
#endif __TREE_WITH_CACHE__

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

                    ptrParentIndexNode->template rebalanceDataNode<std::shared_ptr<CacheType>, std::shared_ptr<DataNodeType>>(m_ptrCache, uidChildNode, ptrChildDataNode, key, m_nDegree, uidToDelete);

#ifdef __TREE_WITH_CACHE__
                    prNodeDetails.second->dirty = true;
                    ptrChildNode->dirty = true;
#endif __TREE_WITH_CACHE__

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

#ifdef __TREE_WITH_CACHE__
public:
    ErrorCode flush()
    {
        m_ptrCache->flush();

        return ErrorCode::Success;
    }

    void applyExistingUpdates(std::shared_ptr<ObjectType> ptrObject
        , std::unordered_map<ObjectUIDType, std::pair<std::optional<ObjectUIDType>, std::shared_ptr<ObjectType>>>& mpUIDUpdates)
    {
        if (std::holds_alternative<std::shared_ptr<IndexNodeType>>(*ptrObject->data))
        {
            std::shared_ptr<IndexNodeType> ptrIndexNode = std::get<std::shared_ptr<IndexNodeType>>(*ptrObject->data);

            auto it = ptrIndexNode->getChildrenBeginIterator();
            while (it != ptrIndexNode->getChildrenEndIterator())
            {
                if (mpUIDUpdates.find(*it) != mpUIDUpdates.end())
                {
                    ObjectUIDType uidTemp = *it;

                    *it = *(mpUIDUpdates[*it].first);

                    mpUIDUpdates.erase(uidTemp);

                    ptrObject->dirty = true;
                }
                it++;
            }
        }
        else //if (std::holds_alternative<std::shared_ptr<DataNodeType>>(*ptrObject->data))
        {
            // Nothing to update in this case!
        }
    }

    void applyExistingUpdates(std::vector<std::pair<ObjectUIDType, std::pair<std::optional<ObjectUIDType>, std::shared_ptr<ObjectType>>>>& vtNodes
        , std::unordered_map<ObjectUIDType, std::pair<std::optional<ObjectUIDType>, std::shared_ptr<ObjectType>>>& mpUIDUpdates)
    {
        auto it = vtNodes.begin();
        while (it != vtNodes.end())
        {
            if (std::holds_alternative<std::shared_ptr<IndexNodeType>>(*(*it).second.second->data))
            {
                std::shared_ptr<IndexNodeType> ptrIndexNode = std::get<std::shared_ptr<IndexNodeType>>(*(*it).second.second->data);

                auto it_children = ptrIndexNode->getChildrenBeginIterator();
                while (it_children != ptrIndexNode->getChildrenEndIterator())
                {
                    if (mpUIDUpdates.find(*it_children) != mpUIDUpdates.end())
                    {
                        ObjectUIDType uidTemp = *it_children;

                        *it_children = *(mpUIDUpdates[*it_children].first);

                        mpUIDUpdates.erase(uidTemp);

                        (*it).second.second->dirty = true;
                    }
                    it_children++;
                }
            }
            else //if (std::holds_alternative<std::shared_ptr<DataNodeType>>(*(*it).second.second->data))
            {
                // Nothing to update in this case!
            }
            it++;
        }
    }

    void prepareFlush(std::vector<std::pair<ObjectUIDType, std::pair<std::optional<ObjectUIDType>, std::shared_ptr<ObjectType>>>>& vtNodes
        , size_t& nPos, size_t nBlockSize, ObjectUIDType::Media nMediaType)
    {
        std::vector<bool> vtAppliedUpdates;
        vtAppliedUpdates.resize(vtNodes.size(), false);

        for (int idx = 0; idx < vtNodes.size(); idx++)
        {
            if (std::holds_alternative<std::shared_ptr<IndexNodeType>>(*vtNodes[idx].second.second->data))
            {
                std::shared_ptr<IndexNodeType> ptrIndexNode = std::get<std::shared_ptr<IndexNodeType>>(*vtNodes[idx].second.second->data);

                auto it = ptrIndexNode->getChildrenBeginIterator();
                while (it != ptrIndexNode->getChildrenEndIterator())
                {
                    for (int jdx = 0; jdx < idx; jdx++)
                    {
                        if (vtAppliedUpdates[jdx])
                            continue;

                        if (*it == vtNodes[jdx].first)
                        {
                            *it = *vtNodes[jdx].second.first;
                            vtNodes[idx].second.second->dirty = true;

                            vtAppliedUpdates[jdx] = true;
                            break;
                        }
                    }
                    it++;
                }

                if (!vtNodes[idx].second.second->dirty)
                {
                    vtNodes.erase(vtNodes.begin() + idx); idx--;
                    continue;
                }

                size_t nNodeSize = ptrIndexNode->getSize();

                ObjectUIDType uidUpdated = ObjectUIDType::createAddressFromArgs(nMediaType, IndexNodeType::UID, nPos, nBlockSize, nNodeSize);

                vtNodes[idx].second.first = uidUpdated;

                nPos += std::ceil(nNodeSize / (float)nBlockSize);
            }
            else if (std::holds_alternative<std::shared_ptr<DataNodeType>>(*vtNodes[idx].second.second->data))
            {
                if (!vtNodes[idx].second.second->dirty)
                {
                    vtNodes.erase(vtNodes.begin() + idx); idx--;
                    continue;
                }

                std::shared_ptr<DataNodeType> ptrDataNode = std::get<std::shared_ptr<DataNodeType>>(*vtNodes[idx].second.second->data);

                size_t nNodeSize = ptrDataNode->getSize();

                ObjectUIDType uidUpdated = ObjectUIDType::createAddressFromArgs(nMediaType, DataNodeType::UID, nPos, nBlockSize, nNodeSize);

                vtNodes[idx].second.first = uidUpdated;

                nPos += std::ceil(nNodeSize / (float)nBlockSize);
            }
        }
    }
#endif __TREE_WITH_CACHE__
};
