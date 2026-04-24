// avl_tree.h - AVL self-balancing BST for indexing.
// key = int (column value), value = rowId (int). Duplicates: store first-match semantics
// for simplicity (can be extended to chain if needed).
#ifndef AVL_TREE_H
#define AVL_TREE_H

struct AVLNode {
    int key;
    int value;        // rowId
    int height;
    AVLNode* left;
    AVLNode* right;
    AVLNode(int k, int v) : key(k), value(v), height(1), left(nullptr), right(nullptr) {}
};

class AVLTree {
    AVLNode* root;
    int nodeCount;

    static int h(AVLNode* n) { return n ? n->height : 0; }
    static int bf(AVLNode* n) { return n ? h(n->left) - h(n->right) : 0; }
    static void updateH(AVLNode* n) {
        int l = h(n->left), r = h(n->right);
        n->height = 1 + (l > r ? l : r);
    }
    AVLNode* rotL(AVLNode* x);
    AVLNode* rotR(AVLNode* y);
    AVLNode* insertRec(AVLNode* n, int key, int value);
    AVLNode* findRec(AVLNode* n, int key) const;
    void freeRec(AVLNode* n);
    void collectInRangeRec(AVLNode* n, int lo, int hi,
                           int*& outIds, int& outCount, int& outCap) const;

public:
    AVLTree();
    ~AVLTree();
    AVLTree(const AVLTree&) = delete;
    AVLTree& operator=(const AVLTree&) = delete;

    void insert(int key, int value);
    // returns rowId or -1 if not found
    int find(int key) const;
    int size() const { return nodeCount; }

    // range scan: returns a freshly-allocated int array of rowIds and its count.
    // caller must delete[] the array.
    int* findInRange(int lo, int hi, int& count) const;
};

#endif
