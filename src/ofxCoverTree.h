#pragma once

#include <Eigen/Eigen>

#include <exception>
#include <fstream>
#include <iostream>
#include <stack>
#include <vector>

/* polyfill from
 * https://xenakios.wordpress.com/2014/09/29/concurrency-in-c-the-cross-platform-way/
 */
#ifdef __MACOSX__

#include "dispatch/dispatch.h"
template <typename F, typename... Args>
auto async_polyfill(F&& f, Args&&... args)
    -> std::future<typename std::result_of<F(Args...)>::type> {
  using result_type = typename std::result_of<F(Args...)>::type;
  using packaged_type = std::packaged_task<result_type()>;

  auto p = new packaged_type(
      std::bind(std::forward<F>(f), std::forward<Args>(args)...));
  auto result = p->get_future();
  dispatch_async_f(
      dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), p,
      [](void* f_) {
        packaged_type* f = static_cast<packaged_type*>(f_);
        (*f)();
        delete f;
      });

  return result;
}

#else

#include <future>
#include <mutex>
#include <thread>

template <typename F, typename... Args>
auto async_polyfill(F&& f, Args&&... args)
    -> std::future<typename std::result_of<F(Args...)>::type> {
  return std::async(std::launch::async, std::forward<F>(f),
                    std::forward<Args>(args)...);
}

#endif

namespace ofxCoverTree {

typedef Eigen::VectorXf point;

struct item : point {
  using ofxCoverTree::point::point;
  size_t id;
};

template <typename point = ofxCoverTree::item, typename numeric = float>
class CoverTree {
 public:
  numeric base;
  numeric powdict[2048];

  // structure for each node
  struct Node {
    point _p;                     // point associated with the node
    std::vector<Node*> children;  // list of children
    int level;                    // current level of the node
    numeric maxdistUB;  // Upper bound of distance to any of descendants
    numeric tempDist;

    inline numeric covdist(const numeric (&powdict)[2048]) {
      return powdict[level + 1024];  // pow(base, level);
    }

    inline numeric sepdist(const numeric (&powdict)[2048]) {
      return powdict[level + 1023];  // pow(base, level - 1);
    }

    inline void setChild(const point& pIns) {
      Node* temp = new Node;
      temp->_p = pIns;
      temp->level = level - 1;
      temp->maxdistUB = 0;
      // maxdistUB = std::max(maxdistUB, tempDist);
      children.push_back(temp);
    }

    inline void setChild(Node* pIns) {
      if (pIns->level != level - 1) {
        Node* current = pIns;
        std::stack<Node*> travel;
        current->level = level - 1;
        travel.push(current);
        while (travel.size() > 0) {
          current = travel.top();
          travel.pop();

          for (const auto& child : *current) {
            child->level = current->level - 1;
            travel.push(child);
          }
        }
      }
      children.push_back(pIns);
    }

    inline numeric dist(const point& pp) const { return (_p - pp).norm(); }

    numeric dist(Node* n) const { return (_p - n->_p).norm(); }

    /*** Iterator access ***/
    inline typename std::vector<Node*>::iterator begin() {
      return children.begin();
    }

    inline typename std::vector<Node*>::iterator end() {
      return children.end();
    }

    inline typename std::vector<Node*>::const_iterator begin() const {
      return children.begin();
    }

    inline typename std::vector<Node*>::const_iterator end() const {
      return children.end();
    }

    friend std::ostream& operator<<(std::ostream& os, const Node& ct) {
      Eigen::IOFormat CommaInitFmt(Eigen::StreamPrecision, Eigen::DontAlignCols,
                                   ", ", ", ", "", "", "[", "]");
      os << "(" << ct._p.format(CommaInitFmt) << ":" << ct.level << ":"
         << ct.maxdistUB << ")";
      return os;
    }
  };

 protected:
  Node* root;    // Root of the tree
  int minScale;  // Minimum scale
  int maxScale;  // Minimum scale

