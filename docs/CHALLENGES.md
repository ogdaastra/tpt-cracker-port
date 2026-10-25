# Challenges

The challenge mode (Challenges… on the page, or the ★ button at the bottom of the second side-bar column
in the game) has 50 challenges in 9 categories. Each one builds a scene, offers only the listed tools (the
eraser always works, but not on the locked parts of the scene), limits how many particles you may place
(ink), and checks its goal after every frame. Stars: up to 3 for time and ink, minus one per hint used.
The definitions, each with a reference solution that the browser tests play, are in
[`src/lua/luascripts/challenges.lua`](../src/lua/luascripts/challenges.lua); this list is generated from it.

## Heat (6)

| Challenge | Difficulty | Goal | Tools | Ink | Time |
|---|---|---|---|---|---|
| Kettle | ●○○○○ | Boil the water: turn 90% of it into steam. | PLSM, FIRE, THRM, COAL | 400 | 90 s |
| Deep freeze | ●○○○○ | Freeze 90% of the water into ice. | LNTG, ICEI, SNOW | 1000 | 60 s |
| Ice dam | ●○○○○ | Melt through the ice dam so 500 particles of water reach the field on the right. | FIRE, PLSM, LAVA, SALT | 300 | 60 s |
| Cool it | ●●○○○ | Cool the red-hot iron block below 400 C. | WATR, ICEI, SNOW, LNTG | 3000 | 60 s |
| Heat relay | ●●○○○ | The furnace is far away. Bring its heat to the tank and boil 80% of the water. | HEAC, METL, TTAN, PIPE | 500 | 120 s |
| Glassworks | ●●●○○ | Make 50 particles of glass from the sand. | PLSM, THRM, FIRE, WATR | 1500 | 60 s |

## Electronics (8)

| Challenge | Difficulty | Goal | Tools | Ink | Time |
|---|---|---|---|---|---|
| First light | ●○○○○ | Light the LED. | BTRY, METL, PSCN | 150 | 30 s |
| Two lights | ●○○○○ | Light both LEDs from the one battery. | METL, PSCN | 500 | 30 s |
| Through the wall | ●●○○○ | Light the LED behind the insulating wall. | METL, PSCN, WIFI | 120 | 30 s |
| Flip the switch | ●●○○○ | The circuit is ready but its switch (SWCH) is off. Turn it on. | BTRY, PSCN, NSCN, METL | 30 | 30 s |
| Neon sign | ●●○○○ | Light up the tube of noble gas: make 150 particles of plasma. | BTRY, METL, PSCN | 60 | 30 s |
| Heat switch | ●●○○○ | The circuit has a gap of NTCT, which only conducts when hot. Light the LED. | FIRE, PLSM, LAVA, COAL | 100 | 30 s |
| Christmas lights | ●●○○○ | Light all five LEDs at the same time. | BTRY, METL, PSCN | 700 | 40 s |
| Electrolysis | ●●●○○ | Split water into hydrogen: make 100 particles of H2. | BTRY, METL, PSCN | 60 | 60 s |

## Explosives (6)

| Challenge | Difficulty | Goal | Tools | Ink | Time |
|---|---|---|---|---|---|
| Demolition | ●○○○○ | Destroy 95% of the brick tower. | BOMB, GUNP, FIRE | 120 | 45 s |
| Chain reaction | ●●○○○ | Set off all five C-4 charges with one battery. | FUSE, METL, BTRY | 800 | 60 s |
| Fireworks show | ●●○○○ | Fireworks! Make them burst higher than the two towers. | FIRW, FIRE | 150 | 30 s |
| Crater | ●●○○○ | Blast a hole through the brick shelf in the marked spot. | BOMB, PLEX, GUNP, FIRE | 60 | 30 s |
| Surgical strike | ●●●○○ | Knock down 90% of the tower, but keep 95% of the house next to it. | BOMB, PLEX, FIRE | 80 | 45 s |
| Hotter than the sun | ●●●●○ | Reach 6000 C anywhere, without plasma. | DEUT, PLUT, NEUT, URAN | 800 | 60 s |

## Chemistry and liquids (11)

