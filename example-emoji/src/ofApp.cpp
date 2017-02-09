#include <chrono>

#include "ofApp.h"
#include "ofxCoverTree.h"

typedef std::chrono::high_resolution_clock hi_clock;

namespace ch = std::chrono;

// from https://github.com/iamcal/emoji-data/
ofImage emoji_sheet;

std::vector<ofImage> emojis;
std::vector<ofxCoverTree::item> unrolled_emoji;

const ofxCoverTree::Default* cover_tree;

/*
 * image_to_item : unrolls an ofImage into an ofxCoverTree::Item
 */
ofxCoverTree::item image_to_item(const ofImage& img, size_t id) {
    size_t n_dims = img.getPixels().getTotalBytes();
    
    ofxCoverTree::item vec(n_dims);
    // copy unrolled image into float vector
    const unsigned char* img_data = img.getPixels().getData();
    for(size_t i = 0; i < n_dims; i++) {
        vec[i] = img_data[i];
    }
    
    vec.id = id;
    return vec;
}

//--------------------------------------------------------------
void ofApp::setup(){

    const size_t n_emoji = 1620; // from trial & error !
    
    std::vector<std::string> sheets{"sheet_apple_32.png",
                                    "sheet_twitter_32.png",
                                    "sheet_google_32.png",
                                    "sheet_emojione_32.png"};
    
    for(const auto& sheet : sheets) {
        
        emoji_sheet.load(sheet);
        emoji_sheet.update();
        
        size_t n_loaded = 0;
        
        for(size_t x = 0; x < emoji_sheet.getWidth(); x += 32) {
            for(size_t y = 0; y < emoji_sheet.getHeight() && n_loaded < n_emoji; y += 32) {
                ofPixels temp;
                emoji_sheet.getPixels().cropTo(temp, (int)x, (int)y, 32, 32);
                emojis.push_back(ofImage(temp));
                n_loaded++;
            }
        }
    }
    
    for(size_t i = 0; i < emojis.size(); i++) {
        const auto& emoji = emojis[i];
        // each of these items are 32 * 32 * 4 dims = 4096
        unrolled_emoji.push_back(image_to_item(emoji, i));
    }

    auto ts = hi_clock::now();
    
    // 6480 images in total
    cover_tree = new ofxCoverTree::Default(unrolled_emoji);
    
    auto tn = hi_clock::now();
    
    std::cout << "Construction took: "
                << ch::duration_cast<ch::milliseconds>(tn - ts).count()
                << "ms"
                << std::endl;
}

/*
 * brute_force_nearest : brute-force nearest neighbor by linear search & 
 * partial heap-sort
 */
std::vector<ofxCoverTree::item>
brute_force_nearest(const ofxCoverTree::item& search,
                    const std::vector<ofxCoverTree::item>& items, size_t n_neighbors) {
    std::vector<ofxCoverTree::item> result(n_neighbors);
    result.reserve(n_neighbors);
    
    std::partial_sort_copy(
                      items.begin(),
                      items.end(),
                      result.begin(),
                      result.end(),
                      [&](const ofxCoverTree::item& a, const ofxCoverTree::item& b) {
                          return (search - a).norm() < (search - b).norm();
                      });
    
    return result;
    
}

//--------------------------------------------------------------
void ofApp::update(){
    
}

bool brute_force = false;
size_t offset = 0;
size_t n_neighbors = 32;

//--------------------------------------------------------------
void ofApp::draw(){
    ch::duration<double> total(0.0);
    
    for(size_t i = 0; i < 32; i++) {
        const auto& search = unrolled_emoji[(offset + i * 2) % unrolled_emoji.size()];
        auto ts = hi_clock::now();
        std::vector<ofxCoverTree::item> nearest;
        if(brute_force) {
            nearest = brute_force_nearest(search, unrolled_emoji, n_neighbors);
        } else {
            nearest = cover_tree->nearNeighbors(search, n_neighbors);
        }
        auto tn = hi_clock::now();
        total += (tn - ts);
        for(size_t j = 0; j < n_neighbors; j++) {
            emojis[nearest[j].id].draw(j * 32, i * 32);
        }
    }
    
    ofDrawBitmapStringHighlight("32 searches took: " + to_string(total.count()) + " seconds",
                                ofGetWindowWidth() - 300, 30);
    ofDrawBitmapStringHighlight("Searching for " + to_string(n_neighbors) + " neighbors",
                                ofGetWindowWidth() - 300, 50);
    ofDrawBitmapStringHighlight(brute_force ? "Using brute force search" : "Using cover tree search",
                                ofGetWindowWidth() - 300, 70);
}
//--------------------------------------------------------------
void ofApp::keyPressed(int key){
    offset = (size_t)std::abs(ofRandom(unrolled_emoji.size()));

    if(key == 'b') {
        brute_force = !brute_force;
    } else if (key == OF_KEY_UP) {
        n_neighbors++;
        n_neighbors %= unrolled_emoji.size();
    } else if (key == OF_KEY_DOWN) {
        n_neighbors--;
        n_neighbors %= unrolled_emoji.size();
    }
    
}

//--------------------------------------------------------------
void ofApp::keyReleased(int key){
    
}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y ){
    
}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button){
    
}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button){
    
}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button){
    
}

//--------------------------------------------------------------
void ofApp::mouseEntered(int x, int y){
    
}

//--------------------------------------------------------------
void ofApp::mouseExited(int x, int y){
    
}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h){
    
}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg){
    
}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo){
    
}
