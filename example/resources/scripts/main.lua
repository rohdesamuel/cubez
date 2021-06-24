function qb.start(width, height)
  print('on start')

  sound = qb.audio.loadwav('jump.wav')

  BallTag = qb.component.create('BallTag', {})

  Position = qb.component.create('Position', {
    { x='number' },
    { y='number' },
  }, opts)

  function Position:oncreate(entity, pos)
    print('Position[' .. entity.id .. '] (' .. pos.x .. ', ' .. pos.y .. ')')
  end
  
  Velocity = qb.component.create('Velocity', {
    { x='number' },
    { y='number' },
  })

  function Velocity:ondestroy(entity, vel)
    --print('Velocity[' .. entity.id .. '] (' .. vel.x .. ', ' .. vel.y .. ')')
  end

  Name = qb.component.create('Name', {
    { name='string' },
  })

  SomeArray = qb.component.create('SomeArray', {
    { map='map[string, number]' },
    { array='array[any]' },
  })

  ball = qb.entity.create(
    SomeArray:create{map={a=0}, array={20, 2}},
    Position:create{ math.random(100), 555 },
    Velocity:create{ x=1, y=3 }
  )

  ball:add(Name:create{ 'ball' })

  ball_name = ball:get(Name)
  print('ball name is ', ball_name.name)
  ball_name.name = 'Hello, World!'
  print(ball_name.name)

  ball_pos = ball:get(Position)
  print(ball_pos.x)
  ball_pos.x = 100
  print(ball_pos.x)

  ball_somearray = ball:get(SomeArray)
  map = ball_somearray.map
  print('arr.map["a"] = ', map['a'])
  map['a'] = 1
  print('arr.map["a"] = ', map['a'])

  arr = ball_somearray.array
  print('arr.arr[1] = ', arr[1])
  some_table = {c=ball}
  arr[1] = {b=some_table}
  print('arr.arr[1] = ', arr[1])
  a = arr[1]
  print(arr[1].b.c)

  ball:add(BallTag:create{})
  
  for i=1, 50 do
    qb.entity.create(
      Position:create{ x=math.random(100), y=math.random(100) }
    )
  end
  
  print('There are', BallTag:count(), 'balls')

  qb.system.create({ Position, Velocity },
    function (entity, pos, vel)
      pos.x = pos.x + vel.x
      pos.y = pos.y + vel.y
    end)
--[[
  qb.system.create({ Position },
    function (entity, pos)
      if not entity:has(Velocity) then
        entity:add(Velocity:create{ x=0, y=1 })
      else
        entity:remove(Velocity)
      end
    end)
--]]

  focused_el = qb.gui.getfocus()

  x, y = qb.mouse.getposition()
  focused_at = qb.gui.getfocusat(x, y)

  --[[
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
    return true
  end

  function el:onclick ()
    print('onclick')
    return true
  end

  function el:onscroll ()
    print('onscroll')
    return true
  end

  function el:onmove ()
    print('onmove')
    return true
  end

  function el:onkey ()
    print('onkey')
    return true
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

  --]]

  adventurer_sheet = qb.sprite.loadsheet('Adventurer/adventurer-Sheet.png', 50, 37, 0)
  adventurer_sheet:setoffset(25, 0)

  idle_animation = qb.animation.fromsheet(adventurer_sheet, 0, 4, 300, true):play()  
  running_animation = qb.animation.fromsheet(adventurer_sheet, 8, 14, 100, true):play()
  jumping_animation = qb.animation.fromsheet(adventurer_sheet, 14, 24, 85, false):play()
  landing_animation = qb.animation.fromsheet(adventurer_sheet, 14, 16, 200, false):play()
  crouching_animation = qb.animation.fromsheet(adventurer_sheet, 4, 8, 400, true):play()

  current_animation = nil

  player = qb.entity.create()
  player:add(Position:create{x=0, y=0})
  facing = 1

end

-- Called on every initialization of a new Lua State in the engine. This occurs
-- when a new QB `Program` is created.
function qb.init()
  print('on init')
  threads = {}
end

function qb.update()
  local player = player
  local pos = player:get(Position)
  
  if qb.keyboard.ispressed('left') then
    pos.x = pos.x - 3
    facing = -1
    if current_animation ~= jumping_animation and current_animation ~= landing_animation then
      current_animation = running_animation      
    end
  elseif qb.keyboard.ispressed('right') then
    pos.x = pos.x + 3
    facing = 1
    if current_animation ~= jumping_animation and current_animation ~= landing_animation then
      current_animation = running_animation
    end
  elseif qb.keyboard.ispressed('down') then
    current_animation = crouching_animation
  elseif current_animation ~= jumping_animation and current_animation ~= landing_animation then
    current_animation = idle_animation
  end

  if qb.keyboard.ispressed('up') then
    pos.y = pos.y - 3
  end

  if qb.keyboard.ispressed('down') then
    --pos.y = pos.y + 3
  end

  if qb.keyboard.ispressed('space') then
    current_animation = jumping_animation
  end

  if current_animation == jumping_animation then
    if current_animation:getframe() >= current_animation:framecount() - 1 then
      current_animation:setframe(0)
      current_animation = landing_animation
    end
  end

  if current_animation == landing_animation then
    if current_animation:getframe() >= current_animation:framecount() - 1 then
      current_animation:setframe(0)
      current_animation = idle_animation
    end
  end

  if qb.keyboard.ispressed('space') then
    --sound:play()
    --sound:setvolume(1)
    qb.entity.create(
      Position:create{ x=0, y=0 },
      Velocity:create{ x=1, y=0 },
      BallTag:create{}
    )
    print('There are', BallTag:count(), 'balls')
  end  
end

function qb.draw()
  local animated = animated
  local player = player
  local pos = player:get(Position)

  local scale_x = 4 * facing
  if current_animation then
    current_animation:draw_ext(pos.x, pos.y, scale_x, 4, 0, {})
  end

  --adventurer_sheet:drawpart(0, 0, pos.x, pos.y, qb.window.width(), qb.window.height())
end

function qb.resize(width, height)
  print(width, height)
end