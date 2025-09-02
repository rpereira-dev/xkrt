/* ************************************************************************** */
/*                                                                            */
/*   dependency-map.hpp                                           .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2025/05/19 00:09:44 by Romain PEREIRA          __/_*_*(_        */
/*   Updated: 2025/08/22 23:32:58 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <rpereira@anl.gov>                                */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

// OpenMP 6.0-alike dependencies

#ifndef __DEPENDENCY_MAP_HPP__
# define __DEPENDENCY_MAP_HPP__

# include <xkrt/memory/access/dependency-domain.hpp>
# include <xkrt/task/task.hpp>

# include <vector>
# include <unordered_map>

XKRT_NAMESPACE_BEGIN

class DependencyMap : public DependencyDomain
{

    class Node {

        public:

            std::vector<access_t *> last_conc_writes;
            std::vector<access_t *> last_seq_reads;
            access_t *  last_seq_write;

        public:

            Node(
            ) :
                last_conc_writes(8),
                last_seq_reads(8),
                last_seq_write()
            {
                last_conc_writes.clear();
                last_seq_reads.clear();
            }

            ~Node() {}

    }; /* Node */

     private:
        std::unordered_map<const void *, Node> map;

     public:
        DependencyMap(const int n = 4096) : map()
        {
            if (n)
                map.reserve(n);
        }

        ~DependencyMap() {}

    public:

        inline void
        insert_empty_write(const void * point)
        {
            // create the empty task node
            thread_t * thread = thread_t::get_tls();
            assert(thread);

            constexpr int AC = 1;
            constexpr task_flag_bitfield_t flags = TASK_FLAG_DEPENDENT;
            constexpr size_t task_size = task_compute_size(flags, AC);

            task_t * extra = thread->allocate_task(task_size);
            assert(extra);
            new (extra) task_t(TASK_FORMAT_NULL, flags);

            task_dep_info_t * dep = TASK_DEP_INFO(extra);
            assert(dep);
            new (dep) task_dep_info_t(AC);

            # ifndef NDEBUG
            snprintf(extra->label, sizeof(extra->label), "cw-empty-node");
            # endif

            access_t * accesses = TASK_ACCESSES(extra, flags);
            assert(accesses);
            {
                constexpr access_mode_t         mode        = ACCESS_MODE_VW;
                constexpr access_concurrency_t  concurrency = ACCESS_CONCURRENCY_SEQUENTIAL;
                constexpr access_scope_t        scope       = ACCESS_SCOPE_NONUNIFIED;
                new (accesses + 0) access_t(extra, point, mode, concurrency, scope);
            }

            // TODO : bellow are really ugly implementation hacks
            // maybe refactor the code to go through the traditionnal runtime routines
            this->link(accesses + 0);

            if (dep->wc.fetch_sub(1, std::memory_order_seq_cst) == 1)
            {
                // all predecessors completed already, we can skip that empty node
            }
            else
            {
                assert(thread->current_task);
                extra->parent = thread->current_task;
                ++thread->current_task->cc;

                this->put(accesses + 0);
            }
        }

        // set all accesses of 'list' as predecessors of 'succ'
        // and remove entries in 'list' that already completed
        static inline void
        link_or_pop(
            std::vector<access_t *> & list,
            // std::list<access_t *> & list,
            access_t * succ
        ) {
            # if 1
            for (auto it = list.begin(); it != list.end(); )
            {
                access_t * pred = *it;

                // return true if pred is not already completed
                if (__access_precedes(pred, succ))
                {
                    ++it;
                }
                // pred completed, we can remove it from the list to dampen
                // future accesses search
                else
                {
                    it = list.erase(it);
                }
            }
            # elif 0
            bool clear = true;
            for (access_t * pred : list)
            {
                if (__access_precedes(pred, succ))
                {
                    clear = false;
                }
            }
            if (clear)
                list.clear();
            # else
            for (access_t * pred : list)
                __access_precedes(pred, succ);
            # endif
        }

