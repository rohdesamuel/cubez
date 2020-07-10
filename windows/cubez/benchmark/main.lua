function qb.start(width, height)
  Position = qb.component_create('Position', {
    { x = 'number' },
    { y = 'number' },
    { t = 'table' },
  })

  Velocity = qb.component_create('Velocity', {
    { x = 'number' },
    { y = 'number' },
  })

  for i=1, 1 do
    qb.entity_create({
      Position:new{ x=0, y=0, t={a=100} },
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