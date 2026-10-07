# Escape Game Cardputer: Explorer 3

A 5-minute mini escape game for the **M5Stack Cardputer ADV**.

> Captain's log, Sol 1: our spaceship **Explorer 3** has crashed on Mars.
> To take off again, the crew must repair the ship and find the rocket's start-up code…
> before the oxygen runs out!

*This is the English version (`english` branch). The French version is on the `main` branch.*

## The game

- **4 puzzles**, solved in order. Each one opens part of the ship and reveals one letter of the 4-letter **start-up code**.
- **5 minutes of oxygen.** A gauge and a countdown stay at the top of the screen, and a "beep beep beep" alarm gets faster as the oxygen runs low.
- **Each wrong answer costs 10 seconds.**
- Once the code is found, type it on the keyboard to launch the **liftoff** (animation and sound), followed by a pixel art end screen.
- If the oxygen reaches zero, the game is lost ("Out of oxygen" screen). You can play again right away.
- The **record** (oxygen left at the end) is kept in memory, even after power off.

### The puzzles

| # | Place | Puzzle type |
|---|-------|-------------|
| 1 | Soldering iron safe | A light blinking in **Morse code** |
| 2 | Fuel tanks | A **space trivia** question (multiple choice) |
| 3 | Spare parts storage | A 5×5 **picross** (nonogram) |
| 4 | On-board computer | An **audio Morse** signal (the alarm goes quiet so you can hear it) |

The solutions are not given here. ⚠️ **Players: don't read the source code, it contains the answers!**

## Controls

| Key | Action |
|-----|--------|
| `ENTER` | Confirm, continue, start, play again |
| Letters | Answer the puzzles, type the code |
| `SPACE` | Replay the Morse signal (puzzles 1 and 4) |
| `TAB` | Show or hide the Morse code chart (puzzles 1 and 4) |
| `;` `.` `,` `/` | Move the picross cursor (up, down, left, right) |
| `ENTER` (picross) | Fill or clear a cell |
| `DEL` | Erase a letter of the code |

### For the game master

Press **`Fn` 3 times in a row** (less than 0.8 s between presses) to **pause** the game: the countdown stops, the sound is muted and the puzzle is hidden. Press **`Fn` 3 times** again to resume.

## Broadcast mode: the game on a PC screen too

At start-up, choose **Solo** (the game on the Cardputer only) or **Broadcast**: the Cardputer screen is mirrored live in the web browser of a PC on the same home network, and **the sound comes out of the PC** (the Cardputer stays silent). You still play with the Cardputer keyboard.

1. Choose **Broadcast**, then the Wi-Fi network and type its password (saved for next time).
2. The Cardputer shows an address, for example `http://192.168.1.42`: open it in the PC's browser (Chrome, Edge, Firefox). Nothing to install.
3. Click the page once to **enable sound** (browsers require it). Double-click: full screen.
4. `ENTER` on the Cardputer to start the game.

### The coded keypad (team play)

In broadcast mode the start-up code is not typed in letters: the on-board computer shows a **coded keypad**, 9 symbols on keys `1` to `9` (♥ ☺ ♪ ☼…). Only the **PC team** sees the **table** giving the symbol of each letter (at that moment the PC no longer shows the Cardputer screen). The Cardputer player says a letter, the PC team describes the symbol, and so on.

- The table is shuffled every game.
- `1` to `9`: pick a symbol, `DEL`: erase, `ENTER`: confirm the 4 symbols (−10 s if the code is wrong).
- The PC page also shows the oxygen countdown and how many symbols have been typed, and flashes red on each error.
- Place the PC so that the Cardputer player can't see its screen!

- `` ` `` on the connecting or address screen: choose another Wi-Fi network.
- The Cardputer only supports 2.4 GHz Wi-Fi. "Guest" networks often block traffic between devices.
- If the page loses the connection, it reconnects by itself; the game goes on on the Cardputer.

## Installation

### With M5Launcher (SD card)

1. Download `Escape-Game-Explorer-3-EN.bin` from the [Releases](../../releases) page.
2. Copy it to the `apps/` folder of the SD card.
3. On the Cardputer, pick the file in the M5Launcher SD menu to install it.

### Build it yourself

The project uses [PlatformIO](https://platformio.org/):

```
pio run                # build
pio run -t upload      # build and flash over USB
```

- Platform `espressif32 @ 6.7.0` (Arduino core 2.0.x)
- Libraries: M5Cardputer, M5Unified, M5GFX, WebSockets (links2004)
- The game: `src/main.cpp`; broadcast mode (Wi-Fi, web page, screen and sound stream): `src/diffusion.cpp`

## Hardware

Made for the **Cardputer ADV** (ESP32-S3, no PSRAM, built-in speaker). No accessory needed.

## License

[MIT](LICENSE)
