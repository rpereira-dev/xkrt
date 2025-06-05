# define KHP_TREE_REBALANCE         0
# define KHP_TREE_CUT_ON_INSERT     0
# define KHP_TREE_MAINTAIN_SIZE     1
# define KHP_TREE_MAINTAIN_HEIGHT   1

# include <xkrt/memory/access/common/khp-tree.hpp>
# define unused_type_t  int

static int next_id = 0;

template <int K>
class NoopKHPTreeNode : public KHPTree<K, unused_type_t>::Node
{
    public:
        using Base = typename KHPTree<K, unused_type_t>::Node;

    public:
        int id;

    public:
        NoopKHPTreeNode(
            const Hypercube & r,
            const int k,
            const Color color
        ) :
            Base(r, k, color),
            id(next_id++)
        {}

        void
        dump_str(FILE * f) const
        {
            fprintf(f, "%d", this->id);
        }

        void
        dump_hypercube_str(FILE * f) const
        {
            fprintf(f, "%d", this->id);
        }

};

template<int K>
class NoopKHPTree : public KHPTree<K, unused_type_t>
{
    public:
        using Hypercube = KHypercube<K>;
        using Node      = NoopKHPTreeNode<K>;
        using NodeBase  = typename Node::Base;

    Node *
    new_node(
    unused_type_t & t,
        const Hypercube & h,
        const int k,
        const Color color
    ) const {
        return new Node(h, k, color);
    }

    Node *
    new_node(
        unused_type_t & t,
        const Hypercube & h,
        const int k,
        const Color color,
        const NodeBase * inherit
    ) const {
        return new Node(h, k, color);
    }

    bool
    should_cut(unused_type_t & t, Hypercube & h, NodeBase * parent, int k) const
    {
        return false;
    }

    void
    on_insert(NodeBase * node, unused_type_t & t)
    {
    }

    void
    on_shrink(NodeBase * node, const Interval & interval, int k)
    {
    }

    bool
    intersect_stop_test(NodeBase * node, unused_type_t & t, const Hypercube & h) const
    {
        return false;
    }

    void
    on_intersect(NodeBase * node, unused_type_t & t, const Hypercube & h) const
    {
    }
};
