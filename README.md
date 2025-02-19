# Multiplayer Battle Game Server

## Overview
This project implements a **multiplayer text-based battle game server** using **Unix sockets**. The game is inspired by **Pokemon battles**, where two players engage in combat until one player loses all hitpoints. Players can attack, use powerful moves, or send messages during the game.

## Demo Video
- A **demo video** of the server running is available on YouTube.
- **Watch the demo here**: [Video Demo](https://youtu.be/RIzUR5u3R8c)

## Features
- **Client Handling**: Supports multiple clients connecting via `nc` (netcat).
- **Matchmaking System**: Pairs players dynamically based on availability.
- **Battle Mechanics**:
  - Randomized hitpoints and power moves for each match.
  - Regular and power attacks with different damage mechanics.
  - Turn-based attacks and in-game messaging.
- **Drop Handling**: Detects client disconnections and announces winners.
- **Concurrency**: Uses `select()` for non-blocking socket communication.
- **Port Selection**: Allows defining the port number at compile time.

## Installation and Compilation
1. Clone the repository:
   ```bash
   git clone <repo_link>
   ```
2. Compile the server using the Makefile:
   ```bash
   make PORT=<your_port>
   ```
   - The default port is set based on the student number convention (`5XXXX`).
   - Example:
     ```bash
     make PORT=53456
     ```
3. Run the server:
   ```bash
   ./battle
   ```

## Running the Game
1. Open a terminal and start the server:
   ```bash
   ./battle
   ```
2. Connect clients using `nc`:
   ```bash
   stty -icanon
   nc <server_ip> <port>
   ```
   Example:
   ```bash
   nc cs.utm.utoronto.ca 53456
   ```
3. Enter your name and wait for an opponent.
4. Use the following commands during battle:
   - `a` - Regular attack
   - `p` - Power move (if available)
   - `s` - Say something to the opponent

## Game Rules
- Players start each match with **20-30 HP** and **1-3 power moves**.
- Regular attacks deal **2-6 HP** damage.
- Power moves have a **50% chance to miss**, but deal **3x damage** if successful.
- Players take turns attacking, and only the **active player** can speak.
- Players wait for a new opponent after a match ends.

## Enhancements
This project includes additional features beyond the basic requirements:
- **Strategic Gameplay Addition**: Players can choose a special ability at the start of a match, such as **increased dodge chance** or **extra power moves**.
- **Message Limit**: Added a message limit by introducing a message counter attribute to the client struct. Each player's message count resets to **0** when they attack, preventing excessive messaging and encouraging balanced gameplay.

## Handling Client Disconnections
- If a player disconnects (`Ctrl+C`), their opponent **automatically wins**.
- The disconnected player is removed, and the server waits for new opponents.

## Testing
- Use multiple `nc` clients to test.
- Run in **noncanonical mode** (`stty -icanon`) for proper interaction.
- Verify matchmaking and battle mechanics with various test cases.
