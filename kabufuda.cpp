/*
Copyright (c) 2022 Andreas Weis (der_ghulbus@ghulbus-inc.de)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <algorithm>
#include <array>
#include <cassert>
#include <numeric>
#include <ranges>
#include <unordered_set>
#include <unordered_map>
#include <vector>

#include <fmt/format.h>
#include <fmt/color.h>

#include <fstream>
#include <regex>
#include <string>
#include <string_view>
#include <sstream>
#include <optional>

#include <chrono>

namespace {
template <class T>
constexpr inline void hash_combine(std::size_t& seed, const T& v) noexcept
{
    std::hash<T> hasher;
    seed ^= hasher(v) + 0x9e3779b9 + (seed<<6) + (seed>>2);
}
}

/** Cards are of one of the ten suits 0-9.
 */
class Card {
    int8_t c;

public:
    explicit Card(int8_t card)
        :c(card)
    {
        assert((c >= 0) && (c < 10));
    }

    operator int8_t() const {
        return c;
    }

    friend bool operator==(Card const&, Card const&) = default;
    friend bool operator!=(Card const&, Card const&) = default;
};

namespace std
{
template<> struct hash<Card>
{
    std::size_t operator()(Card const& c) const noexcept
    {
        std::size_t h = 0;
        hash_combine(h, c);
        return h;
    }
};
}

template<>
struct fmt::formatter<Card>
{
    template<typename ParseContext>
    constexpr auto parse(ParseContext& ctx) {
        return ctx.begin();
    }

    template<typename FormatContext>
    auto format(Card const& c, FormatContext& ctx) const {
        std::array<fmt::rgb, 10> palette = {
            fmt::rgb(0xfb, 0xef, 0xcc),
            fmt::rgb(0xf9, 0xcc, 0xac),
            fmt::rgb(0xf4, 0xa6, 0x88),
            fmt::rgb(0xe0, 0x87, 0x6a),
            fmt::rgb(0xff, 0xf2, 0xdf),
            fmt::rgb(0xd9, 0xad, 0x7c),
            fmt::rgb(0xa2, 0x83, 0x6e),
            fmt::rgb(0x67, 0x4d, 0x3c),
            fmt::rgb(0xa8, 0x8f, 0xac),
            fmt::rgb(0x82, 0x81, 0x6d)
        };


        fmt::fg(fmt::color());
        return fmt::format_to(ctx.out(), fmt::fg(palette[static_cast<int8_t>(c)]), "{}", static_cast<int>(c));
    }
};

/** A stack of cards on the board.
 *
 * Each consists of an arbitrary number of cards stacked on top of each other.
 * Only the top cards of matching suit are accessible.
 * If a stack consists only of the four cards making up a suit, it can be collapsed.
 * Once collapsed, the stack can not be changed anymore.
 */
class CardStack {
    std::vector<Card> stack;

    bool is_collapsed = false;
public:
    CardStack() = default;

    CardStack(std::initializer_list<Card>&& cards)
        :stack(cards)
    {
    }

    Card const& getTop() const
    {
        return stack.back();
    }

    int getTopSize() const
    {
        auto rng = stack 
            | std::ranges::views::reverse
            | std::ranges::views::take_while([top = getTop()](Card const& c) { return c == top; });
        return static_cast<int>(std::ranges::distance(rng));
    }

    bool isEmpty() const {
        return stack.empty();
    }

    void pushCard(Card const& c)
    {
        assert(!isCollapsed());
        stack.push_back(c);
    }

    void pushStack(Card const& c, int size)
    {
        assert(!isCollapsed());
        for (int i = 0; i < size; ++i) {
            stack.push_back(c);
        }
    }

    void popCards(int size) {
        assert(!isCollapsed());
        assert(size <= getTopSize());
        for (int i = 0; i < size; ++i) {
            stack.pop_back();
        }
    }

    bool isCollapsed() const {
        return is_collapsed;
    }

    bool tryCollapse() {
        if ((stack.size() == 4) && (getTopSize() == 4)) {
            is_collapsed = true;
        }
        return isCollapsed();
    }

    auto begin() const { return std::cbegin(stack); }
    auto end() const { return std::cend(stack); }
    auto size() const { return stack.size(); }

    friend bool operator==(CardStack const&, CardStack const&) noexcept = default;
    friend bool operator!=(CardStack const&, CardStack const&) noexcept = default;
};

/** A swap field (free cell) on the top of the playing board.
 *
 * A swap field can hold a single card, once it has been unlocked.
 * Alternativaly it can hold a stack of four cards of the same suit,
 * which will collapse the field and prevent any future changes to it.
 */
