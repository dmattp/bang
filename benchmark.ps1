(
((measure-command  { .\bang .\samples\n-body.bang 12000 }).totalmilliseconds / (measure-command  { lua523r .\samples\n-body.lua 12000 }).totalmilliseconds) +
((measure-command  { .\bang .\samples\spectral.bang 150 }).totalmilliseconds / (measure-command  { lua523r .\samples\spectral.lua 150 }).totalmilliseconds) +
((measure-command  { .\bang .\samples\binary-trees.bang 12 }).totalmilliseconds / (measure-command  { lua523r .\samples\binary-trees.lua 12 }).totalmilliseconds) +
((measure-command  { .\bang .\samples\fannkuck-redux.bang 8 }).totalmilliseconds / (measure-command  { lua523r .\samples\fannkuck-redux.lua 8 }).totalmilliseconds)
) / 4