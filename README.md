# JOW

A plain English coding language. No symbols. No syntax to memorize. Just write how you think.

```
remember the name is archie
if the user says hello then say hello name
```

```
set score to ten
if the score is ten then say you win but if the score is 0 say you lose otherwise say keep playing
```

## Install

```bash
git clone https://github.com/YOUR_USERNAME/jow
cd jow
make install
```

Requires only `gcc`. No dependencies.

## Run

```bash
jow                # REPL
jow game.jow       # run a script
```

## Commands

| JOW | What it does |
|-----|-------------|
| `say hello world` | print text |
| `remember the name is archie` | store a variable |
| `set score to 10` | store a variable (alt syntax) |
| `show score` | print a variable |
| `ask what is your name` | get input → stored as `answer` |
| `if the score is ten then say you win` | condition |
| `if the score is ten say you win but if the score is 0 say you lose` | if / else if |
| `if the score is ten say you win otherwise say keep going` | if / else |
| `but dont answer if the score is 5` | silent branch (do nothing) |
| `if the user says hello then say hi` | listen for user input (auto-loops) |
| `repeat three times say go` | loop |
| `wait 2` | sleep 2 seconds |
| `in python train my ai` | run `python train_my_ai.py` |
| `run ls -la` | run any shell command |
| `make file notes.txt` | create a file |
| `delete file notes.txt` | delete a file |
| `bye` | exit |

## Word numbers

You can use words instead of digits: `zero one two three four five six seven eight nine ten eleven twelve thirteen fourteen fifteen sixteen seventeen eighteen nineteen twenty thirty forty fifty hundred thousand`

## Example — text adventure

```
remember the weapon is sword
remember the enemy is dragon
remember the place is dungeon
if the user says where am i then say you are in the place but if the user says what do i have then say you have a weapon but if the user says attack then say you swing your weapon at the enemy but if the user says run then say you flee the dungeon like a coward
```

## Variables persist

Variables are saved between sessions in `~/.jow_vars` so your game state is remembered.

## License

MIT