class SwapField {
    std::optional<Card> card;
    enum FieldState {
        Locked,
        Free,
        Occupied,
        Collapsed
    } state;
public:
    SwapField()
        :card(), state(FieldState::Locked)
    {}

    void unlock()
    {
        assert(state == FieldState::Locked);
        state = FieldState::Free;
    }

    bool isLocked() const
    {
        return state == FieldState::Locked;
    }

    bool isFree() const
    {
        return state == FieldState::Free;
    }

    bool isOccupied() const
    {
        return state == FieldState::Occupied;
    }

    bool isCollapsed() const
    {
        return state == FieldState::Collapsed;
    }

    void pushCard(Card const& c)
    {
        assert(state == FieldState::Free);
        assert(!card);
        card = c;
        state = FieldState::Occupied;
    }

    void pushStack(Card const& c, int size)
    {
        if (size == 1) {
            pushCard(c);
        } else {
            assert(size == 4);
            pushCard(c);
            state = FieldState::Collapsed;
        }
    }

    Card getCard() const
    {
        assert((state == FieldState::Occupied) || (state == FieldState::Collapsed));
        return *card;
    }

    void popCard()
    {
        assert(state == FieldState::Occupied);
        card = std::nullopt;
        state = FieldState::Free;
    }

    int size() const
    {
        switch (state) {
        case FieldState::Occupied: return 1;
        case FieldState::Collapsed: return 4;
        default: return 0;
        }
    }

    friend bool operator==(SwapField const&, SwapField const&) noexcept = default;
    friend bool operator!=(SwapField const&, SwapField const&) noexcept = default;
};

template<>
struct fmt::formatter<SwapField>
{
    template<typename ParseContext>
    constexpr auto parse(ParseContext& ctx)
    {
        return ctx.begin();
    }

    template<typename FormatContext>
    auto format(SwapField const& s, FormatContext& ctx) const
    {
        if (s.isLocked()) {
            return fmt::format_to(ctx.out(), "<X>");
        } else if (s.isOccupied()) {
            return fmt::format_to(ctx.out(), "<{}>", s.getCard());
        } else if (s.isFree()) {
            return fmt::format_to(ctx.out(), "< >");
        } else if (s.isCollapsed()) {
            return fmt::format_to(ctx.out(), "-{}-", s.getCard());
        } else {
            return fmt::format_to(ctx.out(), "@@@");
        }
    }
};

enum class Difficulty {
    Easy,       // 4 free swaps
    Normal,     // 3 free swaps
    Hard,       // 2 free swaps
    Expert      // 1 free swap
};

/** The playing board, consisting of 8 card stacks and 4 swap fields.
 */
class Board {
public:
    std::array<SwapField, 4> swaps;
    std::array<CardStack, 8> field;

private:
    static int swapIndex(int i) {
        assert((i >= -4) && (i < 0));
        return (-i) - 1;
    }
public:

    Board(Difficulty d)
    {
        int const free_swaps = [](Difficulty d) {
            switch (d) {
            case Difficulty::Easy:      return 4;
            case Difficulty::Normal:    return 3;
            case Difficulty::Hard:      return 2;
            default:                    return 1;
            }
        }(d);
        for (int i = 0; i < free_swaps; ++i) { unlockSwap(); }
    }

    Board()
        :Board(Difficulty::Expert)
    {}

    void unlockSwap()
    {
        for (int i = 0, i_end = static_cast<int>(swaps.size()); i != i_end; ++i) {
            SwapField& swap = swaps[i];
            if (swap.isLocked()) {
                swap.unlock();
                break;
            }
        }
    }

    bool isValid() const
    {
        // there should be 40 cards in total
        int const cards_total =
            std::accumulate(begin(field), end(field), 0, [](int acc, CardStack const& s) { return acc + static_cast<int>(s.size()); }) +
            std::accumulate(begin(swaps), end(swaps), 0, [](int acc, SwapField const& s) { return acc + s.size(); });
        if (cards_total != 40) { fmt::print("Invalid card total. Expected: 40. Found: {}.\n", cards_total); return false; }

        // for each of the 10 suits we should have exactly 4 cards on the board
        bool is_valid = true;
        for (int i = 0; i < 10; ++i) {
            Card const c = Card{ static_cast<int8_t>(i) };
            int count = 0;
            for (auto const& s : field) {
                count += static_cast<int>(std::ranges::count(s, c));
            }
            for (auto const& s : swaps) {
                if (s.isOccupied()) {
                    if (c == s.getCard()) { ++count; }
                } else if (s.isCollapsed()) {
                    if (c == s.getCard()) { count += 4; }
                }
            }
            if (count != 4) { fmt::print("Invalid count for suit {}. Expected: 4. Found: {}.\n", i, count); is_valid = false; }
        }
        return is_valid;
    }

