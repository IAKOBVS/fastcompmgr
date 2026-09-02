
#include "comp_rect.h"



/// Returns true, if r1 fully contains r2
static bool rect_contains(CompRect* r1, CompRect* r2){
    return r1->x1 <= r2->x1 && r1->y1 <= r2->y1 &&
            r1->x2 >= r2->x2 && r1->y2 >=r2->y2;
}


static bool rects_are_intersecting(CompRect* r1, CompRect* r2)
{
    // if the left point of one rect is greater
    // than the right one of the other, nothing intersects.
    if(r1->x1 > r2->x2 || r2->x1 > r1->x2){
        return false;
    }
    if(r1->y1 > r2->y2 || r2->y1 > r1->y2){
        return false;
    }
    return true;
}

/// Fill `out` with the intersection of r1 and r2. Returns false if the rects
/// do not overlap (out is left untouched). Touching rects count as
/// intersecting but yield a zero width or height intersection.
bool rect_intersect(CompRect* r1, CompRect* r2, CompRect* out){
    if(! rects_are_intersecting(r1, r2)){
        return false;
    }
    out->x1 = (r1->x1 > r2->x1) ? r1->x1 : r2->x1;
    out->y1 = (r1->y1 > r2->y1) ? r1->y1 : r2->y1;
    out->x2 = (r1->x2 < r2->x2) ? r1->x2 : r2->x2;
    out->y2 = (r1->y2 < r2->y2) ? r1->y2 : r2->y2;
    out->w = out->x2 - out->x1;
    out->h = out->y2 - out->y1;
    return true;
}

/// Check if we can omit painting a window (rect). E.g., a window
/// completely occluded by another one, does not need to be
/// painted. Further, we try to select the largest possible ignore region
/// Based on window and intersection areas.
bool rect_paint_needed(CompRect* ignore_reg, CompRect* reg){
    if(rect_contains(ignore_reg, reg)){
        // the ignore-region completely occludes the window.
        return false;
    }
    if(! rects_are_intersecting(ignore_reg, reg)){
        // KISS and just use the greater rect as new ignore region.
        if(reg->w*reg->h > ignore_reg->w*ignore_reg->h){
            *ignore_reg = *reg;
        }
        return true;
    }

    CompRect r_intersect;
    rect_intersect(ignore_reg, reg, &r_intersect);

    // KISS and just use the biggest rect as new ignore rect
    if(reg->w*reg->h > ignore_reg->w*ignore_reg->h){
        *ignore_reg = *reg;
    }
    if(r_intersect.w*r_intersect.h > ignore_reg->w*ignore_reg->h){
        *ignore_reg = r_intersect;
    }
    return true;
}
