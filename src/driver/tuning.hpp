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

/*
 *  Measured choices and command durations, shared by every driver.
 *
 *  1. Choices. Some launch parameters of a program are decided by measuring
 *     rather than guessing:
 *       - CHOICE_VARIANT  its implementation: the JIT'd code or the
 *                         ahead-of-time one (cgir CGIR_JIT_VARIANT=auto);
 *       - CHOICE_GRID     its launch size, for a grid_flexible device program.
 *     A choice has `n` candidates and is pooled under a key: every command
 *     instantiated from the same construct shares the statistics. Measuring
 *     must not repeat work (a program may update its data in place), so each
 *     launch runs exactly one candidate, alternating launch by launch per
 *     command -- the first candidate staggered across the commands sharing the
 *     pool, so that a replay-wide effect is not charged to one candidate. The
 *     first launch of each candidate is a discarded warm-up (one-time costs:
 *     lazy loading, cold caches). After samples() timed launches of each, the
 *     fastest best launch wins -- timing noise only ever adds time, so the
 *     minimum is the robust estimator -- ties going to the earlier candidate.
 *     Each command adopts the decision at its next launch.
 *
 *     How a launch is timed (a steady clock around a host body, a pair of
 *     events around a device launch) and how a candidate is installed are the
 *     driver's business; this module only keeps the books.
 *
 *  2. Durations. While a command graph asks for them (cgir
 *     command_graph_t::durations_wanted, set by the `critical-path` pass), the
 *     runtime flags its commands COMMAND_FLAG_MEASURE; drivers time the
 *     flagged launches and report them here, which keeps each command's best
 *     time in cgir::command_t::duration (ns), the first launch again discarded.
 *
 *  Thread-safe.
 */

#ifndef __XKRT_DRIVER_TUNING_HPP__
# define __XKRT_DRIVER_TUNING_HPP__

# include <xkrt/command/command.hpp>

# include <array>
# include <cstddef>
# include <cstdint>
# include <functional>
# include <string>

XKRT_NAMESPACE_BEGIN
namespace tuning {

/* What a choice selects. */
enum choice_kind_t
{
    CHOICE_VARIANT,
    CHOICE_GRID,
};

/* The candidates of a CHOICE_VARIANT, in the order they are tried (ties go to
 * the first: the JIT). */
enum variant_candidate_t
{
    VARIANT_CANDIDATE_JIT,
    VARIANT_CANDIDATE_AOT,
    VARIANT_CANDIDATES
};

/* The cgir decision a variant candidate stands for. */
cgir::command_prog_variant_t variant_of(int candidate);

/* No decision yet. */
constexpr int UNDECIDED = -1;

/* What pools a choice: e.g. (device, JIT function, AOT function). */
typedef std::array<uintptr_t, 3> pool_key_t;

/* Where the time of one timed execution goes, once known: a candidate of a
 * choice's pool, the duration of a command, or both. Opaque to drivers, which
 * only carry it from the launch to record(). */
struct sink_t
{
    void * pool;
    int candidate;
    const command_t * command;

    sink_t(void) :
        pool(NULL),
        candidate(UNDECIDED),
        command(NULL)
    {}

    inline bool
    empty(void) const
    {
        return this->pool == NULL && this->command == NULL;
    }
};

/* Timed launches per candidate (XKRT_PROG_VARIANT_SAMPLES), and per command
 * duration. Set once from the runtime configuration; 3 until then. 0 decides
 * every choice for its first candidate without measuring. */
void     configure(unsigned int samples);
unsigned samples(void);

/* The candidate `command` runs this launch, for the `kind` choice among `n`
 * candidates pooled under `key`:
 *   - decided: returns the decision (and forgets the command; adopt it for
 *     good), `*trial` left empty;
 *   - measuring: returns the candidate to run, and `*trial` names where its
 *     time goes: the caller times this launch and reports it with record().
 * `name` and `label(k)` only serve the decision's log line. `saved`, of
 * `saved_size` bytes, is per-command state the caller needs back until the
 * decision (e.g. the configuration it started with): copied in at the first
 * call, copied back out at every call. */
int choose(choice_kind_t kind, const command_t * command, const pool_key_t & key, unsigned int n,
           const char * name, const std::function<std::string(unsigned int)> & label,
           void * saved, size_t saved_size, sink_t * trial);

/* Settle `command` on its decision, or `fallback` if there is none yet (e.g.
 * frozen into a CUDA graph before it was measured), and forget it. Copies the
 * saved state back like choose(). Returns `fallback` for a command that never
 * joined a choice. */
int settle(choice_kind_t kind, const command_t * command, int fallback,
           void * saved = nullptr, size_t saved_size = 0);

/* Whether `command` joined the `kind` choice and its pool is decided. */
bool decided(choice_kind_t kind, const command_t * command);

/* Add `command`'s duration to `*trial` if its launch should also be timed for
 * it (COMMAND_FLAG_MEASURE). Returns whether `*trial` is non-empty. */
bool measure(const command_t * command, sink_t * trial);

/* Report the time of a timed execution. */
void record(const sink_t & s, double seconds);

/* Whether `command` has a settled duration: samples() timed launches after
 * the warm-up. */
bool duration_settled(const command_t * command);

/* Before a replay of `cg`: if the graph wants durations, flag its commands
 * (down into packs, packs themselves excepted) for timing until they are all
 * measured, then run the `critical-path` pass
 * again with them (so that their priorities reflect what they cost) and
 * unflag them. Gives up after a bounded number of replays (a command that is
 * never launched, a driver that cannot time) and keeps what it has. */
void replay_prepare(cgir::command_graph_t * cg);

} /* namespace tuning */
XKRT_NAMESPACE_END

#endif /* __XKRT_DRIVER_TUNING_HPP__ */