  void insert(Node* current, const point& p) {
#ifndef NDEBUG
    if (current->dist(p) > current->covdist(powdict))
      throw std::runtime_error("Internal insert got wrong input!");
#endif

    bool flag = true;
    for (const auto& child : *current) {
      child->tempDist = child->dist(p);
      if (child->tempDist <= child->covdist(powdict)) {
        insert(child, p);
        flag = false;
        break;
      }
    }

    if (flag) {
      current->setChild(p);
      if (minScale > current->level - 1) {
        minScale = current->level - 1;
        // std::cout<< minScale << " " << maxScale << std::endl;
      }
    }
  }

  void insert(Node* current, Node* p) {
#ifndef NDEBUG
    if (current->dist(p) > current->covdist(powdict))
      throw std::runtime_error("Internal insert got wrong input!");
#endif

    bool flag = true;
    for (const auto& child : *current) {
      child->tempDist = child->dist(p);
      if (child->tempDist <= child->covdist(powdict)) {
        insert(child, p);
        flag = false;
        break;
      }
    }

    if (flag) {
      current->setChild(p);
      if (minScale > current->level - 1) {
        minScale = current->level - 1;
        // std::cout<< minScale << " " << maxScale << std::endl;
      }
    }
  }

  Node* nearestNeighbor(Node* current, const point& p, Node* nn) const {
    if (current->tempDist < nn->tempDist) nn = current;

    for (const auto& child : *current) child->tempDist = child->dist(p);

    auto comp_x = [](Node* a, Node* b) { return a->tempDist < b->tempDist; };
    std::sort(current->begin(), current->end(), comp_x);

    for (const auto& child : *current) {
      if (nn->tempDist > child->tempDist - child->maxdistUB)
        nn = nearestNeighbor(child, p, nn);
    }
    return nn;
  }

  Node* nearestNeighborMulti(Node* current, const point& p, Node* nn) const {
    numeric tempDist = nn->dist(p);
    if (current->dist(p) < tempDist) nn = current;

    for (const auto& child : *current) {
      if (tempDist > child->dist(p) - child->maxdistUB)
        nn = nearestNeighborMulti(child, p, nn);
    }
    return nn;
  }

  void nearNeighbors(Node* current, const point& p,
                     std::vector<Node*>& nnList) const {
    // If the current node is eligible to get into the list
    // TODO: An efficient implementation ?
    auto comp_x = [](Node* a, Node* b) { return a->tempDist < b->tempDist; };

    numeric curDist = current->tempDist;
    numeric bestNow = nnList.back()->tempDist;

    if (curDist < bestNow) {
      nnList.insert(
          std::upper_bound(nnList.begin(), nnList.end(), current, comp_x),
          current);
      nnList.pop_back();
    }

    for (const auto& child : *current) child->tempDist = child->dist(p);
    std::sort(current->begin(), current->end(), comp_x);

    for (const auto& child : *current) {
      if (nnList.back()->tempDist > child->tempDist - child->maxdistUB)
        nearNeighbors(child, p, nnList);
    }
  }

  void nearNeighborsMulti(Node* current, const point& p,
                          std::vector<Node*>& nnList) const {
    // If the current node is eligible to get into the list
    // TODO: An efficient implementation ?
    numeric curDist = current->dist(p);
    numeric bestNow = nnList.back()->dist(p);

    if (curDist < bestNow) {
      auto k = nnList.begin();
      for (; curDist > (*k)->dist(p); ++k)
        ;
      nnList.insert(k, current);
      nnList.pop_back();
    }

    for (const auto& child : *current) {
      if (bestNow > child->dist(p) - child->maxdistUB)
        nearNeighborsMulti(child, p, nnList);
    }
  }

  void rangeNeighbors(Node* current, const point& p, numeric range,
                      std::vector<Node*>& nnList) const {
    // If the current node is eligible to get into the list
    if (current->tempDist < range) nnList.push_back(current);

    // Sort the children
    for (const auto& child : *current) child->tempDist = child->dist(p);
    auto comp_x = [](Node* a, Node* b) { return a->tempDist < b->tempDist; };
    std::sort(current->begin(), current->end(), comp_x);

    for (const auto& child : *current) {
      if (range > child->tempDist - child->maxdistUB)
        rangeNeighbors(child, p, range, nnList);
    }
  }

