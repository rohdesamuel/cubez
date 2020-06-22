function qb.start(width, height)
  Position = qb.component_create('Position', {
    { x = 'number' },
    { y = 'number' },
  })

  Velocity = qb.component_create('Velocity', {
    { x = 'number' },
    { y = 'number' },
  })

  ball = qb.entity_create({
    Position:new{ x=0, y=0 },
    Velocity:new{ x=1, y=0 },
  })

  for i=1, 500000 do
    qb.entity_create({
      Position:new{ x=0, y=0 },
      Velocity:new{ x=1, y=0 },
    })
  end

  qb.system_create({ Position, Velocity },
    function (pos, vel)
    end)
end

function qb.init()
  print('on init')
end

function qb.update()
end