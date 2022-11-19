#ifndef RB_TREE_H
#define RB_TREE_H

namespace ds {
enum color_t
{
    RED,
    BLACK
};
enum direction_t
{
    LEFT,
    RIGHT
};
enum orientation_t
{
    STRAIGHT,
    ANGLED
};

/**
 * Provide the opposite of a given direction.
 * @param dir LEFT or RIGHT.
 * @return RIGHT if given LEFT, and LEFT if given RIGHT.
 */
static direction_t
InvertDir(direction_t dir)
{
    return dir == LEFT ? RIGHT : LEFT;
}

template <typename key_t, typename val_t>
class RbTree
{
public:
    /**
     * Default RB tree constructor, just make sure members are 0ed out.
     */
    RbTree() : root_(nullptr), num_keys_(0)
    {}

    /**
     * Copy constructor, create copy of root.
     * @param rhs RB tree to copy from.
     */
    RbTree(const RbTree& rhs)
    {
        root_ = new RbNode;
        if (rhs.root_) {
            *root_ = *rhs.root_;
        }
        num_keys_ = rhs.num_keys_;
    }

    /**
     * Copy assignment, copy RH's root into this.
     * @param rhs
     * @return
     */
    RbTree& operator=(const RbTree& rhs)
    {
        if (rhs == this) {
            return *this;
        }
        if (rhs.root_) {
            *root_ = rhs.root_;
        } else {
            root_ = nullptr;
        }
        num_keys_ = rhs.num_keys_;
    }

    /**
     * Destructor, cleanup recursive pointer structure rooted at root_.
     */
    ~RbTree()
    {
        delete root_;
    }

    /**
     * Insert a new key-val pair and perform rebalancing ops.
     * @param key Key to insert.
     * @param val Value to insert.
     * @return True if insertion succeeded, false if key already existed in tree.
     */
    bool Insert(const key_t& key, const val_t& val)
    {
        if (root_) {
            RbNode* new_node = root_->BstInsert(key, val);
            if (! new_node)
                return false;

            PostInsertFix(new_node);
        } else {
            root_ = new RbNode(key, val, BLACK, nullptr, nullptr, nullptr);
            PostInsertFix(root_);
        }
        ++num_keys_;
        return true;
    }

    /**
     * Retrieve the value of a given key.
     * @param key Key to lookup.
     * @param val_out A reference which will be set to the value of the key,
     *                if it exists in the tree.
     * @return True if the retrieval was successful, false if the key is not
     *         present within the tree.
     */
    bool Read(const key_t& key, val_t& val_out)
    {
        RbNode* key_node = root_->Find(key);
        if (! key_node)
            return false;
        val_out = key_node->val_;
        return true;
    }

    /**
     * Update the value of a key to a new value.
     * @param key Key whose value will be updated.
     * @param new_val New value of key.
     * @return True if update was successful, false if key was not found in tree.
     */
    bool Update(const key_t& key, const val_t& new_val)
    {
        RbNode* key_node = root_->Find(key);
        if (! key_node)
            return false;
        key_node->val_ = new_val;
        return true;
    }

    /**
     * Delete a key and perform necessary rebalancing operations.
     * @param key Key to delete.
     * @return True if key was successfully deleted, false if it did not exist
     *         in tree to begin with.
     */
    bool Delete(const key_t& key)
    {
        RbNode* node_to_delete = root_->BstDelete(key);
        if (! node_to_delete) {
            return false;
        } else if (node_to_delete == root_) {
            delete root_;
            root_ = nullptr;
        } else {
            // Rebalancing is only necessary when deleting black nodes.
            if (node_to_delete->color_ == BLACK) {
                PreDeletionFix(node_to_delete);
            }

            if (node_to_delete->GetDir() == LEFT) {
                node_to_delete->parent_->left_ = nullptr;
            } else {
                node_to_delete->parent_->right_ = nullptr;
            }
            delete node_to_delete;
        }
        --num_keys_;
        return true;
    }

    /**
     * Print the tree, assuming it's non-empty.
     */
    void Print()
    {
        if (root_) {
            root_->PrintHelper("", false);
        }
    }

private:
    class RbNode;
    RbNode* root_;
    size_t  num_keys_;

