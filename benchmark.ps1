$minbang = 99999
$minlua = 99999

for ($i=0;$i -lt 9;$i++) {
    $tbang = (measure-command  { .\bang .\samples\multi.bang }).totalmilliseconds
    $tlua = (measure-command  { lua523r .\samples\multi.lua }).totalmilliseconds
    if ($tbang -lt $minbang) { $minbang = $tbang }
    if ($tlua -lt $minlua) { $minlua = $tlua }
    echo "bang=$tbang lua=$tlua"
}

echo "minbang=$minbang minlua=$minlua factor=$($minbang/$minlua)"



## (
## ((measure-command  { .\bang .\samples\n-body.bang 12000 }).totalmilliseconds / (measure-command  { lua523r .\samples\n-body.lua 12000 }).totalmilliseconds) +
## ((measure-command  { .\bang .\samples\spectral.bang 150 }).totalmilliseconds / (measure-command  { lua523r .\samples\spectral.lua 150 }).totalmilliseconds) +
## ((measure-command  { .\bang .\samples\binary-trees.bang 12 }).totalmilliseconds / (measure-command  { lua523r .\samples\binary-trees.lua 12 }).totalmilliseconds) +
## ((measure-command  { .\bang .\samples\fannkuck-redux.bang 8 }).totalmilliseconds / (measure-command  { lua523r .\samples\fannkuck-redux.lua 8 }).totalmilliseconds)
## ) / 4