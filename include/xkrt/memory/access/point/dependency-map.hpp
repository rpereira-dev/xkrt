/* ************************************************************************** */
/*                                                                            */
/*   dependency-map.hpp                                           .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2025/05/19 00:09:44 by Romain PEREIRA          __/_*_*(_        */
/*   Updated: 2025/06/03 18:03:59 by Romain PEREIRA         / _______ \       */
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

            std::vector<access_t *> last_seq_reads;
            access_t *  last_seq_write;

        public:

            Node(
            ) :
                last_seq_reads(8),
                last_seq_write()
            {
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
        link(access_t * access)
        {
            assert(this->can_resolve(access));

            // retrieve previous accesses on that point
            auto it = map.find(access->point);

            // if none, no dependencies, return
            if (it == map.end())
                return ;

            // else
            const Node & node = it->second;

            // the generated task is dependent of previous 'reads'
            if ((access->mode & ACCESS_MODE_W) & node.last_seq_reads.size())
            {
                for (access_t * read : node.last_seq_reads)
                    __access_precedes(read, access);

            }
            else if (node.last_seq_write)
            {
                __access_precedes(node.last_seq_write, access);
            }
        }

        inline void
        put(access_t * access)
        {
            assert(this->can_resolve(access));

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
                node.last_seq_reads.clear();
                node.last_seq_write = access;
            }
            else if (access->mode == ACCESS_MODE_R)
                node.last_seq_reads.push_back(access);
        }

        bool
        can_resolve(const access_t * access) const
        {
            assert(access);
            return (access->type == ACCESS_TYPE_POINT);
        }
};

#endif /* __DEPENDENCY_MAP_HPP__ */
