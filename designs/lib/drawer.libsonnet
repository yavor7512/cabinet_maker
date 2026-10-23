local core = import 'project.libsonnet';
{
  // Full-depth sides; ends fit between sides; bottom penetrates four dados.
  drawer(id, c):
    assert core.fields(c, ['name', 'filename', 'board_start', 'width', 'depth', 'height', 'at', 'wall', 'bottom', 'pocket', 'attachments']);
    assert core.fields(c.bottom, ['thickness', 'dado', 'offset', 'clearance']);
    assert core.fields(c.pocket, ['profile', 'count', 'lower', 'upper', 'angle']);
    assert std.length(c.at) == 3 : 'drawer.at needs [X, Y, Z]';
    local p = id + '_';
    local f = id + '_box';
    local b(name) = id + '.' + name;
    local board(name, n, geometry) = { assembly: id, name: name, number: p + 'board_start + ' + n } + geometry;
    {
      parameters: {
        [p + 'width']: c.width, [p + 'depth']: c.depth, [p + 'height']: c.height,
        [p + 'x']: c.at[0], [p + 'y']: c.at[1], [p + 'z']: c.at[2],
        [p + 'board_start']: c.board_start, [p + 'wall']: c.wall,
        [p + 'bottom_thickness']: c.bottom.thickness, [p + 'dado']: c.bottom.dado,
        [p + 'bottom_offset']: c.bottom.offset, [p + 'dado_clearance']: c.bottom.clearance,
        [p + 'screw_count']: c.pocket.count, [p + 'screw_lower']: c.pocket.lower,
        [p + 'screw_upper']: c.pocket.upper, [p + 'pocket_angle']: c.pocket.angle,
        [p + 'screw_spacing']: '(' + p + 'screw_upper - ' + p + 'screw_lower) / max(1, ' + p + 'screw_count - 1)',
      },
      assemblies: { [id]: {
        name: c.name,
        filename: if std.objectHas(c, 'filename') then c.filename else id + '.step',
        part_order: [b(s) for s in ['left', 'right', 'front', 'back', 'bottom']] + ['@hardware'] +
                    (if std.objectHas(c, 'attachments') then c.attachments else []),
      } },
      frames: { [f]: {
        size: { x: p + 'width', y: p + 'depth', z: p + 'height' },
        align: { left_x: p + 'x', front_y: p + 'y', bottom_z: p + 'z' },
      } },
      boards: {
        [b('left')]: board('Left side', 0, {
          size: { x: p + 'wall', y: f + '.depth', z: f + '.height' },
          align: { left_x: f + '.left_x', front_y: f + '.front_y', bottom_z: f + '.bottom_z' },
        }),
        [b('right')]: board('Right side', 1, {
          size: { x: p + 'wall', y: f + '.depth', z: f + '.height' },
          align: { right_x: f + '.right_x', front_y: f + '.front_y', bottom_z: f + '.bottom_z' },
        }),
        [b('front')]: board('Front wall', 2, {
          size: { y: p + 'wall', z: f + '.height' },
          align: { front_y: f + '.front_y', bottom_z: f + '.bottom_z' },
          between: { x: [b('left.right_x'), b('right.left_x')] },
        }),
        [b('back')]: board('Back wall', 3, {
          size: { y: p + 'wall', z: f + '.height' },
          align: { rear_y: f + '.rear_y', bottom_z: f + '.bottom_z' },
          between: { x: [b('left.right_x'), b('right.left_x')] },
        }),
        [b('bottom')]: board('Bottom panel', 4, {
          size: { z: p + 'bottom_thickness' },
          align: { bottom_z: f + '.bottom_z + ' + p + 'bottom_offset' },
          between: {
            x: [b('left.right_x') + ' - ' + p + 'dado', b('right.left_x') + ' + ' + p + 'dado'],
            y: [b('front.rear_y') + ' - ' + p + 'dado', b('back.front_y') + ' + ' + p + 'dado'],
          },
        }),
      },
      cuts: [{ type: 'dado', tool: b('bottom'), targets: [b(s) for s in ['left', 'right', 'front', 'back']],
               clearance: [0, 0, p + 'dado_clearance / 2'] }],
      // Generate the four mirrored corner joints from one pattern, retaining
      // their machining order and screw numbers in the exported assembly.
      hardware: [
        local front = corner < 2;
        local left = corner % 2 == 0;
        local end = if front then 'front' else 'back';
        local side = if left then 'left' else 'right';
        {
          type: 'pocket_screws', assembly: id, profile: c.pocket.profile,
          pocketed: b(end), receiver: b(side), obstacles: [b('bottom')],
          count: p + 'screw_count',
          origin: [b(side) + (if left then '.right_x' else '.left_x'), b(end) + '.center_y',
                   f + '.bottom_z + ' + p + 'screw_lower'],
          step: [0, 0, p + 'screw_spacing'],
          axis: [(if left then '-' else '') + 'cos_deg(' + p + 'pocket_angle)',
                 (if front then '' else '-') + 'sin_deg(' + p + 'pocket_angle)', 0],
          name: (if front then 'Front ' else 'Back ') + side + ' pocket screw',
          start_number: corner + ' * ' + p + 'screw_count + 1',
        }
        for corner in std.range(0, 3)
      ],
      checks: [
        { value: p + 'wall - ' + p + 'dado', min: 0.001, message: id + ': dado must leave wall material.' },
        { value: p + 'dado_clearance', min: 0, message: id + ': dado clearance cannot be negative.' },
        { value: p + 'pocket_angle', min: 0.001, max: 89.999, message: id + ': pocket angle must be between 0 and 90 degrees.' },
      ],
    },
}