  void rangeNeighborsMulti(Node* current, const point& p, numeric range,
                           std::vector<Node*>& nnList) const {
    // If the current node is eligible to get into the list
    if (current->dist(p) < range) nnList.push_back(current);

    for (const auto& child : *current) {
      if (range > child->dist(p) - child->maxdistUB)
        rangeNeighborsMulti(child, p, range, nnList);
    }
  }

  std::vector<Node*> mergeHelper(Node* p, Node* q) {
#ifndef NDEBUG
// assert(root->level == ct->root->level);
// assert(root->dist(ct->root) < root->covdist(powdict));
#endif

    std::vector<Node *> sepcov, uncovered, leftovers;
    for (const auto& r : *q) {
      if (p->dist(r) < p->covdist(powdict)) {
        bool flag = true;
        for (const auto& s : *p) {
          if (s->dist(r) <= s->covdist(powdict)) {
            std::vector<Node*> leftoverss = mergeHelper(s, r);
            leftovers.insert(leftovers.end(), leftoverss.begin(),
                             leftoverss.end());
            flag = false;
            break;
          }
        }

        if (flag) {
          sepcov.push_back(r);
        }
      } else {
        uncovered.push_back(r);
      }
    }

    // children ← children ∪ sepcov
    for (const auto& s : sepcov) p->children.push_back(s);

    insert(p, q->_p);
    delete q;

    for (const auto& r : leftovers) {
      if (p->dist(r) <= p->covdist(powdict))
        insert(p, r);
      else
        uncovered.push_back(r);
    }

    return uncovered;
  }

  static void clear(Node* current) {
    std::stack<Node*> travel;

    travel.push(current);
    while (travel.size() > 0) {
      current = travel.top();
      travel.pop();

      for (const auto& child : *current) travel.push(child);

      delete current;
    }
  }

 public:
  // Inserting a point
  void insert(const point& p) {
    if (root->dist(p) > root->covdist(powdict)) {
      while (root->dist(p) > 2 * root->covdist(powdict)) {
        Node* current = root;
        Node* parent = NULL;
        while (current->children.size() > 0) {
          parent = current;
          current = current->children.back();
        }
        if (parent != NULL) {
          parent->children.pop_back();
          current->level = root->level + 1;
          current->children.push_back(root);
          root = current;
        } else {
          root->level += 1;
        }
      }
      Node* temp = new Node;
      temp->_p = p;
      temp->level = root->level + 1;
      temp->children.push_back(root);
      root = temp;
      maxScale = root->level;
      // std::cout << "Upward: " <<  minScale << " " << maxScale << std::endl;
    } else {
      root->tempDist = root->dist(p);
      insert(root, p);
    }
    return;
  }

  // First the number of nearest neighbor
  point& nearestNeighbor(const point& p) const {
    root->tempDist = root->dist(p);
    return nearestNeighbor(root, p, root)->_p;
  }

  point& nearestNeighborMulti(const point& p) const {
    return nearestNeighborMulti(root, p, root)->_p;
  }

  // Function to obtain the numNbrs nearest neighbors
  std::vector<point> nearNeighbors(const point queryPt, size_t numNbrs) const {
    root->tempDist = root->dist(queryPt);

    // Do the worst initialization
    Node* dummyNode = new Node();
    dummyNode->tempDist = std::numeric_limits<numeric>::max();
    // List of k-nearest points till now
    std::vector<Node*> nnListn(numNbrs, dummyNode);
    nearNeighbors(root, queryPt, nnListn);

    std::vector<point> nnList;
    for (const auto& node : nnListn) nnList.push_back(node->_p);
    return nnList;
  }
  std::vector<point> nearNeighborsMulti(const point queryPt,
                                        size_t numNbrs) const {
    // Do the worst initialization
    Node* dummyNode = new Node();
    dummyNode->_p = std::numeric_limits<numeric>::max() * root->_p;
    dummyNode->tempDist = std::numeric_limits<numeric>::max();
    // List of k-nearest points till now
    std::vector<Node*> nnListn(numNbrs, dummyNode);
    nearNeighborsMulti(root, queryPt, nnListn);

    std::vector<point> nnList;
    for (const auto& node : nnListn) nnList.push_back(node->_p);
    return nnList;
  }

