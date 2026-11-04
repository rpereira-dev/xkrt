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

/* See tuning.hpp. */

# include "tuning.hpp"

# include <xkrt/logger/logger.h>
# include <xkrt/sync/spinlock.h>
# include <cgir/command-graph.hpp>

# include <algorithm>
# include <atomic>
# include <cstring>
# include <limits>
# include <map>
# include <unordered_map>
# include <utility>
# include <vector>

XKRT_NAMESPACE_USE;
using namespace XKRT_NAMESPACE::tuning;

/* pooled measurements of one choice */
typedef struct  pool_t
{
    choice_kind_t kind;

    /* number of candidates */
    unsigned int n;

    /* per candidate, warm-up excluded */
    std::vector<std::vector<double>> seconds;

    /* warm-up launch seen, per candidate */
    std::vector<bool> warm;

    /* commands sharing the pool */
    unsigned int ncmds;

    /* the decided candidate, or UNDECIDED */
    int decision;

    /* for logging */
    std::string name;
    std::vector<std::string> labels;

    pool_t(void) :
        kind(CHOICE_VARIANT),
        n(0),
        seconds(),
        warm(),
        ncmds(0),
        decision(UNDECIDED),
        name("?"),
        labels()
    {}

}               pool_t;

/* a command measuring a choice, until it adopts the decision */
typedef struct  member_t
{
    pool_t * pool;

    /* alternation counter */
    unsigned int launches;

    /* per-command state the caller needs back until the decision */
    std::vector<unsigned char> saved;

}               member_t;

/* a command's duration measurements */
typedef struct  duration_t
{
    /* warm-up included */
    unsigned int samples;

    duration_t(void) : samples(0) {}

}               duration_t;

/* replays spent waiting for a graph's durations */
typedef struct  graph_wait_t
{
    unsigned int replays;

    graph_wait_t(void) : replays(0) {}

}               graph_wait_t;

/* protects every container below */
static spinlock_t lock = SPINLOCK_INITIALIZER;
static std::map<std::pair<choice_kind_t, pool_key_t>, pool_t> pools;
static std::map<std::pair<choice_kind_t, const command_t *>, member_t> members;
static std::unordered_map<const command_t *, duration_t> durations;
static std::unordered_map<const cgir::command_graph_t *, graph_wait_t> graph_waits;
static std::atomic<unsigned int> nsamples(3);

static const char *
kind_name(choice_kind_t kind)
{
    switch (kind)
    {
        case (CHOICE_VARIANT): return "variant";
        case (CHOICE_GRID):    return "blocks";
    }
    return "?";
}

static double
best(const std::vector<double> & v)
{
    return *std::min_element(v.begin(), v.end());
}

/* Decide `p` if every candidate has enough samples. Lock held. */
static void
try_decide_locked(pool_t * p)
{
    if (p->decision != UNDECIDED)
        return ;
    const unsigned int s = nsamples.load(std::memory_order_relaxed);
    for (unsigned int k = 0 ; k < p->n ; ++k)
        if (p->seconds[k].size() < s)
            return ;
    int b = 0;
    for (unsigned int k = 1 ; k < p->n ; ++k)
        if (best(p->seconds[k]) < best(p->seconds[b]))
            b = (int) k;
    p->decision = b;

    std::string line;
    for (unsigned int k = 0 ; k < p->n ; ++k)
    {
        char buf[64];
        snprintf(buf, sizeof(buf), "%s%s:%.1fus", k ? " " : "", p->labels[k].c_str(),
                 1e6 * best(p->seconds[k]));
        line += buf;
    }
    LOGGER_INFO("prog `%s`: %s %s (best of %u launches each): keeping %s",
                p->name.c_str(), kind_name(p->kind), line.c_str(), s, p->labels[b].c_str());
}

static void
copy_out(const member_t & m, void * saved, size_t saved_size)
{
    if (saved && saved_size && m.saved.size() == saved_size)
        memcpy(saved, m.saved.data(), saved_size);
}

