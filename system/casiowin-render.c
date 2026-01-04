//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//

#include <mq/machine.h>
#include <mq/memory.h>
#include <mq/system/casiowin.h>
#include <stdlib.h>

//=== Shape rendering functions ==============================================//

void mq_casiowin_LineToVRAM(
    mqMachine *mach, int x1, int y1, int x2, int y2, int mode)
{
    struct mqCasiowin_TShape shape = { x1, y1, x2, y2, 2, 2, mode, 1, -1, -1 };
    return mq_casiowin_ShapeToVRAM(mach, &shape);
}

void mq_casiowin_ShapeToVRAM(
    mqMachine *mach, struct mqCasiowin_TShape const *shape)
{
    struct mqCasiowin_TShapePixelInfo pixelinfo = {
        MQ_CASIOWIN_SHAPE_MODE_VRAM, 0, 0, 0 };
    mq_casiowin_DrawShape(mach, &pixelinfo, shape);
}

void mq_casiowin_ShapeToDD(
    mqMachine *mach, struct mqCasiowin_TShape const *shape)
{
    struct mqCasiowin_TShapePixelInfo pixelinfo = {
        MQ_CASIOWIN_SHAPE_MODE_DD, 0, 0, 0 };
    mq_casiowin_DrawShape(mach, &pixelinfo, shape);
}

void mq_casiowin_ShapeToDDVRAM(
    mqMachine *mach, struct mqCasiowin_TShape const *shape)
{
    struct mqCasiowin_TShapePixelInfo pixelinfo = {
        MQ_CASIOWIN_SHAPE_MODE_VRAM | MQ_CASIOWIN_SHAPE_MODE_DD, 0, 0, 0 };
    mq_casiowin_DrawShape(mach, &pixelinfo, shape);
}

void mq_casiowin_DrawShape(
    mqMachine *mach, struct mqCasiowin_TShapePixelInfo *pixelinfo,
    struct mqCasiowin_TShape const *shape)
{
    switch(shape->type) {
    case MQ_CASIOWIN_SHAPE_DOT:
        pixelinfo->x = shape->x1;
        pixelinfo->y = shape->y1;
        mq_casiowin_DrawShapePoint(mach, pixelinfo, shape);
        break;

    case MQ_CASIOWIN_SHAPE_SOLID_LINE:
    case MQ_CASIOWIN_SHAPE_ON_OFF_LINE:
    case MQ_CASIOWIN_SHAPE_OFF_ON_LINE:
        mq_casiowin_DrawShapeLine(mach, pixelinfo, shape);
        break;

    case MQ_CASIOWIN_SHAPE_RECT:
        mq_casiowin_DrawShapeRect(mach, pixelinfo, shape);
        break;

    case MQ_CASIOWIN_SHAPE_CIRCLE:
        mq_casiowin_DrawShapeCircle(mach, pixelinfo, shape);
        break;
    }

    if(pixelinfo->mode == MQ_CASIOWIN_SHAPE_MODE_DD) {
        mq_log(MQ_LOG_ERROR, "ShapeToDD: No ScreenRecv");
        // sc029d(); // MAYBE_ScreenRecv_Clear_%29d
    }
}

void mq_casiowin_DrawShapePoint(
    mqMachine *mach, struct mqCasiowin_TShapePixelInfo *pixelinfo,
    struct mqCasiowin_TShape const *shape)
{
    mqCasiowin *Casiowin = mq_casiowin_get(mach);

    if(Casiowin->info->OSSeries == MQ_CASIOWIN_SERIES_FX)
        mq_casiowin_mono_DrawShapePoint(Casiowin->vramLE, pixelinfo, shape);
    else if(Casiowin->info->OSSeries == MQ_CASIOWIN_SERIES_CG)
        mq_log(MQ_LOG_ERROR, "ShapeToVRAM: Not output method for CG!");
}

void mq_casiowin_DrawShapeLine(
    mqMachine *mach, struct mqCasiowin_TShapePixelInfo *pixelinfo,
    struct mqCasiowin_TShape const *shape)
{
    pixelinfo->dash_counter = 0;
    pixelinfo->x = shape->x1;
    pixelinfo->y = shape->y1;

    int x1 = shape->x1, x2 = shape->x2;
    int dx = abs(x2 - x1);
    int sx = (x2 < x1) ? -1 : (x2 == x1) ? 0 : +1;

    int y1 = shape->y1, y2 = shape->y2;
    int dy = abs(y2 - y1);
    int sy = (y2 < y1) ? -1 : (y2 == y1) ? 0 : +1;

    bool horizontal = (dy <= dx);
    int dist_primary_axis = (horizontal ? dx : dy);
    int dist_secondary_axis = (horizontal ? dy : dx);

    int err = -dist_primary_axis;

    for(int i = -1; i <= dist_primary_axis; i++) {
        err += dist_secondary_axis * 2;
        pixelinfo->dash_counter++;

        mq_casiowin_DrawShapePoint(mach, pixelinfo, shape);

        while(err > -1) {
            if(horizontal)
                pixelinfo->y += sy;
            else
                pixelinfo->x += sx;
            err -= 2 * dist_primary_axis;
        }

        if(horizontal)
            pixelinfo->x += sx;
        else
            pixelinfo->y += sy;
    }

    pixelinfo->dash_counter++;
    mq_casiowin_DrawShapePoint(mach, pixelinfo, shape);
}

void mq_casiowin_DrawShapeRect(
    mqMachine *mach, struct mqCasiowin_TShapePixelInfo *pixelinfo,
    struct mqCasiowin_TShape const *shape)
{
    mq_log(MQ_LOG_ERROR, "DrawShapeRect: TODO");
    mq_machine_setStuck(mach);
}

void mq_casiowin_DrawShapeCircle(
    mqMachine *mach, struct mqCasiowin_TShapePixelInfo *pixelinfo,
    struct mqCasiowin_TShape const *shape)
{
    mq_log(MQ_LOG_ERROR, "DrawShapeCircle: TODO");
    mq_machine_setStuck(mach);
}
