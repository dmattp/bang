

local function make_closure( a, b )
   return function() return a + b + 10 end
end

local sum = 0
--local function makeme( i )
for i = 1, 499000 do
   -- local thenext = i + 1
   --local fn = 
   sum = make_closure( i, i + 1 ) () + sum
--   if i < 499000 then
--      return makeme( thenext )
--   end
end

-- makeme(1)
print(sum)

