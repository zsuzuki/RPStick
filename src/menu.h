#include <vector>
#include <string>

/// メニュー基底クラス
class Menu
{
    const char *caption_;

public:
    Menu(const char *c) : caption_(c) {}
    const char *caption() const { return caption_; }
    virtual std::string valueString() const = 0;
    virtual int up() = 0;
    virtual int down() = 0;
    virtual Menu *decide() = 0;
    virtual void cancel() = 0;
};

/// 子メニュー
class MenuChild : public Menu
{
    std::vector<Menu *> menuList_;
    int index_;

public:
    MenuChild(const char *c, std::vector<Menu *> &&m)
        : Menu(c), menuList_(std::move(m)), index_(0) {}
    std::string valueString() const override { return menuList_[index_]->caption(); }
    int up() override
    {
        index_ = (index_ + 1) % menuList_.size();
        return index_;
    }
    int down() override
    {
        index_ = (index_ + menuList_.size() - 1) % menuList_.size();
        return index_;
    }
    Menu *decide() override { return menuList_[index_]; }
    void cancel() override {}
};

/// 数値入力
struct MenuInt : public Menu
{
    const char *caption_;
    int value_;
    int edit_value_;
    int min_;
    int max_;
    int step_;
    const char *suffix_;

public:
    MenuInt(const char *c, int v, int min = 0, int max = 100, int step = 1, const char *sf = nullptr)
        : Menu(c), value_(v), edit_value_(v), min_(min), max_(max), step_(step), suffix_(sf) {}
    std::string valueString() const override { return std::to_string(edit_value_) + suffix_; }
    int up() override
    {
        edit_value_ = std::min(edit_value_ + step_, max_);
        return edit_value_;
    }
    int down() override
    {
        edit_value_ = std::max(edit_value_ - step_, min_);
        return edit_value_;
    }
    Menu *decide() override
    {
        value_ = edit_value_;
        return nullptr;
    }
    void cancel() override { edit_value_ = value_; }
    int value() const { return value_; }
};

/// リスト選択
struct MenuList : public Menu
{
    std::vector<const char *> labels_;
    int index_;
    int edit_index_;

public:
    MenuList(const char *c, std::vector<const char *> &&l)
        : Menu(c), labels_(std::move(l)), index_(0), edit_index_(0) {}
    std::string valueString() const override { return labels_[edit_index_]; }
    int up() override
    {
        edit_index_ = (edit_index_ + 1) % labels_.size();
        return edit_index_;
    }
    int down() override
    {
        edit_index_ = (edit_index_ + labels_.size() - 1) % labels_.size();
        return edit_index_;
    }
    Menu *decide() override
    {
        index_ = edit_index_;
        return nullptr;
    }
    void cancel() override { edit_index_ = index_; }
    int index() const { return index_; }
};

/// メソッド実行
struct MenuFunc : public Menu
{
    void (*func_)() = nullptr;

public:
    MenuFunc(const char *c, void (*f)()) : Menu(c), func_(f) {}
    std::string valueString() const override { return {"execute"}; }
    int up() override { return 0; }
    int down() override { return 0; }
    Menu *decide() override
    {
        if (func_)
            func_();
        return nullptr;
    }
    void cancel() override {}
};
