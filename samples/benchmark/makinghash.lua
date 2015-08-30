

local function make_closure( a, b )
   return { a = a, b = b }
end

local sum = 0
local function makeme( i )
   local thenext = i + 1
   local fn = make_closure( i, thenext )
   sum = sum + fn.a + fn.b
   if i < 499000 then
      return makeme( thenext )
   end
end

makeme(1)
print(sum)