        //  access type         depend on
        //  SEQ-R               SEQ-W, CNC-W,  COM-W
        //  CNC-W               SEQ-R, SEQ-W,  COM-W,
        //  COM-W               SEQ-R, SEQ-W, (COM-W), CNC-W
        //  SEQ-W               SEQ-R, SEQ-W,  COM-W,  CNC-W

        inline void
        link(access_t * access)
        {
            // retrieve previous accesses on that point
            auto it = map.find(access->point);

            // if none, no dependencies, return
            if (it == map.end())
                return ;

            // else, set dependencies
            Node & node = it->second;
            bool seq_w_edge_transitive = false;

            // the generated access depends on previous SEQ-R
            if (node.last_seq_reads.size() && (access->mode & ACCESS_MODE_W))
            {
                # if 1
                // CNC-W
                if (access->concurrency == ACCESS_CONCURRENCY_CONCURRENT)
                {
                    /**
                     * seq-r :        O O O
                     *                 \|/
                     * seq-w:           X       // <- insert that extra node
                     *                 / \
                     * conc-w:        O   O     // <- inserting this
                     */
                    insert_empty_write(access->point);
                }
                // SEQ-W
                else
                # endif
                {
                    link_or_pop(node.last_seq_reads, access);
                    seq_w_edge_transitive = true;
                }
            }

            // the generated access depends on previous CNC-W
            if (node.last_conc_writes.size() && access->concurrency != ACCESS_CONCURRENCY_CONCURRENT)
            {
                # if 1
                if (access->mode & ACCESS_MODE_W)
                # endif
                {
                    link_or_pop(node.last_conc_writes, access);
                    seq_w_edge_transitive = true;
                }
                # if 1
                else
                {
                    assert(access->mode & ACCESS_MODE_R);

                    /**
                     * conc-w:        O O O
                     *                 \|/
                     * seq-w:           X       // <- insert that extra node
                     *                 / \
                     * seq-r:         O   O     // <- inserting this
                     */
                    insert_empty_write(access->point);
                }
                # endif
            }

            // the generated access depends on previous SEQ-W (they all do)
            if (node.last_seq_write)
            {
                if (seq_w_edge_transitive)
                {
                    // nothing to do: another edge already ensure that
                    // dependency by transitivity
                }
                else
                {
                    if (__access_precedes(node.last_seq_write, access))
                    {
                        // nothing to do, last writer has not completed
                    }
                    else
                    {
                        // last writer completed already, can remove it
                        node.last_seq_write = NULL;
                    }
                }
            }
            else
            {
                // nothing to do: no previous writers
            }
        }

        inline void
        put(access_t * access)
        {
            // TODO : redundancy check, if we allow redundant dependencies - see
            // https://github.com/cea-hpc/mpc/blob/master/src/MPC_OpenMP/src/mpcomp_task.c#L1274

            // ensure a node exists on that address
            auto result = map.insert({access->point, Node()});
            if (result.second)
            {
                // node got inserted
            }
            else
            {
                // node already existed
            }

            Node & node = result.first->second;

            if (access->mode & ACCESS_MODE_W)
            {
                if (access->concurrency == ACCESS_CONCURRENCY_CONCURRENT)
                {
                    node.last_conc_writes.push_back(access);
                }
                else
                {
                    assert(access->concurrency == ACCESS_CONCURRENCY_SEQUENTIAL ||
                            access->concurrency == ACCESS_CONCURRENCY_COMMUTATIVE);

                    node.last_seq_reads.clear();
                    node.last_conc_writes.clear();
                    node.last_seq_write = access;
                }
            }
            else if (access->mode & ACCESS_MODE_R)
            {
                node.last_conc_writes.clear();
                node.last_seq_reads.push_back(access);
            }
        }

};

XKRT_NAMESPACE_END

#endif /* __DEPENDENCY_MAP_HPP__ */
