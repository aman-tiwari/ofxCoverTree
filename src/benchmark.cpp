#include <chrono>
#include <random>
#include <vector>
#include "ofxCoverTree.h"

const size_t DIMS = 512;
std::default_random_engine random_gen(1);

struct my_point : ofxCoverTree::point {
  using ofxCoverTree::point::point;
  std::string name;
};

long long to_ms(std::chrono::high_resolution_clock::time_point ts,
                std::chrono::high_resolution_clock::time_point tn) {
  return std::chrono::duration_cast<std::chrono::milliseconds>(ts - tn).count();
}

void benchmark(float base = 1.3f, size_t N = 10000) {
  using std::chrono::high_resolution_clock;

  std::vector<ofxCoverTree::Item> items;

  for (size_t i = 0; i < N; i++) {
    ofxCoverTree::Item temp(DIMS);
    for (size_t j = 0; j < DIMS; j++) {
      temp[j] = random_gen() * random_gen();
    }
    items.push_back(temp);
  }

  auto c_start = std::chrono::high_resolution_clock::now();

  // ofxCoverTree::Default tree(items, base);

  ofxCoverTree::ParallelMake<> maker(0, items.size(), items);
  maker.compute();
  ofxCoverTree::Default* tree = maker.get_result();
  auto c_end = std::chrono::high_resolution_clock::now();
  std::cout << "Build time for " << N << " points : " << to_ms(c_end, c_start)
            << "ms" << std::endl;

  auto q_start = std::chrono::high_resolution_clock::now();
  for (size_t i = 0; i < 1000; i++) {
    volatile auto z = tree->nearestNeighbor(items[i]);
  }
  auto q_end = std::chrono::high_resolution_clock::now();
  std::cout << "1000 queries took: " << to_ms(q_end, q_start) << "ms"
            << std::endl;
  std::cout << "Avg: " << to_ms(q_end, q_start) / 1000 << "ms" << std::endl
            << std::endl;
  delete tree;
}

int main(int argc, char** argv) {
  ofxCoverTree::Item pt(DIMS);

  ofxCoverTree::Item other_pt(DIMS);

  for (size_t i = 0; i < DIMS; i++) {
    pt[i] = random_gen();
  }

  pt.id = 1;

  for (size_t i = 0; i < DIMS; i++) {
    other_pt[i] = random_gen();
  }

  other_pt.id = 2;

  ofxCoverTree::CoverTree<> tree(pt);

  tree.insert(other_pt);

  std::cout << tree.nearestNeighbor(pt).id << std::endl;

  my_point hello_pt(DIMS);
  hello_pt.name = "hello!";

  for (size_t i = 0; i < DIMS; i++) {
    hello_pt[i] = random_gen();
  }

  my_point bye_pt(DIMS);

  bye_pt.name = "bye!";

  for (size_t i = 0; i < DIMS; i++) {
    bye_pt[i] = random_gen();
  }

  ofxCoverTree::CoverTree<my_point> hello(hello_pt);
  hello.insert(bye_pt);

  std::cout << hello.nearNeighbors(bye_pt, 200)[1].name << std::endl;
  std::cout << hello.nearNeighbors(hello_pt, 200)[1].name << std::endl;

  std::vector<my_point> named_points;
  named_points.push_back(hello_pt);
  named_points.push_back(bye_pt);

  /*ofxCoverTree::ParallelMake<my_point> maker(0, named_points.size(),
                                             named_points);
  maker.compute();

  ofxCoverTree::CoverTree<my_point>* cover_tree = maker.get_result();

  std::cout << cover_tree->nearNeighbors(bye_pt, 200)[1].name << std::endl;
  std::cout << cover_tree->nearNeighbors(hello_pt, 200)[1].name << std::endl;
   */
  for (float base = 1.1; base < 3.0; base += 0.1) {
    std::cout << "base: " << base << std::endl << std::endl;
    benchmark(base);
  }

  return 0;
}
