#pragma once
// Scripted console input for application tests: feeds lines to the program one by one and can
// run an action (e.g. "advance the clock 61 s") exactly when the program asks for the next line.
#include <deque>
#include <functional>
#include <istream>
#include <streambuf>
#include <string>
#include <vector>

class ScriptedInput : public std::istream {
public:
    ScriptedInput() : std::istream(&buf_) {}

    ScriptedInput& lines(const std::vector<std::string>& ls) {
        for (const auto& l : ls) buf_.push({false, l, nullptr});
        return *this;
    }
    ScriptedInput& line(const std::string& l) { return lines({l}); }
    ScriptedInput& act(std::function<void()> fn) {
        buf_.push({true, "", std::move(fn)});
        return *this;
    }

private:
    struct Item {
        bool isAction;
        std::string text;
        std::function<void()> fn;
    };
    class Buf : public std::streambuf {
    public:
        void push(Item item) { items_.push_back(std::move(item)); }

    protected:
        int_type underflow() override {
            while (!items_.empty()) {
                Item item = std::move(items_.front());
                items_.pop_front();
                if (item.isAction) { item.fn(); continue; }
                current_ = item.text + "\n";
                setg(&current_[0], &current_[0], &current_[0] + current_.size());
                return traits_type::to_int_type(current_[0]);
            }
            return traits_type::eof();
        }

    private:
        std::deque<Item> items_;
        std::string current_;
    };
    Buf buf_;
};
