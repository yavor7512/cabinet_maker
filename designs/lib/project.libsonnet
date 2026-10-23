// Composition and validation only; no cabinet geometry lives here.
local objectSections = ['parameters', 'assemblies', 'frames', 'boards', 'hardware_profiles'];
local arraySections = ['cuts', 'hardware', 'checks'];
local empty = { [s]: {} for s in objectSections } + { [s]: [] for s in arraySections };
local unique(a, b, section) =
  assert std.length(std.setInter(std.objectFields(a), std.objectFields(b))) == 0
    : 'Duplicate name in ' + section;
  a + b;
{
  fields(value, allowed):
    assert std.type(value) == 'object' : 'Expected a configuration object';
    assert std.length(std.setDiff(std.objectFields(value), std.sort(allowed))) == 0
      : 'Unknown configuration field(s): ' + std.join(', ', std.setDiff(std.objectFields(value), std.sort(allowed)));
    true,

  project(components, aliases={}, order=null):
    local result = std.foldl(function(acc, component)
      assert self.fields(component, objectSections + arraySections);
      local c = empty + component;
      { [s]: unique(acc[s], c[s], s) for s in objectSections } +
      { [s]: acc[s] + c[s] for s in arraySections }, components, empty);
    result + { schema_version: 1, parameter_aliases: aliases } +
    (if order == null then {} else { assembly_order: order }),
}
