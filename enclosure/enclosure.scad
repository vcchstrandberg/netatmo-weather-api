// ============================================================
// Netatmo Weather Display Enclosure
// ============================================================
//
// Two printed parts:
//   body  — back shell, screws to wall with 4 × M4 countersunk holes
//   lid   — front face with OLED window, clips into body lip
//
// Hardware layout (inside body, viewed from front):
//
//   ┌──────────────────────────────────────────┐
//   │  ┌─────────────────────────────────┐     │
//   │  │     Arduino Uno R4 WiFi         │     │
//   │  │     68.58 × 53.34 mm            │     │
//   │  │     5 × M3 standoffs            │     │
//   │  └─────────────────────────────────┘     │
//   │  ┌──────────────────┐ ┌──────────────┐   │
//   │  │  LiPo battery    │ │ USB-C charge │◄──┤ USB-C port on right wall
//   │  │  ~50 × 34 × 7 mm│ │ module ~25×18│   │
//   │  └──────────────────┘ └──────────────┘   │
//   └──────────────────────────────────────────┘
//
// Note: The Arduino's own USB-C (for programming/serial) is on the top
// edge of the PCB. Open the lid to reflash; no separate cutout included.
//
// Print settings: 0.2 mm layer height, 3 perimeters, 20 % infill, PETG or PLA
// ============================================================

// ── Tolerances & shell geometry ─────────────────────────────
wall       = 2.5;    // shell wall thickness
corner_r   = 4.0;    // outer corner fillet radius
lip_inset  = 1.5;    // lid lip inset from inner wall face
lip_h      = 4.5;    // lid lip height (drops into body opening)
clearance  = 0.25;   // lid-lip printing tolerance (gap on each side)

// ── Internal cavity ─────────────────────────────────────────
inner_w    = 114;    // width  — Arduino (68.6) + battery + margins
inner_h    = 82;     // height — Arduino (53.4) + battery row + margins
inner_d    = 36;     // depth  — Arduino headers (~12 mm) + battery (7 mm) + wiring

outer_w    = inner_w + 2 * wall;
outer_h    = inner_h + 2 * wall;
outer_d    = inner_d + wall;   // back wall only; front is open for lid

// ── OLED window (SSD1306 128×64, 0.96") ─────────────────────
// Visible display area ≈ 26.5 × 14.7 mm; give 1.5 mm border all round
oled_win_w = 30;
oled_win_h = 18;
oled_cx    = outer_w / 2;                         // centred horizontally
oled_cy    = outer_h - wall - 7 - oled_win_h / 2; // 7 mm gap from top edge

// OLED PCB M2 mounting hole pitch: 32 × 23 mm, centred on window
oled_mnt_dx = 16;    // half of 32 mm
oled_mnt_dy = 11.5;  // half of 23 mm

// ── Arduino standoffs ────────────────────────────────────────
// PCB centred left/right; top of PCB 4 mm from inner top
ard_ox  = wall + (inner_w - 68.58) / 2;
ard_oy  = wall + inner_h - 53.34 - 4;
ard_h   = 5;    // standoff height above inner floor
ard_od  = 6;    // standoff outer diameter
// Mounting holes in mm from PCB bottom-left corner (USB-C edge = top)
ard_holes = [
    [ 2.54,  2.54],   // bottom-left
    [66.04,  7.62],   // bottom-right
    [ 2.54, 50.80],   // top-left
    [66.04, 35.56]    // top-right
];

// ── Wall-mount holes (M4 countersunk) ────────────────────────
mnt_r      = 2.3;    // M4 clearance drill radius
mnt_csk_r  = 4.6;    // countersink radius (fits M4 flat-head)
mnt_csk_h  = 3.5;    // countersink depth
mnt_margin = 8;      // hole centre distance from outer edge

// ── USB-C charger port (right side wall) ─────────────────────
// Charge module sits bottom-right. USB-C receptacle: ≈ 9 × 3.2 mm opening.
// Slot slightly oversized for easy connector insertion.
usbc_slot_w  = 11;   // slot width  (along Y)
usbc_slot_h  = 5;    // slot height (along Z)
usbc_pos_y   = wall + 14;  // centre of slot from bottom of box
usbc_pos_z   = wall + 9;   // centre of slot from back of box