    bool hasWon() const
    {
        // we have won if there are no cards on the board that are not part of a collapsed field
        for (auto const& s : field) {
            if (!s.isEmpty() && !s.isCollapsed()) { return false; }
        }
        for (auto const& s : swaps) {
            if (s.isOccupied()) { return false; }
        }
        return true;
    }

    int getMaxSize() const
    {
        auto const size_compare = [](CardStack const& s1, CardStack const& s2) { return s1.size() < s2.size(); };
        return static_cast<int>(std::ranges::max(field, size_compare).size());
    }

    CardStack& getField(int index)
    {
        assert((index >= 0) && (index < 8));
        return field[index];
    }

    CardStack const& getField(int index) const
    {
        assert((index >= 0) && (index < 8));
        return field[index];
    }

    SwapField& getSwap(int index)
    {
        return swaps[swapIndex(index)];
    }

    SwapField const& getSwap(int index) const
    {
        return swaps[swapIndex(index)];
    }

    friend bool operator==(Board const&, Board const&) noexcept = default;
    friend bool operator!=(Board const&, Board const&) noexcept = default;
};

namespace std
{
template<> struct hash<Board>
{
    std::size_t operator()(Board const& b) const noexcept
    {
        std::size_t h = 0;
        for (auto const& s : b.field) {
            hash_combine(h, std::accumulate(s.begin(), s.end(), std::size_t{ 0 },
                                            [](std::size_t acc, Card const& c) { return acc * 10 + c; }));
        }
        for (auto const& s : b.swaps) {
            if (s.isLocked()) {
                hash_combine(h, 11111111);
            } else if (s.isFree()) {
                hash_combine(h, 0);
            } else if (s.isOccupied()) {
                hash_combine(h, 1000 * s.getCard());
            } else if (s.isCollapsed()) {
                hash_combine(h, -s.getCard());
            }
        }
        return h;
    }
};
}

template<>
struct fmt::formatter<Board>
{
    template<typename ParseContext>
    constexpr auto parse(ParseContext& ctx)
    {
        return ctx.begin();
    }

    template<typename FormatContext>
    auto format(Board const& b, FormatContext& ctx) const {
        // print swaps
        fmt::format_to(ctx.out(), "Swaps: ");
        for (auto const& s : b.swaps) {
            fmt::format_to(ctx.out(), "{} ", s);
        }
        fmt::format_to(ctx.out(), "\n");
        // print stacks
        int max_depth = b.getMaxSize();
        for (int i = 0; i < max_depth; ++i) {
            for (auto const& s : b.field) {
                fmt::format_to(ctx.out(), " ");
                if (i < s.size()) {
                    if (s.isCollapsed()) {
                        fmt::format_to(ctx.out(), "-{}-", *(s.begin() + i));
                    } else {
                        fmt::format_to(ctx.out(), " {} ", *(s.begin() + i));
                    }
                } else {
                    fmt::format_to(ctx.out(), "   ");
                }
            }
            fmt::format_to(ctx.out(), "\n");
        }
        return ctx.out();
    }
};


std::optional<std::string> readInputFile(char const* filename)
{
    std::ifstream fin(filename);
    if (!fin) {
        fmt::print(stderr, "Unable to open input file '{}' for reading.\n", filename);
        return std::nullopt;
    }

    std::stringstream sstr;
    sstr << fin.rdbuf();
    if (!fin) {
        fmt::print(stderr, "Unable to read input from file '{}'.\n", filename);
        return std::nullopt;
    }
    return sstr.str();
}

