// avl_tree.cpp
#include "avl_tree.h"

AVLTree::AVLTree() : root(nullptr), nodeCount(0) {}
AVLTree::~AVLTree() { freeRec(root); }

void AVLTree::freeRec(AVLNode* n) {
    if (!n) return;
    freeRec(n->left);
    freeRec(n->right);
    delete n;
}

// left rotation around x  (ASCII:  x right-child y, y right-child z)
AVLNode* AVLTree::rotL(AVLNode* x) {
    AVLNode* y = x->right;
    AVLNode* T2 = y->left;
    y->left = x;
    x->right = T2;
    updateH(x);
    updateH(y);
    return y;
}

// right rotation around y  (ASCII:  y left-child x, x children T1 T2)
AVLNode* AVLTree::rotR(AVLNode* y) {
    AVLNode* x = y->left;
    AVLNode* T2 = x->right;
    x->right = y;
    y->left = T2;
    updateH(y);
    updateH(x);
    return x;
}

AVLNode* AVLTree::insertRec(AVLNode* n, int key, int value) {
    if (!n) { nodeCount++; return new AVLNode(key, value); }
    if (key < n->key)       n->left  = insertRec(n->left,  key, value);
    else if (key > n->key)  n->right = insertRec(n->right, key, value);
    else return n;   // duplicate key: ignore (keep first)

    updateH(n);
    int b = bf(n);

    // Left-Left
    if (b > 1 && key < n->left->key)  return rotR(n);
    // Right-Right
    if (b < -1 && key > n->right->key) return rotL(n);
    // Left-Right
    if (b > 1 && key > n->left->key) {
        n->left = rotL(n->left);
        return rotR(n);
    }
    // Right-Left
    if (b < -1 && key < n->right->key) {
        n->right = rotR(n->right);
        return rotL(n);
    }
    return n;
}

void AVLTree::insert(int key, int value) {
    root = insertRec(root, key, value);
}

AVLNode* AVLTree::findRec(AVLNode* n, int key) const {
    while (n) {
        if (key == n->key) return n;
        if (key < n->key) n = n->left;
        else n = n->right;
    }
    return nullptr;
}

int AVLTree::find(int key) const {
    AVLNode* f = findRec(root, key);
    return f ? f->value : -1;
}

void AVLTree::collectInRangeRec(AVLNode* n, int lo, int hi,
                                int*& outIds, int& outCount, int& outCap) const {
    if (!n) return;
    if (n->key > lo) collectInRangeRec(n->left, lo, hi, outIds, outCount, outCap);
    if (n->key >= lo && n->key <= hi) {
        if (outCount == outCap) {
            int nc = outCap ? outCap * 2 : 16;
            int* tmp = new int[nc];
            for (int i = 0; i < outCount; i++) tmp[i] = outIds[i];
            delete[] outIds; outIds = tmp; outCap = nc;
        }
        outIds[outCount++] = n->value;
    }
    if (n->key < hi) collectInRangeRec(n->right, lo, hi, outIds, outCount, outCap);
}

int* AVLTree::findInRange(int lo, int hi, int& count) const {
    int* ids = new int[16];
    int cap = 16; count = 0;
    collectInRangeRec(root, lo, hi, ids, count, cap);
    return ids;
}
