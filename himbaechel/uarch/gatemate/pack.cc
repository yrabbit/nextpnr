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

#include "pack.h"
#include "design_utils.h"
#include "gatemate_util.h"

#define HIMBAECHEL_CONSTIDS "uarch/gatemate/constids.inc"
#include "himbaechel_constids.h"

NEXTPNR_NAMESPACE_BEGIN

void GateMatePacker::flush_cells()
{
    for (auto pcell : packed_cells) {
        for (auto &port : ctx->cells[pcell]->ports) {
            ctx->cells[pcell]->disconnectPort(port.first);
        }
        ctx->cells.erase(pcell);
    }
    packed_cells.clear();
}

void GateMatePacker::disconnect_if_gnd(CellInfo *cell, IdString input)
{
    NetInfo *net = cell->getPort(input);
    if (!net)
        return;
    if (net->name.in(ctx->id("$PACKER_GND"))) {
        cell->disconnectPort(input);
    }
}

CellInfo *GateMatePacker::move_ram_i(CellInfo *cell, IdString origPort, bool place)
{
    CellInfo *cpe_half = nullptr;
    NetInfo *net = cell->getPort(origPort);
    if (net) {
        cpe_half = create_cell_ptr(id_CPE_HALF, ctx->idf("%s$%s_cpe_half", cell->name.c_str(ctx), origPort.c_str(ctx)));
        if (place) {
            cell->constr_children.push_back(cpe_half);
            cpe_half->cluster = cell->cluster;
            cpe_half->constr_abs_z = false;
            cpe_half->constr_z = PLACE_DB_CONSTR + origPort.index;
        }
        cpe_half->params[id_C_RAM_I] = Property(1, 1);

        NetInfo *ram_i = ctx->createNet(ctx->idf("%s$ram_i", cpe_half->name.c_str(ctx)));
        cell->movePortTo(origPort, cpe_half, id_OUT);
        cell->connectPort(origPort, ram_i);
        cpe_half->connectPort(id_RAM_I, ram_i);
    }
    return cpe_half;
}

CellInfo *GateMatePacker::move_ram_o(CellInfo *cell, IdString origPort, bool place)
{
    CellInfo *cpe_half = nullptr;
    NetInfo *net = cell->getPort(origPort);
    if (net) {
        cpe_half = create_cell_ptr(id_CPE_HALF, ctx->idf("%s$%s_cpe_half", cell->name.c_str(ctx), origPort.c_str(ctx)));
        if (place) {
            cell->constr_children.push_back(cpe_half);
            cpe_half->cluster = cell->cluster;
            cpe_half->constr_abs_z = false;
            cpe_half->constr_z = PLACE_DB_CONSTR + origPort.index;
        }
        if (net->name == ctx->id("$PACKER_GND")) {
            cpe_half->params[id_INIT_L00] = Property(0b0000, 4);
            cell->disconnectPort(origPort);
        } else if (net->name == ctx->id("$PACKER_VCC")) {
            cpe_half->params[id_INIT_L00] = Property(0b1111, 4);
            cell->disconnectPort(origPort);
        } else {
            cpe_half->params[id_INIT_L00] = Property(0b1010, 4);
            cell->movePortTo(origPort, cpe_half, id_IN1);
        }
        cpe_half->params[id_INIT_L10] = Property(0b1010, 4);
        cpe_half->params[id_C_O] = Property(0b11, 2);
        cpe_half->params[id_C_RAM_O] = Property(1, 1);

        NetInfo *ram_o = ctx->createNet(ctx->idf("%s$ram_o", cpe_half->name.c_str(ctx)));
        cell->connectPort(origPort, ram_o);
        cpe_half->connectPort(id_RAM_O, ram_o);
    }
    return cpe_half;
}

CellInfo *GateMatePacker::move_ram_i_fixed(CellInfo *cell, IdString origPort, Loc fixed)
{
    CellInfo *cpe = move_ram_i(cell, origPort, false);
    if (cpe) {
        BelId b = ctx->getBelByLocation(uarch->getRelativeConstraint(fixed, origPort));
        ctx->bindBel(b, cpe, PlaceStrength::STRENGTH_FIXED);
    }
    return cpe;
}

CellInfo *GateMatePacker::move_ram_o_fixed(CellInfo *cell, IdString origPort, Loc fixed)
{
    CellInfo *cpe = move_ram_o(cell, origPort, false);
    if (cpe) {
        BelId b = ctx->getBelByLocation(uarch->getRelativeConstraint(fixed, origPort));
        ctx->bindBel(b, cpe, PlaceStrength::STRENGTH_FIXED);
    }
    return cpe;
}

