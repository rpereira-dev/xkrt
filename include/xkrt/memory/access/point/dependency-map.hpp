/* ************************************************************************** */
/*                                                                            */
/*   dependency-map.hpp                                           .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2025/05/19 00:09:44 by Romain PEREIRA          __/_*_*(_        */
/*   Updated: 2025/07/17 19:49:03 by Romain PEREIRA         / _______ \       */
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
            const Node & node = it->second;
            bool seq_w_edge_transitive = false;

            // the generated access depends on previous SEQ-R
            if (node.last_seq_reads.size() && (access->mode & ACCESS_MODE_W))
            {
                # if 0
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
                    # if 0
                    access_t * extra = NULL;
                    constexpr access_mode_t         mode        = ACCESS_MODE_V | ACCESS_MODE_W;
                    constexpr access_concurrency_t  concurrency = ACCESS_CONCURRENCY_SEQUENTIAL;
                    constexpr access_scope_t        scope       = ACCESS_SCOPE_NONUNIFIED;
                    new (extra) access_t(NULL, access->point, mode, concurrency, scope);

                    for (access_t * read : node.last_seq_reads)
                        __access_precedes(read, );
                    # else
                    LOGGER_FATAL("TODO");
                    # endif
                }
                // SEQ-W
                else
                # endif
                {
                    for (access_t * read : node.last_seq_reads)
                        __access_precedes(read, access);
                }
                seq_w_edge_transitive = true;
            }

            // the generated access depends on previous CNC-W
            if (node.last_conc_writes.size() && access->concurrency != ACCESS_CONCURRENCY_CONCURRENT)
            {
                # if 0
                if (access->mode & ACCESS_MODE_W)
                {
                # endif
                    for (access_t * cw : node.last_conc_writes)
                        __access_precedes(cw, access);
                # if 0
                }
                else
                {
                    /**
                     * conc-w:        O O O
                     *                 \|/
                     * seq-w:           X       // <- insert that extra node
                     *                 / \
                     * seq-r:         O   O     // <- inserting this
                     */
                    # if 0
                    # else
                    LOGGER_FATAL("TODO");
                    # endif
                }
                # endif
                seq_w_edge_transitive = true;
            }

            // the generated access depends on previous SEQ-W (they all do)
            if (1)
            {
                if (seq_w_edge_transitive)
                {
                    // nothing to do:
                    // if 'last_conc_writes' or 'last_seq_reads' are not empty,
                    // then 'access' already depends on 'last_seq_write' by
                    // transitivity
                }
                else if (node.last_seq_write)
                {
                    __access_precedes(node.last_seq_write, access);
                }
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

#endif /* __DEPENDENCY_MAP_HPP__ */
