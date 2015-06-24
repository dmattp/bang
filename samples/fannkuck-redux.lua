-- The Computer Language Benchmarks Game
-- http://benchmarksgame.alioth.debian.org/
-- contributed by Mike Pall

local function fannkuch(n)
   local p, q, s, sign, maxflips, sum = {}, {}, {}, 1, 0, 0
   for i=1,n do p[i] = i; q[i] = i; s[i] = i end
   repeat
      -- Copy and flip.
      local q1 = p[1]				-- Cache 1st element.
--      print(string.format('stack=0 q1=%d', q1))
      if q1 ~= 1 then
--         print(string.format( "a02 [%s %s %s %s]", q[1], q[2], q[3], q[4]))
--         print(string.format( "a02.p [%s %s %s %s]", p[1], p[2], p[3], p[4]))
         for i=2,n do q[i] = p[i] end		-- Work on a copy.
--         print(string.format( "stack=0 a02b [%s %s %s %s]", q[1], q[2], q[3], q[4]))
         local flips = 1
         repeat -- :flipper-loop
            local qq = q[q1]
--            print(string.format("stack=0 q1=%d qq=%d sum=%d sign=%d flips=%d", q1, qq, sum, sign, flips))
            if qq == 1 then				-- ... until 1st element is 1.
               sum = sum + sign*flips
               if flips > maxflips then maxflips = flips end -- New maximum?
               break
            end
            q[q1] = q1
--            print(string.format( "stack=0 a03 [%s %s %s %s]", q[1], q[2], q[3], q[4]))
            if q1 >= 4 then
               local i, j = 2, q1 - 1
               repeat q[i], q[j] = q[j], q[i]; i = i + 1; j = j - 1; until i >= j
            end
--            print(string.format('stack=0 flips=%d q1=%d qq=%d', flips, q1, qq))
            q1 = qq; flips = flips + 1
--            print(string.format( "a01 [%s %s %s %s]", q[1], q[2], q[3], q[4]))
         until false
--         print('stack=0 exited flipper loop!')
      end
--      print('loopish')
      -- Permute.
--      print(string.format('stack=0 sign=%d', sign))
      if sign == 1 then
         p[2], p[1] = p[1], p[2]; sign = -1	-- Rotate 1<-2.
      else
         p[2], p[3] = p[3], p[2]; sign = 1		-- Rotate 1<-2 and 1<-2<-3.
         for i=3,n do
            local sx = s[i]
--            print(string.format('stack=0      i = %s sx=%s',i,sx))
            if sx ~= 1 then s[i] = sx-1; break end
            if i == n then print 'got exit'; return sum, maxflips end	-- Out of permutations.
            s[i] = i
            -- Rotate 1<-...<-i+1.
            local t = p[1]; for j=1,i do p[j] = p[j+1] end; p[i+1] = t
--            print(string.format( "b01.p [%s %s %s %s]", p[1], p[2], p[3], p[4]))
            
         end
      end
   until false
end

local n = tonumber(arg and arg[1]) or 9
local sum, flips = fannkuch(n)
io.write(sum, "\nPfannkuchen(", n, ") = ", flips, "\n")

-- lua523r time for '9' on my box = 970ms