  // Function to get the neighbors around the range
  std::vector<point> rangeNeighbors(const point queryPt, numeric range) const {
    root->tempDist = root->dist(queryPt);
    // List of nearest neighbors in the range
    std::vector<Node*> nnListn;
    rangeNeighbors(root, queryPt, range, nnListn);

    std::vector<point> nnList;
    for (const auto& node : nnListn) nnList.push_back(node->_p);
    return nnList;
  }

  // Function to get the neighbors around the range
  std::vector<point> rangeNeighborsMulti(const point queryPt,
                                         numeric range) const {
    // List of nearest neighbors in the range
    std::vector<Node*> nnListn;
    ;
    rangeNeighborsMulti(root, queryPt, range, nnListn);

    std::vector<point> nnList;
    for (const auto& node : nnListn) nnList.push_back(node->_p);
    return nnList;
  }

  void merge(CoverTree* ct) {
#ifndef NDEBUG
    assert(root->level >= ct->root->level);
#endif

    // Make sure d(p,q) < covdist(p)
    while (root->dist(ct->root) > root->covdist(powdict)) {
      // std::cout << "Inside while 1" << std::endl;
      Node* current = root;
      Node* parent = NULL;
      while (current->children.size() > 0) {
        parent = current;
        current = current->children.back();
      }
      if (parent != NULL) {
        parent->children.pop_back();
        current->level = root->level + 1;
        current->children.push_back(root);
        root = current;
      } else {
        root->level += 1;
      }
    }

    // Make sure level(p) == level(q)
    while (root->level > ct->root->level) {
      // std::cout << "Inside while 2" << std::endl;
      Node* current = ct->root;
      Node* parent = NULL;
      while (current->children.size() > 0) {
        parent = current;
        current = current->children.back();
      }
      if (parent != NULL) {
        parent->children.pop_back();
        current->level = ct->root->level + 1;
        current->children.push_back(ct->root);
        ct->root = current;
      } else {
        ct->root->level += 1;
      }
    }

    std::vector<Node*> leftovers = mergeHelper(root, ct->root);
    for (const auto& p : leftovers) insert(root, p);

    return;
  }

  // contructor: needs atleast 1 point to make a valid covertree
  CoverTree(const point& p, numeric _base = 1.3) : base(_base) {
    for (int i = 0; i < 2048; ++i) {
      powdict[i] = pow(base, i - 1024);
    }

    minScale = 1000;
    maxScale = 0;

    root = new Node;
    root->_p = p;
    root->level = 0;
    root->maxdistUB = 0;
  }

  // contructor: needs atleast 1 point to make a valid covertree
  CoverTree(std::vector<point>& pList, numeric _base = 1.3) : base(_base) {
    for (int i = 0; i < 2048; ++i) {
      powdict[i] = pow(base, i - 1024);
    }

    point temp = pList.back();
    pList.pop_back();

    minScale = 1000;
    maxScale = 0;

    root = new Node;
    root->_p = temp;
    root->level = 0;
    root->maxdistUB = 0;

    int i = 0;
    for (const auto& p : pList) insert(p);

    pList.push_back(temp);

    calc_maxdist();
  }

  // contructor: needs atleast 1 point to make a valid covertree
  CoverTree(std::vector<point>& pList, int begin, int end, numeric _base = 1.3)
      : base(_base) {
    for (int i = 0; i < 2048; ++i) {
      powdict[i] = pow(base, i - 1024);
    }

    point temp = pList[begin];

    minScale = 1000;
    maxScale = 0;

    root = new Node;
    root->_p = temp;
    root->level = 0;
    root->maxdistUB = 0;

    for (int i = begin + 1; i < end; ++i) {
      insert(pList[i]);
    };

    calc_maxdist();
  }

