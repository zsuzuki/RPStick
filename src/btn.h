#include <Arduino.h>

class Button
{
    int pin_;
    int count_;
    bool press_;
    bool prev_;
    bool on_;
    bool repeat_;

public:
    static int REPEAT_START;
    static int REPEAT_CONTINUE;

    Button(int p) : pin_(p), count_(0), press_(false), prev_(false), on_(false), repeat_(false)
    {
    }

    void init()
    {
        pinMode(pin_, INPUT_PULLUP);
    }

    void update()
    {
        prev_ = press_;
        press_ = digitalRead(pin_) == LOW;
        on_ = press_ && !prev_;
        count_ = press_ ? count_ + 1 : 0;
        repeat_ = false;
        if (count_ >= REPEAT_START / 10)
        {
            if (count_ % (REPEAT_CONTINUE / 10) == 0)
                repeat_ = true;
        }
    }

    bool on() const { return on_; }
    bool pressed() const { return press_; }
    bool release() const { return !press_ && prev_; }
    bool repeat() const { return on_ || repeat_; }
    bool long_pressed(int ms) { return count_ > (ms / 10); }
    int press_count() const { return count_ * 10; }
};