    /**
     * Following an insertion, perform an operation to restore the red-black
     * properties (i.e. rebalance the tree).
     * @param new_node The newly inserted node. At start of function, this will
     *                 violate at most one property; either it will be a red
     *                 root or it will be a red child of a red node. Tree will
     *                 otherwise be valid.
     */
    void PostInsertFix(RbNode* new_node)
    {
        RbNode* problem_node = new_node;

        // Loop invariant gives us a few things for free; since parent is red,
        // it can't be root, therefore grandparent always exists for duration
        // of loop. The loop invariant also guarantees that the violation owes
        // to "problem_node" being a red child of a red child; the only other
        // possible violation, a red root, would skip past the loop.
        while (problem_node->parent_ && problem_node->parent_->color_ == RED) {
            RbNode* uncle       = problem_node->GetUncle();
            color_t uncle_color = uncle ? uncle->color_ : BLACK;

            // Case 1: Red uncle. Here, just color parent/uncle black. This does
            // not affect black height of subtree, but it may propagate the
            // double-red violation upwards to the grandparent, so set
            // problem_node to grandparent and continue.
            if (uncle_color == RED) {
                problem_node->parent_->color_          = BLACK;
                uncle->color_                          = BLACK;
                problem_node->parent_->parent_->color_ = RED;
                problem_node = problem_node->parent_->parent_;
            } else if (uncle_color == BLACK) {
                // Case 2: Problem node is angled and uncle is black. Solution
                // is to straighten out the bend (i.e. make problem node left
                // child of left child or right child of right child) by
                // rotating the parent away from the problem node; this will
                // make a straight line from GRANDPARENT -> OLD PROBLEM NODE ->
                // OLD PARENT. Old parent becomes problem node, since it is a
                // red child of a red child. But, since it's straight, we lead
                // directly into case 3 (which is terminal).
                if (problem_node->GetOrientation() == ANGLED) {
                    direction_t opp_dir = InvertDir(problem_node->GetDir());
                    problem_node        = problem_node->parent_;
                    Rotate(opp_dir, problem_node);
                }

                // Case 3: Problem node is straight and uncle is black. By
                // coloring parent black and rotating grandparent away from it,
                // the black parent becomes the new root of the subtree. Here,
                // we know that there are no double-red violations in subtree,
                // and we know that the subtree root cannot violate the double
                // red property (since it is black). Hence, this case is
                // terminal.
                direction_t parent_dir        = problem_node->parent_->GetDir();
                direction_t parent_opp_dir    = InvertDir(parent_dir);
                problem_node->parent_->color_ = BLACK;
                problem_node->parent_->parent_->color_ = RED;
                Rotate(parent_opp_dir, problem_node->parent_->parent_);
            }
        }
        root_->color_ = BLACK;
    }