| Challenge | Difficulty | Goal | Tools | Ink | Time |
|---|---|---|---|---|---|
| Fill it up | ●○○○○ | Fill the glass to the line. Don't spill! | WATR | 2200 | 30 s |
| Seawater | ●○○○○ | Turn 1000 particles of the fresh water into salt water. | SALT | 600 | 45 s |
| Etching | ●○○○○ | Dissolve 75% of the metal block with acid. | ACID | 2000 | 60 s |
| Salt flats | ●●○○○ | Get 100 particles of salt out of the salt water. | PLSM, FIRE, THRM, COAL | 400 | 90 s |
| Neutralise | ●●○○○ | Get rid of 90% of the acid. | BASE, WATR, SOAP | 1200 | 90 s |
| Rust bucket | ●●○○○ | Rust 100 particles of iron (it turns into breakable metal, BMTL). | WATR, SLTW, SALT, O2 | 2500 | 60 s |
| Make TNT | ●●○○○ | Make 150 particles of TNT (BANG) without setting it off. | NITR, OIL, WATR | 300 | 45 s |
| Oil spill | ●●○○○ | Get rid of 90% of the oil on the lake, and keep 80% of the water. | FIRE, PLSM, SOAP, WATR | 50 | 60 s |
| Lava to stone | ●●○○○ | Cool the lava pool until 90% of it has set into stone. | WATR, ICEI, SNOW, LNTG | 4000 | 60 s |
| Rocket fuel | ●●○○○ | Burn the hydrogen to make 150 particles of water (steam counts). | FIRE, PLSM | 20 | 40 s |
| Desalination | ●●●○○ | Make 150 particles of distilled water (DSTW) from the salt water. | PLSM, FIRE, ICEI, GLAS, TTAN | 900 | 120 s |

## Machines and force (5)

| Challenge | Difficulty | Goal | Tools | Ink | Time |
|---|---|---|---|---|---|
| Copy machine | ●○○○○ | Make 1000 particles of water. You have no water tool, only a few drops. | CLNE | 20 | 45 s |
| Portal plumbing | ●●○○○ | Move 300 particles of water into the high tank. | PRTI, PRTO | 60 | 60 s |
| Sorting | ●●●○○ | Separate the mix: 70% of the dust into the bin, and almost no water. | LIQD wall, POWDR wall, WALL wall | no limit | 60 s |
| Pressure cooker | ●●●○○ | Keep the pressure in the sealed chamber above 50 for 10 seconds. | PUMP, BTRY, PSCN, METL, HEAT tool | 60 | 40 s |
| Vacuum | ●●●○○ | Pull the pressure in the sealed chamber below -50 and keep it there for 5 seconds. | PUMP, BTRY, PSCN, METL, COOL tool | 60 | 40 s |

## Gravity and space (2)

| Challenge | Difficulty | Goal | Tools | Ink | Time |
|---|---|---|---|---|---|
| Black hole | ●○○○○ | Clean up: get rid of 90% of the dust cloud. | NBHL | 30 | 60 s |
| White hole | ●●○○○ | Push 90% of the dust off the screen. | NWHL | 30 | 60 s |

## Life and nature (6)

| Challenge | Difficulty | Goal | Tools | Ink | Time |
|---|---|---|---|---|---|
| Green thumb | ●○○○○ | Grow 400 particles of plant. | PLNT, WATR | 40 | 60 s |
| Glider | ●●○○○ | Build a Game of Life glider that flies into the box on the right. | GOL (Life) | 12 | 60 s |
| Outbreak | ●●○○○ | Stop the virus: keep it out of the town for 30 seconds. | WALL wall, SOAP, WATR | no limit | 30 s to hold |
| Plant a tree | ●●○○○ | Grow 150 particles of tree (wood and leaves) from a seed. | SEED, WATR | 600 | 90 s |
| Aquarium | ●●○○○ | The fish tank is dry. Keep at least 3 fish alive for 30 seconds. | WATR, DSTW | 3000 | 30 s to hold |
| Wildfire | ●●○○○ | A fire will start in the forest. Save 60% of the trees. | WATR, WALL wall, SAND | 2000 | 30 s to hold |

## Characters (2)

| Challenge | Difficulty | Goal | Tools | Ink | Time |
|---|---|---|---|---|---|
| Pest control | ●●○○○ | Get rid of the three fighters in the pit. | LAVA, ACID, FIRE, WATR | 700 | 45 s |
| Shelter | ●●○○○ | Lava will rain down soon. Keep the stickman alive for 30 seconds. | WALL wall, GLAS, STNE, INSL | 400 | 30 s to hold |

## Puzzles and records (4)

| Challenge | Difficulty | Goal | Tools | Ink | Time |
|---|---|---|---|---|---|
| Reach for the sky | ●○○○○ | Build a brick tower from the floor up to the flag, with no gaps. | BRCK | 300 | 30 s |
| Speed melt | ●●○○○ | Melt 90% of the iron block, as fast as you can. | PLSM, THRM, FIRE, LAVA | 1200 | 60 s |
| Ramp | ●●○○○ | Guide the falling sand into the bin: 150 grains. | GLAS, STNE, WALL wall | 700 | 60 s |
| Minimalist | ●●●○○ | Light the far LED using as few particles as you can. | METL, PSCN, WIFI | 400 | 30 s |
