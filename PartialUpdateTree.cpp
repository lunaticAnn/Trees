#include <vector>
#include <memory>
#include <iostream>
#include <stdlib.h>
#include <time.h>
#include <string>

class Cache
{
public:
    Cache() = default;
    bool getDirty() const { return dirty; }
    void markDirty() { dirty = true; }
    void finishCaching() { dirty = false; }

    void submitNode(int id) { cacheData.emplace_back(id); }
    void reset() { cacheData.clear(); }
    void submitList(Cache* childCache);
    
    const std::vector<int>& getCachedData() const { return cacheData; }
    
private:
    bool dirty = true;
    std::vector<int> cacheData;
};

void Cache::submitList(Cache* childCache)
{
    const std::vector<int> childCacheData = childCache->getCachedData();
    cacheData.insert(cacheData.end(), childCacheData.begin(), childCacheData.end());
}

/*
   Tree Node.
 */
class Node
{
public:
    Node() = default;
    int id = 0; /* so that we can visualize the tree update. */
    
    void onChildAdded(std::shared_ptr<Node> child);
    void setParent(std::shared_ptr<Node> node);
    std::shared_ptr<Node> getParent() { return parent; }
    
    typedef std::vector<std::shared_ptr<Node>> Nodes;
    Nodes getChildren() { return children; }
    void submitNode(Cache* cache);
    void markDirty();
    
    Cache* getSubtreeCache() { return subtreeCache; }
    void attachSubtreeCache(Cache* cache);
    
private:
    std::shared_ptr<Node> parent;
    Nodes children;
    Cache* subtreeCache = nullptr;
};

void Node::onChildAdded(std::shared_ptr<Node> child)
{
    children.push_back(child);
}

void Node::setParent(std::shared_ptr<Node> node)
{
    parent = node;
    markDirty();
}

void Node::attachSubtreeCache(Cache* cache)
{
    subtreeCache = cache;
    markDirty();
}

void Node::markDirty()
{
    Node* node = getParent().get();
    while (node != nullptr)
    {
        if (node->getSubtreeCache() != nullptr)
        {
            node->getSubtreeCache()->markDirty();
        }
        node = node->getParent().get();
    }
}

/* Tree */
class Tree
{
public:
    Tree();
    void submitOrderedTreeNodes();
    void reset();
    void printTree();
    
    void insertTreeNode(int parentId);
    void attachCache(int parentId, Cache* cache);

    void beginCaching(Cache* cache);
    void endCaching(Cache* cache);
    void submitCache(Cache* cache);
    void submitNode(std::shared_ptr<Node> node);

private:
    std::shared_ptr<Node> root;
    std::vector<std::shared_ptr<Node>> nodeData;
    Cache finalCache; // use to validate the final result;
    
    std::vector<Cache*> cacheQueue; // the back of vector is the active cache
};

Tree::Tree()
{
    // create root node no. 0
    nodeData.emplace_back(new Node());
    root = nodeData.back();
}

void Tree::beginCaching(Cache* cache)
{
    cache->reset();
    cacheQueue.push_back(cache);
}

void Tree::endCaching(Cache* cache)
{
    cache->finishCaching();
    cacheQueue.pop_back();
}

void Tree::submitCache(Cache* cache)
{
    cacheQueue.back()->submitList(cache);
}

void Tree::submitNode(std::shared_ptr<Node> node)
{
    Cache* subtreeCache = node->getSubtreeCache();
    if (subtreeCache != nullptr)
    {
        if (subtreeCache->getDirty())
        {
            beginCaching(subtreeCache);
            cacheQueue.back()->submitNode(node->id);
            for (auto child:node->getChildren())
            {
                submitNode(child);
            }
            endCaching(subtreeCache);
        }
        submitCache(subtreeCache);
    }
    else
    {
        cacheQueue.back()->submitNode(node->id);
        for (auto child:node->getChildren())
        {
            submitNode(child);
        }
    }
}

