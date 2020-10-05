function qb.start(width, height)
  print('on start')

  BallTag = qb.component.create('BallTag', {})

  opts = {
    oncreate=function (entity, pos)
      print(entity, pos)
      print(entity:has(Velocity))
    end
  }

  Position = qb.component.create('Position', {
    { x='number' },
    { y='number' },
  }, opts)

  Velocity = qb.component.create('Velocity', {
    { x='number' },
    { y='number' },
  })


  --[[
  Position:oncreate(
    function(entity, pos)
      print(entity.id, pos)
    end)

  pos = ball:get(Position)

  --]]

  Name = qb.component.create('Name', {
    { name='string' },
  })

  ball = qb.entity.create(
    Position:create{ 10, 555 },
    Velocity:create{ x=1, y=3 }
  )

  ball:add(Name:create{ 'ball' })

  ball_name = ball:get(Name)
  print('ball name is ', ball_name.name)
  ball_name.name = 'Hello, World!'
  print(ball_name.name)

  ball_pos = ball:get(Position)
  print(ball_pos)
  print(ball_pos.x)
  ball_pos.x = 100
  print(ball_pos.x)


  ball:add(BallTag:create{})
  
  for i=1, 50 do
    qb.entity.create(
      Position:create{ x=0, y=0 }
    )
  end
  
  print('There are', BallTag:count(), 'balls')

  qb.system.create({ Position, Velocity },
    function (entity, pos, vel)
      pos.x = pos.x + vel.x
      pos.y = pos.y + vel.y
    end)

  qb.system.create({ Position },
    function (entity, pos)
      if not entity:has(Velocity) then
        entity:add(Velocity:create{ x=0, y=1 })
      else
        entity:remove(Velocity)
      end
    end)
    
  focused_el = qb.gui.getfocus()

  x, y = qb.mouse.getposition()
  focused_at = qb.gui.getfocusat(x, y)

  el = qb.gui.element.create('something', {
    backgroundcolor={1.0, 0.0, 0.0, 0.9},
    radius=5,
  })

  el = qb.gui.element.find('something')
  el:setconstraint(
    { 'x', 'pixel', 500 },
    { 'y', 'pixel', 500 },
    { 'width', 'pixel', 100 },
    { 'height', 'pixel', 100 }
  )

  function el:onfocus ()
    print('onfocus')
  end

  function el:onclick ()
    print('onclick')
  end

  function el:onscroll ()
    print('onscroll')
  end

  function el:onmove ()
    print('onmove')
  end

  function el:onkey ()
    print('onkey')
  end

  function el:onopen ()
    print('onopen')
  end

  function el:onclose ()
    print('onclose')
  end

  function el:onsetvalue (old, new)
    return new
  end

  function el:ongetvalue (val)
    return val
  end

  print(el:getvalue())
  el:setvalue('Hello, world!')
  print(el:getvalue())

end

-- Called on every initialization of a new Lua State in the engine. This occurs
-- when a new QB `Program` is created.
function qb.init()
  print('on init')
  threads = {}
end

function qb.update()
  if qb.keyboard.ispressed('space') then
    qb.entity.create({
      Position:create{ x=0, y=0 },
      Velocity:create{ x=1, y=0 },
      BallTag:create{}
    })
    print('There are', BallTag:count(), 'balls')
  end  
end