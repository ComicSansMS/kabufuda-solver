Kabufuda Solitaire
===

A solver for the Kabufuda Solitaire games from Zachtronics.

The solver performs a primitive backtracking search. The only heuristic being used is that moves that move many cards at once are preferred over moves with fewer cards.

Build with CMake and your favorite C++20 compiler.

Usage
---

Pass a text file containing the puzzle input as argument

    kabufuda_solver puzzle_input.txt

Puzzle Input files are plain text files which are structured as follows:

````
 5   2   9   9   0   4   2   6
 6   4   2   1   8   0   0   0
 4   6   2   1   3   5   3   8
 8   3   7   1   7   5   4   9
 7   6   3   1   8   5   7   9
Hard
````

Cards are represented as numbers from 0-9.

Cards are expected to be arranged in 8 stacks with 5 cards each.

Optionally, a difficulty level (Easy, Medium, Hard, Expert) can be given, which determines the number of swap slots available at the start of the puzzle. If no difficulty is specified, Expert difficulty will be used.

