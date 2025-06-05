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

/**
 *  Keep track of pinned, touched and memory being transfered
 */

#ifndef __REGISTER_MEMORY_TREE_HPP__
# define __REGISTER_MEMORY_TREE_HPP__

# define KHP_TREE_REBALANCE         0
# define KHP_TREE_CUT_ON_INSERT     0
# define KHP_TREE_MAINTAIN_SIZE     0
# define KHP_TREE_MAINTAIN_HEIGHT   0
# include <xkrt/memory/access/common/khp-tree.hpp>

//  TODO : the design of this is terrible with a cyclic ownership with 'xkrt_runtime_t'
//  Redesign me !! This should be fully independent with callbacks that can be parametrized, raised with global device ids

# include <xkrt/logger/logger.h>
# include <xkrt/logger/todo.h>
# include <xkrt/sync/bits.h>
# include <xkrt/sync/lockable.hpp>

# include <xkrt/task/task.hpp>          // this should gtfo

# include <algorithm>  // std::sort
# include <cstdint>
# include <functional>
# include <numeric> // std::iota

/* storage passed when searching in the tree */
template <int K>
class KRegisterTreeNodeSearch {

   public:
       KRegisterTreeNodeSearch() : KRegisterTreeNodeSearch() {}
       virtual ~KRegisterTreeNodeSearch() {}

}; /* KRegisterTreeNodeSearch */

template <int K>
class KRegisterTreeNode : public KHPTree<K, KRegisterTreeNodeSearch<K>>::Node {

    using Base = typename KHPTree<K, KRegisterTreeNodeSearch<K>>::Node;
    using Hypercube = KHypercube<K>;
    using Node = KRegisterTreeNode<K>;
    using Search = KRegisterTreeNodeSearch<K>;

    public:

        typedef struct  state_t
        {
            /* the block is pinned */
            bool pinned     : 1;

            /* the block is being pinned */
            bool pinning    : 1;

            /* the block had been touched */
            bool touched    : 1;

            /* the block is being touched */
            bool touching   : 1;

        }               state_t;

        /* the block state */
        state_t state;

    public:

        /* the cube was never accessed before, create a new node */
        KRegisterTreeNode<K>(
            const Hypercube & r,
            const int k,
            const Color color
        ) :
            Base(r, k, color),
            state{}
        {}

        /**
         * A new node is being created from a split, make it inherit its original node 'src'
         *  - r - the shrinked cube that this is inheriting from
         *  - k - the dimension that got splitted
         *  - color - the node color
         *  - src - the node that got split
         *
         * We have:
         *  U (src->hypercube, r) == the node cube before being shrinked
         *  n (src->hypercube, r) = {} - empty intersection
         */
        KRegisterTreeNode<K>(
            const Hypercube & r,
            const int k,
            const Color color,
            const Node * src
        ) :
            Base(r, k, color),
            state{}
        {
            LOGGER_FATAL("TODO");
        }

    public:

        void
        dump_str(FILE * f) const
        {
            Base::dump_str(f);
        }

        void
        dump_hypercube_str(FILE * f) const
        {
            Base::dump_hypercube_str(f);
        }

}; /* KRegisterTreeNode */

template <int K>
class KRegisterTree : public KHPTree<K, KRegisterTreeNodeSearch<K>>, public Lockable {

    public:
        using Base = KHPTree<K, KRegisterTreeNodeSearch<K>>;
        using Hypercube = KHypercube<K>;
        using Node = KRegisterTreeNode<K>;
        using NodeBase = typename KHPTree<K, KRegisterTreeNodeSearch<K>>::Node;
        using Search = KRegisterTreeNodeSearch<K>;

    public:

        KRegisterTree(void) : Base() {}
        ~KRegisterTree() {}

    public:

        void touched(void);
        void touching(void);
        void pin(void);
        void pinning(void);

        //////////////
        //  INSERT  //
        //////////////

        void
        on_insert(
            NodeBase * nodebase,
            Search & search
        ) {
            (void) nodebase;
            (void) search;
            assert(search.type == Search::Type::INSERTING_BLOCKS);
        }

        /* shrinking on dimension 'k' from 'this->hypercube[k]' to 'interval' */
        void
        on_shrink(
            NodeBase * nodebase,
            const Interval & interval,
            int k
        ) {
            static_assert(K == 2);
            Node * node = reinterpret_cast<Node *>(nodebase);

            assert(k < K);
            assert(node->hypercube[k].includes(interval));

            ///////////////////////
            //  SHRINK HOST VIEW //
            ///////////////////////

            assert(node->hypercube[k].a <= interval.a);
            const INTERVAL_DIFF_TYPE_T da = interval.a - node->hypercube[k].a;

            assert(node->hypercube[k].b >= interval.b);

            // must be aligned on sizeof(type)
            if (k == ACCESS_CUBE_ROW_DIM)
            {
                const INTERVAL_DIFF_TYPE_T db = node->hypercube[k].b - interval.b;
                (void) db;
                assert(da % this->sizeof_type == 0);
                assert(db % this->sizeof_type == 0);
            }

            // shrinked-left, gotta offset the views
            if (da)
            {
                // REPLICATES VIEW
                for (MemoryReplicate & replicate : node->block.replicates)
                {
                    for (memory_allocation_view_id_t i = 0 ; i < replicate.nallocations ; ++i)
                    {
                        MemoryReplicateAllocationView * allocation_view = replicate.allocations[i];
                        const INTERVAL_DIFF_TYPE_T offset = (k == ACCESS_CUBE_ROW_DIM) ? da : (da * allocation_view->view.ld * this->sizeof_type);
                        allocation_view->view.addr += offset;
                        assert(allocation_view->view.addr >= allocation_view->chunk->ptr);
                    }
                }
            }
        }