    /**
     * Restore red-black properties prior to deletion.
     * @param node The leaf node which will be removed following this function.
     */
    void PreDeletionFix(RbNode* node)
    {
        // Problems only arise if node to delete is black. Removing red node
        // won't cause a red node to become root, since the root had to be black
        // beforehand; it won't cause a black height violation, since red nodes
        // have no effect on black height; it won't cause a red node to have a
        // red child, since no parent or child of the removed node can be red
        // to begin with; and it won't cause leaves to be black. So, red node
        // removal won't violate any of the red-black tree properties. We also
        // know that removing root won't cause issues, since it will just leave
        // the tree empty (which is valid).
        RbNode* problem_node = node;
        while (problem_node != root_ && problem_node->color_ == BLACK) {
            // We know that a sibling exists, because we're a black node in the
            // other subtree of its parent; if this sibling didn't exist, there
            // would be a black height violation. Nephews may be NIL, though.
            RbNode* sibling      = problem_node->GetSibling();
            RbNode* inner_nephew = problem_node->GetNephew(ANGLED);
            RbNode* outer_nephew = problem_node->GetNephew(STRAIGHT);

            // Account for fact that leaves (NILs) are black.
            color_t inner_nephew_color =
                inner_nephew ? inner_nephew->color_ : BLACK;
            color_t outer_nephew_color =
                outer_nephew ? outer_nephew->color_ : BLACK;

            if (sibling->color_ == BLACK) {
                // Case 1: Here, we color the sibling red. This means that the
                // black height of the subtree opposite problem_node is reduced
                // by one, which is desirable, because we are removing a black
                // node from problem_node's subtree. However, this may disrupt
                // the black height of the subtree rooted at the problem node's
                // parent; so, the problem node's parent becomes the new problem
                // node. Can lead to any other case, including itself; worst
                // case is just a string of case 1s recursing up the tree until
                // reaching the root, which is O(lg(n)).
                if (inner_nephew_color == BLACK &&
                    outer_nephew_color == BLACK) {
                    sibling->color_ = RED;
                    problem_node    = problem_node->parent_;
                }

                // Case 2: We know that case 3 (see below) is immediately term-
                // inal, so it is desirable to transition to case 3. To do this,
                // simply set sibling red and rotate it to become the outer
                // node. Since inner nephew (which becomes sibling post-rotation) is red, its children must be black, so the rotation leaves a black inner nephew and red outer nephew with a black parent. Note that these rotations maintain black height throughout sibling's subtree. Since this goes to case 3, which is term- inal, case 2 is O(1).
                else if (inner_nephew_color == RED &&
                         outer_nephew_color == BLACK) {
                    sibling->color_      = RED;
                    inner_nephew->color_ = BLACK;
                    Rotate(sibling->GetDir(), sibling);
                }

                // Case 3: Immediately terminal. We know that problem_node is
                // either a black node to be deleted or its ancestor; hence, the
                // subtree rooted at problem_node has a black height of 1 less
                // than it should. By rotating the problem node's parent into
                // the subtree and coloring it black, we increase the black
                // height of that subtree by 1. With the rotation, the inner
                // nephew will become a sibling of the problem node. This is
                // desirable, because we know that the subtree rooted at the
                // problem node has a black height of 1 less than it should, so
                // its black height is thus equivalent to that of a node one
                // level down (e.g. its nephew). Because the outer nephew is
                // red, we can simply color it black in order to maintain the
                // height of the subtree opposite the problem node. And, by
                // setting the problem node's former sibling to the color of its
                // former parent, we ensure that the subtree itself maintains
                // its black height. Thus, black height property is fully
                // restored and we can break. O(1) by virtue of being terminal.
                else if (outer_nephew_color == RED) {
                    sibling->color_ = problem_node->parent_->color_;
                    problem_node->parent_->color_ = BLACK;
                    outer_nephew->color_          = BLACK;
                    Rotate(problem_node->GetDir(), problem_node->parent_);
                    problem_node = root_;
                }
            }
            // Case 4: Sibling being red implies parent and nephews are black.
            // This means that, after rotating parent towards problem node
            // (leaving former sibling as root of subtree and inner nephew as
            // new sibling of problem node), sibling becomes black. This is
            // desirable, because it prevents indefinite looping in case 4.
            // All other cases are O(1) or O(lg(n)), so this means case 4 will
            // transition to a situation which is at worst O(lg(n)).
            else if (sibling->color_ == RED) {
                sibling->color_               = BLACK;
                problem_node->parent_->color_ = RED;
                Rotate(problem_node->GetDir(), problem_node->parent_);
            }
        }
        problem_node->color_ = BLACK;
    }

    /**
     * Rotation. Make "node" into "dir" child of its !"dir" child. E.g. A left
     * rotation of a node makes it into the left child of its right child. The
     * inner grandchild of the rotated node becomes its new inner child, which
     * is valid by transitive property.
     * @param dir Direction of rotation.
     * @param node The node to rotate (i.e. one which will be lowered in tree).
     */
    void Rotate(direction_t dir, RbNode* node)
    {
        if (dir == LEFT) {
            RbNode* new_par = node->right_;
            node->right_    = new_par->left_;
            if (node->right_) {
                node->right_->parent_ = node;
            }
            new_par->parent_ = node->parent_;
            if (! node->parent_) {
                root_ = new_par;
            } else if (node->GetDir() == LEFT) {
                node->parent_->left_ = new_par;
            } else {
                node->parent_->right_ = new_par;
            }
            new_par->left_ = node;
            node->parent_  = new_par;
        } else {
            RbNode* new_par = node->left_;
            node->left_     = new_par->right_;
            if (node->left_) {
                node->left_->parent_ = node;
            }
            new_par->parent_ = node->parent_;
            if (! node->parent_) {
                root_ = new_par;
            } else if (node->GetDir() == RIGHT) {
                node->parent_->right_ = new_par;
            } else {
                node->parent_->left_ = new_par;
            }
            new_par->right_ = node;
            node->parent_   = new_par;
        }

        Print();
    }
};

template <typename key_t, typename val_t>
class RbTree<key_t, val_t>::RbNode
{
public:
    /**
     * Constructor for RB node.
     * @param key Key of new node.
     * @param val Value of new node.
     * @param color Color (red or black only) of new node.
     * @param left The left child of this node.
     * @param right The right child of this node.
     * @param parent The parent of this node.
     */
    RbNode(const key_t& key,
           const val_t& val,
           color_t      color,
           RbNode*      left,
           RbNode*      right,
           RbNode*      parent)
        : key_(key)
        , val_(val)
        , color_(color)
        , left_(left)
        , right_(right)
        , parent_(parent)
    {}