/* The non-pack commands of `cg`, and of the packs in it, recursively. */
static void
collect_leaves(cgir::command_graph_t * cg, std::vector<command_t *> & out)
{
    /* not with cg->walk: its per-node stamps are not safe against a
     * concurrent walk of a pack's graph by a device thread */
    std::unordered_map<cgir::command_graph_node_t *, bool> seen;
    std::vector<cgir::command_graph_node_t *> todo;
    todo.push_back(cg->node_get_entry());
    seen[cg->node_get_entry()] = true;
    while (!todo.empty())
    {
        cgir::command_graph_node_t * node = todo.back();
        todo.pop_back();
        if (node->type == cgir::COMMAND_GRAPH_NODE_TYPE_COMMAND && node->command)
        {
            if (node->command->type == cgir::COMMAND_TYPE_PACK && node->command->pack.cg)
                collect_leaves(node->command->pack.cg, out);
            else if (node->command->type == cgir::COMMAND_TYPE_EMPTY)
                ;   /* nothing to time */
            else
                out.push_back((command_t *) node->command);
        }
        for (cgir::command_graph_node_t * succ : node->successors)
        {
            if (!seen[succ])
            {
                seen[succ] = true;
                todo.push_back(succ);
            }
        }
    }
}

cgir::command_prog_variant_t
XKRT_NAMESPACE::tuning::variant_of(int candidate)
{
    switch (candidate)
    {
        case (VARIANT_CANDIDATE_JIT): return cgir::CGIR_COMMAND_PROG_VARIANT_JIT;
        case (VARIANT_CANDIDATE_AOT): return cgir::CGIR_COMMAND_PROG_VARIANT_AOT;
        default:                      return cgir::CGIR_COMMAND_PROG_VARIANT_UNDECIDED;
    }
}

void
XKRT_NAMESPACE::tuning::configure(unsigned int samples)
{
    nsamples.store(samples, std::memory_order_relaxed);
}

unsigned int
XKRT_NAMESPACE::tuning::samples(void)
{
    return nsamples.load(std::memory_order_relaxed);
}

int
XKRT_NAMESPACE::tuning::choose(
    choice_kind_t kind,
    const command_t * command,
    const pool_key_t & key,
    unsigned int n,
    const char * name,
    const std::function<std::string(unsigned int)> & label,
    void * saved,
    size_t saved_size,
    sink_t * trial
) {
    *trial = sink_t();
    int k;

    SPINLOCK_LOCK(lock);
    {
        auto it = members.find(std::make_pair(kind, command));
        if (it == members.end())
        {
            pool_t & p = pools[std::make_pair(kind, key)];
            if (p.n == 0)
            {
                p.kind = kind;
                p.n    = n;
                p.seconds.assign(n, std::vector<double>());
                p.warm.assign(n, false);
                for (unsigned int c = 0 ; c < n ; ++c)
                    p.labels.push_back(label(c));
                if (name)
                    p.name = name;
                if (nsamples.load(std::memory_order_relaxed) == 0)
                    p.decision = 0;
            }
            member_t m;
            m.pool     = &p;
            m.launches = p.ncmds++;   /* stagger the first candidate across the pool */
            if (saved && saved_size)
                m.saved.assign((unsigned char *) saved, (unsigned char *) saved + saved_size);
            it = members.emplace(std::make_pair(kind, command), std::move(m)).first;
        }

        member_t & m = it->second;
        copy_out(m, saved, saved_size);
        pool_t * p = m.pool;
        if (p->decision != UNDECIDED)
        {
            k = p->decision;
            members.erase(it);
        }
        else
        {
            k = (int) (m.launches++ % p->n);
            trial->pool      = p;
            trial->candidate = k;
        }
    }
    SPINLOCK_UNLOCK(lock);

    return k;
}