Board parseBoard(std::string_view input)
{
    std::regex rx_difficulty(R"(^(Easy|Medium|Hard|Expert)$)");
    std::match_results<std::string_view::iterator> smatch;
    bool const has_difficulty = std::regex_search(begin(input), end(input), smatch, rx_difficulty);
    
    Difficulty const difficulty = [has_difficulty, &smatch]() {
        if (has_difficulty) {
            if (smatch[1] == "Easy") {
                return Difficulty::Easy;
            } else if(smatch[1] == "Medium") {
                return Difficulty::Normal;
            } else if(smatch[1] == "Hard") {
                return Difficulty::Hard;
            }
        }
        return Difficulty::Expert;
    }();
    Board ret{ difficulty };

    std::regex rx_line(R"(^\s*(\d)\s+(\d)\s+(\d)\s+(\d)\s+(\d)\s+(\d)\s+(\d)\s+(\d)\s*?$)");
    for (int iline = 0; iline < 5; ++iline) {
        bool const does_match = std::regex_search(begin(input), end(input), smatch, rx_line);
        if (!does_match) {
            fmt::print(stderr, "Invalid board data.\n");
            return {};
        }
        for (int i = 0; i < 8; ++i) {
            auto const m = smatch[i + 1];
            ret.field[i].pushCard(Card{ static_cast<std::int8_t>(std::stoi(m.str())) });
        }
        input.remove_prefix(input.length() - smatch.suffix().length());
    }

    return ret;
}

/** A move takes 1-4 cards from one place to another.
 * The to and from fields contain an index to a card stack or swap field on the board.
 * Card Stacks are indexed left-to-right by the positive indices [0..8).
 * Swap Fields are indexed right-to-left by the negative indices [-4..-1]
 */
struct Move {
    int from;
    int to;
    int size;

    friend bool operator==(Move const&, Move const&) noexcept = default;
};

bool isFieldIndex(int i)
{
    assert((i >= -4) && (i < 8));
    return i >= 0;
}

bool isSwapIndex(int i)
{
    assert((i >= -4) && (i < 8));
    return i < 0;
}

template<>
struct fmt::formatter<Move>
{
    template<typename ParseContext>
    constexpr auto parse(ParseContext& ctx)
    {
        return ctx.begin();
    }

    template<typename FormatContext>
    auto format(Move const& m, FormatContext& ctx) const
    {
        return fmt::format_to(ctx.out(), "{} card{} from {} -> {}", m.size, (m.size == 1) ? "" : "s", m.from, m.to);
    }
};

bool moveIsValid(Move const& m)
{
    auto const fieldIsValid = [](int i) { return ((i >= -4) && (i < 8)); };
    // can only move to and from valid fields
    if (!fieldIsValid(m.from) || !fieldIsValid(m.to)) { return false; }
    // can move between 1 and 4 cards
    if (!((m.size > 0) && (m.size < 5))) { return false; }
    // can not move in place
    if (m.from == m.to) { return false; }
    // can only move 1 card from swap
    if (isSwapIndex(m.from)) {
        if (m.size != 1) { return false; }
    }
    // can only move 1 or 4 cards to swap
    if (isSwapIndex(m.to)) {
        if ((m.size != 1) && (m.size != 4)) { return false; }
    }
    return true;
}


bool moveIsValidForBoard(Board const& b, Move const& m)
{
    
    if (isSwapIndex(m.from)) {
        // can move from swap only if occupied
        if (!b.getSwap(m.from).isOccupied()) { return false; }
        // cam move only single card from stack
        if (m.size != 1) { return false; }
    } else {
        // can move from stack only if enough cards are available
        if (b.getField(m.from).size() < m.size) { return false; }
    }

    Card const& from_card = isFieldIndex(m.from) ?
        b.getField(m.from).getTop() : b.getSwap(m.from).getCard();
    // target stack must be able to accept card
    if (isFieldIndex(m.to)) {
        CardStack const& to_stack = b.field[m.to];
        // stack must either have a matching card at top or be empty
        if (!to_stack.isEmpty()) {
            if (to_stack.getTop() != from_card) { return false; }
        }
    } else {
        // can only move to free swap
        if (!b.getSwap(m.to).isFree()) { return false; }
    }

    // can only move matching cards together
    if (m.size > 1) {
        CardStack const& from_stack = b.field[m.from];
        if (from_stack.getTopSize() < m.size) { return false; }
    }

    return true;
}

Board executeMove(Board b, Move const& m)
{
    assert(moveIsValid(m));
    assert(moveIsValidForBoard(b, m));

    Card const c = isSwapIndex(m.from) ? (b.getSwap(m.from).getCard()) : (b.getField(m.from).getTop());
    // remove from
    if (isSwapIndex(m.from)) {
        assert(m.size == 1);
        b.getSwap(m.from).popCard();
    } else {
        b.getField(m.from).popCards(m.size);
    }
    // push to
    if (isSwapIndex(m.to)) {
        b.getSwap(m.to).pushStack(c, m.size);
    } else {
        CardStack& s = b.getField(m.to);
        s.pushStack(c, m.size);
        if (s.tryCollapse()) { b.unlockSwap(); }
    }

    assert(b.isValid());
    return b;
}