    /**
     * Destructor, delete left and right children.
     */
    ~RbNode()
    {
        delete left_;
        delete right_;
    }

    /**
     * Copy constructor, make copies of all children.
     * @param rhs The node to copy.
     */
    RbNode(const RbNode& rhs)
    {
        if (rhs.left_) {
            left_ = new RbNode(*rhs.left_);
        }
        if (rhs.right_) {
            right_ = new RbNode(*rhs.right_);
        }
        key_   = rhs.key_;
        val_   = rhs.val_;
        color_ = rhs.color_;
    }

    /**
     * Copy assignment operator, make copies of another tree's children and
     * store them in this node.
     * @param rhs The node to copy from.
     * @return *this.
     */
    RbNode& operator=(const RbNode& rhs)
    {
        if (&rhs == this) {
            return *this;
        }

        if (rhs.left_) {
            *left_ = *rhs.left_;
        } else {
            left_ = nullptr;
        }

        if (rhs.right_) {
            *right_ = *rhs.right_;
        } else {
            right_ = nullptr;
        }

        if (rhs.parent_) {
            *parent_ = *rhs.parent_;
        } else {
            parent_ = nullptr;
        }

        key_   = rhs.key_;
        val_   = rhs.val_;
        color_ = rhs.color_;
        return *this;
    }

    /**
     * Standard BST insertion without rebalancing (which is implemented by tree).
     * @param key Key to insert.
     * @param val Val to insert.
     * @return The newly created node which has been inserted into this (sub)tree or nullptr if the key already exists.
     */
    RbNode* BstInsert(const key_t& key, const val_t& val)
    {
        if (key < key_) {
            if (left_) {
                return left_->BstInsert(key, val);
            } else {
                left_ = new RbNode(key, val, RED, nullptr, nullptr, this);
                return left_;
            }
        } else if (key > key_) {
            if (right_) {
                return right_->BstInsert(key, val);
            } else {
                right_ = new RbNode(key, val, RED, nullptr, nullptr, this);
                return right_;
            }
        } else {
            return nullptr;
        }
    }

    /**
     * Standard BST deletion without rebalancing (implemented by tree class).
     * Find key to delete; if its node is internal, transplant with in-order pred or succ; then return the leaf node to delete.
     * @param key Key to delete.
     * @return If the key is held at a leaf node, the leaf node containing the
     *         key; otherwise, the in-order-successor or predecessor of the node
     *         (post-transplant), which will be deleted in its stead.
     */
    RbNode* BstDelete(const key_t& key)
    {
        RbNode* node = Find(key);
        if (! node) {
            return nullptr;
        }

        if (node->left_) {
            // In-order pred is rightmost key in left subtree.
            RbNode* in_order_pred = node->left_->Max();
            node->key_            = in_order_pred->key_;
            node->val_            = in_order_pred->val_;
            return in_order_pred;
        } else if (node->right_) {
            // In-order succ is leftmost key in right subtree.
            RbNode* in_order_succ = node->right_->Min();
            node->key_            = in_order_succ->key_;
            node->val_            = in_order_succ->val_;
            return in_order_succ;
        }

        return node;
    }