int
XKRT_NAMESPACE::tuning::settle(choice_kind_t kind, const command_t * command, int fallback,
                               void * saved, size_t saved_size)
{
    int d = UNDECIDED;

    SPINLOCK_LOCK(lock);
    {
        auto it = members.find(std::make_pair(kind, command));
        if (it != members.end())
        {
            copy_out(it->second, saved, saved_size);
            d = it->second.pool->decision;
            members.erase(it);
        }
    }
    SPINLOCK_UNLOCK(lock);

    return (d != UNDECIDED) ? d : fallback;
}

bool
XKRT_NAMESPACE::tuning::decided(choice_kind_t kind, const command_t * command)
{
    bool r;

    SPINLOCK_LOCK(lock);
    {
        auto it = members.find(std::make_pair(kind, command));
        r = (it != members.end() && it->second.pool->decision != UNDECIDED);
    }
    SPINLOCK_UNLOCK(lock);

    return r;
}

bool
XKRT_NAMESPACE::tuning::measure(const command_t * command, sink_t * trial)
{
    if (command->flags & COMMAND_FLAG_MEASURE)
        trial->command = command;
    return !trial->empty();
}

void
XKRT_NAMESPACE::tuning::record(const sink_t & s, double seconds)
{
    SPINLOCK_LOCK(lock);
    if (s.pool)
    {
        pool_t * p = (pool_t *) s.pool;
        if (!p->warm[s.candidate])
            p->warm[s.candidate] = true;
        else
            p->seconds[s.candidate].push_back(seconds);
        try_decide_locked(p);
    }
    if (s.command)
    {
        duration_t & d = durations[s.command];
        if (d.samples++ > 0)
        {
            /* the command is not running: its replay is the one being timed */
            command_t * c = const_cast<command_t *>(s.command);
            const uint64_t ns = (uint64_t) std::max(1.0, 1e9 * seconds);
            c->duration = (c->duration == 0) ? ns : std::min(c->duration, ns);
        }
    }
    SPINLOCK_UNLOCK(lock);
}

bool
XKRT_NAMESPACE::tuning::duration_settled(const command_t * command)
{
    bool r;

    SPINLOCK_LOCK(lock);
    {
        auto it = durations.find(command);
        r = (it != durations.end() &&
             it->second.samples > std::max(1u, nsamples.load(std::memory_order_relaxed)));
    }
    SPINLOCK_UNLOCK(lock);

    return r;
}

void
XKRT_NAMESPACE::tuning::replay_prepare(cgir::command_graph_t * cg)
{
    if (!cg->durations_wanted)
        return ;

    /* The commands to time: every command but packs, down into packs. A pack
     * is not timed itself -- it runs unpacked and serialized while its
     * commands are being timed, which says nothing of its packed execution --
     * and the pass weighs it by its inner critical path instead. */
    std::vector<command_t *> cmds;
    collect_leaves(cg, cmds);

    bool settled = true;
    for (command_t * c : cmds)
        settled = settled && duration_settled(c);

    /* give up waiting after enough replays: warm-up + samples, with slack for
     * commands that start late (a choice still being measured) */
    unsigned int replays;
    SPINLOCK_LOCK(lock);
    replays = graph_waits[cg].replays++;
    SPINLOCK_UNLOCK(lock);
    const unsigned int bound = 4 * (std::max(1u, samples()) + 1) + 8;

    if (!settled && replays < bound)
    {
        for (command_t * c : cmds)
            c->flags = c->flags | COMMAND_FLAG_MEASURE;
        return ;
    }

    for (command_t * c : cmds)
        c->flags = (command_flag_t) (c->flags & ~COMMAND_FLAG_MEASURE);
    cg->optimize(cgir::COMMAND_GRAPH_PASS_CRITICAL_PATH);
    if (cg->durations_wanted)
    {
        LOGGER_INFO("critical path: %zu commands, some never timed after %u replays: "
                    "they weigh nothing", cmds.size(), replays);
        cg->durations_wanted = false;
    }
    else
        LOGGER_INFO("critical path: %zu commands timed over %u replays: priorities set",
                    cmds.size(), replays);
    SPINLOCK_LOCK(lock);
    graph_waits.erase(cg);
    SPINLOCK_UNLOCK(lock);
}
