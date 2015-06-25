local N = 900999

local coro
coro = function()
    local inner
    inner = function( n )
       -- print( n, 'hello from coroutine!' )
       coroutine.yield()
       return inner( n + 1 )
    end
    return inner(1000)
end

local co = coroutine.create( coro )

local testme
testme = function( n )
   coroutine.resume(co)
   -- print (n, 'returned from coroutine' )
   if (n < N ) then
      return testme( n + 1 )
   else
      print( "Reached final value=", n )
   end
end

testme(1)
