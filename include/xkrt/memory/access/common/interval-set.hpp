/*
** Copyright 2024,2025 INRIA
**
** Contributors :
** Romain PEREIRA, romain.pereira@inria.fr + rpereira@anl.gov
**
** This software is a computer program whose purpose is to execute
** blas subroutines on multi-GPUs system.
**
** This software is governed by the CeCILL-C license under French law and
** abiding by the rules of distribution of free software.  You can  use,
** modify and/ or redistribute the software under the terms of the CeCILL-C
** license as circulated by CEA, CNRS and INRIA at the following URL
** "http://www.cecill.info".

** As a counterpart to the access to the source code and  rights to copy,
** modify and redistribute granted by the license, users are provided only
** with a limited warranty  and the software's author,  the holder of the
** economic rights,  and the successive licensors  have only  limited
** liability.

** In this respect, the user's attention is drawn to the risks associated
** with loading,  using,  modifying and/or developing or reproducing the
** software by the user in light of its specific status of free software,
** that may mean  that it is complicated to manipulate,  and  that  also
** therefore means  that it is reserved for developers  and  experienced
** professionals having in-depth computer knowledge. Users are therefore
** encouraged to load and test the software's suitability as regards their
** requirements in conditions enabling the security of their systems and/or
** data to be ensured and,  more generally, to use and operate it in the
** same conditions as regards security.

** The fact that you are presently reading this means that you have had
** knowledge of the CeCILL-C license and that you accept its terms.
**/

#ifndef __INTERVAL_SET_HPP__
# define __INTERVAL_SET_HPP__

# include <assert.h>
# include <vector>

# include <xkrt/memory/access/common/interval.hpp>

#ifndef MIN
# define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
#endif

#ifndef MAX
# define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))
#endif

template<typename TYPE>
class IntervalSet
{
    public:
        struct Node
        {
            TYPE start;
            TYPE end;
            Node * left;
            Node * right;
            int height;

            Node(TYPE s, TYPE e) : start(s), end(e), left(nullptr), right(nullptr), height(1) {}
        };

    private:
        Node * root;

        inline void
        free_all(Node * n)
        {
            if (!n) return;
            free_all(n->left);
            free_all(n->right);
            delete n;
        }


    public:
        IntervalSet() : root(nullptr) {}
        ~IntervalSet() { free_all(root); }

    private:
        int  height(Node * n) { return n ? n->height : 0; }
        int balance(Node * n) { return n ? height(n->left) - height(n->right) : 0; }

        inline Node *
        rotate_right(Node * y)
        {
            Node * x = y->left;
            Node * z = x->right;

            x->right = y;
            y->left  = z;

            y->height = MAX(height(y->left), height(y->right)) + 1;
            x->height = MAX(height(x->left), height(x->right)) + 1;

            return x;
        }

        inline Node *
        rotate_left(Node * x)
        {
            Node * y = x->right;
            Node * z = y->left;

            y->left  = x;
            x->right = z;

            x->height = MAX(height(x->left), height(x->right)) + 1;
            y->height = MAX(height(y->left), height(y->right)) + 1;

            return y;
        }

        inline Node * balance_node(Node * n)
        {
            if (!n)
                return n;

            n->height = MAX(height(n->left), height(n->right)) + 1;
            int b = balance(n);

            if (b > 1  && balance(n->left)  >= 0) return rotate_right(n);
            if (b > 1  && balance(n->left)   < 0) { n->left = rotate_left(n->left); return rotate_right(n); }
            if (b < -1 && balance(n->right) <= 0) return rotate_left(n);
            if (b < -1 && balance(n->right)  > 0) { n->right = rotate_right(n->right); return rotate_left(n); }

            return n;
        }

        inline Node * insert(Node * root, TYPE s, TYPE e)
        {
            if (!root)
                return new Node(s, e);

            if (e <= root->start)
                root->left = insert(root->left, s, e);
            else if (s >= root->end)
                root->right = insert(root->right, s, e);
            else
            {
                // should not happen for disjoint insertions
                assert(0);
                return root;
            }

            return balance_node(root);
        }

        inline Node * find_min(Node * n)
        {
            while (n->left)
                n = n->left;
            return n;
        }

        inline Node * remove(Node * root, TYPE s)
        {
            if (!root)
                return root;

            if (s < root->start)
                root->left = remove(root->left, s);
            else if (s > root->start)
                root->right = remove(root->right, s);
            else
            {
                if (!root->left || !root->right)
                {
                    Node* tmp = root->left ? root->left : root->right;
                    delete root;
                    return tmp;
                }
                else
                {
                    Node * tmp = find_min(root->right);
                    root->start = tmp->start;
                    root->end   = tmp->end;
                    root->right = remove(root->right, tmp->start);
                }
            }

            return balance_node(root);
        }

        inline void
        collect_and_merge(Node * & n, TYPE & a, TYPE & b)
        {
            if (!n)
                return;

            if (b <= n->start)
                collect_and_merge(n->left, a, b);
            else if (a >= n->end)
                collect_and_merge(n->right, a, b);
            else
            {
                a = MIN(a, n->start);
                b = MAX(b, n->end);
                n = remove(n, n->start);
                collect_and_merge(n, a, b);
            }
        }

        inline void
        fill_range(Node * & n, TYPE a, TYPE b)
        {
            collect_and_merge(n, a, b);
            n = insert(n, a, b);
        }

        inline void
        unfill_range(Node * & n, TYPE a, TYPE b)
        {
            if (!n)
                return;

            if (b <= n->start)
                unfill_range(n->left, a, b);
            else if (a >= n->end)
                unfill_range(n->right, a, b);
            else
            {
                TYPE s = n->start;
                TYPE e = n->end;
                n = remove(n, s);
                if (s < a) n = insert(n, s, a);
                if (e > b) n = insert(n, b, e);
                unfill_range(n, a, b);
            }
        }

        inline void
        find_intersect(Node * n, TYPE a, TYPE b, std::vector<Interval> & intervals) const
        {
            if (!n) return;

            // if [a,b) is entirely before this node
            if (b <= n->start)
            {
                find_intersect(n->left, a, b, intervals);
            }
            // if [a,b) is entirely after this node
            else if (a >= n->end)
            {
                find_intersect(n->right, a, b, intervals);
            }
            else
            {
                // they intersect
                intervals.push_back(Interval(n->start, n->end));

                // explore both sides (since overlapping region may cross them)
                find_intersect(n->left, a, b, intervals);
                find_intersect(n->right, a, b, intervals);
            }
        }

    public:
        inline void   fill(TYPE a, TYPE b) { if (a < b)   fill_range(root, a, b); }
        inline void unfill(TYPE a, TYPE b) { if (a < b) unfill_range(root, a, b); }

        // ---- new method ----
        inline void
        find(TYPE a, TYPE b, std::vector<Interval> & intervals) const
        {
            find_intersect(root, a, b, intervals);
        }


};

# endif /* __INTERVAL_SET_HPP__ */
