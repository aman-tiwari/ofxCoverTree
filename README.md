ofxCoverTree
=====================================

Introduction
------------
A [cover tree](https://en.wikipedia.org/wiki/Cover_tree) is a data-structure for efficiently finding nearest neighbors between points in a high dimensional space.

ofxCoverTree makes it easy to use [CoverTree](https://github.com/manzilzaheer/CoverTree/) in openFrameworks.

#####All credit for the actual core implementation of the cover tree goes to the original authors, my part was to clean up and document the API, and restructuring the code to make it easier to use.

This project uses no openframeworks specific code, but the folder structure is so as to be easily used as an openframeworks addon.

This implementation supports parallel tree construction and multiple concurrent readers (but only a single writer) using the `Multi` suffixed methods. 

Performance when compiled in release mode is often 10-100x greater than in debug mode.

Usage
------------
####Initialisation

```c++
#include "ofxCoverTree.h"

size_t dims = 128;
	
ofxCoverTree:item item_1(dims);
ofxCoverTree:item item_2(dims);
	
/* fill in item_1 and item_2 */
for(size_t i = 0; i < dims; i++) {
	item_1[i] = random();
	item_2[i] = random();
}
	
item_1.id = 1;
item_2.id = 2;
	
/* Need atleast 1 point to construct the tree */
ofxCoverTree::Default tree(item_1);
	
/* insert more items into the tree */
tree.insert(item_2);
	
/* alternatively, can construct from a vector of points */
std::vector<ofxCoverTree:item> items;
items.push_back(item_1); items.push_back(item_2);
	
ofxCoverTree::Default plant(items);

/* if the cover tree isn't performing well enough, 
 * or taking too long to build, try 
 * tuning the base parameter */
ofxCoverTree::Default plant_2(items, 2.0);

```
Efficient tree building sometimes requires tuning the `base` parameter in the constructor. I am not entirely sure what determines the optimal value to use, but it depends on the distribution and fractal dimension of the data used. Generally values in the range `1.3` to `2.5` should be used. 

#### K-Nearest Neighbors
```c++
ofxCoverTree::item search_item(dims);
for(size_t i = 0; i < dims; i++) {
	search_item[i] = i / 0.5;
}

/* finds the 2 nearest neighbors */
auto results = tree.nearNeighbors(search_item, 2);

/* can search for more items than in tree without crashing */
std::vector<ofxCoverTree::item> two_results = tree.nearNeighbors(search_item, 200);
```

#### Neighbors within a radius
```c++
/* finds the neighbors within a radius of a point */
auto findings = tree.rangeNeighbors(search_item, 1.5);
```

#### Extending the point class
If you'd like to stuff extra data into the points stored by the
cover tree, you can extend `ofxCoverTree::point` like so:

```c++
struct named_point : ofxCoverTree::point {
	using ofxCoverTree::point::point; // required!
	std::string name;
};

ofxCoverTree::CoverTree<named_point> named_point_tree(...);
```
`named_point_tree` will then store and return `named_point`s.

_NB: `ofxCoverTree::point` is just `Eigen::VectorXd`_

##### Parallel construction
This uses a work-stealing fork-join algorithm to construct the cover tree in parallel. 

```c++
std::vector<ofxCoverTree::item> points = ...
ofxCoverTree::ParallelMake<> maker(0, points.size(), points);
maker.compute();

ofxCoverTree::Default* cover_tree = maker.get_result();
```

### Full API
In the following api, `point` and `numeric` refer to the template parameters of `ofxCoverTree::CoverTree`, representing the points store and the type used for numeric calculations (by default, `ofxCoverTree::item` and `float` respectively), i.e:

```
template <typename point = ofxCoverTree::item, typename numeric = float>
class CoverTree {
...
}
```

#####`ofxCoverTree::CoverTree(const point& p, numeric base = 1.3)`
Constructs a cover tree containing `p` with a base of `base`, with default value of `1.3`.

#####`ofxCoverTree::CoverTree(const point& p, numeric base = 1.3)`
Constructs a cover tree containing `p` with a base of `base`, with default value of `1.3`.

#####`ofxCoverTree::CoverTree(std::vector<point>& pList, numeric base = 1.3)`
Constructs a cover tree containing the points in `pList` with a base of `base`, with default value of `1.3`.

#####`ofxCoverTree::CoverTree(std::vector<point>& pList, int begin, int end, numeric _base = 1.3)`
Constructs a cover tree containing the points in `pList[begin:end]` with a base of `base`, with a default value of `1.3`.

----
#####`ofxCoverTree::Default`
An alias for `ofxCoverTree::CoverTree<ofxCoverTree::item, float>`

----
##### The following are methods of the `ofxCoverTree::CoverTree` class.

#####`point& nearestNeighbour(const point queryPt) const`
Returns the nearest neighbor to `queryPt`.

#####`std::vector<point> nearNeighbors(const point queryPt, int N) const`
Returns the `N` nearest neighbors to `queryPt`.

#####`std::vector<point> rangeNeighbors(const point queryPt, numeric range) const`
Returns the points within `range` of `queryPt`.

#####`void merge(CoverTree* ct)`
Merges `this` with `ct`, resulting in a cover tree containing both sets of points. Using `ct` after merging is undefined behaviour. 

----
#####`ofxCoverTree::ParallelMake<point, numeric>(std::vector<point>& points)`
Initialise the parallel cover tree builder.
The following are methods of the parallel builder:

#####`void compute()`
Builds the cover tree using a work-stealing fork-join. Blocks till the cover tree is built.

#####`ofxCoverTree::CoverTree<point, numeric> get_result()`
Returns the built cover tree.

License
-------
Apache 2.0

Installation
------------
Copy into your addons folder (e.g `git clone https://github.com/aman-tiwari/ofxCoverTree`).

Run `make` in the downloaded folder to create the benchmark.

Dependencies
------------
[Eigen](http://eigen.tuxfamily.org/index.php?title=Main_Page) is used but included under the [libs](libs/) folder. 

Compatibility
------------
At least 0.9.0^, although since no oF specific code is used, it should work with any version of openframeworks that compiles with C++11.

#####C++11 or newer#####
