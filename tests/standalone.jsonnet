// An unrelated one-board assembly proves that the engine/Makefile need no
// knowledge of drawers, carcasses, production aliases or fixed assembly IDs.
local wood = import '../designs/lib/joinery.libsonnet';
wood.project([
  { assemblies: { shelf: { name: 'Étagère shelf', filename: 'display shelf.step' } } },
  wood.panel('shelf.board', {
    assembly: 'shelf', name: 'Shelf board', number: 1,
    size: [300, 400, 18],
    align: { left_x: -20, front_y: 30, bottom_z: 100 },
    offset: [0, 0, 0],
  }),
])
