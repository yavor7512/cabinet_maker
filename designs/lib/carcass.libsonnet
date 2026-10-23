local core = import 'project.libsonnet';
{
  // Full-height sides outside the opening; caps fit between them; two stiles.
  carcass(id, c):
    assert core.fields(c, ['name', 'filename', 'board_start', 'frame', 'opening', 'depth', 'stock', 'stile', 'at']);
    assert std.length(c.at) == 3 && std.length(c.opening) == 2 && std.length(c.stile) == 2 : 'Invalid carcass dimensions';
    local p = id + '_';
    local f = c.frame;
    local b(name) = id + '.' + name;
    local board(name, n, geometry) = { assembly: id, name: name, number: p + 'board_start + ' + n } + geometry;
    {
      parameters: {
        [p + 'opening_width']: c.opening[0], [p + 'opening_height']: c.opening[1],
        [p + 'depth']: c.depth, [p + 'stock']: c.stock,
        [p + 'stile_width']: c.stile[0], [p + 'stile_thickness']: c.stile[1],
        [p + 'x']: c.at[0], [p + 'y']: c.at[1], [p + 'z']: c.at[2], [p + 'board_start']: c.board_start,
      },
      assemblies: { [id]: { name: c.name, filename: if std.objectHas(c, 'filename') then c.filename else id + '.step' } },
      frames: { [f]: {
        size: { x: p + 'opening_width', y: p + 'depth', z: p + 'opening_height' },
        align: { left_x: p + 'x', front_y: p + 'y', bottom_z: p + 'z' },
      } },
      boards: {
        [b(side)]: board((if side == 'left' then 'Left' else 'Right') + ' carcass side', i, {
          size: { x: p + 'stock' },
          align: { [if side == 'left' then 'right_x' else 'left_x']: f + (if side == 'left' then '.left_x' else '.right_x') },
          between: {
            y: [f + '.front_y + ' + p + 'stile_thickness', f + '.rear_y'],
            z: [f + '.bottom_z - ' + p + 'stock', f + '.top_z + ' + p + 'stock'],
          },
        })
        for i in std.range(0, 1)
        for side in [if i == 0 then 'left' else 'right']
      } + {
        [b(cap)]: board('Carcass ' + cap, i + 2, {
          size: { z: p + 'stock' },
          align: { [if cap == 'bottom' then 'top_z' else 'bottom_z']: f + (if cap == 'bottom' then '.bottom_z' else '.top_z') },
          between: { x: [b('left.right_x'), b('right.left_x')], y: [b('left.front_y'), b('left.rear_y')] },
        })
        for i in std.range(0, 1)
        for cap in [if i == 0 then 'bottom' else 'top']
      } + {
        [b(side + '_stile')]: board((if side == 'left' then 'Left' else 'Right') + ' face-frame stile', i + 4, {
          size: { x: p + 'stile_width', y: p + 'stile_thickness' },
          align: {
            [if side == 'left' then 'right_x' else 'left_x']: f + (if side == 'left' then '.left_x' else '.right_x'),
            rear_y: b(side + '.front_y'),
          },
          between: { z: [b(side + '.bottom_z'), b(side + '.top_z')] },
        })
        for i in std.range(0, 1)
        for side in [if i == 0 then 'left' else 'right']
      },
      checks: [{ value: p + 'stile_width - ' + p + 'stock', min: 0,
                 message: id + ': stiles must be at least as wide as carcass thickness.' }],
    },
}
