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

# include <xkrt/support.h>

# if XKRT_SUPPORT_CAIRO
#  include <cairo/cairo.h>
#  include <cairo/cairo-svg.h>
#  include <xkrt/memory/access/blas/region/memory-tree.hpp>
void
xkrt_cairo_memory_trees(
    xkrt_runtime_t * runtime
) {
    for (MemoryCoherencyController * memcontroller : runtime->memcontrollers)
    {
        MemoryTree * memtree = (MemoryTree *) memcontroller;
        assert(memtree);

        if (memtree->root == NULL)
            continue ;

        // Step 1: Use a recording surface (size is dynamically determined)
        cairo_surface_t *rec_surface = cairo_recording_surface_create(CAIRO_CONTENT_COLOR_ALPHA, NULL);
        cairo_t *rec_ctx = cairo_create(rec_surface);

        // Step 2: Set stroke properties (line width and color)
        cairo_set_line_width(rec_ctx, 3); // Border thickness
        cairo_set_source_rgb(rec_ctx, 0, 0, 0); // Black stroke

        // Step 2: Draw elements without knowing the final size
        double offset = (double) memtree->root->cube[1].a;
        std::function<void(MemoryTree::NodeBase *, void *)> f = [offset, rec_ctx](MemoryTree::NodeBase * node, void * args) {
            double y1 = (double) node->cube[0].a;
            double y2 = (double) node->cube[0].b;
            double x2 = (double) offset - (double) node->cube[1].a;
            double x1 = (double) offset - (double) node->cube[1].b;
            cairo_rectangle(rec_ctx, x1, y1, x2-x1, y2-y1);
            cairo_stroke(rec_ctx);
            LOGGER_WARN("Drawing %lf %lf %lf %lf", x1, y1, x2-x1, y2-y1);
            (void) args;
        };
        memtree->foreach_node(f, NULL);

        // Step 3: Get the bounding box of everything drawn
        double x1, y1, x2, y2;
        cairo_recording_surface_ink_extents(rec_surface, &x1, &y1, &x2, &y2);

        // Step 4: Create a properly sized SVG surface
        char filename[128];
        snprintf(filename, sizeof(filename), "memtree-%lu-%lu.svg", memtree->ld, memtree->sizeof_type);
        cairo_surface_t *svg_surface = cairo_svg_surface_create(filename, x2, y2);
        cairo_t *svg_ctx = cairo_create(svg_surface);
        LOGGER_WARN("Exporting memory tree to `%s`", filename);

        // Step 5: Replay recorded drawing onto the final surface
        cairo_set_source_surface(svg_ctx, rec_surface, 0, 0);
        cairo_paint(svg_ctx);

        // Cleanup
        cairo_destroy(rec_ctx);
        cairo_surface_destroy(rec_surface);
        cairo_destroy(svg_ctx);
        cairo_surface_destroy(svg_surface);
    }
}

# endif /* XKRT_SUPPORT_CAIRO */

