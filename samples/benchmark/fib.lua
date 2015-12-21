

function fibR(n)
if (n < 2) then return n end
return (fibR(n-2) + fibR(n-1))
end

print(fibR(34))