  // destructor: deallocating all memories by a post order traversal
  ~CoverTree() { clear(root); }

  // get root level == max_level
  int get_level() { return root->level; }

  // Debug function
  friend std::ostream& operator<<(std::ostream& os, const CoverTree& ct) {
    Eigen::IOFormat CommaInitFmt(Eigen::StreamPrecision, Eigen::DontAlignCols,
                                 ", ", ", ", "", "", "[", "]");

    std::stack<Node*> travel;
    Node* curNode;

    // Initialize with root
    travel.push(ct.root);

    // Qualititively keep track of number of prints
    int numPrints = 0;
    // Pop, print and then push the children
    while (travel.size() > 0) {
      // if (numPrints > 5000)
      //  throw std::runtime_error(
      //    "Printing stopped prematurely, something wrong!");
      numPrints++;

      // Pop
      curNode = travel.top();
      travel.pop();

      // Print the current -> children pair
      // os << *curNode << std::endl;
      for (const auto& child : *curNode)
        os << *curNode << " -> " << *child << std::endl;

      // Now push the children
      for (int i = curNode->children.size() - 1; i >= 0; --i)
        travel.push(curNode->children[i]);
    }

    return os;
  }

  void update() { return calc_maxdist(); }

  // find true maxdist
  void calc_maxdist() {
    std::vector<Node*> travel;
    std::vector<Node*> active;

    Node* current = root;

    root->maxdistUB = 0;
    travel.push_back(root);
    while (travel.size() > 0) {
      current = travel.back();

      if (current->maxdistUB == 0) {
        while (current->children.size() > 0) {
          active.push_back(current);
          // push the children
          for (int i = current->children.size() - 1; i >= 0; --i) {
            current->children[i]->maxdistUB = 0;
            travel.push_back(current->children[i]);
          }
          current = current->children[0];
        }
      } else
        active.pop_back();

      // find distance with current node
      for (const auto& n : active)
        n->maxdistUB = std::max(n->maxdistUB, n->dist(current));

      // Pop
      travel.pop_back();
    }
  }
};  // class CoverTree

typedef ofxCoverTree::CoverTree<> Default;

template <typename point = ofxCoverTree::item, typename numeric = float>
class ParallelMake {
  int left;
  int right;
  numeric base;
  std::vector<point>& pList;

  CoverTree<point, numeric>* CT;

  void run() { CT = new CoverTree<point, numeric>(pList, left, right, base); }

 public:
  ParallelMake(std::vector<point>& points, numeric base = 1.3)
      : pList(points), base(base) {
    this->left = 0;
    this->right = points.size();
    this->CT = NULL;
  }

  ParallelMake(int left, int right, std::vector<point>& pL, numeric base = 1.3)
      : pList(pL), base(base) {
    this->left = left;
    this->right = right;
    this->CT = NULL;
  }
  ~ParallelMake() {}

  int compute() {
    if (right - left < 50000) {
      run();
      return 0;
    }

    int split = (right - left) / 2;

    ParallelMake<point, numeric>* t1 =
        new ParallelMake<point, numeric>(left, left + split, pList, base);
    ParallelMake<point, numeric>* t2 =
        new ParallelMake<point, numeric>(left + split, right, pList, base);

    auto f1 = async_polyfill(&ParallelMake::compute, t1);
    auto f2 = async_polyfill(&ParallelMake::compute, t2);

    f1.get();
    f2.get();

    if (t1->CT->get_level() > t1->CT->get_level()) {
      t1->CT->merge(t2->CT);
      CT = t1->CT;
    } else {
      t2->CT->merge(t1->CT);
      CT = t2->CT;
    }

    delete t1;
    delete t2;

    return 0;
  }

  CoverTree<point, numeric>* get_result() { return CT; }
};

}  // namespace CoverTree