CellInfo *GateMatePacker::move_ram_io(CellInfo *cell, IdString iPort, IdString oPort, bool place)
{
    CellInfo *cpe_half = nullptr;
    NetInfo *i_net = cell->getPort(iPort);
    NetInfo *o_net = cell->getPort(oPort);
    if (!i_net && !o_net)
        return cpe_half;

    cpe_half = create_cell_ptr(id_CPE_HALF, ctx->idf("%s$%s_cpe_half", cell->name.c_str(ctx), oPort.c_str(ctx)));
    if (place) {
        cell->constr_children.push_back(cpe_half);
        cpe_half->cluster = cell->cluster;
        cpe_half->constr_abs_z = false;
        cpe_half->constr_z = PLACE_DB_CONSTR + oPort.index;
    }

    if (o_net) {
        if (o_net->name == ctx->id("$PACKER_GND")) {
            cpe_half->params[id_INIT_L00] = Property(0b0000, 4);
            cell->disconnectPort(oPort);
        } else if (o_net->name == ctx->id("$PACKER_VCC")) {
            cpe_half->params[id_INIT_L00] = Property(0b1111, 4);
            cell->disconnectPort(oPort);
        } else {
            cpe_half->params[id_INIT_L00] = Property(0b1010, 4);
            cell->movePortTo(oPort, cpe_half, id_IN1);
        }
        cpe_half->params[id_INIT_L10] = Property(0b1010, 4);
        cpe_half->params[id_C_O] = Property(0b11, 2);
        cpe_half->params[id_C_RAM_O] = Property(1, 1);

        NetInfo *ram_o = ctx->createNet(ctx->idf("%s$ram_o", cpe_half->name.c_str(ctx)));
        cell->connectPort(oPort, ram_o);
        cpe_half->connectPort(id_RAM_O, ram_o);
    }
    if (i_net) {
        cpe_half->params[id_C_RAM_I] = Property(1, 1);

        NetInfo *ram_i = ctx->createNet(ctx->idf("%s$ram_i", cpe_half->name.c_str(ctx)));
        cell->movePortTo(iPort, cpe_half, id_OUT);
        cell->connectPort(iPort, ram_i);
        cpe_half->connectPort(id_RAM_I, ram_i);
    }
    // TODO: set proper timing model, without this it detects combinational loops
    cpe_half->timing_index = ctx->get_cell_timing_idx(id_CPE_DFF);
    return cpe_half;
}

void GateMatePacker::pack_misc()
{
    for (auto &cell : ctx->cells) {
        CellInfo &ci = *cell.second;
        if (!ci.type.in(id_CC_USR_RSTN))
            continue;
        ci.type = id_USR_RSTN;
        ci.cluster = ci.name;
        Loc fixed_loc = uarch->locations[std::make_pair(id_USR_RSTN, uarch->preferred_die)];
        ctx->bindBel(ctx->getBelByLocation(fixed_loc), &ci, PlaceStrength::STRENGTH_FIXED);

        move_ram_i_fixed(&ci, id_USR_RSTN, fixed_loc);
    }
    for (auto &cell : ctx->cells) {
        CellInfo &ci = *cell.second;
        if (!ci.type.in(id_CC_CFG_CTRL))
            continue;
        ci.type = id_CFG_CTRL;
        ci.cluster = ci.name;
        Loc fixed_loc = uarch->locations[std::make_pair(id_CFG_CTRL, uarch->preferred_die)];
        ctx->bindBel(ctx->getBelByLocation(fixed_loc), &ci, PlaceStrength::STRENGTH_FIXED);

        move_ram_o_fixed(&ci, id_CLK, fixed_loc);
        move_ram_o_fixed(&ci, id_EN, fixed_loc);
        move_ram_o_fixed(&ci, id_VALID, fixed_loc);
        move_ram_o_fixed(&ci, id_RECFG, fixed_loc);
        for (int i = 0; i < 8; i++)
            move_ram_o_fixed(&ci, ctx->idf("DATA[%d]", i), fixed_loc);
    }
    for (auto &cell : ctx->cells) {
        CellInfo &ci = *cell.second;
        if (!ci.type.in(id_CC_ODDR, id_CC_IDDR))
            continue;
        log_error("Cell '%s' of type %s is not connected to GPIO pin.\n", ci.name.c_str(ctx), ci.type.c_str(ctx));
    }
}

void GateMatePacker::remove_not_used()
{
    for (auto &cell : ctx->cells) {
        CellInfo &ci = *cell.second;
        for (auto &p : ci.ports) {
            if (p.second.type == PortType::PORT_OUT) {
                NetInfo *net = ci.getPort(p.first);
                if (net && net->users.entries() == 0) {
                    ci.disconnectPort(p.first);
                }
            }
        }
    }
}

void GateMatePacker::copy_constraint(NetInfo *in_net, NetInfo *out_net)
{
    if (!in_net || !out_net)
        return;
    if (ctx->debug)
        log_info("copy clock period constraint on net '%s' from net '%s'\n", out_net->name.c_str(ctx),
                 in_net->name.c_str(ctx));
    if (out_net->clkconstr.get() != nullptr)
        log_warning("found multiple clock constraints on net '%s'\n", out_net->name.c_str(ctx));
    if (in_net->clkconstr) {
        out_net->clkconstr = std::unique_ptr<ClockConstraint>(new ClockConstraint());
        out_net->clkconstr->low = in_net->clkconstr->low;
        out_net->clkconstr->high = in_net->clkconstr->high;
        out_net->clkconstr->period = in_net->clkconstr->period;
    }
}

void GateMateImpl::pack()
{
    const ArchArgs &args = ctx->args;
    if (args.options.count("ccf")) {
        parse_ccf(args.options.at("ccf"));
    }

    GateMatePacker packer(ctx, this);
    packer.pack_constants();
    packer.remove_not_used();
    packer.pack_io();
    packer.insert_pll_bufg();
    packer.sort_bufg();
    packer.pack_pll();
    packer.pack_bufg();
    packer.pack_io_sel(); // merge in FF and DDR
    packer.pack_misc();
    packer.pack_ram();
    packer.pack_serdes();
    packer.pack_addf();
    packer.pack_cpe();
    packer.remove_constants();
    packer.remove_clocking();
}

NEXTPNR_NAMESPACE_END
