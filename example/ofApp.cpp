#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::setup(){
    
    Eigen::Matrix< double, 2, 2 > matA;
    
    matA <<
    1, 2,
    1, 0;
    
    Eigen::EigenSolver< Eigen::Matrix< double, 2, 2 > > solver( matA );
    
    cout << "Matrix: " << endl << matA << endl << endl;
    cout << "Eigen Values: " << endl << solver.eigenvalues() << endl << endl;
    cout << "Eigen Vectors: " << endl << solver.eigenvectors() << endl << endl;
    
}

//--------------------------------------------------------------
void ofApp::update(){

}

//--------------------------------------------------------------
void ofApp::draw(){

}

//--------------------------------------------------------------
void ofApp::keyPressed(int key){

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
