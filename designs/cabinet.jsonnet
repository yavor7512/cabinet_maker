// Production dimensions are millimetres; vectors are [X, Y, Z].
local wood = import 'lib/joinery.libsonnet';
local makeAliases = import 'lib/make-aliases.libsonnet';

// One construction specification, reused by both drawers.
local drawer = {
  width: 477, depth: 523, wall: 12,
  bottom: { thickness: 12, dado: 6, offset: 6, clearance: 0 },
  pocket: { profile: 'drawer_screw', count: 2, lower: 35, upper: 65, angle: 15 },
};

wood.project([
  wood.drawer('upper', drawer + {
    name: 'Upper inner drawer', board_start: 1,
    height: 102, at: [0, 12, 619],
  }),
  wood.drawer('lower', drawer + {
    name: 'Lower pull-out tray with tall front', board_start: 6,
    height: 203, at: [0, 12, 35],
    attachments: ['lower.face'],
  }),
  wood.panel('lower.face', {
    assembly: 'lower', name: 'Applied drawer face', number: 'lower_board_start + 5',
    size: [469.9, 'lower_wall', 711.2],
    align: {
      center_x: 'lower_box.center_x',
      rear_y: 'lower_box.front_y',  // The face touches the drawer front.
      top_z: 'upper_box.top_z',
    },
    offset: [0, 0, 0],
    overlaps: 'lower.front',
  }),
  wood.carcass('carcass', {
    name: 'Cabinet carcass and face frame', board_start: 12,
    frame: 'opening', opening: [494, 737], depth: 597, stock: 18,
    stile: [38, 18],
    at: ['lower_box.center_x - carcass_opening_width / 2', 3.5, 0],
  }),
  wood.pocketScrew('drawer_screw', {
    length: 19.05, diameter: 3.5,
    head_diameter: 5.5, head_height: 1.8, tip_length: 3,
    pocket_diameter: 7, seat_distance: 10.5,
    edge_clearance: 2, tip_clearance: 0.5,
  }),
], aliases=makeAliases)