void Tree::insertTreeNode(int parentId)
{
    parentId = std::min(parentId, int(nodeData.size()) - 1);
    parentId = std::max(parentId, 0);

    nodeData.emplace_back(new Node());
    Node* child = nodeData.back().get();
    child->id = int(nodeData.size()) - 1;
    child->setParent(nodeData[parentId]);
    nodeData[parentId]->onChildAdded(nodeData.back());
}

void Tree::attachCache(int parentId, Cache* cache)
{
    parentId = std::min(parentId, int(nodeData.size()) - 1);
    parentId = std::max(parentId, 0);
    nodeData[parentId]->attachSubtreeCache(cache);
}

void Tree::reset()
{
    nodeData.clear();
    nodeData.emplace_back(new Node());
    root = nodeData.back();
}

void printNode(const std::shared_ptr<Node>& node, int depth)
{
    for (int i = 0; i < depth - 1; ++i)
    {
        std::cout<<"  ";
    }
    std::cout<<(depth == 0 ? "":"|_")<<node->id;
    if (node->getSubtreeCache() != nullptr)
    {
        std::cout<<(node->getSubtreeCache()->getDirty() ? " [NeedsUpdate]" : "")<<" Cache:";
        for(const auto& nid:node->getSubtreeCache()->getCachedData())
            std::cout<<nid<<"|";
    }
    std::cout<<std::endl;
    for (const auto& child : node->getChildren())
    {
        printNode(child, depth + 1);
    }
}

void Clear()
{
#if defined _WIN32
    system("cls");
#elif defined (__LINUX__) || defined(__gnu_linux__) || defined(__linux__)
    system("clear");
#elif defined (__APPLE__)
    system("clear");
#endif
}

void Tree::printTree()
{
    Clear();
    std::cout<<"[Current Tree Status]"<<std::endl;
    printNode(root, 0);
}

void Tree::submitOrderedTreeNodes()
{
    beginCaching(&finalCache);
    submitNode(root);
    endCaching(&finalCache);
    for(const auto& nid:finalCache.getCachedData())
        std::cout<<nid<<"|";
    std::cout<<std::endl;
}

/* Helper Functions */
void generateRandomTree(Tree* tree, int kNodeCount)
{
    tree->reset();
    for (int i = 0; i < kNodeCount - 1; ++i)
    {
        int target = rand() % (i + 1);
        tree->insertTreeNode(target);
    }
}

const std::string manual = "P/p: print current tree\n"
                           "S/s: update dirty caches\n"
                           "I/i: insert new node\n"
                           "C/c: attach cache on a tree node\n"
                           "R/r: reset tree\n"
                           "H/h: show this manual\n";
int main()
{
    srand (time(NULL));
    Tree tree;
    generateRandomTree(&tree, 10);
    tree.printTree();
    std::cout<<manual;

    std::vector<Cache> cachePool;
    cachePool.reserve(100);

    while (1)
    {
        std::cout<<":";
        std::string operation;
        std::cin>> operation;
        if (operation == "P" || operation == "p")
        {
            tree.printTree();
        }
        else if (operation == "S" || operation == "s")
        {
            tree.submitOrderedTreeNodes();
        }
        else if (operation == "I" || operation == "i")
        {
            std::cout<<"Which node you want to insert the new node under?"<<std::endl;
            int i;
            std::cin>>i;
            tree.insertTreeNode(i);
            tree.printTree();
            std::cout<<"finish inserting node"<<std::endl;
        }
        else if (operation == "C" || operation == "c")
        {
            std::cout<<"Which node you want to attach the cache under?"<<std::endl;
            int i;
            cachePool.push_back(Cache());
            std::cin>>i;
            tree.attachCache(i, &cachePool.back());
            tree.printTree();
            std::cout<<"finish attaching cache"<<std::endl;
        }
        else if (operation == "R" || operation == "r")
        {
            std::cout<<"How many preset tree nodes you want to create with?"<<std::endl;
            int i;
            std::cin>>i;
            generateRandomTree(&tree, i);
            tree.printTree();
        }
        else if (operation == "H" || operation == "h")
        {
            std::cout<<manual;
        }
        else if (operation == "Q" || operation == "q" || operation == "exit")
        {
            break;
        }
    }
    return 0;
}
