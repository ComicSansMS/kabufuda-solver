
#include <algorithm>
#include <array>
#include <cassert>
#include <numeric>
#include <vector>

#include <optional>
#include <ranges>

#include <fmt/format.h>

class Card {
    int8_t c;

public:
    Card()
        :c(-1)
    {}

    explicit Card(int8_t card)
        :c(card)
    {
        assert((c >= 0) && (c < 10));
    }

    operator int8_t() const {
        return c;
    }

    friend bool operator==(Card const&, Card const&) = default;
};

inline Card empty = Card();

bool cardIsEmpty(Card const& c) {
    return c == empty;
}

template<>
struct fmt::formatter<Card>
{
    template<typename ParseContext>
    constexpr auto parse(ParseContext& ctx) {
        return ctx.begin();
    }

    template<typename FormatContext>
    auto format(Card const& c, FormatContext& ctx) {
        return fmt::format_to(ctx.out(), "{}", static_cast<int>(c));
    }
};


class CardStack {
    std::vector<Card> stack;

    bool is_collapsed = false;
public:
    CardStack() = default;

    CardStack(std::initializer_list<Card>&& cards)
        :stack(cards)
    {
        assert(std::ranges::none_of(cards, cardIsEmpty));
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
};

class SwapField {
    Card card;
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

    bool isLocked() const {
        return state == FieldState::Locked;
    }

    bool isFree() const {
        return state == FieldState::Free;
    }

    bool isOccupied() const {
        return state == FieldState::Occupied;
    }

    bool isCollapsed() const {
        return state == FieldState::Collapsed;
    }

    void pushCard(Card const& c) {
        assert(state == FieldState::Free);
        assert(card == empty);
        card = c;
        state = FieldState::Occupied;
    }

    void pushStack(Card const& c, int size) {
        if (size == 1) {
            pushCard(c);
        } else {
            assert(size == 4);
            pushCard(c);
            state = FieldState::Collapsed;
        }
    }

    Card getCard() const {
        assert((state == FieldState::Occupied) || (state == FieldState::Collapsed));
        return card;
    }

    void popCard() {
        assert(state == FieldState::Occupied);
        card = empty;
        state = FieldState::Free;
    }

    int size() const {
        switch (state) {
        case FieldState::Occupied: return 1;
        case FieldState::Collapsed: return 4;
        default: return 0;
        }
    }
};

template<>
struct fmt::formatter<SwapField>
{
    template<typename ParseContext>
    constexpr auto parse(ParseContext& ctx) {
        return ctx.begin();
    }