        //////////////////
        //  INTERSECT   //
        //////////////////
        inline bool
        intersect_stop_test(
            NodeBase * nodebase,
            Search & search,
            const Hypercube & h
        ) const {

            (void) nodebase;
            (void) search;
            (void) h;

            // TODO : can we fasten intersection by keeping track of an included 'coherency' bitmask ?

            return false;
        }

        /**
         * The passed cube is intersecting with 'this'
         */
        inline void
        on_intersect(
            NodeBase * nodebase,
            Search & search,
            const Hypercube & h
        ) const {

            assert(nodebase);
            Node * node = reinterpret_cast<Node *>(nodebase);
            assert(h.intersects(node->hypercube));

            switch (search.type)
            {
                case (Search::Type::SEARCH_FOR_PARTITION):
                {
                    /* intersecting against 'cube' that had been inserted
                     * previously, so 'node' must be a sub-block of 'cube' */
                    assert(h.includes(node->hypercube));
                    search.partition.partites.push_back(Partite(&(node->block), node->hypercube));
                    break ;
                }

                /* search for tasks awaiting on that cube for a given allocation */
                case (Search::Type::SEARCH_AWAITING):
                {
                    const xkrt_device_global_id_bitfield_t devbit = (xkrt_device_global_id_bitfield_t) (1 << search.device_global_id);
                    MemoryReplicate & replicate = node->block.replicates[search.device_global_id];

                    /* this is called when searching for awaiting tasks after completing a fetch.
                     * There must be at least one allocation available (the one that just got fetched...) */
                    assert(replicate.nallocations);

                    /* for each allocation of that block */
                    for (memory_allocation_view_id_t allocation_view_id = 0 ; allocation_view_id < replicate.nallocations ; ++allocation_view_id)
                    {
                        const memory_allocation_view_id_bitfield_t allocbit = (memory_allocation_view_id_bitfield_t) (1 << allocation_view_id);
                        MemoryReplicateAllocationView * allocation_view = replicate.allocations[allocation_view_id];

                        /* if it matches the allocation being searched */
                        if (allocation_view->chunk == search.chunk)
                        {
                            /* move the awaiting tasks */
                            search.awaiting.accesses.insert(
                                search.awaiting.accesses.end(),
                                allocation_view->awaiting.accesses.begin(),
                                allocation_view->awaiting.accesses.end()
                            );
                            allocation_view->awaiting.accesses.clear();

                            /* move awaiting forwards */
                            search.awaiting.forwards.insert(
                                search.awaiting.forwards.end(),
                                allocation_view->awaiting.forwards.begin(),
                                allocation_view->awaiting.forwards.end()
                            );
                            allocation_view->awaiting.forwards.clear();

                            /* this replicate just got fetched and is now coherent */

                            // this assertion is not always true, if coming from
                            // an ACCESS_MODE_W, the data was already set coherent
                            // assert((replicate.coherency & allocbit) == 0);
                            replicate.coherency |= (memory_allocation_view_id_bitfield_t) allocbit;

                            assert(replicate.fetching & allocbit);
                            replicate.fetching &= (memory_allocation_view_id_bitfield_t) ~allocbit;

                            break ;
                        }
                    }

                    /* set device bits */
                    assert(replicate.coherency);
                    node->block.coherency |= devbit;

                    if (replicate.fetching == 0)
                        node->block.fetching &= ~devbit;

                    break ;
                }

                /* search for owners of the access */
                case (Search::Type::SEARCH_OWNERS):
                {
                    Hypercube intersect;
                    Hypercube::intersection(&intersect, h, node->hypercube);
                    const size_t bytes = intersect.size();
                    for (xkrt_device_global_id_t device_global_id = 0 ; device_global_id < XKRT_DEVICES_MAX ; ++device_global_id)
                        if (node->block.coherency & (1 << device_global_id))
                            search.bytes_owned[device_global_id] += bytes;
                    break ;
                }

                default:
                {
                    LOGGER_FATAL("Invalid search type in memory tree");
                    assert(0);
                }
            }
        }

        Node *
        new_node(
            Search & search,
            const Hypercube & h,
            const int k,
            const Color color
        ) const {
            assert(search.type == Search::Type::INSERTING_BLOCKS);
            return new Node(search.access, h, k, color);
        }

        Node *
        new_node(
            Search & search,
            const Hypercube & h,
            const int k,
            const Color color,
            const NodeBase * inherit
        ) const {
            (void) search;
            assert(search.type == Search::Type::INSERTING_BLOCKS);
            assert(!h.intersects(inherit->hypercube));
            return new Node(h, k, color, reinterpret_cast<const Node *>(inherit), this->sizeof_type);
        }
};

using RegisterTree = KRegisterTree<2>;

#endif /* __REGISTER_MEMORY_TREE_HPP__ */
