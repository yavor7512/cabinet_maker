// Reusable construction recipes. Edit these relationships to change joinery;
// C++ only receives ordinary boards, cuts and hardware operations.
local core = import 'project.libsonnet';
local drawers = import 'drawer.libsonnet';
local carcasses = import 'carcass.libsonnet';
{
  project: core.project,
  drawer: drawers.drawer,
  carcass: carcasses.carcass,

  panel(id, c):
    local p = std.strReplace(id, '.', '_') + '_';
    local axes = ['x', 'y', 'z'];
    local anchors = [['left_x', 'center_x', 'right_x'],
                     ['front_y', 'center_y', 'rear_y'],
                     ['bottom_z', 'center_z', 'top_z']];
    assert core.fields(c, ['assembly', 'name', 'number', 'size', 'align', 'offset', 'overlaps']);
    assert std.length(c.size) == 3 && std.length(c.offset) == 3 : 'panel needs three sizes and offsets';
    assert core.fields(c.align, std.flattenArrays(anchors));
    local selected = [[a for a in anchors[i] if std.objectHas(c.align, a)] for i in std.range(0, 2)];
    assert std.all([std.length(a) == 1 for a in selected]) : 'panel needs one alignment on each axis';
    {
      parameters:
        { [p + axes[i]]: c.size[i] for i in std.range(0, 2) } +
        { [p + 'offset_' + axes[i]]: c.offset[i] for i in std.range(0, 2) } +
        { [p + selected[i][0]]: c.align[selected[i][0]] for i in std.range(0, 2) },
      boards: {
        [id]: {
          assembly: c.assembly, name: c.name, number: c.number,
          size: { [axes[i]]: p + axes[i] for i in std.range(0, 2) },
          align: { [selected[i][0]]: p + selected[i][0] + ' + ' + p + 'offset_' + axes[i]
                   for i in std.range(0, 2) },
        },
      },
      checks: [
        {
          value: 'min(' + id + '.' + a[2] + ', ' + c.overlaps + '.' + a[2] + ') - max(' +
                 id + '.' + a[1] + ', ' + c.overlaps + '.' + a[1] + ')',
          min: 0.001, message: id + ' must overlap ' + c.overlaps + ' on ' + a[0] + '.',
        }
        for a in [['X', 'left_x', 'right_x'], ['Z', 'bottom_z', 'top_z']]
        if std.objectHas(c, 'overlaps')
      ],
    },

  pocketScrew(id, c):
    local fields = ['length', 'diameter', 'head_diameter', 'head_height', 'tip_length',
                    'pocket_diameter', 'seat_distance', 'edge_clearance', 'tip_clearance'];
    assert core.fields(c, fields);
    {
      parameters: { [id + '_' + key]: c[key] for key in fields },
      hardware_profiles: { [id]: { [key]: id + '_' + key for key in fields } },
    },
}