    template<typename FormatContext>
    auto format(SwapField const& s, FormatContext& ctx) {
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


int swapIndex(int i) {
    assert((i >= -4) && (i < 0));
    return (-i) - 1;
}

class Board {
public:
    std::array<SwapField, 4> swaps;
    std::array<CardStack, 8> field;

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

    bool isValid() const {
        int const cards_total =
            std::accumulate(begin(field), end(field), 0, [](int acc, CardStack const& s) { return acc + static_cast<int>(s.size()); }) +
            std::accumulate(begin(swaps), end(swaps), 0, [](int acc, SwapField const& s) { return acc + s.size(); });
        if (cards_total != 40) { return false; }
        
        for (int i = 0; i < 10; ++i) {
            Card const c = Card{ static_cast<int8_t>(i) };
            int count = 0;
            for (auto const& s : field) {
                count += static_cast<int>(std::ranges::count(s, c));
                if (std::ranges::count(s, empty) != 0) { return false; }
            }
            for (auto const& s : swaps) {
                if (s.isOccupied()) {
                    if (c == s.getCard()) { ++count; }
                } else if (s.isCollapsed()) {
                    if (c == s.getCard()) { count += 4; }
                }
            }
            if (count != 4) { return false; }
        }
        return true;
    }

    bool hasWon() const {
        for (auto const& s : field) {
            if (!s.isEmpty() || !s.isCollapsed()) { return false; }
        }
        for (auto const& s : swaps) {
            if (s.isOccupied()) { return false; }
        }
        return true;
    }

    int getMaxSize() const {
        auto const size_compare = [](CardStack const& s1, CardStack const& s2) { return s1.size() < s2.size(); };
        return static_cast<int>(std::ranges::max(field, size_compare).size());
    }

    CardStack& getField(int index) {
        assert((index >= 0) && (index < 8));
        return field[index];
    }

    CardStack const& getField(int index) const {
        assert((index >= 0) && (index < 8));
        return field[index];
    }

    SwapField& getSwap(int index) {
        return swaps[swapIndex(index)];
    }

    SwapField const& getSwap(int index) const {
        return swaps[swapIndex(index)];
    }
};

template<>
struct fmt::formatter<Board>
{
    template<typename ParseContext>
    constexpr auto parse(ParseContext& ctx) {
        return ctx.begin();
    }

    template<typename FormatContext>
    auto format(Board const& b, FormatContext& ctx) {
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

struct Move {
    int from;
    int to;
    int size;
};

template<>
struct fmt::formatter<Move>
{
    template<typename ParseContext>
    constexpr auto parse(ParseContext& ctx) {
        return ctx.begin();
    }

    template<typename FormatContext>
    auto format(Move const& m, FormatContext& ctx) {
        return fmt::format_to(ctx.out(), "{} card{} from {} -> {}", m.size, (m.size == 1) ? "" : "s", m.from, m.to);
    }
};

bool moveIsValid(Move const& m) {
    auto const fieldIsValid = [](int i) { return ((i > -4) && (i < 8)); };
    // can only move to and from valid fields
    if (!fieldIsValid(m.from) || !fieldIsValid(m.to)) { return false; }
    // can move between 1 and 4 cards
    if (!((m.size > 0) && (m.size < 5))) { return false; }
    // can not move in place
    if (m.from == m.to) { return false; }
    // can only move 1 card from swap
    if (m.from < 0) { 
        if (m.size != 1) { return false; }
    }
    // can only move 1 or 4 cards to swap
    if (m.to < 0) {
        if ((m.size != 1) && (m.size != 4)) { return false; }
    }
    return true;
}


bool moveIsValidForBoard(Board const& b, Move const& m)
{
    
    if (m.from < 0) {
        // can move from swap only if occupied
        if (!b.getSwap(m.from).isOccupied()) { return false; }
        // cam move only single card from stack
        if (m.size != 1) { return false; }
    } else {
        // can move from stack only if enough cards are available
        if (b.getField(m.from).size() < m.size) { return false; }
    }

    Card const& from_card = (m.from >= 0) ?
        b.getField(m.from).getTop() : b.getSwap(m.from).getCard();
    // target stack must be able to accept card
    if (m.to >= 0) {
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

    Card const c = (m.from < 0) ? (b.getSwap(m.from).getCard()) : (b.getField(m.from).getTop());
    // remove from
    if (m.from < 0) {
        assert(m.size == 1);
        b.getSwap(m.from).popCard();
    } else {
        b.getField(m.from).popCards(m.size);
    }
    // push to
    if (m.to < 0) {
        b.getSwap(m.to).pushStack(c, m.size);
    } else {
        CardStack& s = b.getField(m.to);
        s.pushStack(c, m.size);
        if (s.tryCollapse()) { b.unlockSwap(); }
    }

    assert(b.isValid());
    return b;
}

int main()
{
    fmt::print("*** KABUFUDA SOLITAIRE ***\n");

    Board b;
    b.field[0] = CardStack{ Card{ 8 }, Card{ 5 }, Card{ 1 }, Card{ 4 }, Card{ 9 } };
    b.field[1] = CardStack{ Card{ 4 }, Card{ 3 }, Card{ 0 }, Card{ 9 }, Card{ 2 } };
    b.field[2] = CardStack{ Card{ 0 }, Card{ 2 }, Card{ 0 }, Card{ 3 }, Card{ 2 } };
    b.field[3] = CardStack{ Card{ 5 }, Card{ 7 }, Card{ 7 }, Card{ 1 }, Card{ 1 } };
    b.field[4] = CardStack{ Card{ 5 }, Card{ 5 }, Card{ 3 }, Card{ 1 }, Card{ 7 } };
    b.field[5] = CardStack{ Card{ 9 }, Card{ 8 }, Card{ 4 }, Card{ 6 }, Card{ 4 } };
    b.field[6] = CardStack{ Card{ 6 }, Card{ 8 }, Card{ 8 }, Card{ 3 }, Card{ 6 } };
    b.field[7] = CardStack{ Card{ 0 }, Card{ 6 }, Card{ 2 }, Card{ 9 }, Card{ 7 } };

    assert(b.isValid());
    assert(b.field[0].getTopSize() == 1);
    assert(b.field[3].getTopSize() == 2);

    fmt::print("Board:\n{}\n", b);
    b = executeMove(b, Move{ .from = 1, .to = 2, .size = 1 });
    fmt::print("Board:\n{}\n", b);

    b = executeMove(b, Move{ .from = 0, .to = 1, .size = 1 });
    b = executeMove(b, Move{ .from = 7, .to = 4, .size = 1 });
    fmt::print("Board:\n{}\n", b);

    b = executeMove(b, Move{ .from = 1, .to = 7, .size = 2 });
    fmt::print("Board:\n{}\n", b);

    b = executeMove(b, Move{ .from = 0, .to = -1, .size = 1 });
    fmt::print("Board:\n{}\n", b);

    b = executeMove(b, Move{ .from = 3, .to = 0, .size = 2 });
    fmt::print("Board:\n{}\n", b);

    b = executeMove(b, Move{ .from = 4, .to = 3, .size = 2 });
    fmt::print("Board:\n{}\n", b);

    b = executeMove(b, Move{ .from = -1, .to = 5, .size = 1 });
    fmt::print("Board:\n{}\n", b);

    b = executeMove(b, Move{ .from = 3, .to = -1, .size = 4 });
    fmt::print("Board:\n{}\n", b);

    b = executeMove(b, Move{ .from = 0, .to = 4, .size = 3 });
    fmt::print("Board:\n{}\n", b);

    b = executeMove(b, Move{ .from = 3, .to = 0, .size = 1 });
    fmt::print("Board:\n{}\n", b);

    b = executeMove(b, Move{ .from = 4, .to = 3, .size = 4 });
    fmt::print("Board:\n{}\n", b);

    b = executeMove(b, Move{ .from = 4, .to = -2, .size = 1 });
    fmt::print("Board:\n{}\n", b);

    b = executeMove(b, Move{ .from = 0, .to = 4, .size = 1 });
    b = executeMove(b, Move{ .from = 0, .to = 4, .size = 1 });
    fmt::print("Board:\n{}\n", b);

    assert(b.isValid());
    assert(!b.hasWon());
}
