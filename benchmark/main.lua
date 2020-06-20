Position = qb.component_create({
  x = 'number',
  y = 'number',
})

Velocity = qb.component_create({
  x = 'number',
  y = 'number', 
})

for i=1,100000 do
  qb.entity_create({
    Position:new{ 0, 0 },
    Velocity:new{ 1, 0 },
  })
end

qb.system_create({ Position, Velocity },
  function (pos, vel)
    pos.x = pos.x + vel.x
    pos.y = pos.y + vel.y
  end)
