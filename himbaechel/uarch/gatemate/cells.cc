/*
 *  nextpnr -- Next Generation Place and Route
 *
 *  Copyright (C) 2024  The Project Peppercorn Authors.
 *
 *  Permission to use, copy, modify, and/or distribute this software for any
 *  purpose with or without fee is hereby granted, provided that the above
 *  copyright notice and this permission notice appear in all copies.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 *  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 *  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 *  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 *  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 *  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

#include "nextpnr.h"
#include "pack.h"

#define HIMBAECHEL_CONSTIDS "uarch/gatemate/constids.inc"
#include "himbaechel_constids.h"

NEXTPNR_NAMESPACE_BEGIN

CellInfo *GateMatePacker::create_cell_ptr(IdString type, IdString name)
{
    CellInfo *cell = ctx->createCell(name, type);

    auto add_port = [&](const IdString id, PortType dir) {
        cell->ports[id].name = id;
        cell->ports[id].type = dir;
    };
    if (type.in(id_CPE_LT, id_CPE_LT_U, id_CPE_LT_L, id_CPE_L2T4, id_CPE_DUMMY)) {
        add_port(id_IN1, PORT_IN);
        add_port(id_IN2, PORT_IN);
        add_port(id_IN3, PORT_IN);
        add_port(id_IN4, PORT_IN);
        add_port(id_OUT, PORT_OUT);
        add_port(id_CPOUT, PORT_OUT);
        // These are used to propagate alternate inputs for first LUT2 levels
        add_port(id_CINX, PORT_IN);
        add_port(id_PINX, PORT_IN);
        add_port(id_PINY1, PORT_IN);
        // For EN_CIN input
        add_port(id_CINY1, PORT_IN);
        if (type.in(id_CPE_LT_L)) {
            add_port(id_CINY2, PORT_IN);
            add_port(id_PINY2, PORT_IN);
            add_port(id_COMBIN, PORT_IN);

            add_port(id_COUTX, PORT_OUT);
            add_port(id_POUTX, PORT_OUT);
            add_port(id_COUTY1, PORT_OUT);
            add_port(id_POUTY1, PORT_OUT);
            add_port(id_COUTY2, PORT_OUT);
            add_port(id_POUTY2, PORT_OUT);
        }
    } else if (type.in(id_CLKIN)) {
        for (int i = 0; i < 4; i++) {
            add_port(ctx->idf("CLK%d", i), PORT_IN);
            add_port(ctx->idf("CLK_REF%d", i), PORT_OUT);
        }
        add_port(id_SER_CLK, PORT_IN);
    } else if (type.in(id_GLBOUT)) {
        for (int i = 0; i < 4; i++) {
            add_port(ctx->idf("CLK0_%d", i), PORT_IN);
            add_port(ctx->idf("CLK90_%d", i), PORT_IN);
            add_port(ctx->idf("CLK180_%d", i), PORT_IN);
            add_port(ctx->idf("CLK270_%d", i), PORT_IN);
            add_port(ctx->idf("CLK_REF_OUT%d", i), PORT_IN);
            add_port(ctx->idf("USR_GLB%d", i), PORT_IN);
            add_port(ctx->idf("USR_FB%d", i), PORT_IN);
            add_port(ctx->idf("CLK_FB%d", i), PORT_OUT);
            add_port(ctx->idf("GLB%d", i), PORT_OUT);
        }
    } else if (type.in(id_CC_BUFG)) {
        add_port(id_I, PORT_IN);
        add_port(id_O, PORT_OUT);
    } else if (type.in(id_CPE_RAMIO, id_CPE_RAMI, id_CPE_RAMO)) {
        add_port(id_I, PORT_IN);
        add_port(id_RAM_I, PORT_IN);
        add_port(id_RAM_O, PORT_OUT);
        add_port(id_OUT, PORT_OUT);
    } else if (type.in(id_CPE_COMP)) {
        add_port(id_COMB1, PORT_IN);
        add_port(id_COMB2, PORT_IN);
        add_port(id_COMPOUT, PORT_OUT);
    } else if (type.in(id_CPE_CPLINES)) {
        add_port(id_OUT1, PORT_IN);
        add_port(id_OUT2, PORT_IN);
        add_port(id_COMPOUT, PORT_IN);

        add_port(id_CINX, PORT_IN);
        add_port(id_PINX, PORT_IN);
        add_port(id_CINY1, PORT_IN);
        add_port(id_PINY1, PORT_IN);
        add_port(id_CINY2, PORT_IN);
        add_port(id_PINY2, PORT_IN);

        add_port(id_COUTX, PORT_OUT);
        add_port(id_POUTX, PORT_OUT);
        add_port(id_COUTY1, PORT_OUT);
        add_port(id_POUTY1, PORT_OUT);
        add_port(id_COUTY2, PORT_OUT);
        add_port(id_POUTY2, PORT_OUT);
    } else {
        log_error("Trying to create unknown cell type %s\n", type.c_str(ctx));
    }
    return cell;
}

NEXTPNR_NAMESPACE_END
