local wood = import '../designs/lib/joinery.libsonnet';
local production = import '../designs/cabinet.jsonnet';
local drawer = {
  name: 'Reusable drawer', board_start: 1, width: 477, depth: 523, height: 102,
  at: [0, 12, 0], wall: 12,
  bottom: { thickness: 12, dado: 6, offset: 6, clearance: 0 },
  pocket: { profile: 'drawer_screw', count: 2, lower: 35, upper: 65, angle: 15 },
};
local hardware = {
  parameters: { [k]: production.parameters[k] for k in std.objectFields(production.parameters)
                if std.startsWith(k, 'drawer_screw_') },
  hardware_profiles: production.hardware_profiles,
};
function(case)
  if case == 'repeated' then
    wood.project([wood.drawer('sample' + i, drawer + {
      board_start: 1 + 5 * i, height: 102 + 20 * i, at: [0, 12, 200 * i],
    }) for i in std.range(0, 2)] + [hardware])
  else if case == 'typo' then
    wood.project([wood.drawer('sample', drawer + { widht: 500 }), hardware])
  else if case == 'duplicate' then
    wood.project([wood.drawer('sample', drawer), wood.drawer('sample', drawer), hardware])
  else if case == 'alternate-joinery' then
    local carcass = wood.carcass('sample', {
      name: 'Caps over sides', board_start: 1, frame: 'opening',
      opening: [494, 737], depth: 597, stock: 18, stile: [38, 18], at: [0, 0, 0],
    });
    // Override the reusable recipe externally: caps cover the side end grain.
    wood.project([carcass + { boards: carcass.boards + {
      ['sample.' + cap]: carcass.boards['sample.' + cap] + {
        between: carcass.boards['sample.' + cap].between + {
          x: ['opening.left_x - sample_stock', 'opening.right_x + sample_stock'],
        },
      } for cap in ['top', 'bottom']
    } + {
      ['sample.' + side]: carcass.boards['sample.' + side] + {
        between: carcass.boards['sample.' + side].between + {
          z: ['sample.bottom.top_z', 'sample.top.bottom_z'],
        },
      } for side in ['left', 'right']
    } }])
  else error 'Unknown template test case: ' + case