    /**
     * Return a pointer to the node containing a key, if one exists.
     * @param key Key to lookup.
     * @return Ptr to node holding key if one exists, else nullptr.
     */
    RbNode* Find(const key_t& key)
    {
        if (key < key_) {
            return left_ ? left_->Find(key) : nullptr;
        } else if (key > key_) {
            return right_ ? right_->Find(key) : nullptr;
        }
        return this;
    }

    /**
     * Return the node containing the smallest key in a subtree (i.e. leftmost
     * node).
     * @return Leftmost node in subtree.
     */
    RbNode* Min()
    {
        if (! left_) {
            return this;
        }
        return left_->Min();
    }

    /**
     * Return the node containing the largest key in a subtree (i.e. rightmost
     * node).
     * @return Rightmost node in subtree.
     */
    RbNode* Max()
    {
        if (! right_) {
            return this;
        }
        return right_->Max();
    }

    /**
     * Is this node a left child or a right child?
     * @return Direction enum.
     */
    direction_t GetDir()
    {
        return this == parent_->left_ ? LEFT : RIGHT;
    }

    /**
     * Does the path from this node's grandparent to parent to this node form
     * a straight line (STRAIGHT) or have a bend (ANGLED)?
     * @return STRAIGHT if this node is the left child of a left child or right
     *         child of a right child. ANGLED if it is the right child or a left
     *         child or the left child or a right child.
     */
    orientation_t GetOrientation()
    {
        if (parent_->GetDir() == GetDir()) {
            return STRAIGHT;
        }
        return ANGLED;
    }

    /**
     * Get the left or right child of this node, depending on direction given.
     * @param dir Either left or right.
     * @return Ptr to left or right child, depending on given direction.
     */
    RbNode* GetChild(direction_t dir)
    {
        if (dir == LEFT) {
            return left_;
        }
        return right_;
    }

    /**
     * Get the uncle of this node.
     * @return A pointer to the sibling to this node's parent, if one exists.
     */
    RbNode* GetUncle()
    {
        if (! parent_ || ! parent_->parent_) {
            return nullptr;
        }
        return parent_->GetDir() == LEFT ? parent_->parent_->right_
                                         : parent_->parent_->left_;
    }

    /**
     * Get the sibling of this node.
     * @return A pointer to the sibling of this node.
     */
    RbNode* GetSibling()
    {
        if (! parent_) {
            return nullptr;
        }
        direction_t sibling_dir = InvertDir(GetDir());
        return parent_->GetChild(sibling_dir);
    }

    /**
     * Get the inner or outer nephew depending on orientation given.
     * @param orientation Either angled or straight.
     * @return If STRAIGHT, give the outer nephew (i.e. the one for which a
     *         straight path exists from grandparent to parent to itself. If
     *         ANGLED, give the inner nephew.
     */
    RbNode* GetNephew(orientation_t orientation)
    {
        RbNode* sibling = GetSibling();
        if (! sibling) {
            return nullptr;
        }

        direction_t dir;
        if (orientation == STRAIGHT) {
            dir = sibling->GetDir();
        } else {
            dir = InvertDir(sibling->GetDir());
        }
        return sibling->GetChild(dir);
    }

    /**
     * Recursive subroutine to print the subtree rooted at this node.
     * @param prefix The prefix to be printed before the value of this node.
     * @param is_left Is this node a left node or a right node?
     */
    void PrintHelper(const char* const prefix, bool is_left)
    {
        /*
         * I'm commenting this out for now, since it'd take a bit more
         * maintenance to get this working with existing logging fetaures.
         * Once we implement a string.h, it should work fine.
         */

        // Log("%s", prefix);
        // Log(is_left ? "├──" : "└──");
        //// Lazy hack, use generic kernel to-string method when you get around to / writing one.
        // Log("%d [%s]\n", val_, color_ == RED ? "R" : "B");

        //// Leave extra chars for add-on to current prefix, 1 for null char.
        // char *new_prefix = (char*) malloc(strlen(prefix) + 32 + 1);
        // strcpy(new_prefix, prefix);
        // strcat(new_prefix, is_left ? "│   " : "    ");

        // if(left_) {
        //     left_->PrintHelper(new_prefix, true);
        // } if(right_) {
        //     right_->PrintHelper(new_prefix, false);
        // }

        // free(new_prefix);
    }

    key_t   key_;
    val_t   val_;
    color_t color_;
    RbNode *left_, *right_, *parent_;
};

}
#endif