// ────────────────────────────────────────────────────────────
$fn = 48;

// Rounded-corner box: W × H in XY, extruded D along Z
module rbox(w, h, d, r) {
    linear_extrude(d)
        offset(r = r, $fn = 32)
            square([w - 2 * r, h - 2 * r]);
}

// Rounded-rectangle slot centred at origin.
// Width (w) along Y, height (h) along Z, depth along +X.
module usbc_slot(w, h, depth) {
    r = h / 2;
    hull()
        for (dy = [-(w / 2 - r), (w / 2 - r)])
            translate([0, dy, 0])
                rotate([0, 90, 0])
                    cylinder(r = r, h = depth, $fn = 24);
}

// ─────────────────────────────────────────────────────────────
// BODY (back shell)
// ─────────────────────────────────────────────────────────────
module body() {
    difference() {
        // Outer shell
        rbox(outer_w, outer_h, outer_d, corner_r);

        // Hollow interior (open at front, +Z direction)
        translate([wall, wall, wall])
            cube([inner_w, inner_h, inner_d + 1]);

        // Lid seat: step at the front opening that the lid lip drops into
        translate([wall + lip_inset, wall + lip_inset, outer_d - lip_h])
            cube([inner_w - 2 * lip_inset, inner_h - 2 * lip_inset, lip_h + 1]);

        // Wall-mount holes — 4 corners, countersunk flush to back face
        for (x = [mnt_margin, outer_w - mnt_margin])
        for (y = [mnt_margin, outer_h - mnt_margin])
            translate([x, y, 0]) {
                cylinder(r = mnt_r,    h = wall + 1);                    // through
                cylinder(r1 = mnt_csk_r, r2 = mnt_r, h = mnt_csk_h);   // countersink
            }

        // USB-C charger port cutout — right side wall
        translate([outer_w - wall - 1, usbc_pos_y, usbc_pos_z])
            usbc_slot(usbc_slot_w, usbc_slot_h, wall + 2);
    }

    // Arduino M3 standoffs (drill/tap M3 after printing, or use heat-set inserts)
    for (h = ard_holes)
        translate([ard_ox + h[0], ard_oy + h[1], wall])
            difference() {
                cylinder(r = ard_od / 2, h = ard_h);
                cylinder(r = 1.5, h = ard_h + 1);   // 3 mm pilot hole for M3 tap
            }
}

// ─────────────────────────────────────────────────────────────
// LID (front face)
// ─────────────────────────────────────────────────────────────
module lid() {
    lip_w = inner_w - 2 * (lip_inset + clearance);
    lip_h_actual = inner_h - 2 * (lip_inset + clearance);

    difference() {
        union() {
            // Face plate
            rbox(outer_w, outer_h, wall, corner_r);
            // Inset lip — snaps into body seat
            translate([wall + lip_inset + clearance, wall + lip_inset + clearance, wall])
                cube([lip_w, lip_h_actual, lip_h]);
        }

        // OLED display window
        translate([oled_cx - oled_win_w / 2, oled_cy - oled_win_h / 2, -1])
            cube([oled_win_w, oled_win_h, wall + lip_h + 2]);

        // OLED PCB M2 mounting holes (32 × 23 mm pitch)
        for (dx = [-oled_mnt_dx, oled_mnt_dx])
        for (dy = [-oled_mnt_dy, oled_mnt_dy])
            translate([oled_cx + dx, oled_cy + dy, -1])
                cylinder(r = 1.1, h = wall + lip_h + 2, $fn = 16);

        // M2 lid retention screws — 4 positions inside the lip corners
        for (x = [wall + lip_inset + clearance + 4, outer_w - wall - lip_inset - clearance - 4])
        for (y = [wall + lip_inset + clearance + 4, outer_h - wall - lip_inset - clearance - 4])
            translate([x, y, -1])
                cylinder(r = 1.1, h = wall + lip_h + 2, $fn = 16);
    }
}

// ─────────────────────────────────────────────────────────────
// Render — body on left, lid on right (exploded view)
// Remove the translate to render only one part for slicing.
// ─────────────────────────────────────────────────────────────
body();
translate([outer_w + 20, 0, 0]) lid();