std::vector<Move> getAllValidMoves(Board const& b)
{
    std::vector<Move> ret;

    for (int i_from = -4; i_from < 8; ++i_from) {
        // are there cards in from slot?
        auto const max_count = [&b](int idx) {
            if (isSwapIndex(idx)) {
                SwapField const& s = b.getSwap(idx);
                return (s.isOccupied()) ? 1 : 0;
            } else {
                CardStack const& s = b.getField(idx);
                if (s.isEmpty() || s.isCollapsed()) { return 0; }
                return s.getTopSize();
            }
        }(i_from);
        if (max_count == 0) { /* no cards to move in from slot */ continue; }
        assert((max_count >= 1) && (max_count <= 4));
        // get the from card
        Card const c = isSwapIndex(i_from) ? b.getSwap(i_from).getCard() : b.getField(i_from).getTop();
        // check for a matching to slot
        for (int i_to = -4; i_to < 8; ++i_to) {
            if (i_to == i_from) { continue; }
            if (isSwapIndex(i_to)) {
                // move to swap field
                SwapField const& s = b.getSwap(i_to);
                if (s.isFree()) {
                    ret.push_back(Move{ .from = i_from, .to = i_to, .size = 1 });
                    if (max_count == 4) {
                        ret.push_back(Move{ .from = i_from, .to = i_to, .size = 4 });
                    }
                }
            } else {
                // move to swap card stack
                CardStack const& s = b.getField(i_to);
                if (s.isEmpty() || s.getTop() == c) {
                    for (int i_count = 1; i_count <= max_count; ++i_count) {
                        ret.push_back(Move{ .from = i_from, .to = i_to, .size = i_count });
                    }
                }
            }
        }
    }

    // Arrange moves by putting moves with more cards first
    // Without this, traversing the search space will take very long
    std::ranges::sort(ret, [](Move const& lhs, Move const& rhs) { return rhs.size < lhs.size; });

    return ret;
}

std::vector<Move> solve(Board b)
{
    if (b.hasWon()) { return {}; }
    struct State {
        Board b;
        std::vector<Move> valid_moves;
        std::size_t current_move = 0;
    };
    std::vector<State> stack;
    std::vector<Move> move_stack;
    std::unordered_set<Board> boards;

    // traverse the search space in a depth-first manner, favoring moves that move many cards at once
    stack.push_back(State{ .b = b, .valid_moves = getAllValidMoves(b) });
    while (!stack.empty()) {
        auto const& [board, moves, current_move] = stack.back();
        if (current_move == moves.size()) {
            // no more moves at this level; backtrack
            stack.pop_back();
            move_stack.pop_back();
            continue;
        } else {
            auto const& m = moves[current_move];
            ++stack.back().current_move;
            Board const new_board = executeMove(board, m);
            if (boards.contains(new_board)) { continue; }
            boards.insert(new_board);
            move_stack.push_back(m);
            if (new_board.hasWon()) { fmt::print("!!! We have a winner !!!\n"); break; }
            stack.push_back(State{ .b = new_board, .valid_moves = getAllValidMoves(new_board) });
        }
    }

    return move_stack;
}

int main(int argc, char* argv[])
{
    fmt::print("*** KABUFUDA SOLITAIRE ***\n");

    if (argc != 2) {
        fmt::print("\nUsage: {} <input_file.txt>\n", argv[0]);
        return 0;
    }

    auto const in = readInputFile(argv[1]);
    if (!in) {
        fmt::print("Error reading input file {}\n", argv[1]);
        return 1;
    }
    Board const b = parseBoard(*in);
    if (!b.isValid()) {
        fmt::print("Input file {} does not contain a valid puzzle input.\n"
                   "\nHere's what I got from that file:\n{}\n", argv[1], b);
        return 1;
    }

    fmt::print("Puzzle input:\n{}\n", b);

    auto const t0 = std::chrono::steady_clock::now();
    auto const moves = solve(b);
    auto const t1 = std::chrono::steady_clock::now();

    if (moves.empty()) {
        fmt::print("Could not find a solution. :(\n");
    } else {
        fmt::print("*** Winning Moves: ***\n");
        for (auto const m : moves) {
            fmt::print(" - {}\n", m);
        }
        Board b2 = b;
        fmt::print("Board:\n{}\n", b2);
        for (auto const m : moves) {
            b2 = executeMove(b2, m);
            fmt::print("Moving {}:\n{}\n", m, b2);
        }
    }

    fmt::print("\nSolving the puzzle took {} ms.\n",
               std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count());

    return 0;
}
